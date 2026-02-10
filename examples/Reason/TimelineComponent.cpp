#include "TimelineComponent.h"
#include "TrackColors.h"
#include <cmath>
#include <unordered_set>

TimelineComponent::TimelineComponent()
{
    setOpaque (true);
    formatManager.registerBasicFormats();
    addAndMakeVisible (scrollBar);
    addAndMakeVisible (markerEditor);

    scrollBar.setRangeLimits (0.0, contentEndSeconds);
    scrollBar.setCurrentRange (viewStartSeconds, getVisibleDurationSeconds());
    scrollBar.addListener (this);

    markerEditor.setVisible (false);
    markerEditor.setMultiLine (false);
    markerEditor.setReturnKeyStartsNewLine (false);
    markerEditor.onReturnKey = [this] { commitMarkerEdit(); };
    markerEditor.onEscapeKey = [this] { cancelMarkerEdit(); };
    markerEditor.onFocusLost = [this] { commitMarkerEdit(); };

    startTimerHz (30);
}

TimelineComponent::~TimelineComponent()
{
    scrollBar.removeListener (this);
}

void TimelineComponent::setSession (SessionController* newSession)
{
    session = newSession;
}

void TimelineComponent::setSelectedClipId (uint64_t newId)
{
    selectedClipId = newId;
    selectedClipIds.clear();
    if (newId != 0)
        selectedClipIds.insert (newId);
    repaint();
}

void TimelineComponent::clearSelection() noexcept
{
    selectedClipId = 0;
    selectedClipIds.clear();
    repaint();
}

void TimelineComponent::setRowHeight (int height)
{
    rowHeight = height;
}

void TimelineComponent::setScrollOffset (int offset)
{
    const int maxOffset = juce::jmax (0, getContentHeight() - getVisibleTrackHeight());
    scrollOffset = juce::jlimit (0, maxOffset, offset);
    repaint();
}

int TimelineComponent::getContentHeight() const noexcept
{
    if (session == nullptr)
        return 0;
    return session->getTrackCount() * rowHeight;
}

int TimelineComponent::getVisibleTrackHeight() const noexcept
{
    const int totalHeight = getHeight() - scrollBarHeight - rulerHeight - markerLaneHeight;
    return juce::jmax (0, totalHeight);
}

int TimelineComponent::getTrackIndexAtY (int y) const noexcept
{
    const int contentY = y + scrollOffset - rulerHeight - markerLaneHeight;
    if (contentY < 0)
        return -1;
    const int index = contentY / rowHeight;
    if (session == nullptr)
        return -1;
    return index >= 0 && index < session->getTrackCount() ? index : -1;
}

double TimelineComponent::getTimeAtX (int x) const noexcept
{
    return viewStartSeconds + (x / pixelsPerSecond);
}

void TimelineComponent::setView (double newViewStartSeconds, double newPixelsPerSecond)
{
    suppressViewCallback = true;
    pixelsPerSecond = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, newPixelsPerSecond);
    setViewStartSeconds (newViewStartSeconds);
    suppressViewCallback = false;
}

void TimelineComponent::addIdeaMarker (double timeSeconds)
{
    IdeaMarker marker;
    marker.id = nextMarkerId++;
    marker.timeSeconds = juce::jmax (0.0, timeSeconds);
    marker.text = "Idea";
    markers.push_back (marker);
    repaint();
}

void TimelineComponent::resized()
{
    auto r = getLocalBounds();
    auto scrollArea = r.removeFromBottom (scrollBarHeight);
    scrollBar.setBounds (scrollArea.reduced (4, 2));
    updateScrollBar();
}

