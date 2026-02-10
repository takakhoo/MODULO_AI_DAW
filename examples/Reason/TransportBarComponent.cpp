#include "TransportBarComponent.h"

TransportBarComponent::TransportBarComponent()
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (fileButton);
    addAndMakeVisible (chordsButton);
    addAndMakeVisible (metronomeButton);
    addAndMakeVisible (midiInputButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (barsLabel);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (timeSigLabel);
    addAndMakeVisible (keyLabel);

    juce::Path playShape;
    playShape.addTriangle (0.15f, 0.1f, 0.9f, 0.5f, 0.15f, 0.9f);
    playButton.setShape (playShape, false, true, false);
    playButton.setBorderSize ({ 2, 2, 2, 2 });
    playButton.setColours (juce::Colour (0xFF8A621F),
                           juce::Colour (0xFFB6842E),
                           juce::Colour (0xFF5B3D12));

    juce::Path stopShape;
    stopShape.addRectangle (0.18f, 0.18f, 0.64f, 0.64f);
    stopButton.setShape (stopShape, false, true, false);
    stopButton.setBorderSize ({ 2, 2, 2, 2 });
    stopButton.setColours (juce::Colour (0xFF6A4A18),
                           juce::Colour (0xFF916523),
                           juce::Colour (0xFF4B320E));

    juce::Path recordShape;
    recordShape.addEllipse (0.15f, 0.15f, 0.7f, 0.7f);
    recordButton.setShape (recordShape, false, true, false);
    recordButton.setBorderSize ({ 2, 2, 2, 2 });
    recordButton.setColours (juce::Colour (0xFFD94848),
                             juce::Colour (0xFFFF5A5A),
                             juce::Colour (0xFFB02E2E));
    styleActionButton (fileButton, juce::Colour (0xFF6B4A18));
    styleActionButton (chordsButton, juce::Colour (0xFF8B6428));
    styleActionButton (midiInputButton, juce::Colour (0xFF7A5A22));

    for (auto* button : { &fileButton, &chordsButton, &midiInputButton })
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    chordsButton.setButtonText ("   AI Generate Chords");
    chordsButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF2B1905));
    chordsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFFD9A84D));
    chordsButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFE9C37A));
    chordsIcon = juce::ImageFileFormat::loadFrom (BinaryData::piano_16822471_png, BinaryData::piano_16822471_pngSize);

    if (auto image = juce::ImageFileFormat::loadFrom (BinaryData::metronome_png, BinaryData::metronome_pngSize);
        image.isValid())
    {
        auto normal = std::make_unique<juce::DrawableImage>();
        normal->setImage (image);
        auto over = std::make_unique<juce::DrawableImage>();
        over->setImage (image);
        over->setOpacity (0.85f);
        auto down = std::make_unique<juce::DrawableImage>();
        down->setImage (image);
        down->setOpacity (0.7f);

        metronomeButton.setImages (normal.get(), over.get(), down.get(), normal.get(), over.get(), down.get());
    }
    metronomeButton.setTooltip ("Metronome");
    metronomeButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colour (0xFFE0B15A));
    metronomeButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colour (0xFFF0C875));

    if (auto image = juce::ImageFileFormat::loadFrom (BinaryData::setting_png, BinaryData::setting_pngSize);
        image.isValid())
    {
        auto normal = std::make_unique<juce::DrawableImage>();
        normal->setImage (image);
        auto over = std::make_unique<juce::DrawableImage>();
        over->setImage (image);
        over->setOpacity (0.85f);
        auto down = std::make_unique<juce::DrawableImage>();
        down->setImage (image);
        down->setOpacity (0.7f);
        settingsButton.setImages (normal.get(), over.get(), down.get(), normal.get(), over.get(), down.get());
    }
    settingsButton.setTooltip ("Settings");
    settingsButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colour (0xFFE0B15A));
    settingsButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colour (0xFFF0C875));

    playButton.onClick = [this]
    {
        if (onPlay)
            onPlay();
    };

    stopButton.onClick = [this]
    {
        if (onStop)
            onStop();
    };

    recordButton.onClick = [this]
    {
        if (onRecord)
            onRecord();
    };

    fileButton.onClick = [this]
    {
        if (onFileMenu)
            onFileMenu();
    };

    chordsButton.onClick = [this]
    {
        if (onGenerateChords)
            onGenerateChords();
    };

    metronomeButton.setClickingTogglesState (true);
    metronomeButton.onClick = [this]
    {
        setMetronomeActive (metronomeButton.getToggleState());
        if (onMetronomeChanged)
            onMetronomeChanged (metronomeButton.getToggleState());
    };

    midiInputButton.onClick = [this]
    {
        if (onMidiInput)
            onMidiInput();
    };

    settingsButton.onClick = [this]
    {
        if (onSettings)
            onSettings();
    };

    timeLabel.setJustificationType (juce::Justification::centred);
    barsLabel.setJustificationType (juce::Justification::centred);
    tempoLabel.setJustificationType (juce::Justification::centred);
    timeSigLabel.setJustificationType (juce::Justification::centred);
    keyLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    barsLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    tempoLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    timeSigLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    keyLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    timeLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE8B3));
    barsLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD8B87A));
    tempoLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE0A0));
    timeSigLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE0A0));
    keyLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE0A0));
    tempoLabel.setInterceptsMouseClicks (true, true);
    timeSigLabel.setInterceptsMouseClicks (true, true);
    keyLabel.setInterceptsMouseClicks (true, true);

    tempoLabel.setEditable (false, true, true);
    tempoLabel.onEditorHide = [this]
    {
        if (onTempoChanged)
            onTempoChanged (tempoLabel.getText());
    };

    timeSigLabel.setEditable (false, true, true);
    timeSigLabel.onEditorHide = [this]
    {
        if (onTimeSignatureChanged)
            onTimeSignatureChanged (timeSigLabel.getText());
    };

    keyLabel.setEditable (false, false, false);
    tempoLabel.addMouseListener (this, true);
    timeSigLabel.addMouseListener (this, true);
    keyLabel.addMouseListener (this, true);

    setRecordActive (false);
    setMetronomeActive (false);
}

