#pragma once

#include <JuceHeader.h>
#include <unordered_set>
#include <unordered_map>

#include "SessionController.h"

class TimelineComponent : public juce::Component,
                          private juce::Timer,
                          private juce::ChangeListener,
                          private juce::ScrollBar::Listener
{
public:
    TimelineComponent();
    ~TimelineComponent() override;

    void setSession (SessionController* newSession);
    void setRowHeight (int height);
    void setScrollOffset (int offset);
    int getScrollOffset() const noexcept { return scrollOffset; }
    int getContentHeight() const noexcept;
    int getVisibleTrackHeight() const noexcept;
    int getTrackIndexAtY (int y) const noexcept;
    double getTimeAtX (int x) const noexcept;
    void addIdeaMarker (double timeSeconds);
    void setView (double newViewStartSeconds, double newPixelsPerSecond);
    uint64_t getSelectedClipId() const noexcept { return selectedClipId; }
    void setSelectedClipId (uint64_t newId);
    void clearSelection() noexcept;
    double getViewStartSeconds() const noexcept { return viewStartSeconds; }
    double getPixelsPerSecond() const noexcept { return pixelsPerSecond; }
    int getHeaderHeight() const noexcept { return rulerHeight + markerLaneHeight; }

    std::function<void (int)> onTrackSelected;
    std::function<void (const SessionController::ClipInfo&)> onClipDoubleClicked;
    std::function<void (double viewStart, double pixelsPerSecond)> onViewChanged;
    std::function<void (int)> onVerticalScrollChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details) override;
    void mouseMagnify (const juce::MouseEvent& event, float scaleFactor) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    struct ClipRect
    {
        SessionController::ClipInfo info;
        juce::Rectangle<int> bounds;
    };

    struct ThumbnailEntry
    {
        std::unique_ptr<juce::AudioThumbnail> thumbnail;
        juce::String sourcePath;
    };

    struct IdeaMarker
    {
        uint64_t id = 0;
        double timeSeconds = 0.0;
        juce::String text;
    };

    struct MarkerRect
    {
        int index = -1;
        juce::Rectangle<int> bounds;
    };

    enum class ResizeEdge
    {
        none,
        left,
        right
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    std::vector<ClipRect> buildClipRects() const;
    std::vector<MarkerRect> buildMarkerRects (juce::Graphics* g) const;
    void updateThumbnails();
    ThumbnailEntry* getThumbnailForClip (const SessionController::ClipInfo& clip);
    juce::AudioThumbnail* findThumbnail (uint64_t clipId) const;

    void updateScrollBar();
    void setViewStartSeconds (double newStart);
    double getVisibleDurationSeconds() const;
    void applyZoom (double scaleFactor, double anchorX);
    void beginEditMarker (int markerIndex, juce::Rectangle<int> bounds);
    void commitMarkerEdit();
    void cancelMarkerEdit();
    juce::String formatTime (double seconds) const;

    void drawRuler (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawMarkerLane (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawGridLines (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawClip (juce::Graphics& g, const ClipRect& clipRect) const;
    const ClipRect* findClipRectAt (juce::Point<int> position, std::vector<ClipRect>& clipRects) const;
    ResizeEdge getClipResizeEdgeAt (const ClipRect& clipRect, juce::Point<int> position) const;
    void updateMouseCursorForPosition (juce::Point<int> position);

    SessionController* session = nullptr;

    double pixelsPerSecond = 100.0;
    double viewStartSeconds = 0.0;
    double contentEndSeconds = 30.0;
    const double minPixelsPerSecond = 20.0;
    const double maxPixelsPerSecond = 800.0;
    int rulerHeight = 28;
    int markerLaneHeight = 26;
    int rowHeight = 72;
    int scrollBarHeight = 12;
    bool ignoreScrollCallback = false;
    bool suppressViewCallback = false;
    int scrollOffset = 0;

    uint64_t selectedClipId = 0;
    std::unordered_set<uint64_t> selectedClipIds;

    uint64_t draggingClipId = 0;
    double dragStartSeconds = 0.0;
    double dragOffsetSeconds = 0.0;
    double clipOriginalStartSeconds = 0.0;
    double clipOriginalLengthSeconds = 0.0;
    double resizePreviewStartSeconds = 0.0;
    double resizePreviewLengthSeconds = 0.0;
    bool isResizingClip = false;
    ResizeEdge activeResizeEdge = ResizeEdge::none;
    int resizeHoverEdgeMarginPx = 6;
    int dragSourceTrackIndex = -1;
    int dragTargetTrackIndex = -1;
    int dragTrackDelta = 0;
    std::unordered_map<uint64_t, double> dragOriginalClipStarts;
    std::unordered_map<uint64_t, int> dragOriginalClipTracks;

    std::vector<IdeaMarker> markers;
    uint64_t nextMarkerId = 1;
    int editingMarkerIndex = -1;
    juce::TextEditor markerEditor;

    juce::ScrollBar scrollBar { false };
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 128 };
    mutable std::unordered_map<uint64_t, ThumbnailEntry> thumbnails;
};
