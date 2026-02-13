#include "ChordInspectorComponent.h"
#include "ResizeCursors.h"

ChordInspectorComponent::ChordInspectorComponent()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Chords", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE6B3));
    titleLabel.setFont (juce::FontOptions (13.5f, juce::Font::bold));

    addAndMakeVisible (chordList);
    chordList.setModel (this);
    chordList.setRowHeight (34);

    addAndMakeVisible (barHeaderLabel);
    barHeaderLabel.setText ("Bar", juce::dontSendNotification);
    barHeaderLabel.setJustificationType (juce::Justification::centredLeft);
    barHeaderLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFE8CC94));
    barHeaderLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));

    addAndMakeVisible (staffLabel);
    staffLabel.setJustificationType (juce::Justification::centredLeft);
    staffLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD7BE8D));
    staffLabel.setFont (juce::FontOptions (12.0f));
    staffLabel.setText ("-", juce::dontSendNotification);

    addAndMakeVisible (semitoneUpButton);
    semitoneUpButton.setButtonText (juce::String::fromUTF8 (u8"♯"));
    semitoneUpButton.setTooltip ("Sharp (raise by semitone)");
    semitoneUpButton.onClick = [this] { applyChordActionFromButton (ChordCellAction::semitoneUp); };
    addAndMakeVisible (semitoneDownButton);
    semitoneDownButton.setButtonText (juce::String::fromUTF8 (u8"♭"));
    semitoneDownButton.setTooltip ("Flat (lower by semitone)");
    semitoneDownButton.onClick = [this] { applyChordActionFromButton (ChordCellAction::semitoneDown); };

    static const char* sharpLabels[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatLabels[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const bool useSharps = ! juce::String ("Cmaj").containsIgnoreCase ("b");
    for (int i = 0; i < 12; ++i)
    {
        const char* label = useSharps ? sharpLabels[i] : flatLabels[i];
        juce::String rootValue (label);
        auto* b = rootButtons.add (new juce::TextButton (label));
        rootButtonValues.add (rootValue);
        b->onClick = [this, rootValue]
        {
            selectedRoot = rootValue;
            if (selectedMeasure > 0 && selectedBeat > 0)
                applyChordFromPicker (selectedQualitySuffix);
            updatePickerButtonStyles();
        };
        addAndMakeVisible (b);
    }

    const juce::StringArray qualityLabels { "Major", "Minor", "Major 7", "Minor 7", "7th", "Diminished", "sus2", "sus4" };
    qualitySuffixes = { "", "m", "maj7", "m7", "7", "dim", "sus2", "sus4" };
    for (int i = 0; i < qualityLabels.size(); ++i)
    {
        auto* b = qualityButtons.add (new juce::TextButton (qualityLabels[i]));
        const auto suffix = qualitySuffixes[i];
        b->onClick = [this, suffix]
        {
            selectedQualitySuffix = suffix;
            applyChordFromPicker (suffix);
            updatePickerButtonStyles();
        };
        addAndMakeVisible (b);
    }

    struct ActionSpec
    {
        const char* label;
        ChordCellAction action;
    };
    static const ActionSpec actionSpecs[] =
    {
        { "Block", ChordCellAction::block },
        { "Arpeggio", ChordCellAction::arpeggio },
        { "Double Time", ChordCellAction::doubleTime },
        { "Delete", ChordCellAction::deleteChord },
        { "Octave Up", ChordCellAction::octaveUp },
        { "Octave Down", ChordCellAction::octaveDown },
        { "Inversion Up", ChordCellAction::inversionUp },
        { "Inversion Down", ChordCellAction::inversionDown }
    };
    for (const auto& spec : actionSpecs)
    {
        auto* b = actionButtons.add (new juce::TextButton (spec.label));
        b->onClick = [this, action = spec.action] { applyChordActionFromButton (action); };
        addAndMakeVisible (b);
    }

    velocitySlider.setSliderStyle (juce::Slider::LinearVertical);
    velocitySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    velocitySlider.setRange (1.0, 127.0, 1.0);
    velocitySlider.setValue (64.0);
    velocitySlider.setVisible (false);
    velocitySlider.onValueChange = [this] { applyVelocitySliderChange(); };
    addAndMakeVisible (velocitySlider);

    updatePickerButtonStyles();
}

static bool chordKeyUsesFlats (const juce::String& keySig)
{
    const juce::String k = keySig.trim().toLowerCase();
    if (k.startsWith ("f#") || k.startsWith ("c#"))
        return false;
    if (k.startsWith ("f"))
        return true;
    if (k.startsWith ("bb") || k.startsWith ("eb") || k.startsWith ("ab")
        || k.startsWith ("db") || k.startsWith ("gb") || k.startsWith ("cb"))
        return true;
    return false;
}

void ChordInspectorComponent::setKeySignature (const juce::String& keySignature)
{
    keySignatureForRoots = keySignature.trim();
    if (keySignatureForRoots.isEmpty())
        keySignatureForRoots = "Cmaj";

    static const char* sharpLabels[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatLabels[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const bool useSharps = ! chordKeyUsesFlats (keySignatureForRoots);

    rootButtonValues.clear();
    for (int i = 0; i < rootButtons.size() && i < 12; ++i)
    {
        const char* label = useSharps ? sharpLabels[i] : flatLabels[i];
        if (auto* b = rootButtons[i])
            b->setButtonText (label);
        rootButtonValues.add (juce::String (label));
    }
    updatePickerButtonStyles();
}

void ChordInspectorComponent::setChords (const juce::String& trackName,
                                         const juce::StringArray& chordSymbols,
                                         const juce::Array<double>& chordStartTimesSeconds,
                                         const juce::Array<int>& chordMeasures,
                                         const juce::Array<int>& chordBeats,
                                         int beatsPerBar,
                                         const juce::String& staffText)
{
    symbols = chordSymbols;
    chordStarts = chordStartTimesSeconds;
    measures = chordMeasures;
    beats = chordBeats;
    beatsInBar = juce::jlimit (1, 16, beatsPerBar);
    rowCount = 1;
    for (int i = 0; i < measures.size(); ++i)
        rowCount = juce::jmax (rowCount, measures[i]);
    titleLabel.setText (trackName.isNotEmpty() ? ("Chords - " + trackName) : "Chords",
                        juce::dontSendNotification);
    chordList.updateContent();
    chordList.repaint();

    staffLabel.setText (staffText.isNotEmpty() ? staffText : "-", juce::dontSendNotification);

    if (selectedMeasure > 0 && selectedBeat > 0 && onChordCellSelected != nullptr)
        onChordCellSelected (selectedMeasure, selectedBeat);
    else
        setPreviewNotes ({});
}

void ChordInspectorComponent::setPreviewNotes (const std::vector<PreviewNote>& notes)
{
    previewNotes = notes;
    if (previewChordSymbol.isEmpty())
        previewChordSymbol = "-";
    updateVelocitySliderFromSelection();
    repaint (previewBounds.expanded (2));
}

void ChordInspectorComponent::setCurrentTimeSeconds (double timeSeconds)
{
    currentTimeSeconds = juce::jmax (0.0, timeSeconds);

    int nextActive = -1;
    for (int i = 0; i < chordStarts.size(); ++i)
    {
        if (chordStarts[i] <= currentTimeSeconds)
            nextActive = i;
        else
            break;
    }

    int nextMeasure = -1;
    int nextBeat = -1;
    if (nextActive >= 0
        && juce::isPositiveAndBelow (nextActive, measures.size())
        && juce::isPositiveAndBelow (nextActive, beats.size()))
    {
        nextMeasure = measures[nextActive];
        nextBeat = beats[nextActive];
    }

    if (nextMeasure != activeMeasure || nextBeat != activeBeat)
    {
        activeMeasure = nextMeasure;
        activeBeat = nextBeat;
        chordList.repaint();
        if (activeMeasure > 0)
            chordList.scrollToEnsureRowIsOnscreen (activeMeasure - 1);

        if (activeMeasure > 0 && activeBeat > 0)
        {
            previewChordSymbol = getChordSymbolAtCell (activeMeasure, activeBeat);
            previewCellText = "Live: Bar " + juce::String (activeMeasure) + " Beat " + juce::String (activeBeat);
            if (onChordCellSelected != nullptr)
                onChordCellSelected (activeMeasure, activeBeat);
        }
    }
}

bool ChordInspectorComponent::deleteSelectedChordIfAny()
{
    if (selectedMeasure < 1 || selectedBeat < 1 || onChordCellAction == nullptr)
        return false;

    bool hasChord = false;
    for (int i = 0; i < symbols.size(); ++i)
    {
        if (juce::isPositiveAndBelow (i, measures.size()) && juce::isPositiveAndBelow (i, beats.size())
            && measures[i] == selectedMeasure && beats[i] == selectedBeat)
        {
            hasChord = true;
            break;
        }
    }

    if (! hasChord)
        return false;

    onChordCellAction (selectedMeasure, selectedBeat, ChordCellAction::deleteChord);
    return true;
}

void ChordInspectorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF14110C));
    g.setColour (juce::Colour (0xFF5A431D));
    g.drawRect (getLocalBounds());

    if (! workshopBannerBounds.isEmpty())
    {
        const auto banner = workshopBannerBounds.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF1E160D), banner.getX(), banner.getY(),
                                                 juce::Colour (0xFF120D07), banner.getX(), banner.getBottom(), false));
        g.fillRoundedRectangle (banner, 5.0f);
        g.setColour (juce::Colour (0xFF6B4F24));
        g.drawRoundedRectangle (banner, 5.0f, 1.0f);
        g.setColour (juce::Colour (0xFFE6C47E));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("CHORD WORKSHOP", workshopBannerBounds, juce::Justification::centred);
    }

    drawPreview (g);
}

void ChordInspectorComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (20);
    titleLabel.setBounds (header);

    r.removeFromTop (6);
    auto listHeader = r.removeFromTop (18);
    barHeaderLabel.setBounds (listHeader.removeFromLeft (42));
    r.removeFromTop (2);
    auto staffArea = r.removeFromBottom (24);
    staffLabel.setBounds (staffArea);

    const int pickerHeight = juce::jmin (320, juce::jmax (230, (int) (r.getHeight() * 0.56f)));
    auto pickerArea = r.removeFromBottom (pickerHeight);
    r.removeFromBottom (4);

    const int previewHeight = juce::jlimit (160, 290, (int) (r.getHeight() * 0.56f));
    auto previewRow = r.removeFromBottom (previewHeight);
    r.removeFromBottom (4);
    const int velStripW = 24;
    const bool showVel = ! selectedPreviewNotes.isEmpty() && selectedMeasure > 0 && selectedBeat > 0;
    velocitySlider.setVisible (showVel);
    if (showVel)
    {
        velocitySlider.setBounds (previewRow.removeFromLeft (velStripW).reduced (2));
        previewRow.removeFromLeft (2);
    }
    previewBounds = previewRow;

    const int bannerHeight = juce::jlimit (34, 54, r.getHeight() / 5);
    workshopBannerBounds = r.removeFromBottom (bannerHeight);
    r.removeFromBottom (6);

    auto actionArea = pickerArea.removeFromBottom (92);
    pickerArea.removeFromBottom (6);
    auto rootsArea = pickerArea.removeFromTop (58);
    pickerArea.removeFromTop (6);

    const int rootCols = 6;
    const int rootRows = 2;
    const int rootCellW = juce::jmax (1, rootsArea.getWidth() / rootCols);
    const int rootCellH = juce::jmax (18, rootsArea.getHeight() / rootRows);
    for (int i = 0; i < rootButtons.size(); ++i)
    {
        const int row = i / rootCols;
        const int col = i % rootCols;
        rootButtons[i]->setBounds (rootsArea.getX() + col * rootCellW + 1,
                                   rootsArea.getY() + row * rootCellH + 2,
                                   rootCellW - 2,
                                   rootCellH - 4);
    }

    auto arrowArea = pickerArea.removeFromLeft (38);
    semitoneUpButton.setBounds (arrowArea.removeFromTop (juce::jmax (24, arrowArea.getHeight() / 2)).reduced (3, 2));
    semitoneDownButton.setBounds (arrowArea.reduced (3, 2));
    pickerArea.removeFromLeft (6);

    const int cols = 2;
    const int rows = juce::jmax (1, (qualityButtons.size() + cols - 1) / cols);
    const int cellW = juce::jmax (1, pickerArea.getWidth() / cols);
    const int cellH = juce::jmax (20, pickerArea.getHeight() / rows);
    const int qualityButtonH = juce::jlimit (22, 30, cellH - 10);
    for (int i = 0; i < qualityButtons.size(); ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        const int cellY = pickerArea.getY() + row * cellH;
        const int yOffset = (cellH - qualityButtonH) / 2;
        qualityButtons[i]->setBounds (pickerArea.getX() + col * cellW + 1,
                                      cellY + yOffset,
                                      cellW - 2,
                                      qualityButtonH);
    }

    const int actionCols = 4;
    const int actionRows = juce::jmax (1, (actionButtons.size() + actionCols - 1) / actionCols);
    const int actionCellW = juce::jmax (1, actionArea.getWidth() / actionCols);
    const int actionCellH = juce::jmax (20, actionArea.getHeight() / actionRows);
    const int actionButtonH = juce::jlimit (22, 30, actionCellH - 8);
    for (int i = 0; i < actionButtons.size(); ++i)
    {
        const int col = i % actionCols;
        const int row = i / actionCols;
        const int cellY = actionArea.getY() + row * actionCellH;
        const int yOffset = (actionCellH - actionButtonH) / 2;
        actionButtons[i]->setBounds (actionArea.getX() + col * actionCellW + 1,
                                     cellY + yOffset,
                                     actionCellW - 2,
                                     actionButtonH);
    }

    chordList.setBounds (r);
}