void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF191B20));

    auto area = getLocalBounds();
    area.removeFromBottom (scrollBarHeight);

    auto ruler = area.removeFromTop (rulerHeight);
    auto markerLane = area.removeFromTop (markerLaneHeight);

    drawRuler (g, ruler);
    drawMarkerLane (g, markerLane);

    const int trackCount = session != nullptr ? session->getTrackCount() : 0;
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        auto trackArea = juce::Rectangle<int> (area.getX(),
                                               rulerHeight + markerLaneHeight + trackIndex * rowHeight - scrollOffset,
                                               area.getWidth(),
                                               rowHeight);
        const auto base = reason::trackcolours::base (trackIndex);
        g.setColour (base);
        g.fillRect (trackArea);
        g.setColour (juce::Colour (0xFF2B2F36));
        g.drawLine ((float) trackArea.getX(), (float) trackArea.getBottom() - 1.0f,
                    (float) trackArea.getRight(), (float) trackArea.getBottom() - 1.0f);
    }

    drawGridLines (g, area);

    if (session != nullptr)
    {
        auto clipRects = buildClipRects();
        for (const auto& clipRect : clipRects)
            drawClip (g, clipRect);

        const double playheadSeconds = session->getCurrentTimeSeconds();
        const int playheadX = (int) ((playheadSeconds - viewStartSeconds) * pixelsPerSecond);

        g.setColour (juce::Colour (0xFFEA5656));
        g.drawLine ((float) playheadX, (float) (rulerHeight + markerLaneHeight),
                    (float) playheadX, (float) (getHeight() - scrollBarHeight));
    }
}

void TimelineComponent::mouseDown (const juce::MouseEvent& event)
{
    if (session == nullptr)
        return;

    if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus();

    if (event.y >= rulerHeight && event.y < rulerHeight + markerLaneHeight)
    {
        const auto markerRects = buildMarkerRects (nullptr);
        for (const auto& marker : markerRects)
        {
            if (marker.bounds.contains (event.getPosition()))
            {
                editingMarkerIndex = marker.index;
                repaint();
                break;
            }
        }
    }

    auto clipRects = buildClipRects();
    draggingClipId = 0;
    dragOffsetSeconds = 0.0;
    dragSourceTrackIndex = -1;
    dragTargetTrackIndex = -1;
    dragTrackDelta = 0;
    dragOriginalClipStarts.clear();
    dragOriginalClipTracks.clear();
    isResizingClip = false;
    activeResizeEdge = ResizeEdge::none;
    resizePreviewLengthSeconds = 0.0;
    resizePreviewStartSeconds = 0.0;
    clipOriginalLengthSeconds = 0.0;

    const bool additiveSelect = event.mods.isShiftDown() || event.mods.isCommandDown();
    const auto* hitClipRect = findClipRectAt (event.getPosition(), clipRects);
    if (hitClipRect != nullptr)
    {
        const uint64_t hitId = hitClipRect->info.id;
        const bool wasAlreadySelected = selectedClipIds.find (hitId) != selectedClipIds.end();

        if (additiveSelect)
        {
            if (wasAlreadySelected)
                selectedClipIds.erase (hitId);
            else
                selectedClipIds.insert (hitId);

            if (selectedClipIds.empty())
                selectedClipId = 0;
            else
                selectedClipId = hitId;
        }
        else
        {
            if (! wasAlreadySelected)
            {
                selectedClipIds.clear();
                selectedClipIds.insert (hitId);
            }
            selectedClipId = hitId;
        }

        session->setSelectedTrack (hitClipRect->info.trackIndex);
        if (onTrackSelected)
            onTrackSelected (hitClipRect->info.trackIndex);

        if (! additiveSelect)
        {
            draggingClipId = hitId;
            dragStartSeconds = viewStartSeconds + event.position.x / pixelsPerSecond;
            clipOriginalStartSeconds = hitClipRect->info.startSeconds;
            clipOriginalLengthSeconds = hitClipRect->info.lengthSeconds;
            resizePreviewStartSeconds = clipOriginalStartSeconds;
            resizePreviewLengthSeconds = clipOriginalLengthSeconds;
            dragSourceTrackIndex = hitClipRect->info.trackIndex;
            dragTargetTrackIndex = hitClipRect->info.trackIndex;
            activeResizeEdge = (hitClipRect->info.type == SessionController::ClipType::midi)
                ? getClipResizeEdgeAt (*hitClipRect, event.getPosition())
                : ResizeEdge::none;
            isResizingClip = activeResizeEdge != ResizeEdge::none;

            for (const auto& clip : clipRects)
            {
                if (selectedClipIds.find (clip.info.id) != selectedClipIds.end())
                {
                    dragOriginalClipStarts[clip.info.id] = clip.info.startSeconds;
                    dragOriginalClipTracks[clip.info.id] = clip.info.trackIndex;
                }
            }
        }
    }
    else if (! additiveSelect)
    {
        selectedClipId = 0;
        selectedClipIds.clear();
    }

    const double clickTime = viewStartSeconds + event.position.x / pixelsPerSecond;
    session->setCursorTimeSeconds (clickTime);

    if (selectedClipId == 0)
    {
        const int trackIndex = getTrackIndexAtY ((int) event.position.y);
        if (trackIndex >= 0)
        {
            session->setSelectedTrack (trackIndex);
            if (onTrackSelected)
                onTrackSelected (trackIndex);
        }
    }

    repaint();
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (session == nullptr || draggingClipId == 0)
        return;

    const double currentTime = viewStartSeconds + event.position.x / pixelsPerSecond;
    if (isResizingClip)
    {
        constexpr double minClipLength = 0.01;

        if (activeResizeEdge == ResizeEdge::right)
        {
            resizePreviewStartSeconds = clipOriginalStartSeconds;
            resizePreviewLengthSeconds = juce::jmax (minClipLength, currentTime - clipOriginalStartSeconds);
        }
        else if (activeResizeEdge == ResizeEdge::left)
        {
            const double originalEnd = clipOriginalStartSeconds + clipOriginalLengthSeconds;
            const double maxLeft = originalEnd - minClipLength;
            const double clampedStart = juce::jlimit (0.0, maxLeft, currentTime);
            resizePreviewStartSeconds = clampedStart;
            resizePreviewLengthSeconds = juce::jmax (minClipLength, originalEnd - clampedStart);
        }

        repaint();
        return;
    }

    dragOffsetSeconds = currentTime - dragStartSeconds;
    double minStart = clipOriginalStartSeconds;
    for (const auto& [_, start] : dragOriginalClipStarts)
        minStart = juce::jmin (minStart, start);
    if (minStart + dragOffsetSeconds < 0.0)
        dragOffsetSeconds = -minStart;

    const int edgeMargin = 24;
    if (event.position.x < edgeMargin)
        setViewStartSeconds (viewStartSeconds - 0.25);
    else if (event.position.x > getWidth() - edgeMargin)
        setViewStartSeconds (viewStartSeconds + 0.25);

    const int hoverTrack = getTrackIndexAtY ((int) event.position.y);
    if (hoverTrack >= 0)
    {
        dragTargetTrackIndex = hoverTrack;
        dragTrackDelta = hoverTrack - dragSourceTrackIndex;
    }

    repaint();
}

