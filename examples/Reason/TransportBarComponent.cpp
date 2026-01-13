#include "TransportBarComponent.h"

TransportBarComponent::TransportBarComponent()
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (tempoLabel);

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

    importButton.onClick = [this]
    {
        if (onImport)
            onImport();
    };

    settingsButton.onClick = [this]
    {
        if (onSettings)
            onSettings();
    };

    timeLabel.setJustificationType (juce::Justification::centred);
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    timeLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    tempoLabel.setFont (juce::FontOptions (12.0f));
}

void TransportBarComponent::setTimeText (const juce::String& text)
{
    timeLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::setTempoText (const juce::String& text)
{
    tempoLabel.setText (text, juce::dontSendNotification);
}

void TransportBarComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1E1F23));
    g.setColour (juce::Colour (0xFF2A2C31));
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void TransportBarComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto left = r.removeFromLeft (360);

    playButton.setBounds (left.removeFromLeft (70).reduced (2));
    stopButton.setBounds (left.removeFromLeft (70).reduced (2));
    importButton.setBounds (left.removeFromLeft (140).reduced (2));

    settingsButton.setBounds (r.removeFromRight (100).reduced (2));
    tempoLabel.setBounds (r.removeFromRight (140).reduced (2));
    timeLabel.setBounds (r.reduced (2));
}