int ChordInspectorComponent::getNumRows()
{
    return rowCount;
}

void ChordInspectorComponent::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused (rowIsSelected);
    const int measureNumber = rowNumber + 1;
    g.fillAll (measureNumber == activeMeasure ? juce::Colour (0xFF2E2517) : juce::Colour (0xFF1A140D));

    const int labelWidth = 42;
    g.setColour (juce::Colour (0xFF7A5A22));
    g.fillRect (0, 0, labelWidth, height);
    g.setColour (juce::Colour (0xFFFFE6B3));
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (juce::String (measureNumber), 4, 0, labelWidth - 8, height, juce::Justification::centredLeft);

    auto beatArea = juce::Rectangle<int> (labelWidth, 0, juce::jmax (0, width - labelWidth), height).reduced (2, 3);
    const int cellW = juce::jmax (1, beatArea.getWidth() / juce::jmax (1, beatsInBar));
    for (int b = 0; b < beatsInBar; ++b)
    {
        auto cell = juce::Rectangle<int> (beatArea.getX() + b * cellW,
                                          beatArea.getY(),
                                          (b == beatsInBar - 1 ? beatArea.getRight() - (beatArea.getX() + b * cellW) : cellW),
                                          beatArea.getHeight());

        const bool isActive = (measureNumber == activeMeasure && (b + 1) == activeBeat);
        g.setColour (isActive ? juce::Colour (0xFF8A621F) : juce::Colour (0xFF241B10));
        g.fillRoundedRectangle (cell.toFloat().reduced (1.0f, 0.0f), 4.0f);
        g.setColour (juce::Colour (0xFF5A431D));
        g.drawRoundedRectangle (cell.toFloat().reduced (1.0f, 0.0f), 4.0f, 1.0f);

        juce::String chordText;
        for (int i = 0; i < symbols.size(); ++i)
        {
            if (juce::isPositiveAndBelow (i, measures.size()) && juce::isPositiveAndBelow (i, beats.size())
                && measures[i] == measureNumber && beats[i] == (b + 1))
            {
                chordText = symbols[i];
                break;
            }
        }

        g.setColour (isActive ? juce::Colour (0xFFFFF1CF) : juce::Colour (0xFFE6CB96));
        g.setFont (juce::FontOptions (11.5f, isActive ? juce::Font::bold : juce::Font::plain));
        g.drawText (chordText, cell.reduced (4, 0), juce::Justification::centred);

        const bool isSelected = (measureNumber == selectedMeasure && (b + 1) == selectedBeat);
        if (isSelected)
        {
            g.setColour (juce::Colour (0xFFFFE6B3));
            g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 4.0f, 1.3f);
        }
    }
}

