#pragma once

#include <JuceHeader.h>

#include "TrackStripComponent.h"

class TrackListComponent : public juce::Component
{
public:
    TrackListComponent();

    void setTracks (const juce::StringArray& trackNames);
    void setTrackVolumes (const juce::Array<float>& volumes);
    void setSelectedIndex (int index);

    int getSelectedIndex() const noexcept { return selectedIndex; }
    int getRowHeight() const noexcept { return rowHeight; }

    std::function<void (int)> onSelectionChanged;
    std::function<void (int, float)> onVolumeChanged;

    void resized() override;

private:
    void rebuildStrips();

    juce::StringArray tracks;
    juce::Array<float> trackVolumes;
    juce::OwnedArray<TrackStripComponent> strips;

    int selectedIndex = 0;
    static constexpr int rowHeight = 72;
};
