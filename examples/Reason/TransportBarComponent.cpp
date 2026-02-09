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
    playButton.setColours (juce::Colour (0xFF5BC16D),
                           juce::Colour (0xFF75D98A),
                           juce::Colour (0xFF3E8C4F));

    juce::Path stopShape;
    stopShape.addRectangle (0.18f, 0.18f, 0.64f, 0.64f);
    stopButton.setShape (stopShape, false, true, false);
    stopButton.setBorderSize ({ 2, 2, 2, 2 });
    stopButton.setColours (juce::Colour (0xFFD34A4A),
                           juce::Colour (0xFFEB5B5B),
                           juce::Colour (0xFFB23535));

    juce::Path recordShape;
    recordShape.addEllipse (0.15f, 0.15f, 0.7f, 0.7f);
    recordButton.setShape (recordShape, false, true, false);
    recordButton.setBorderSize ({ 2, 2, 2, 2 });
    recordButton.setColours (juce::Colour (0xFFD94848),
                             juce::Colour (0xFFFF5A5A),
                             juce::Colour (0xFFB02E2E));
    styleActionButton (fileButton, juce::Colour (0xFF45505C));
    styleActionButton (chordsButton, juce::Colour (0xFF586E84));
    styleActionButton (metronomeButton, juce::Colour (0xFF4C6540));
    styleActionButton (midiInputButton, juce::Colour (0xFF4E5E70));
    styleActionButton (settingsButton, juce::Colour (0xFF45505C));

    for (auto* button : { &fileButton, &chordsButton, &metronomeButton, &midiInputButton, &settingsButton })
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);

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
    timeLabel.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    barsLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    tempoLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    timeSigLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    keyLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    timeLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFE7F6FF));
    barsLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFA9C7D9));
    tempoLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCDE8FF));
    timeSigLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCDE8FF));
    keyLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCDE8FF));
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
    const auto baseColour = juce::Colour (0xFF3B4C2A);
    const auto colour = shouldShowActive ? baseColour.withMultipliedBrightness (1.2f)
                                         : baseColour.withMultipliedBrightness (0.8f);
    metronomeButton.setColour (juce::TextButton::buttonColourId, colour);
    metronomeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void TransportBarComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = juce::Colour (0xFF3A434D);
    auto bottom = juce::Colour (0xFF1A2028);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) r.getBottom(), false));
    g.fillRect (r);

    auto drawButtonWell = [&g] (juce::Rectangle<int> bounds)
    {
        auto well = bounds.expanded (2, 2).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (well.translated (0.0f, 1.5f), 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF4E5966), well.getX(), well.getY(),
                                                 juce::Colour (0xFF29313B), well.getX(), well.getBottom(), false));
        g.fillRoundedRectangle (well, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawRoundedRectangle (well, 6.0f, 1.0f);
    };

    for (auto* b : { (juce::Component*) &playButton, (juce::Component*) &stopButton, (juce::Component*) &recordButton,
                     (juce::Component*) &fileButton,
                     (juce::Component*) &chordsButton, (juce::Component*) &metronomeButton, (juce::Component*) &midiInputButton,
                     (juce::Component*) &settingsButton })
        drawButtonWell (b->getBounds());

    const auto panel = displayPanelBounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (panel.translated (0.0f, 2.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF0D2A3A), panel.getX(), panel.getY(),
                                             juce::Colour (0xFF0A1824), panel.getX(), panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (juce::Colour (0x884FE6FF));
    g.drawRoundedRectangle (panel, 8.0f, 1.5f);

    if (recordActive)
    {
        const float cx = (float) recordButton.getRight() + 10.0f;
        const float cy = (float) recordButton.getBounds().getCentreY();
        const auto light = recordBlinkOn ? juce::Colour (0xFFFF4D4D) : juce::Colour (0xFF4D1B1B);
        g.setColour (light.withAlpha (0.25f));
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (light);
        g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
    }

    g.setColour (juce::Colour (0xFF2A2C31));
    g.fillRect (r.removeFromBottom (1));
}

void TransportBarComponent::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    const int laneHeight = area.getHeight();
    const int gap = 6;

    int leftX = area.getX();
    auto placeLeft = [&leftX, &area, laneHeight] (juce::Component& c, int width)
    {
        c.setBounds (leftX, area.getY(), width, laneHeight);
        leftX += width + gap;
    };

    const int transportButtonW = 34;
    placeLeft (playButton, transportButtonW);
    placeLeft (stopButton, transportButtonW);
    placeLeft (recordButton, transportButtonW + 2);
    placeLeft (fileButton, 66);
    placeLeft (chordsButton, 150);

    int rightX = area.getRight();
    auto placeRight = [&rightX, &area, laneHeight] (juce::Component& c, int width)
    {
        rightX -= width;
        c.setBounds (rightX, area.getY(), width, laneHeight);
        rightX -= gap;
    };

    placeRight (settingsButton, 92);
    placeRight (midiInputButton, 148);
    placeRight (metronomeButton, 104);

    const int centerLeft = leftX + 8;
    const int centerRight = rightX - 8;
    const int centerWidth = juce::jmax (0, centerRight - centerLeft);
    auto center = juce::Rectangle<int> (centerLeft, area.getY(), centerWidth, laneHeight);
    displayPanelBounds = center;

    auto panel = center.reduced (10, 5);
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