void ChordInspectorComponent::listBoxItemClicked (int row, const juce::MouseEvent& event)
{
    if (row < 0)
        return;

    const int width = chordList.getWidth();
    const int height = chordList.getRowHeight();
    const int labelWidth = 42;
    const int localX = event.getPosition().x;
    if (localX < labelWidth || width <= labelWidth)
        return;

    const auto beatArea = juce::Rectangle<int> (labelWidth, 0, juce::jmax (0, width - labelWidth), height).reduced (2, 3);
    const int cellW = juce::jmax (1, beatArea.getWidth() / juce::jmax (1, beatsInBar));
    const int beatIndex = juce::jlimit (0, juce::jmax (0, beatsInBar - 1), (localX - beatArea.getX()) / juce::jmax (1, cellW));

    const int measureNumber = row + 1;
    const int beatNumber = beatIndex + 1;
    selectedMeasure = measureNumber;
    selectedBeat = beatNumber;
    selectedPreviewNotes.clear();
    previewChordSymbol = getChordSymbolAtCell (selectedMeasure, selectedBeat);
    previewCellText = "Selected: Bar " + juce::String (selectedMeasure) + " Beat " + juce::String (selectedBeat);

        juce::String chordText;
        for (int i = 0; i < symbols.size(); ++i)
        {
            if (juce::isPositiveAndBelow (i, measures.size()) && juce::isPositiveAndBelow (i, beats.size())
                && measures[i] == selectedMeasure && beats[i] == selectedBeat)
            {
                chordText = symbols[i].trim();
                break;
            }
        }

    if (chordText.isNotEmpty())
    {
        static const char* allRootNames[] = { "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B", "Cb", "B#" };
        static const int pitchIndexForRoot[] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11, 11, 11 };
        juce::String matchedRoot;
        int matchedPitchIndex = -1;
        for (size_t r = 0; r < sizeof (allRootNames) / sizeof (allRootNames[0]); ++r)
        {
            if (chordText.startsWithIgnoreCase (juce::String (allRootNames[r])))
            {
                matchedRoot = allRootNames[r];
                matchedPitchIndex = pitchIndexForRoot[r];
                break;
            }
        }

        if (matchedRoot.isNotEmpty() && matchedPitchIndex >= 0 && matchedPitchIndex < rootButtonValues.size())
        {
            selectedRoot = rootButtonValues[matchedPitchIndex];
            auto suffix = chordText.substring (matchedRoot.length()).trim().toLowerCase();
            if (suffix == "maj")
                suffix = "";

            const juce::StringArray supportedSuffixes { "", "m", "maj7", "m7", "7", "dim", "sus2", "sus4" };
            if (supportedSuffixes.contains (suffix))
                selectedQualitySuffix = suffix;
            else if (suffix.startsWith ("maj7"))
                selectedQualitySuffix = "maj7";
            else if (suffix.startsWith ("m7"))
                selectedQualitySuffix = "m7";
            else if (suffix.startsWith ("sus2"))
                selectedQualitySuffix = "sus2";
            else if (suffix.startsWith ("sus4"))
                selectedQualitySuffix = "sus4";
            else if (suffix.startsWith ("dim"))
                selectedQualitySuffix = "dim";
            else if (suffix.startsWith ("m"))
                selectedQualitySuffix = "m";
            else if (suffix.startsWith ("7"))
                selectedQualitySuffix = "7";
            else
                selectedQualitySuffix = "";
        }
    }

    updatePickerButtonStyles();
    chordList.repaint();

    if (onChordCellSelected != nullptr)
        onChordCellSelected (selectedMeasure, selectedBeat);
}

void ChordInspectorComponent::updatePickerButtonStyles()
{
    for (auto* b : rootButtons)
    {
        if (b == nullptr)
            continue;
        int idx = -1;
        for (int i = 0; i < rootButtons.size(); ++i)
            if (rootButtons[i] == b) { idx = i; break; }
        const auto mappedRoot = (idx >= 0 && idx < rootButtonValues.size()) ? rootButtonValues[idx] : b->getButtonText();
        const bool selected = mappedRoot == selectedRoot;
        b->setColour (juce::TextButton::buttonColourId, selected ? juce::Colour (0xFF9C6D23) : juce::Colour (0xFF4A3416));
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFFFE8BE));
    }

    const bool hasTargetCell = selectedMeasure > 0 && selectedBeat > 0;
    for (auto* b : qualityButtons)
    {
        if (b == nullptr)
            continue;
        int idx = -1;
        for (int j = 0; j < qualityButtons.size(); ++j)
            if (qualityButtons[j] == b) { idx = j; break; }
        const bool selected = idx >= 0 && idx < qualitySuffixes.size() && qualitySuffixes[idx] == selectedQualitySuffix;
        b->setColour (juce::TextButton::buttonColourId,
                      hasTargetCell ? (selected ? juce::Colour (0xFF8B6322) : juce::Colour (0xFF2B1F10))
                                    : juce::Colour (0xFF1C1711));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF6A4A18));
        b->setColour (juce::TextButton::textColourOffId,
                      hasTargetCell ? (selected ? juce::Colour (0xFFFFF2D3) : juce::Colour (0xFFE6CB96))
                                    : juce::Colour (0xFF8C7A58));
    }

    for (auto* b : actionButtons)
    {
        if (b == nullptr)
            continue;
        b->setColour (juce::TextButton::buttonColourId, hasTargetCell ? juce::Colour (0xFF2B1F10) : juce::Colour (0xFF1C1711));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF6A4A18));
        b->setColour (juce::TextButton::textColourOffId, hasTargetCell ? juce::Colour (0xFFE6CB96) : juce::Colour (0xFF8C7A58));
    }

    for (auto* b : { &semitoneUpButton, &semitoneDownButton })
    {
        b->setColour (juce::TextButton::buttonColourId, hasTargetCell ? juce::Colour (0xFF3B2A12) : juce::Colour (0xFF1C1711));
        b->setColour (juce::TextButton::textColourOffId, hasTargetCell ? juce::Colour (0xFFFFE8BE) : juce::Colour (0xFF8C7A58));
    }
}