void TimelineComponent::mouseUp (const juce::MouseEvent&)
{
    if (session == nullptr || draggingClipId == 0)
        return;

    if (isResizingClip)
    {
        session->resizeClipRange (draggingClipId, resizePreviewStartSeconds, resizePreviewLengthSeconds);
    }
    else
    {
        const int trackCount = juce::jmax (0, session->getTrackCount());
        for (const auto& [clipId, originalStart] : dragOriginalClipStarts)
        {
            const double newStart = juce::jmax (0.0, originalStart + dragOffsetSeconds);
            const int originalTrack = dragOriginalClipTracks.count (clipId) != 0 ? dragOriginalClipTracks[clipId] : dragSourceTrackIndex;
            int targetTrack = originalTrack;
            targetTrack = juce::jlimit (0, juce::jmax (0, trackCount - 1), targetTrack + dragTrackDelta);

            const bool movedTrack = dragTrackDelta != 0 && targetTrack != originalTrack;
            if (movedTrack)
                session->moveClipToTrack (clipId, targetTrack, newStart);
            else
                session->moveClip (clipId, newStart);
        }
    }

    draggingClipId = 0;
    dragOffsetSeconds = 0.0;
    dragSourceTrackIndex = -1;
    dragTargetTrackIndex = -1;
    dragTrackDelta = 0;
    dragOriginalClipStarts.clear();
    dragOriginalClipTracks.clear();
    isResizingClip = false;
    activeResizeEdge = ResizeEdge::none;
    resizePreviewStartSeconds = 0.0;
    resizePreviewLengthSeconds = 0.0;
    clipOriginalLengthSeconds = 0.0;
    repaint();
}

