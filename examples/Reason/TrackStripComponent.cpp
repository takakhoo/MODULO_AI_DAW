#include "TrackStripComponent.h"

TrackStripComponent::TrackStripComponent (int index)
    : trackIndex (index)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (aiButton);

    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange (0.0, 1.0, 0.001);
    volumeSlider.onValueChange = [this]
    {
        if (onVolumeChanged)
            onVolumeChanged (trackIndex, (float) volumeSlider.getValue());
    };

    aiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    aiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void TrackStripComponent::setTrackName (const juce::String& name)
{
    nameLabel.setText (name, juce::dontSendNotification);
}

void TrackStripComponent::setSelected (bool shouldSelect)
{
    isSelected = shouldSelect;
    repaint();
}

void TrackStripComponent::setVolumeNormalized (float value)
{
    volumeSlider.setValue (juce::jlimit (0.0f, 1.0f, value), juce::dontSendNotification);
}

void TrackStripComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (isSelected ? juce::Colour (0xFF243B52) : juce::Colour (0xFF1C1E22));
    g.fillRect (r);

    g.setColour (juce::Colour (0xFF2C2F36));
    g.drawRect (r);
}

void TrackStripComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto top = r.removeFromTop (22);

    nameLabel.setBounds (top.removeFromLeft (r.getWidth() - 40));
    aiButton.setBounds (top.removeFromRight (36).reduced (0, 2));

    r.removeFromTop (6);
    volumeSlider.setBounds (r.removeFromTop (18));
}

void TrackStripComponent::mouseDown (const juce::MouseEvent&)
{
    if (onSelected)
        onSelected (trackIndex);
}