void TransportBarComponent::styleActionButton (juce::TextButton& button, juce::Colour baseColour)
{
    button.setColour (juce::TextButton::buttonColourId, baseColour.darker (0.5f));
    button.setColour (juce::TextButton::buttonOnColourId, baseColour.brighter (0.35f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.95f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void TransportBarComponent::setTimeText (const juce::String& text)
{
    timeLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setBarsText (const juce::String& text)
{
    barsLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setTempoText (const juce::String& text)
{
    if (tempoLabel.isBeingEdited())
        return;
    tempoLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setTimeSignatureText (const juce::String& text)
{
    if (timeSigLabel.isBeingEdited())
        return;
    timeSigLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setKeySignatureText (const juce::String& text)
{
    keyLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setRecordActive (bool shouldShowActive)
{
    recordActive = shouldShowActive;
    if (recordActive)
    {
        recordBlinkOn = true;
        startTimerHz (2);
    }
    else
    {
        stopTimer();
        recordBlinkOn = false;
    }

    if (shouldShowActive)
    {
        recordButton.setColours (juce::Colour (0xFFFF5A5A),
                                 juce::Colour (0xFFFF7A7A),
                                 juce::Colour (0xFFCC3A3A));
        recordButton.setOutline (juce::Colours::white.withAlpha (0.7f), 1.0f);
    }
    else
    {
        recordButton.setColours (juce::Colour (0xFFD94848),
                                 juce::Colour (0xFFFF5A5A),
                                 juce::Colour (0xFFB02E2E));
        recordButton.setOutline (juce::Colours::transparentBlack, 0.0f);
    }

    repaint();
}

void TransportBarComponent::setMidiInputText (const juce::String& text)
{
    midiInputButton.setButtonText (text);
}

void TransportBarComponent::setMetronomeActive (bool shouldShowActive)
{
    metronomeButton.setToggleState (shouldShowActive, juce::dontSendNotification);
    repaint (metronomeButton.getBounds().expanded (4));
}

void TransportBarComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = juce::Colour (0xFFFFE2A3);
    auto bottom = juce::Colour (0xFFB8832E);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) r.getBottom(), false));
    g.fillRect (r);

    auto drawButtonWell = [&g] (juce::Rectangle<int> bounds)
    {
        auto well = bounds.expanded (2, 2).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (well.translated (0.0f, 1.5f), 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFF5D491), well.getX(), well.getY(),
                                                 juce::Colour (0xFFC18A34), well.getX(), well.getBottom(), false));
        g.fillRoundedRectangle (well, 6.0f);
        g.setColour (juce::Colour (0xFF7A511A).withAlpha (0.55f));
        g.drawRoundedRectangle (well, 6.0f, 1.0f);
    };

    for (auto* b : { (juce::Component*) &playButton, (juce::Component*) &stopButton, (juce::Component*) &recordButton,
                     (juce::Component*) &fileButton,
                     (juce::Component*) &metronomeButton, (juce::Component*) &midiInputButton,
                     (juce::Component*) &settingsButton })
        drawButtonWell (b->getBounds());

    // Blend Generate Chords into the metallic bar: softer fill, minimal border.
    const auto chordBounds = chordsButton.getBounds().toFloat();
    g.setColour (juce::Colour (0x99E1B45E));
    g.fillRoundedRectangle (chordBounds.expanded (1.0f, 1.0f), 6.0f);
    g.setColour (juce::Colour (0x554B2F0C));
    g.drawRoundedRectangle (chordBounds.expanded (1.0f, 1.0f), 6.0f, 0.8f);

    if (chordsIcon.isValid())
    {
        auto iconArea = chordsButton.getBounds().reduced (6, 6).removeFromLeft (16);
        g.drawImageWithin (chordsIcon, iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(),
                           juce::RectanglePlacement::centred);
    }

    if (metronomeButton.getToggleState())
    {
        const auto mb = metronomeButton.getBounds().toFloat().expanded (1.0f);
        g.setColour (juce::Colour (0xCC5A3A10));
        g.drawRoundedRectangle (mb, 6.0f, 1.5f);
    }

    const auto panel = displayPanelBounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (panel.translated (0.0f, 2.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF3A2A12), panel.getX(), panel.getY(),
                                             juce::Colour (0xFF171007), panel.getX(), panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 8.0f);
    const auto panelBorder = recordActive && recordBlinkOn ? juce::Colour (0xCCEAC57A)
                                                           : juce::Colour (0x88E0B15A);
    g.setColour (panelBorder);
    g.drawRoundedRectangle (panel, 8.0f, recordActive ? 2.0f : 1.5f);

    if (! moduloBounds.isEmpty())
    {
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xFF5A3B12).withAlpha (0.35f));
        g.drawText ("MODULO", moduloBounds.translated (0, 2), juce::Justification::centred, true);
        g.setColour (juce::Colour (0xFFFFE6AE));
        g.drawText ("MODULO", moduloBounds, juce::Justification::centred, true);
    }

    if (recordActive)
    {
        const float cx = ((float) recordButton.getRight() + (float) displayPanelBounds.getX()) * 0.5f;
        const float cy = (float) recordButton.getBounds().getCentreY();
        const auto light = recordBlinkOn ? juce::Colour (0xFFFF4D4D) : juce::Colour (0xFF4D1B1B);
        g.setColour (light.withAlpha (0.25f));
        g.fillEllipse (cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (light);
        g.fillEllipse (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
    }

    g.setColour (juce::Colour (0xFF6E4817));
    g.fillRect (r.removeFromBottom (1));
}

void TransportBarComponent::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    const int laneHeight = area.getHeight();
    const int buttonHeight = juce::jmax (30, laneHeight - 12);
    const int buttonY = area.getY() + (laneHeight - buttonHeight) / 2;
    const int gap = 8;

    int fileW = 66;
    int chordsW = 170;
    int midiW = 148;
    int settingsW = 34;

    int leftX = area.getX();
    int rightX = area.getRight();

    fileButton.setBounds (leftX, buttonY, fileW, buttonHeight);
    leftX += fileW + gap;
    chordsButton.setBounds (leftX, buttonY, chordsW, buttonHeight);
    leftX += chordsW + gap;

    rightX -= settingsW;
    settingsButton.setBounds (rightX, buttonY, settingsW, buttonHeight);
    rightX -= gap;
    rightX -= midiW;
    midiInputButton.setBounds (rightX, buttonY, midiW, buttonHeight);
    rightX -= gap;

    const int playW = 34;
    const int stopW = 34;
    const int recordW = 36;
    const int transportGroupW = playW + gap + stopW + gap + recordW;
    int metronomeW = 34;

    const int centerLeftLimit = leftX + transportGroupW + gap;
    const int centerRightLimit = rightX - (metronomeW + gap);
    const int maxCenterWidth = juce::jmax (0, centerRightLimit - centerLeftLimit);
    int centerWidth = juce::jmin (520, maxCenterWidth);

    if (centerWidth < 180)
    {
        const int minMetro = 30;
        const int shrink = juce::jmin (metronomeW - minMetro, 180 - centerWidth);
        metronomeW -= shrink;
        const int recenteredRightLimit = rightX - (metronomeW + gap);
        centerWidth = juce::jmin (520, juce::jmax (0, recenteredRightLimit - centerLeftLimit));
    }

    centerWidth = juce::jmax (100, centerWidth);
    auto center = juce::Rectangle<int> (0, area.getY(), centerWidth, laneHeight)
                      .withCentre (area.getCentre());
    center.setX (juce::jlimit (centerLeftLimit, juce::jmax (centerLeftLimit, centerRightLimit - centerWidth), center.getX()));
    displayPanelBounds = center;

    int transportRight = center.getX() - gap;
    recordButton.setBounds (transportRight - recordW, buttonY, recordW, buttonHeight);
    transportRight -= recordW + gap;
    stopButton.setBounds (transportRight - stopW, buttonY, stopW, buttonHeight);
    transportRight -= stopW + gap;
    playButton.setBounds (transportRight - playW, buttonY, playW, buttonHeight);

    const int metroX = center.getRight() + gap;
    const int metroMaxW = juce::jmax (0, rightX - metroX);
    metronomeButton.setBounds (metroX, buttonY, juce::jmax (0, juce::jmin (metronomeW, metroMaxW)), buttonHeight);

    const int moduloX = chordsButton.getRight() + gap;
    const int moduloW = juce::jmax (0, playButton.getX() - gap - moduloX);
    moduloBounds = juce::Rectangle<int> (moduloX, buttonY, moduloW, buttonHeight);

    auto panel = center.reduced (8, 3);
    auto topRow = panel.removeFromTop ((int) (panel.getHeight() * 0.58f));
    timeLabel.setBounds (topRow);

    auto bottomRow = panel;
    auto colW = juce::jmax (1, bottomRow.getWidth() / 4);
    barsLabel.setBounds (bottomRow.removeFromLeft (colW));
    tempoLabel.setBounds (bottomRow.removeFromLeft (colW));
    timeSigLabel.setBounds (bottomRow.removeFromLeft (colW));
    keyLabel.setBounds (bottomRow);
}

void TransportBarComponent::timerCallback()
{
    if (! recordActive)
        return;

    recordBlinkOn = ! recordBlinkOn;
    repaint (recordButton.getBounds().expanded (28, 6));
}

void TransportBarComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.eventComponent == &keyLabel)
    {
        showKeySignatureMenu();
        return;
    }

    if (event.eventComponent == &tempoLabel)
    {
        tempoLabel.showEditor();
        return;
    }

    if (event.eventComponent == &timeSigLabel)
    {
        timeSigLabel.showEditor();
        return;
    }
}