void TimelineComponent::mouseMove (const juce::MouseEvent& event)
{
    updateMouseCursorForPosition (event.getPosition());
}

void TimelineComponent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details)
{
    if (event.mods.isCommandDown())
    {
        const double zoomFactor = std::pow (1.1, details.deltaY * 10.0);
        applyZoom (zoomFactor, event.position.x);
        return;
    }

    if (event.mods.isShiftDown() || details.deltaX != 0.0f)
    {
        const float delta = (details.deltaX != 0.0f) ? details.deltaX : details.deltaY;
        const double pixelDelta = -delta * 300.0;
        setViewStartSeconds (viewStartSeconds + (pixelDelta / pixelsPerSecond));
        return;
    }

    const int delta = (int) std::round (-details.deltaY * 60.0f);
    if (delta == 0)
        return;

    setScrollOffset (scrollOffset + delta);
    if (onVerticalScrollChanged)
        onVerticalScrollChanged (scrollOffset);
}

void TimelineComponent::mouseMagnify (const juce::MouseEvent& event, float scaleFactor)
{
    applyZoom (scaleFactor, event.position.x);
}

void TimelineComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.y >= rulerHeight && event.y < rulerHeight + markerLaneHeight)
    {
        const auto markerRects = buildMarkerRects (nullptr);
        for (const auto& marker : markerRects)
        {
            if (marker.bounds.contains (event.getPosition()))
            {
                beginEditMarker (marker.index, marker.bounds);
                return;
            }
        }
    }

    const auto clipRects = buildClipRects();
    for (const auto& clipRect : clipRects)
    {
        if (clipRect.bounds.contains (event.getPosition()))
        {
            if (onClipDoubleClicked)
                onClipDoubleClicked (clipRect.info);
            return;
        }
    }
}

void TimelineComponent::timerCallback()
{
    updateThumbnails();
    repaint();
}

void TimelineComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void TimelineComponent::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved != &scrollBar || ignoreScrollCallback)
        return;

    setViewStartSeconds (newRangeStart);
}

std::vector<TimelineComponent::ClipRect> TimelineComponent::buildClipRects() const
{
    std::vector<ClipRect> clipRects;

    if (session == nullptr)
        return clipRects;

    const int trackCount = session->getTrackCount();
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        auto clips = session->getClipsForTrack (trackIndex);
        for (const auto& clip : clips)
        {
            int drawTrackIndex = trackIndex;
            if (draggingClipId != 0 && ! isResizingClip)
            {
                auto itTrack = dragOriginalClipTracks.find (clip.id);
                if (itTrack != dragOriginalClipTracks.end())
                {
                    const int maxTrack = juce::jmax (0, session->getTrackCount() - 1);
                    drawTrackIndex = juce::jlimit (0, maxTrack, itTrack->second + dragTrackDelta);
                }
            }

            const int y = rulerHeight + markerLaneHeight + drawTrackIndex * rowHeight + 8 - scrollOffset;
            const int height = rowHeight - 16;

            double startSeconds = clip.startSeconds;
            if (draggingClipId != 0 && ! isResizingClip)
            {
                auto itStart = dragOriginalClipStarts.find (clip.id);
                if (itStart != dragOriginalClipStarts.end())
                    startSeconds = juce::jmax (0.0, itStart->second + dragOffsetSeconds);
            }

            if (isResizingClip && clip.id == draggingClipId)
                startSeconds = resizePreviewStartSeconds;

            const int x = (int) ((startSeconds - viewStartSeconds) * pixelsPerSecond);
            const double displayLength = (isResizingClip && clip.id == draggingClipId)
                ? resizePreviewLengthSeconds
                : clip.lengthSeconds;
            const int w = (int) juce::jmax (1.0, displayLength * pixelsPerSecond);

            ClipRect rect;
            rect.info = clip;
            rect.bounds = { x, y, w, height };
            clipRects.push_back (rect);
        }
    }

    return clipRects;
}