void ChordInspectorComponent::applyChordFromPicker (const juce::String& qualitySuffix)
{
    if (selectedMeasure < 1 || selectedBeat < 1)
        return;
    if (onEmptyChordCellChosen == nullptr)
        return;

    onEmptyChordCellChosen (selectedMeasure, selectedBeat, selectedRoot + qualitySuffix);
}

void ChordInspectorComponent::applyChordActionFromButton (ChordCellAction action)
{
    if (selectedMeasure < 1 || selectedBeat < 1)
        return;
    if (onChordCellAction == nullptr)
        return;
    onChordCellAction (selectedMeasure, selectedBeat, action);
}

void ChordInspectorComponent::drawPreview (juce::Graphics& g) const
{
    if (previewBounds.isEmpty())
        return;

    auto area = previewBounds.reduced (1);
    g.setColour (juce::Colour (0xFF16120D));
    g.fillRoundedRectangle (area.toFloat(), 5.0f);
    g.setColour (juce::Colour (0xFF6B4F24));
    g.drawRoundedRectangle (area.toFloat(), 5.0f, 1.0f);

    auto inner = area.reduced (6, 4);
    auto topRow = inner.removeFromTop (16);
    g.setColour (juce::Colour (0xFF8C6A31));
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("Chord MIDI Preview", topRow.removeFromLeft (136), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xFFF1D7A1));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("Chord: " + (previewChordSymbol.isNotEmpty() ? previewChordSymbol : "-"),
                topRow.removeFromLeft (juce::jmax (90, topRow.getWidth() / 2)),
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xFFC3A974));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (previewCellText.isNotEmpty() ? previewCellText : "-", topRow, juce::Justification::centredRight);
    inner.removeFromTop (2);

    if (inner.getHeight() < 20)
        return;

    if (previewNotes.empty())
    {
        g.setColour (juce::Colour (0xFF9A8862));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Select a chord cell to preview", inner, juce::Justification::centred);
        return;
    }

    int lowPitch = 127, highPitch = 0;
    double endBeat = 1.0;
    for (const auto& n : previewNotes)
    {
        lowPitch = juce::jmin (lowPitch, n.pitch);
        highPitch = juce::jmax (highPitch, n.pitch);
        endBeat = juce::jmax (endBeat, n.startBeats + n.lengthBeats);
    }
    lowPitch = juce::jmax (0, lowPitch - 1);
    highPitch = juce::jmin (127, highPitch + 1);
    const int pitchSpan = juce::jmax (1, highPitch - lowPitch + 1);

    // Grid lines (vertical) at grid beats to aid quantization
    const double gridBeats = getGridBeats();
    for (double b = 0.0; b <= endBeat + 0.001; b += gridBeats)
    {
        const float gx = (float) inner.getX() + (float) (b / endBeat) * (float) inner.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawVerticalLine ((int) gx, (float) inner.getY(), (float) inner.getBottom());
    }

    for (int p = lowPitch; p <= highPitch; ++p)
    {
        const float y = (float) inner.getBottom() - (float) (p - lowPitch + 1) * (float) inner.getHeight() / (float) pitchSpan;
        const bool blackKey = (p % 12 == 1 || p % 12 == 3 || p % 12 == 6 || p % 12 == 8 || p % 12 == 10);
        g.setColour (blackKey ? juce::Colour (0x22110A06) : juce::Colour (0x16110A06));
        g.fillRect (inner.getX(), (int) y, inner.getWidth(), juce::jmax (1, inner.getHeight() / pitchSpan));
    }

    int idx = 0;
    for (const auto& n : previewNotes)
    {
        const bool isDragging = (idx == draggingPreviewNoteIndex);
        double startBeats = n.startBeats;
        double lengthBeats = n.lengthBeats;
        if (isDragging && previewDragMode == PreviewDragMode::resize)
        {
            startBeats = dragStartPreviewTime;
            lengthBeats = dragPreviewLength;
        }
        else if (isDragging && previewDragMode == PreviewDragMode::moveStart)
        {
            startBeats = dragPreviewStart;
            lengthBeats = juce::jmax (0.0625, (dragStartPreviewTime + dragStartPreviewLength) - dragPreviewStart);
        }
        const bool isSelected = selectedPreviewNotes.contains (idx);

        const float x = (float) inner.getX() + (float) (startBeats / endBeat) * (float) inner.getWidth();
        const float w = juce::jmax (2.0f, (float) (lengthBeats / endBeat) * (float) inner.getWidth());
        const float yTop = (float) inner.getBottom() - (float) (n.pitch - lowPitch + 1) * (float) inner.getHeight() / (float) pitchSpan;
        const float yBottom = (float) inner.getBottom() - (float) (n.pitch - lowPitch) * (float) inner.getHeight() / (float) pitchSpan;
        auto noteRect = juce::Rectangle<float> (x, yTop + 0.7f, w, juce::jmax (2.0f, yBottom - yTop - 1.4f));

        if (isSelected)
            g.setColour (juce::Colour (0xFFD4A85A));
        else
            g.setColour (juce::Colour (0xFFB98A2E));
        g.fillRoundedRectangle (noteRect, 1.8f);
        g.setColour (isSelected ? juce::Colour (0xFFFFE6B3) : juce::Colour (0xFFF3D18A));
        g.drawRoundedRectangle (noteRect, 1.8f, isSelected ? 1.2f : 0.8f);

        // Draw resize handles (left = start time, right = length)
        if (isSelected && noteRect.getWidth() > 12.0f)
        {
            g.setColour (juce::Colour (0xFFFFE6B3));
            g.fillRect (noteRect.getX(), noteRect.getY(), 4.0f, noteRect.getHeight());
            g.fillRect (noteRect.getRight() - 6.0f, noteRect.getY(), 4.0f, noteRect.getHeight());
        }

        // Note label on MIDI block (always shown in chord workshop)
        g.setColour (juce::Colour (0xFF1C1408));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (getNoteNameWithOctave (n.pitch),
                    noteRect.toNearestInt().reduced (2, 0),
                    juce::Justification::centredLeft,
                    true);
        ++idx;
    }
}

