#pragma once

#include <JuceHeader.h>

#include "TrackStripComponent.h"

class TrackListComponent : public juce::Component
{
public:
    TrackListComponent();

    void setTracks (const juce::StringArray& trackNames);
    void setTrackInstrumentNames (const juce::StringArray& instrumentNames);
    void setTrackVolumes (const juce::Array<float>& volumes);
    void setTrackPans (const juce::Array<float>& pans);
    void setTrackEffectCounts (const juce::Array<int>& counts);
    void setTrackMuteStates (const juce::Array<bool>& muteStates);
    void setTrackSoloStates (const juce::Array<bool>& soloStates);
    void setSelectedIndex (int index);
    void setHeaderHeight (int height);
    void setVisibleTrackHeightOverride (int height);
    void setScrollOffset (int offset);
    int getScrollOffset() const noexcept { return scrollOffset; }
    int getContentHeight() const noexcept { return tracks.size() * rowHeight; }
    int getTrackIndexAtY (int y) const noexcept;

    int getSelectedIndex() const noexcept { return selectedIndex; }
    int getRowHeight() const noexcept { return rowHeight; }
    int getHeaderHeight() const noexcept { return headerHeight; }

    std::function<void (int)> onSelectionChanged;
    std::function<void (int, const juce::String&)> onNameChanged;
    std::function<void (int, float)> onVolumeChanged;
    std::function<void (int, float)> onPanChanged;
    std::function<void (int)> onInstrumentClicked;
    std::function<void (int)> onFxClicked;
    std::function<void (int, bool)> onMuteChanged;
    std::function<void (int, bool)> onSoloChanged;
    std::function<void (int, juce::Component*)> onContextMenuRequested;
    std::function<void (int sourceIndex, int targetIndex)> onTrackReordered;
    std::function<void (int)> onVerticalScrollChanged;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details) override;

private:
    void rebuildStrips();

    juce::StringArray tracks;
    juce::StringArray instruments;
    juce::Array<float> trackVolumes;
    juce::Array<float> trackPans;
    juce::Array<int> trackEffectCounts;
    juce::Array<bool> trackMuted;
    juce::Array<bool> trackSoloed;
    juce::OwnedArray<TrackStripComponent> strips;

    int selectedIndex = 0;
    int headerHeight = 54;
    static constexpr int rowHeight = 104;
    int scrollOffset = 0;
    int visibleHeightOverride = -1;
    int dragReorderSourceIndex = -1;
    int dragReorderTargetIndex = -1;
    int dragStripY = -1;
    bool dragReorderActive = false;
};