std::vector<TimelineComponent::MarkerRect> TimelineComponent::buildMarkerRects (juce::Graphics* g) const
{
    std::vector<MarkerRect> rects;
    juce::ignoreUnused (g);
    if (markers.empty())
        return rects;

    const juce::Font font (juce::FontOptions (12.0f));
    const int paddingX = 10;
    const int paddingY = 4;

    for (int i = 0; i < (int) markers.size(); ++i)
    {
        const auto& marker = markers[(size_t) i];
        const double time = marker.timeSeconds;
        const int x = (int) ((time - viewStartSeconds) * pixelsPerSecond);

        const juce::String label = formatTime (time) + "  " + marker.text;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, label, 0.0f, 0.0f);
        const int textWidth = (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth());
        const int width = juce::jmax (60, textWidth + paddingX * 2);
        const int height = markerLaneHeight - paddingY * 2;

        juce::Rectangle<int> bounds (x, rulerHeight + paddingY, width, height);

        MarkerRect rect;
        rect.index = i;
        rect.bounds = bounds;
        rects.push_back (rect);
    }

    return rects;
}

void TimelineComponent::updateThumbnails()
{
    if (session == nullptr)
        return;

    std::unordered_set<uint64_t> activeIds;
    double maxEndSeconds = 10.0;
    const int trackCount = session->getTrackCount();

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        auto clips = session->getClipsForTrack (trackIndex);
        for (const auto& clip : clips)
        {
            activeIds.insert (clip.id);
            maxEndSeconds = juce::jmax (maxEndSeconds, clip.startSeconds + clip.lengthSeconds);

            if (clip.type != SessionController::ClipType::audio)
                continue;

            getThumbnailForClip (clip);
        }
    }

    for (const auto& marker : markers)
        maxEndSeconds = juce::jmax (maxEndSeconds, marker.timeSeconds + 2.0);

    const double visible = getVisibleDurationSeconds();
    maxEndSeconds = juce::jmax (maxEndSeconds, visible);

    if (std::abs (maxEndSeconds - contentEndSeconds) > 0.01)
    {
        contentEndSeconds = maxEndSeconds;
        setViewStartSeconds (viewStartSeconds);
    }

    for (auto it = thumbnails.begin(); it != thumbnails.end();)
    {
        if (activeIds.find (it->first) == activeIds.end())
            it = thumbnails.erase (it);
        else
            ++it;
    }
}

TimelineComponent::ThumbnailEntry* TimelineComponent::getThumbnailForClip (const SessionController::ClipInfo& clip)
{
    auto it = thumbnails.find (clip.id);
    if (it != thumbnails.end())
    {
        if (it->second.sourcePath == clip.sourcePath)
            return &it->second;

        thumbnails.erase (it);
    }

    ThumbnailEntry entry;
    entry.sourcePath = clip.sourcePath;
    entry.thumbnail = std::make_unique<juce::AudioThumbnail> (512, formatManager, thumbnailCache);
    entry.thumbnail->addChangeListener (this);
    entry.thumbnail->setSource (new juce::FileInputSource (juce::File (clip.sourcePath)));

    auto [inserted, _] = thumbnails.emplace (clip.id, std::move (entry));
    return &inserted->second;
}

juce::AudioThumbnail* TimelineComponent::findThumbnail (uint64_t clipId) const
{
    auto it = thumbnails.find (clipId);
    if (it == thumbnails.end())
        return nullptr;

    return it->second.thumbnail.get();
}

void TimelineComponent::updateScrollBar()
{
    const double visibleSeconds = getVisibleDurationSeconds();
    ignoreScrollCallback = true;
    scrollBar.setRangeLimits (0.0, contentEndSeconds);
    scrollBar.setCurrentRange (viewStartSeconds, visibleSeconds);
    scrollBar.setSingleStepSize (0.25);
    ignoreScrollCallback = false;
}

void TimelineComponent::setViewStartSeconds (double newStart)
{
    const double visible = getVisibleDurationSeconds();
    const double maxStart = juce::jmax (0.0, contentEndSeconds - visible);
    const double nextStart = juce::jlimit (0.0, maxStart, newStart);
    if (std::abs (nextStart - viewStartSeconds) < 0.0001)
        return;

    viewStartSeconds = nextStart;
    updateScrollBar();
    repaint();

    if (onViewChanged && ! suppressViewCallback)
        onViewChanged (viewStartSeconds, pixelsPerSecond);
}

