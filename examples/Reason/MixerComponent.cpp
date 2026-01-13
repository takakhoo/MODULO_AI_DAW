#include "MixerComponent.h"

MixerComponent::MixerComponent()
{
    setOpaque (true);
}

void MixerComponent::setTracks (const juce::StringArray& trackNames)
{
    tracks = trackNames;
    rebuildSliders();
    resized();
    repaint();
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1C1D21));
    g.setColour (juce::Colour (0xFF2A2C31));
    g.drawRect (getLocalBounds());
}

void MixerComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    const int widthPerTrack = 80;

    for (int i = 0; i < sliders.size(); ++i)
    {
        auto column = r.removeFromLeft (widthPerTrack);
        sliders[i]->setBounds (column.removeFromTop (column.getHeight() - 18));
        labels[i]->setBounds (column.removeFromTop (18));
    }
}

void MixerComponent::rebuildSliders()
{
    sliders.clear();
    labels.clear();

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* slider = sliders.add (new juce::Slider());
        slider->setSliderStyle (juce::Slider::LinearVertical);
        slider->setRange (0.0, 1.0, 0.01);
        slider->setValue (0.8);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);

        auto* label = labels.add (new juce::Label());
        label->setText (tracks[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (label);
    }
}
