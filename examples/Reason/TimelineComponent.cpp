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
    auto tracksArea = area;
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        auto trackArea = tracksArea.removeFromTop (rowHeight);
        trackArea.translate (0, -scrollOffset);
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
    selectedClipId = 0;
    draggingClipId = 0;
    dragOffsetSeconds = 0.0;
    dragSourceTrackIndex = -1;
    dragTargetTrackIndex = -1;

    for (const auto& clipRect : clipRects)
    {
        if (clipRect.bounds.contains (event.getPosition()))
        {
            selectedClipId = clipRect.info.id;
            draggingClipId = clipRect.info.id;
            dragStartSeconds = viewStartSeconds + event.position.x / pixelsPerSecond;
            clipOriginalStartSeconds = clipRect.info.startSeconds;
            dragSourceTrackIndex = clipRect.info.trackIndex;
            dragTargetTrackIndex = clipRect.info.trackIndex;
            session->setSelectedTrack (clipRect.info.trackIndex);
            if (onTrackSelected)
                onTrackSelected (clipRect.info.trackIndex);
            break;
        }
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
    double newStart = clipOriginalStartSeconds + (currentTime - dragStartSeconds);
    newStart = juce::jmax (0.0, newStart);
    dragOffsetSeconds = newStart - clipOriginalStartSeconds;

    const int edgeMargin = 24;
    if (event.position.x < edgeMargin)
        setViewStartSeconds (viewStartSeconds - 0.25);
    else if (event.position.x > getWidth() - edgeMargin)
        setViewStartSeconds (viewStartSeconds + 0.25);

    const int hoverTrack = getTrackIndexAtY ((int) event.position.y);
    if (hoverTrack >= 0)
        dragTargetTrackIndex = hoverTrack;

    repaint();
}

void TimelineComponent::mouseUp (const juce::MouseEvent&)
{
    if (session == nullptr || draggingClipId == 0)
        return;

    const double newStart = clipOriginalStartSeconds + dragOffsetSeconds;
    if (dragTargetTrackIndex >= 0 && dragTargetTrackIndex != dragSourceTrackIndex)
        session->moveClipToTrack (draggingClipId, dragTargetTrackIndex, newStart);
    else
        session->moveClip (draggingClipId, newStart);

    draggingClipId = 0;
    dragOffsetSeconds = 0.0;
    dragSourceTrackIndex = -1;
    dragTargetTrackIndex = -1;
    repaint();
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
            const int drawTrackIndex = (clip.id == draggingClipId && dragTargetTrackIndex >= 0)
                ? dragTargetTrackIndex
                : trackIndex;

            const int y = rulerHeight + markerLaneHeight + drawTrackIndex * rowHeight + 8 - scrollOffset;
            const int height = rowHeight - 16;

            const double startSeconds = (draggingClipId == clip.id)
                ? clip.startSeconds + dragOffsetSeconds
                : clip.startSeconds;

            const int x = (int) ((startSeconds - viewStartSeconds) * pixelsPerSecond);
            const int w = (int) juce::jmax (1.0, clip.lengthSeconds * pixelsPerSecond);

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

    const double maxSeconds = area.getWidth() / pixelsPerSecond;
    double step = 0.25;
    while (step * pixelsPerSecond < 8.0)
        step *= 2.0;

    double labelSpacing = 70.0;
    double lastLabelX = -1e9;

    for (double t = std::floor (viewStartSeconds / step) * step;
         t <= viewStartSeconds + maxSeconds + 0.0001;
         t += step)
    {
        const int x = (int) ((t - viewStartSeconds) * pixelsPerSecond);
        const bool isMajor = std::fmod (t, 1.0) < 0.0001;
        const bool isMedium = ! isMajor && (std::fmod (t, 0.5) < 0.0001);

        const int tickHeight = isMajor ? 16 : (isMedium ? 10 : 6);
        const float alpha = isMajor ? 0.7f : (isMedium ? 0.5f : 0.3f);

        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawLine ((float) x, (float) area.getBottom(), (float) x, (float) (area.getBottom() - tickHeight));

        if (isMajor && (x - lastLabelX) > labelSpacing)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (juce::String (t, 0) + "s", x + 4, area.getY(), 40, area.getHeight(),
                        juce::Justification::centredLeft);
            lastLabelX = (double) x;
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
        g.setColour (juce::Colour (0xFF4F6FE3));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xFF3C8A63));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    }

    g.setColour (juce::Colour (0xFF121418));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    juce::Rectangle<int> iconBounds (bounds.getX() + 6, bounds.getY() + 4, 18, 18);
    g.setColour (juce::Colour (0xFF101216));
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

        const double noteOffset = (draggingClipId == clip.id) ? dragOffsetSeconds : 0.0;
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
}