juce::String ChordInspectorComponent::getChordSymbolAtCell (int measure, int beat) const
{
    for (int i = 0; i < symbols.size(); ++i)
    {
        if (juce::isPositiveAndBelow (i, measures.size()) && juce::isPositiveAndBelow (i, beats.size())
            && measures[i] == measure && beats[i] == beat)
            return symbols[i];
    }
    return {};
}

juce::String ChordInspectorComponent::getNoteNameWithOctave (int midiNote) const
{
    const bool useSharp = ! chordKeyUsesFlats (keySignatureForRoots);
    static constexpr const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static constexpr const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int clamped = juce::jlimit (0, 127, midiNote);
    const int octave = clamped / 12 - 1;
    const char* name = useSharp ? sharpNames[clamped % 12] : flatNames[clamped % 12];
    return juce::String (name) + juce::String (octave);
}

juce::Rectangle<int> ChordInspectorComponent::getPreviewNoteGridInner() const
{
    if (previewBounds.isEmpty())
        return {};
    auto area = previewBounds.reduced (1);
    auto inner = area.reduced (6, 4);
    inner.removeFromTop (16);
    inner.removeFromTop (2);
    return inner;
}

std::vector<ChordInspectorComponent::PreviewNoteRect> ChordInspectorComponent::getPreviewNoteRects() const
{
    std::vector<PreviewNoteRect> out;
    auto inner = getPreviewNoteGridInner();
    if (inner.getHeight() < 20 || previewNotes.empty())
        return out;

    int lowPitch = 127, highPitch = 0;
    double endBeat = 1.0;
    for (const auto& n : previewNotes)
    {
        lowPitch = juce::jmin (lowPitch, n.pitch);
        highPitch = juce::jmax (highPitch, n.pitch);
        endBeat = juce::jmax (endBeat, n.startBeats + n.lengthBeats);
    }
    lowPitch = juce::jmax (0, lowPitch - 1);
    highPitch = juce::jmin (127, highPitch + 1);
    const int pitchSpan = juce::jmax (1, highPitch - lowPitch + 1);

    int idx = 0;
    for (const auto& n : previewNotes)
    {
        const float x = (float) inner.getX() + (float) (n.startBeats / endBeat) * (float) inner.getWidth();
        const float w = juce::jmax (2.0f, (float) (n.lengthBeats / endBeat) * (float) inner.getWidth());
        const float yTop = (float) inner.getBottom() - (float) (n.pitch - lowPitch + 1) * (float) inner.getHeight() / (float) pitchSpan;
        const float yBottom = (float) inner.getBottom() - (float) (n.pitch - lowPitch) * (float) inner.getHeight() / (float) pitchSpan;
        PreviewNoteRect pr;
        pr.noteIndex = idx++;
        pr.bounds = juce::Rectangle<float> (x, yTop + 0.7f, w, juce::jmax (2.0f, yBottom - yTop - 1.4f));
        out.push_back (pr);
    }
    return out;
}

int ChordInspectorComponent::getPitchFromPreviewY (float yInInner) const
{
    auto inner = getPreviewNoteGridInner();
    if (inner.getHeight() < 20 || previewNotes.empty())
        return 60;

    int lowPitch = 127, highPitch = 0;
    for (const auto& n : previewNotes)
    {
        lowPitch = juce::jmin (lowPitch, n.pitch);
        highPitch = juce::jmax (highPitch, n.pitch);
    }
    lowPitch = juce::jmax (0, lowPitch - 1);
    highPitch = juce::jmin (127, highPitch + 1);
    const int pitchSpan = juce::jmax (1, highPitch - lowPitch + 1);
    const float rel = (float) (inner.getBottom() - yInInner) / (float) inner.getHeight();
    const int p = lowPitch + (int) (rel * (float) pitchSpan);
    return juce::jlimit (0, 127, p);
}

double ChordInspectorComponent::getTimeFromPreviewX (float xInInner) const
{
    auto inner = getPreviewNoteGridInner();
    if (inner.getWidth() < 20 || previewNotes.empty())
        return 0.0;

    double endBeat = 1.0;
    for (const auto& n : previewNotes)
        endBeat = juce::jmax (endBeat, n.startBeats + n.lengthBeats);

    const float rel = (xInInner - (float) inner.getX()) / (float) inner.getWidth();
    return juce::jmax (0.0, rel * endBeat);
}

double ChordInspectorComponent::getGridBeats() const
{
    return 0.25; // 16th note grid
}