double TimelineComponent::getVisibleDurationSeconds() const
{
    const int width = juce::jmax (1, getWidth());
    return width / pixelsPerSecond;
}

void TimelineComponent::applyZoom (double scaleFactor, double anchorX)
{
    if (scaleFactor <= 0.0)
        return;

    const double anchorTime = viewStartSeconds + anchorX / pixelsPerSecond;
    const double newPps = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, pixelsPerSecond * scaleFactor);
    if (std::abs (newPps - pixelsPerSecond) < 0.0001)
        return;

    const double previousStart = viewStartSeconds;
    const double newStart = anchorTime - anchorX / newPps;
    pixelsPerSecond = newPps;
    setViewStartSeconds (newStart);

    if (std::abs (newStart - previousStart) < 0.0001)
    {
        updateScrollBar();
        repaint();
        if (onViewChanged && ! suppressViewCallback)
            onViewChanged (viewStartSeconds, pixelsPerSecond);
    }
}

void TimelineComponent::beginEditMarker (int markerIndex, juce::Rectangle<int> bounds)
{
    if (markerIndex < 0 || markerIndex >= (int) markers.size())
        return;

    editingMarkerIndex = markerIndex;
    markerEditor.setText (markers[(size_t) markerIndex].text, juce::dontSendNotification);
    markerEditor.setBounds (bounds.reduced (6, 2));
    markerEditor.setVisible (true);
    markerEditor.grabKeyboardFocus();
}

void TimelineComponent::commitMarkerEdit()
{
    if (editingMarkerIndex < 0 || editingMarkerIndex >= (int) markers.size())
    {
        markerEditor.setVisible (false);
        return;
    }

    auto text = markerEditor.getText().trim();
    if (text.isEmpty())
        text = "Idea";

    markers[(size_t) editingMarkerIndex].text = text;
    markerEditor.setVisible (false);
    editingMarkerIndex = -1;
    repaint();
}

void TimelineComponent::cancelMarkerEdit()
{
    markerEditor.setVisible (false);
    editingMarkerIndex = -1;
    repaint();
}

juce::String TimelineComponent::formatTime (double seconds) const
{
    const int minutes = (int) (seconds / 60.0);
    const double secs = seconds - (minutes * 60.0);
    return juce::String::formatted ("%02d:%04.1f", minutes, secs);
}

void TimelineComponent::drawRuler (juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto top = juce::Colour (0xFF2B2F36);
    auto bottom = juce::Colour (0xFF23262C);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, (float) area.getY(),
                                             bottom, 0.0f, (float) area.getBottom(), false));
    g.fillRect (area);

    if (session == nullptr)
        return;

    auto& tempoSeq = session->getEdit().tempoSequence;
    const double visibleSeconds = area.getWidth() / pixelsPerSecond;
    const double startSeconds = viewStartSeconds;
    const double endSeconds = viewStartSeconds + visibleSeconds;

    const auto& timeSig = tempoSeq.getTimeSigAt (te::TimePosition::fromSeconds (startSeconds));
    const int numerator = juce::jmax (1, timeSig.numerator.get());
    const auto startBarsBeats = tempoSeq.toBarsAndBeats (te::TimePosition::fromSeconds (startSeconds));
    int barIndex = juce::jmax (0, startBarsBeats.bars);

    double labelSpacing = 56.0;
    double lastLabelX = -1e9;

    for (;; ++barIndex)
    {
        for (int beat = 0; beat < numerator; ++beat)
        {
            const double beatPosition = (double) barIndex * (double) numerator + (double) beat;
            const auto beatPos = te::BeatPosition::fromBeats (beatPosition);
            const double time = tempoSeq.toTime (beatPos).inSeconds();

            if (time > endSeconds + 0.01)
                return;

            if (time < startSeconds - 0.01)
                continue;

            const int x = (int) ((time - viewStartSeconds) * pixelsPerSecond);
            const bool isBar = (beat == 0);
            const bool isBeat = ! isBar;
            const int tickHeight = isBar ? 16 : 8;
            const float alpha = isBar ? 0.75f : (isBeat ? 0.45f : 0.3f);

            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawLine ((float) x, (float) area.getBottom(), (float) x, (float) (area.getBottom() - tickHeight));

            if (isBar && (x - lastLabelX) > labelSpacing)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawText ("Bar " + juce::String (barIndex + 1), x + 4, area.getY(), 72, area.getHeight(),
                            juce::Justification::centredLeft);
                lastLabelX = (double) x;
            }
        }
    }
}

