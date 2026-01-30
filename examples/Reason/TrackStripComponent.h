#pragma once

#include <JuceHeader.h>

class TrackStripComponent : public juce::Component
{
public:
    explicit TrackStripComponent (int index);

    void setTrackName (const juce::String& name);
    void setInstrumentName (const juce::String& name);
    void setTrackNumber (int number);
    void setSelected (bool shouldSelect);
    void setVolumeNormalized (float value);
    void setEffectCount (int count);
    void setMuted (bool shouldMute);
    void setSolo (bool shouldSolo);

    int getTrackIndex() const noexcept { return trackIndex; }

    std::function<void (int)> onSelected;
    std::function<void (int, const juce::String&)> onNameChanged;
    std::function<void (int, float)> onVolumeChanged;
    std::function<void (int)> onInstrumentClicked;
    std::function<void (int)> onFxClicked;
    std::function<void (int, bool)> onMuteChanged;
    std::function<void (int, bool)> onSoloChanged;
    std::function<void (int, juce::Component*)> onContextMenuRequested;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    int trackIndex = 0;
    bool isSelected = false;

    juce::Label numberLabel;
    juce::Label nameLabel;
    juce::Slider volumeSlider;
    juce::TextButton instrumentButton { "Instrument" };
    juce::TextButton fxButton { "FX" };
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton aiButton { "AI" };
};
