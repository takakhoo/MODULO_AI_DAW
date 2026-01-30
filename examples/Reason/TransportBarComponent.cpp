#include "TransportBarComponent.h"

TransportBarComponent::TransportBarComponent()
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (projectButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (pluginsButton);
    addAndMakeVisible (chordsButton);
    addAndMakeVisible (midiInputButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (barsLabel);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (timeSigLabel);
    addAndMakeVisible (keyLabel);

    playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2D6B3A));
    stopButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF733131));
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    projectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    importButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    pluginsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    chordsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    midiInputButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
    settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));

    for (auto* button : { &playButton, &stopButton, &recordButton, &projectButton,
                          &importButton, &pluginsButton, &chordsButton, &midiInputButton, &settingsButton })
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

    importButton.onClick = [this]
    {
        if (onImport)
            onImport();
    };

    projectButton.onClick = [this]
    {
        if (onProject)
            onProject();
    };

    pluginsButton.onClick = [this]
    {
        if (onPlugins)
            onPlugins();
    };

    chordsButton.onClick = [this]
    {
        if (onGenerateChords)
            onGenerateChords();
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
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    timeLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    barsLabel.setFont (juce::FontOptions (12.0f));
    tempoLabel.setFont (juce::FontOptions (12.0f));
    timeSigLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    keyLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));

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

    keyLabel.setEditable (false, true, true);
    keyLabel.onEditorHide = [this]
    {
        if (onKeySignatureChanged)
            onKeySignatureChanged (keyLabel.getText());
    };

    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2C31));
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
    tempoLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setTimeSignatureText (const juce::String& text)
{
    timeSigLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setKeySignatureText (const juce::String& text)
{
    keyLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setRecordActive (bool shouldShowActive)
{
    auto colour = shouldShowActive ? juce::Colour (0xFFC14A4A) : juce::Colour (0xFF2A2C31);
    recordButton.setColour (juce::TextButton::buttonColourId, colour);
}

void TransportBarComponent::setMidiInputText (const juce::String& text)
{
    midiInputButton.setButtonText (text);
}

void TransportBarComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = juce::Colour (0xFF2A2F36);
    auto bottom = juce::Colour (0xFF1D2026);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colour (0xFF2A2C31));
    g.fillRect (r.removeFromBottom (1));
}

void TransportBarComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto left = r.removeFromLeft (720);

    playButton.setBounds (left.removeFromLeft (70).reduced (2));
    stopButton.setBounds (left.removeFromLeft (70).reduced (2));
    recordButton.setBounds (left.removeFromLeft (80).reduced (2));
    projectButton.setBounds (left.removeFromLeft (90).reduced (2));
    importButton.setBounds (left.removeFromLeft (120).reduced (2));
    pluginsButton.setBounds (left.removeFromLeft (90).reduced (2));
    chordsButton.setBounds (left.removeFromLeft (110).reduced (2));

    settingsButton.setBounds (r.removeFromRight (100).reduced (2));
    midiInputButton.setBounds (r.removeFromRight (160).reduced (2));
    keyLabel.setBounds (r.removeFromRight (90).reduced (2));
    timeSigLabel.setBounds (r.removeFromRight (70).reduced (2));
    tempoLabel.setBounds (r.removeFromRight (120).reduced (2));

    auto center = r.reduced (2);
    auto timeArea = center.removeFromTop (center.getHeight() / 2);
    timeLabel.setBounds (timeArea);
    barsLabel.setBounds (center);
}