double ChordInspectorComponent::snapLengthBeatsToGridIfClose (double lengthBeats) const
{
    const double gridBeats = getGridBeats();
    if (gridBeats <= 0.0 || lengthBeats <= 0.0)
        return lengthBeats;
    const double rounded = std::round (lengthBeats / gridBeats) * gridBeats;
    const double snapped = juce::jmax (0.0625, rounded);
    const double threshold = juce::jmax (0.02, gridBeats * 0.15);
    if (std::fabs (lengthBeats - snapped) <= threshold)
        return snapped;
    return lengthBeats;
}

double ChordInspectorComponent::snapStartBeatsToGridIfClose (double startBeats) const
{
    const double gridBeats = getGridBeats();
    if (gridBeats <= 0.0)
        return startBeats;
    const double rounded = std::round (startBeats / gridBeats) * gridBeats;
    const double threshold = juce::jmax (0.02, gridBeats * 0.15);
    if (std::fabs (startBeats - rounded) <= threshold)
        return juce::jmax (0.0, rounded);
    return startBeats;
}

void ChordInspectorComponent::mouseDown (const juce::MouseEvent& event)
{
    if (! previewBounds.contains (event.getPosition()))
    {
        grabKeyboardFocus();
        return;
    }

    grabKeyboardFocus();
    auto inner = getPreviewNoteGridInner();
    if (inner.getHeight() < 20 || selectedMeasure < 1 || selectedBeat < 1)
        return;

    // Control-click to add note
    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        if (onChordPreviewNoteAdd != nullptr)
        {
            const int pitch = getPitchFromPreviewY ((float) event.getPosition().y);
            const double clickTime = getTimeFromPreviewX ((float) event.getPosition().x);
            const double gridBeats = getGridBeats();
            const double quantizedTime = std::round (clickTime / gridBeats) * gridBeats;
            const double length = gridBeats;
            onChordPreviewNoteAdd (selectedMeasure, selectedBeat, pitch, quantizedTime, length);
            if (onPreviewNotePlay != nullptr)
                onPreviewNotePlay (pitch);
        }
        return;
    }

    if (previewNotes.empty())
        return;

    auto rects = getPreviewNoteRects();
    for (const auto& pr : rects)
    {
        if (! pr.bounds.contains (event.getPosition().toFloat()))
            continue;

        // Check if clicking on resize handle (right edge) or start handle (left edge)
        const float resizeHandleWidth = 6.0f;
        const float mx = (float) event.getPosition().x;
        const bool onResizeHandle = mx >= pr.bounds.getRight() - resizeHandleWidth;
        const bool onStartHandle = (mx >= pr.bounds.getX() && mx < pr.bounds.getX() + resizeHandleWidth);

        if (event.mods.isCommandDown() || event.mods.isShiftDown())
        {
            // Multi-select
            if (selectedPreviewNotes.contains (pr.noteIndex))
                selectedPreviewNotes.removeAllInstancesOf (pr.noteIndex);
            else
                selectedPreviewNotes.addIfNotAlreadyThere (pr.noteIndex);
        }
        else
        {
            // Single select
            if (! selectedPreviewNotes.contains (pr.noteIndex) || selectedPreviewNotes.size() <= 1)
            {
                selectedPreviewNotes.clear();
                selectedPreviewNotes.add (pr.noteIndex);
            }
        }

        draggingPreviewNoteIndex = pr.noteIndex;
        if (onResizeHandle)
            previewDragMode = PreviewDragMode::resize;
        else if (onStartHandle)
            previewDragMode = PreviewDragMode::moveStart;
        else
            previewDragMode = PreviewDragMode::move;

        if (juce::isPositiveAndBelow (pr.noteIndex, (int) previewNotes.size()))
        {
            dragStartPreviewTime = previewNotes[(size_t) pr.noteIndex].startBeats;
            dragStartPreviewLength = previewNotes[(size_t) pr.noteIndex].lengthBeats;
            dragPreviewLength = dragStartPreviewLength;
            dragPreviewStart = dragStartPreviewTime;
            if (onPreviewNotePlay != nullptr)
                onPreviewNotePlay (previewNotes[(size_t) pr.noteIndex].pitch);
        }

        updateVelocitySliderFromSelection();
        repaint (previewBounds.expanded (2));
        return;
    }

    // Clicked on empty space - clear selection
    if (! event.mods.isCommandDown() && ! event.mods.isShiftDown())
        selectedPreviewNotes.clear();
    updateVelocitySliderFromSelection();
    repaint (previewBounds.expanded (2));
}