void TransportBarComponent::showKeySignatureMenu()
{
    const juce::String current = keyLabel.getText().trim();

    juce::PopupMenu menu;
    juce::PopupMenu majorMenu;
    juce::PopupMenu minorMenu;
    std::vector<juce::String> keyOptions;

    auto addKeyItem = [&keyOptions, &current] (juce::PopupMenu& target, int& id, const juce::String& name)
    {
        const bool isSelected = current.equalsIgnoreCase (name);
        target.addItem (id, name, true, isSelected);
        keyOptions.push_back (name);
        ++id;
    };

    int id = 1;
    const juce::StringArray majorKeys { "Cmaj", "Gmaj", "Dmaj", "Amaj", "Emaj", "Bmaj",
                                        "F#maj", "C#maj", "Fmaj", "Bbmaj", "Ebmaj", "Abmaj",
                                        "Dbmaj", "Gbmaj", "Cbmaj" };
    const juce::StringArray minorKeys { "Cmin", "Gmin", "Dmin", "Amin", "Emin", "Bmin",
                                        "F#min", "C#min", "Fmin", "Bbmin", "Ebmin", "Abmin",
                                        "Dbmin", "Gbmin", "Cbmin" };

    for (const auto& key : majorKeys)
        addKeyItem (majorMenu, id, key);
    for (const auto& key : minorKeys)
        addKeyItem (minorMenu, id, key);

    menu.addSubMenu ("Major", majorMenu);
    menu.addSubMenu ("Minor", minorMenu);

    auto options = juce::PopupMenu::Options().withTargetComponent (keyLabel);
    menu.showMenuAsync (options, [this, keyOptions] (int result)
    {
        if (result <= 0 || result > (int) keyOptions.size())
            return;

        const auto& choice = keyOptions[(size_t) result - 1];
        keyLabel.setText (choice, juce::dontSendNotification);
        if (onKeySignatureChanged)
            onKeySignatureChanged (choice);
    });
}
