#pragma once

#include <JuceHeader.h>

class TrackStripComponent : public juce::Component
{
public:
    explicit TrackStripComponent (int index);

    void setTrackName (const juce::String& name);
    void setSelected (bool shouldSelect);
    void setVolumeNormalized (float value);

    int getTrackIndex() const noexcept { return trackIndex; }

    std::function<void (int)> onSelected;
    std::function<void (int, float)> onVolumeChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    int trackIndex = 0;
    bool isSelected = false;

    juce::Label nameLabel;
    juce::Slider volumeSlider;
    juce::TextButton aiButton { "AI" };
};