void TimelineComponent::drawMarkerLane (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xFF20242B));
    g.fillRect (area);

    g.setColour (juce::Colour (0xFF2D3138));
    g.drawLine ((float) area.getX(), (float) area.getBottom() - 1.0f,
                (float) area.getRight(), (float) area.getBottom() - 1.0f);

    auto markerRects = buildMarkerRects (&g);
    g.setFont (juce::FontOptions (12.0f));

    for (const auto& marker : markerRects)
    {
        const auto& data = markers[(size_t) marker.index];
        const juce::String label = formatTime (data.timeSeconds) + "  " + data.text;
        auto bounds = marker.bounds;

        g.setColour (juce::Colour (0xFF2E445A));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (label, bounds.reduced (8, 2), juce::Justification::centredLeft, true);
    }
}

void TimelineComponent::drawGridLines (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (session == nullptr)
        return;

    auto& edit = session->getEdit();
    auto& tempoSeq = edit.tempoSequence;

    const double visibleSeconds = area.getWidth() / pixelsPerSecond;
    const double startSeconds = viewStartSeconds;
    const double endSeconds = viewStartSeconds + visibleSeconds;

    const auto& timeSig = tempoSeq.getTimeSigAt (te::TimePosition::fromSeconds (startSeconds));
    const int numerator = juce::jmax (1, timeSig.numerator.get());

    const auto startBarsBeats = tempoSeq.toBarsAndBeats (te::TimePosition::fromSeconds (startSeconds));
    int barIndex = startBarsBeats.bars;
    if (barIndex < 0)
        barIndex = 0;

    const double minSubdivisionPixels = 12.0;

    for (;; ++barIndex)
    {
        for (int beat = 0; beat < numerator; ++beat)
        {
            const double beatPosition = (double) barIndex * (double) numerator + (double) beat;
            const auto beatPos = te::BeatPosition::fromBeats (beatPosition);
            const double time = tempoSeq.toTime (beatPos).inSeconds();

            if (time > endSeconds + 0.01)
                return;

            if (time < startSeconds - 0.01)
                continue;

            const int x = (int) ((time - viewStartSeconds) * pixelsPerSecond);
            const bool isBar = (beat == 0);
            g.setColour (juce::Colours::white.withAlpha (isBar ? 0.18f : 0.08f));
            g.drawLine ((float) x, (float) area.getY(), (float) x, (float) area.getBottom());

            if (pixelsPerSecond >= minSubdivisionPixels * 4.0)
            {
                const double halfBeat = beatPosition + 0.5;
                const auto halfBeatPos = te::BeatPosition::fromBeats (halfBeat);
                const double halfTime = tempoSeq.toTime (halfBeatPos).inSeconds();
                const int hx = (int) ((halfTime - viewStartSeconds) * pixelsPerSecond);
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.drawLine ((float) hx, (float) area.getY(), (float) hx, (float) area.getBottom());
            }
        }
    }
}

