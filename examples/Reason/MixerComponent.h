#pragma once

#include <JuceHeader.h>

class MixerComponent : public juce::Component
{
public:
    MixerComponent();

    void setTracks (const juce::StringArray& trackNames);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildSliders();

    juce::StringArray tracks;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
};