void ChordInspectorComponent::mouseMove (const juce::MouseEvent& event)
{
    if (! previewBounds.contains (event.getPosition()))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    auto inner = getPreviewNoteGridInner();
    if (inner.getHeight() < 20 || previewNotes.empty() || ! inner.contains (event.getPosition()))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    const float resizeHandleWidth = 6.0f;
    const auto rects = getPreviewNoteRects();

    for (const auto& pr : rects)
    {
        if (! pr.bounds.contains (event.getPosition().toFloat()))
            continue;

        const float mx = (float) event.getPosition().x;
        const bool onLeftEdge  = (mx >= pr.bounds.getX() && mx < pr.bounds.getX() + resizeHandleWidth);
        const bool onRightEdge = (mx > pr.bounds.getRight() - resizeHandleWidth && mx <= pr.bounds.getRight());

        if (onLeftEdge)
        {
            setMouseCursor (ReasonResizeCursors::getLeftBracketResizeCursor());
            return;
        }
        if (onRightEdge)
        {
            setMouseCursor (ReasonResizeCursors::getRightBracketResizeCursor());
            return;
        }

        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ChordInspectorComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingPreviewNoteIndex < 0 || selectedMeasure < 1 || selectedBeat < 1)
        return;

    auto inner = getPreviewNoteGridInner();
    if (inner.getHeight() < 20 || ! inner.contains (event.getPosition()))
        return;

    if (previewDragMode == PreviewDragMode::move && onChordPreviewNotePitchChange != nullptr)
    {
        const int newPitch = getPitchFromPreviewY ((float) event.getPosition().y);
        if (newPitch != lastPreviewDragPitch)
        {
            lastPreviewDragPitch = newPitch;
            if (onPreviewNotePlay != nullptr)
                onPreviewNotePlay (newPitch);
            onChordPreviewNotePitchChange (selectedMeasure, selectedBeat, draggingPreviewNoteIndex, newPitch);
        }
    }
    else if (previewDragMode == PreviewDragMode::resize && onChordPreviewNoteResize != nullptr)
    {
        if (juce::isPositiveAndBelow (draggingPreviewNoteIndex, (int) previewNotes.size()))
        {
            const double clickTime = getTimeFromPreviewX ((float) event.getPosition().x);
            const double newLength = juce::jmax (0.0625, clickTime - dragStartPreviewTime);
            dragPreviewLength = newLength;
            repaint (previewBounds.expanded (2));
        }
    }
    else if (previewDragMode == PreviewDragMode::moveStart)
    {
        if (juce::isPositiveAndBelow (draggingPreviewNoteIndex, (int) previewNotes.size()))
        {
            const double clickTime = getTimeFromPreviewX ((float) event.getPosition().x);
            const double noteEnd = dragStartPreviewTime + dragStartPreviewLength;
            dragPreviewStart = juce::jmax (0.0, juce::jmin (noteEnd - 0.0625, clickTime));
            repaint (previewBounds.expanded (2));
        }
    }
}

void ChordInspectorComponent::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    if (previewDragMode == PreviewDragMode::resize && draggingPreviewNoteIndex >= 0
        && onChordPreviewNoteResize != nullptr && selectedMeasure > 0 && selectedBeat > 0)
    {
        const double lengthToApply = snapLengthBeatsToGridIfClose (dragPreviewLength);
        onChordPreviewNoteResize (selectedMeasure, selectedBeat, draggingPreviewNoteIndex, lengthToApply);
    }
    if (previewDragMode == PreviewDragMode::moveStart && draggingPreviewNoteIndex >= 0
        && onChordPreviewNoteStartChange != nullptr && selectedMeasure > 0 && selectedBeat > 0)
    {
        const double startToApply = snapStartBeatsToGridIfClose (dragPreviewStart);
        onChordPreviewNoteStartChange (selectedMeasure, selectedBeat, draggingPreviewNoteIndex, startToApply);
    }

    draggingPreviewNoteIndex = -1;
    lastPreviewDragPitch = -1;
    previewDragMode = PreviewDragMode::none;
    dragPreviewLength = 0.0;
    repaint (previewBounds.expanded (2));
}

bool ChordInspectorComponent::keyPressed (const juce::KeyPress& key)
{
    if (selectedMeasure < 1 || selectedBeat < 1)
        return false;

    // Delete key
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (! selectedPreviewNotes.isEmpty() && onChordPreviewNoteDelete != nullptr)
        {
            auto toDelete = selectedPreviewNotes;
            for (int i = toDelete.size() - 1; i >= 0; --i)
                onChordPreviewNoteDelete (selectedMeasure, selectedBeat, toDelete[i]);
            selectedPreviewNotes.clear();
            return true;
        }
        return false;
    }

    // Quantize (Q key)
    const int code = key.getKeyCode();
    if (! key.getModifiers().isAnyModifierKeyDown() && (code == 'q' || code == 'Q'))
    {
        if (onChordPreviewNotesQuantize != nullptr)
        {
            const double gridBeats = getGridBeats();
            onChordPreviewNotesQuantize (selectedMeasure, selectedBeat, gridBeats);
            return true;
        }
    }

    return false;
}

void ChordInspectorComponent::updateVelocitySliderFromSelection()
{
    if (selectedMeasure < 1 || selectedBeat < 1 || previewNotes.empty())
    {
        velocitySlider.setVisible (false);
        return;
    }
    if (selectedPreviewNotes.isEmpty())
    {
        velocitySlider.setVisible (false);
        return;
    }
    int sum = 0;
    int count = 0;
    for (const int idx : selectedPreviewNotes)
    {
        if (juce::isPositiveAndBelow (idx, (int) previewNotes.size()))
        {
            sum += previewNotes[(size_t) idx].velocity;
            ++count;
        }
    }
    if (count == 0)
    {
        velocitySlider.setVisible (false);
        return;
    }
    const int avg = sum / count;
    velocitySliderLastValue = (double) juce::jlimit (1, 127, avg);
    velocitySlider.setValue (velocitySliderLastValue, juce::dontSendNotification);
    velocitySlider.setVisible (true);
    resized();
}

void ChordInspectorComponent::applyVelocitySliderChange()
{
    if (selectedMeasure < 1 || selectedBeat < 1 || onChordPreviewNoteVelocityChange == nullptr)
        return;
    if (selectedPreviewNotes.isEmpty())
        return;
    const double newVal = velocitySlider.getValue();
    const int delta = (int) std::round (newVal - velocitySliderLastValue);
    velocitySliderLastValue = newVal;
    for (const int idx : selectedPreviewNotes)
    {
        if (! juce::isPositiveAndBelow (idx, (int) previewNotes.size()))
            continue;
        const int newVel = juce::jlimit (1, 127, previewNotes[(size_t) idx].velocity + delta);
        onChordPreviewNoteVelocityChange (selectedMeasure, selectedBeat, idx, newVel);
    }
    repaint (previewBounds.expanded (2));
}