void TimelineComponent::drawClip (juce::Graphics& g, const ClipRect& clipRect) const
{
    const auto& clip = clipRect.info;
    auto bounds = clipRect.bounds;

    if (clip.type == SessionController::ClipType::midi)
    {
        const auto fill = reason::trackcolours::highlight (clip.trackIndex).withMultipliedBrightness (1.15f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xFF3C8A63));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }

    g.setColour (reason::trackcolours::accent (clip.trackIndex).withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    juce::Rectangle<int> iconBounds (bounds.getX() + 6, bounds.getY() + 4, 18, 18);
    g.setColour (reason::trackcolours::base (clip.trackIndex).darker (0.3f));
    g.fillRoundedRectangle (iconBounds.toFloat(), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText (clip.type == SessionController::ClipType::midi ? "M" : "A", iconBounds, juce::Justification::centred);

    if (clip.type == SessionController::ClipType::audio)
    {
        if (auto* thumbnail = findThumbnail (clip.id))
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            thumbnail->drawChannels (g, bounds.reduced (4, 18),
                                     0.0,
                                     clip.lengthSeconds,
                                     0.8f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            for (int i = 0; i < bounds.getWidth(); i += 6)
            {
                const int x = bounds.getX() + i;
                const int y = bounds.getCentreY();
                const int h = (i % 12 == 0) ? 18 : 10;
                g.drawLine ((float) x, (float) (y - h / 2), (float) x, (float) (y + h / 2));
            }
        }
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        const int maxNotesToDraw = 200;
        int noteCount = 0;

            double noteOffset = 0.0;
            if (draggingClipId != 0 && ! isResizingClip)
            {
                auto itStart = dragOriginalClipStarts.find (clip.id);
                if (itStart != dragOriginalClipStarts.end())
                    noteOffset = dragOffsetSeconds;
            }
        for (const auto& note : clip.notes)
        {
            if (noteCount++ > maxNotesToDraw)
                break;

            const double noteStart = note.startSeconds + noteOffset;

            const int x = (int) ((noteStart - viewStartSeconds) * pixelsPerSecond);
            const int w = (int) juce::jmax (1.0, note.lengthSeconds * pixelsPerSecond);

            if (x > bounds.getRight() || x + w < bounds.getX())
                continue;

            const float pitchNorm = (float) note.noteNumber / 127.0f;
            const int noteY = bounds.getBottom() - (int) (pitchNorm * (bounds.getHeight() - 10)) - 6;
            juce::Rectangle<int> noteRect (x, noteY, w, 4);
            noteRect = noteRect.getIntersection (bounds.reduced (2));

            g.fillRect (noteRect);
        }
    }

    const juce::String label = clip.name;
    g.setColour (juce::Colours::white);
    g.drawText (label, bounds.reduced (28, 2).withHeight (16), juce::Justification::centredLeft, true);

    if (selectedClipId == clip.id)
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 2.0f);
    }
    else if (selectedClipIds.find (clip.id) != selectedClipIds.end())
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.5f);
    }

    if (clip.type == SessionController::ClipType::midi)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        const int leftHandleX = bounds.getX() + 3;
        g.drawLine ((float) leftHandleX, (float) bounds.getY() + 5.0f, (float) leftHandleX, (float) bounds.getBottom() - 5.0f, 1.5f);
        const int handleX = bounds.getRight() - 4;
        g.drawLine ((float) handleX, (float) bounds.getY() + 5.0f, (float) handleX, (float) bounds.getBottom() - 5.0f, 1.5f);
    }
}

const TimelineComponent::ClipRect* TimelineComponent::findClipRectAt (juce::Point<int> position,
                                                                       std::vector<ClipRect>& clipRects) const
{
    for (auto it = clipRects.rbegin(); it != clipRects.rend(); ++it)
        if (it->bounds.contains (position))
            return &(*it);
    return nullptr;
}

TimelineComponent::ResizeEdge TimelineComponent::getClipResizeEdgeAt (const ClipRect& clipRect,
                                                                       juce::Point<int> position) const
{
    const auto leftEdgeBounds = clipRect.bounds.withWidth (resizeHoverEdgeMarginPx * 2);
    if (leftEdgeBounds.contains (position))
        return ResizeEdge::left;

    const auto rightEdgeBounds = clipRect.bounds.withX (clipRect.bounds.getRight() - resizeHoverEdgeMarginPx)
        .withWidth (resizeHoverEdgeMarginPx * 2);
    if (rightEdgeBounds.contains (position))
        return ResizeEdge::right;

    return ResizeEdge::none;
}

void TimelineComponent::updateMouseCursorForPosition (juce::Point<int> position)
{
    auto clipRects = buildClipRects();
    if (const auto* clipRect = findClipRectAt (position, clipRects))
    {
        if (clipRect->info.type == SessionController::ClipType::midi
            && getClipResizeEdgeAt (*clipRect, position) != ResizeEdge::none)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}
