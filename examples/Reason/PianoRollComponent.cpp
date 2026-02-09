#include "PianoRollComponent.h"

#include <cmath>
#include <limits>

PianoRollComponent::PianoRollComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

void PianoRollComponent::setSession (SessionController* newSession)
{
    session = newSession;
}

void PianoRollComponent::setClipId (uint64_t newClipId)
{
    clipId = newClipId;
    selectedNoteState = {};
    draggingNoteState = {};
    selectedNotes.clear();
    dragMode = DragMode::none;
    dragAllNotes = false;
    isLassoSelecting = false;
    lassoSelection = {};
    repaint();
}

void PianoRollComponent::setView (double newViewStartSeconds, double newPixelsPerSecond)
{
    viewStartSeconds = newViewStartSeconds;
    pixelsPerSecond = newPixelsPerSecond;
    repaint();
}

void PianoRollComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1B1C20));

    auto area = getLocalBounds();
    auto keyboardArea = area.removeFromLeft (keyboardWidth);
    auto rulerArea = showRuler ? area.removeFromTop (rulerHeight) : juce::Rectangle<int>();
    auto sustainArea = area.removeFromBottom (sustainLaneHeight);
    auto noteArea = area;

    drawKeyboard (g, keyboardArea);
    if (showRuler)
        drawRuler (g, rulerArea);
    drawGrid (g, noteArea);

    if (session == nullptr || clipId == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText ("Select a MIDI clip to edit", noteArea, juce::Justification::centred);
        return;
    }

    const auto notes = buildNoteRects();
    drawNotes (g, noteArea, notes);
    drawChordLabels (g, noteArea);
    drawSustainLane (g, sustainArea);

    if (isLassoSelecting && ! lassoSelection.isEmpty())
    {
        g.setColour (juce::Colour (0x66A5D8FF));
        g.fillRect (lassoSelection);
        g.setColour (juce::Colour (0xFF9DDCFF));
        g.drawRect (lassoSelection, 1);
    }

    if (session != nullptr)
    {
        const double playheadSeconds = session->getCurrentTimeSeconds();
        const int x = (int) ((playheadSeconds - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        g.setColour (juce::Colour (0xFFEA5656));
        g.drawLine ((float) x, (float) noteArea.getY(), (float) x, (float) sustainArea.getBottom(), 1.2f);
    }
}

void PianoRollComponent::resized()
{
}

void PianoRollComponent::timerCallback()
{
    if (isShowing() && session != nullptr && clipId != 0)
        repaint();
}

void PianoRollComponent::mouseDown (const juce::MouseEvent& event)
{
    if (session == nullptr || clipId == 0)
        return;

    grabKeyboardFocus();

    auto local = getLocalBounds();
    local.removeFromLeft (keyboardWidth);
    const bool inRuler = showRuler && event.y < rulerHeight;
    if (showRuler)
        local.removeFromTop (rulerHeight);
    auto sustainArea = local.removeFromBottom (sustainLaneHeight);
    auto noteArea = local;

    if (event.x >= keyboardWidth)
    {
        const double clickTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
        session->setCursorTimeSeconds (juce::jmax (0.0, clickTime));
        if (inRuler)
        {
            repaint();
            return;
        }
    }

    if (sustainArea.contains (event.getPosition()))
        return;

    auto noteRects = buildNoteRects();
    selectedNoteState = {};
    draggingNoteState = {};
    dragMode = DragMode::none;
    dragAllNotes = false;
    dragAllResizing = false;
    dragAllResizeDelta = 0.0;
    dragAllDeltaSeconds = 0.0;
    dragAllDeltaSemitones = 0;
    dragAllBaseNotes.clear();
    isLassoSelecting = false;
    lassoSelection = {};
    lassoAdditive = false;

    for (const auto& note : noteRects)
    {
        if (! note.bounds.contains (event.getPosition()))
            continue;

        if (event.mods.isCommandDown() || event.mods.isShiftDown())
        {
            if (isNoteSelected (note.info.state))
                selectedNotes.removeAllInstancesOf (note.info.state);
            else
                selectedNotes.addIfNotAlreadyThere (note.info.state);
        }
        else
        {
            // Keep multi-selection when clicking any currently selected note.
            if (! isNoteSelected (note.info.state) || selectedNotes.size() <= 1)
            {
                selectedNotes.clear();
                selectedNotes.add (note.info.state);
            }
        }

        selectedNoteState = note.info.state;
        draggingNoteState = note.info.state;
        dragStartSeconds = note.info.startSeconds;
        dragStartLength = note.info.lengthSeconds;
        dragStartNote = note.info.noteNumber;
        dragPreviewStart = dragStartSeconds;
        dragPreviewLength = dragStartLength;
        dragPreviewNote = dragStartNote;

        const int resizeHandle = 6;
        if (event.x > note.bounds.getRight() - resizeHandle)
        {
            dragMode = DragMode::resize;
        }
        else
        {
            dragMode = DragMode::move;
            const double clickTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
            dragOffsetSeconds = clickTime - dragStartSeconds;
        }

        if (dragMode == DragMode::move && selectedNotes.size() > 1)
        {
            dragAllNotes = true;
            dragAllBaseNotes = session->getMidiNotesForClip (clipId);
        }
        else if (dragMode == DragMode::resize && selectedNotes.size() > 1)
        {
            dragAllResizing = true;
            dragAllBaseNotes = session->getMidiNotesForClip (clipId);
        }

        repaint();
        return;
    }

    if (event.x < keyboardWidth)
        return;

    if (! event.mods.isCtrlDown())
    {
        isLassoSelecting = true;
        lassoAdditive = event.mods.isCommandDown() || event.mods.isShiftDown();
        lassoStartPoint = event.getPosition();
        lassoSelection = juce::Rectangle<int> (lassoStartPoint, lassoStartPoint).getIntersection (noteArea);
        if (! lassoAdditive)
            selectedNotes.clear();
        selectedNoteState = {};
        repaint();
        return;
    }

    const double clickTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
    const double startTime = quantizeSeconds (juce::jmax (0.0, clickTime));
    const double length = getGridSeconds();

    const int noteNumber = getNoteFromY (event.y, noteArea);

    if (session->addMidiNote (clipId, noteNumber, startTime, length))
        repaint();
}

void PianoRollComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (session == nullptr || clipId == 0)
        return;

    if (isLassoSelecting)
    {
        auto noteArea = getNoteGridArea();
        lassoSelection = juce::Rectangle<int> (lassoStartPoint, event.getPosition()).getIntersection (noteArea);
        repaint();
        return;
    }

    if (dragMode == DragMode::none || ! draggingNoteState.isValid())
        return;

    auto local = getLocalBounds();
    local.removeFromLeft (keyboardWidth);
    if (showRuler)
        local.removeFromTop (rulerHeight);
    local.removeFromBottom (sustainLaneHeight);
    const auto noteArea = local;

    if (dragMode == DragMode::move)
    {
        const double dragTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
        const double startTime = juce::jmax (0.0, dragTime - dragOffsetSeconds);
        const int noteNumber = getNoteFromY (event.y, noteArea);

        dragPreviewStart = startTime;
        dragPreviewNote = noteNumber;
        dragPreviewLength = dragStartLength;

        if (dragAllNotes)
        {
            dragAllDeltaSeconds = startTime - dragStartSeconds;
            dragAllDeltaSemitones = noteNumber - dragStartNote;
        }
    }
    else if (dragMode == DragMode::resize)
    {
        const double dragTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
        const double newLength = dragTime - dragStartSeconds;
        dragPreviewLength = juce::jmax (0.01, newLength);
        dragPreviewStart = dragStartSeconds;
        dragPreviewNote = dragStartNote;

        if (dragAllResizing)
            dragAllResizeDelta = dragPreviewLength - dragStartLength;
    }

    repaint();
}

void PianoRollComponent::mouseUp (const juce::MouseEvent&)
{
    if (session == nullptr || clipId == 0)
        return;

    if (isLassoSelecting)
    {
        auto notes = buildNoteRects();
        for (const auto& note : notes)
        {
            if (note.bounds.intersects (lassoSelection))
                selectedNotes.addIfNotAlreadyThere (note.info.state);
        }

        if (selectedNotes.size() == 1)
            selectedNoteState = selectedNotes.getFirst();
        else
            selectedNoteState = {};

        isLassoSelecting = false;
        lassoSelection = {};
        repaint();
        return;
    }

    if (dragMode == DragMode::move && draggingNoteState.isValid())
    {
        if (dragAllNotes)
        {
            for (const auto& note : dragAllBaseNotes)
            {
                if (! isNoteSelected (note.state))
                    continue;

                const double newStart = juce::jmax (0.0, note.startSeconds + dragAllDeltaSeconds);
                const int newNote = juce::jlimit (lowestNote, highestNote, note.noteNumber + dragAllDeltaSemitones);
                session->updateMidiNote (clipId, note.state, newStart, note.lengthSeconds, newNote);
            }
        }
        else
        {
            session->updateMidiNote (clipId, draggingNoteState,
                                     dragPreviewStart, dragPreviewLength, dragPreviewNote);
        }
    }
    else if (dragMode == DragMode::resize && draggingNoteState.isValid())
    {
        if (dragAllResizing)
        {
            for (const auto& note : dragAllBaseNotes)
            {
                if (! isNoteSelected (note.state))
                    continue;
                double newLength = note.lengthSeconds + dragAllResizeDelta;
                newLength = juce::jmax (0.01, newLength);
                session->resizeMidiNote (clipId, note.state, newLength);
            }
        }
        else
        {
            session->resizeMidiNote (clipId, draggingNoteState, dragPreviewLength);
        }
    }

    dragMode = DragMode::none;
    draggingNoteState = {};
    dragAllNotes = false;
    dragAllResizing = false;
    repaint();
}

void PianoRollComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details)
{
    const float delta = (details.deltaX != 0.0f) ? details.deltaX : details.deltaY;

    if (event.mods.isCommandDown())
    {
        const double zoomFactor = std::pow (1.1, delta * 10.0);
        const double anchorTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
        const double newPps = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, pixelsPerSecond * zoomFactor);
        pixelsPerSecond = newPps;
        viewStartSeconds = juce::jmax (0.0, anchorTime - (event.position.x - keyboardWidth) / pixelsPerSecond);
    }
    else
    {
        const double pixelDelta = -delta * 300.0;
        viewStartSeconds = juce::jmax (0.0, viewStartSeconds + (pixelDelta / pixelsPerSecond));
    }

    if (onViewChanged)
        onViewChanged (viewStartSeconds, pixelsPerSecond);

    repaint();
}

void PianoRollComponent::mouseMagnify (const juce::MouseEvent& event, float scaleFactor)
{
    if (scaleFactor <= 0.0f)
        return;

    const double anchorTime = viewStartSeconds + (event.position.x - keyboardWidth) / pixelsPerSecond;
    pixelsPerSecond = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, pixelsPerSecond * (double) scaleFactor);
    viewStartSeconds = juce::jmax (0.0, anchorTime - (event.position.x - keyboardWidth) / pixelsPerSecond);

    if (onViewChanged)
        onViewChanged (viewStartSeconds, pixelsPerSecond);
    repaint();
}

bool PianoRollComponent::keyPressed (const juce::KeyPress& key)
{
    if (session == nullptr || clipId == 0)
        return false;

    if (key.getModifiers().isCommandDown())
    {
        const int code = key.getKeyCode();
        if (code == 'a' || code == 'A')
        {
            selectedNotes.clear();
            for (const auto& note : session->getMidiNotesForClip (clipId))
                selectedNotes.addIfNotAlreadyThere (note.state);
            selectedNoteState = {};
            repaint();
            return true;
        }

        if (code == 'c' || code == 'C')
        {
            clipboardNotes.clear();
            if (selectedNotes.isEmpty() && selectedNoteState.isValid())
                selectedNotes.add (selectedNoteState);

            if (selectedNotes.isEmpty())
                return true;

            double minStart = std::numeric_limits<double>::max();
            for (const auto& note : session->getMidiNotesForClip (clipId))
            {
                if (! isNoteSelected (note.state))
                    continue;
                clipboardNotes.push_back (note);
                minStart = juce::jmin (minStart, note.startSeconds);
            }
            clipboardStartSeconds = std::isfinite (minStart) ? minStart : 0.0;
            return true;
        }

        if (code == 'v' || code == 'V')
        {
            if (clipboardNotes.empty())
                return true;

            const double pasteStart = session->getCursorTimeSeconds();
            selectedNotes.clear();
            for (const auto& note : clipboardNotes)
            {
                const double offset = note.startSeconds - clipboardStartSeconds;
                const double start = juce::jmax (0.0, pasteStart + offset);
                if (session->addMidiNote (clipId, note.noteNumber, start, note.lengthSeconds))
                {
                    auto notesNow = session->getMidiNotesForClip (clipId);
                    if (! notesNow.empty())
                        selectedNotes.addIfNotAlreadyThere (notesNow.back().state);
                }
            }
            repaint();
            return true;
        }
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (! selectedNotes.isEmpty())
        {
            auto toDelete = selectedNotes;
            for (const auto& noteState : toDelete)
                session->deleteMidiNote (clipId, noteState);
            selectedNoteState = {};
            selectedNotes.clear();
            repaint();
            return true;
        }

        if (selectedNoteState.isValid())
        {
            session->deleteMidiNote (clipId, selectedNoteState);
            selectedNoteState = {};
            selectedNotes.clear();
            repaint();
            return true;
        }
    }

    return false;
}

std::vector<PianoRollComponent::NoteRect> PianoRollComponent::buildNoteRects() const
{
    std::vector<NoteRect> rects;
    if (session == nullptr || clipId == 0)
        return rects;

    auto notes = session->getMidiNotesForClip (clipId);
    auto noteArea = getLocalBounds().withTrimmedLeft (keyboardWidth);
    if (showRuler)
        noteArea.removeFromTop (rulerHeight);
    noteArea.removeFromBottom (sustainLaneHeight);
    const float noteHeight = getNoteHeight (noteArea);

    for (const auto& note : notes)
    {
        const bool isDraggingThis = draggingNoteState.isValid() && note.state == draggingNoteState;
        double startSeconds = isDraggingThis ? dragPreviewStart : note.startSeconds;
        double lengthSeconds = isDraggingThis ? dragPreviewLength : note.lengthSeconds;
        int noteNumber = isDraggingThis ? dragPreviewNote : note.noteNumber;

        if (dragAllNotes && isNoteSelected (note.state))
        {
            startSeconds = note.startSeconds + dragAllDeltaSeconds;
            noteNumber = note.noteNumber + dragAllDeltaSemitones;
        }

        const int x = (int) ((startSeconds - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        const int w = (int) juce::jmax (1.0, lengthSeconds * pixelsPerSecond);

        noteNumber = juce::jlimit (lowestNote, highestNote, noteNumber);
        const int noteIndex = noteNumber - lowestNote;
        if (noteIndex < 0 || noteIndex > (highestNote - lowestNote))
            continue;

        const int y = noteArea.getBottom() - (int) ((noteIndex + 1) * noteHeight);
        const int h = (int) noteHeight - 1;

        NoteRect rect;
        rect.info = note;
        rect.bounds = { x, y, w, h };
        rects.push_back (rect);
    }

    return rects;
}

void PianoRollComponent::drawKeyboard (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xFF141518));
    g.fillRect (area);

    const float noteHeight = getNoteHeight (area);
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        const int noteIndex = note - lowestNote;
        const int y = area.getBottom() - (int) ((noteIndex + 1) * noteHeight);
        const int h = (int) noteHeight;

        const bool black = isBlackKey (note);
        g.setColour (black ? juce::Colour (0xFF202328) : juce::Colour (0xFF2B2F36));
        g.fillRect (area.getX(), y, area.getWidth(), h);

        if (! black && (note % 12) == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("C" + juce::String (note / 12 - 1),
                        area.getX() + 6, y, area.getWidth() - 8, h,
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (juce::Colour (0xFF3A3F46));
    g.drawLine ((float) area.getRight(), (float) area.getY(),
                (float) area.getRight(), (float) area.getBottom());
}

void PianoRollComponent::drawGrid (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xFF1E2025));
    g.fillRect (area);

    const float noteHeight = getNoteHeight (area);
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        const int noteIndex = note - lowestNote;
        const int y = area.getBottom() - (int) ((noteIndex + 1) * noteHeight);
        const bool black = isBlackKey (note);
        g.setColour (black ? juce::Colour (0xFF1A1C20) : juce::Colour (0xFF21242A));
        g.drawLine ((float) area.getX(), (float) y, (float) area.getRight(), (float) y);
    }

    const double grid = getGridSeconds();
    if (grid <= 0.0)
        return;

    const double visibleSeconds = area.getWidth() / pixelsPerSecond;
    const double start = std::floor (viewStartSeconds / grid) * grid;

    for (double t = start; t <= viewStartSeconds + visibleSeconds + 0.0001; t += grid)
    {
        const int x = (int) ((t - viewStartSeconds) * pixelsPerSecond) + area.getX();
        const bool isBar = std::fmod (t, grid * 4.0) < 0.0001;
        g.setColour (juce::Colours::white.withAlpha (isBar ? 0.25f : 0.12f));
        g.drawLine ((float) x, (float) area.getY(), (float) x, (float) area.getBottom());
    }
}

void PianoRollComponent::drawNotes (juce::Graphics& g, juce::Rectangle<int>, const std::vector<NoteRect>& notes) const
{
    for (const auto& note : notes)
    {
        auto bounds = note.bounds;
        const bool selected = isNoteSelected (note.info.state);

        const float velNorm = (float) juce::jlimit (1, 127, note.info.velocity) / 127.0f;
        auto base = juce::Colour::fromHSV (juce::jmap (velNorm, 0.62f, 0.02f),
                                           0.75f, juce::jmap (velNorm, 0.55f, 0.96f), 1.0f);
        if (selected)
            base = base.interpolatedWith (juce::Colour (0xFFFFD469), 0.45f);

        g.setColour (base);
        g.fillRect (bounds);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRect (bounds, selected ? 2 : 1);
    }
}

void PianoRollComponent::drawChordLabels (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (session == nullptr || clipId == 0)
        return;

    const auto labels = session->getChordLabelsForClip (clipId);
    if (labels.empty())
        return;

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.85f));

    const int y = area.getY() + 6;
    for (const auto& label : labels)
    {
        const int x = (int) ((label.startSeconds - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        if (x < area.getX() - 40 || x > area.getRight() + 40)
            continue;

        juce::Rectangle<int> textArea (x + 2, y, 80, 16);
        g.drawText (label.symbol, textArea, juce::Justification::centredLeft, true);
    }
}

void PianoRollComponent::drawRuler (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xFF202227));
    g.fillRect (area);

    if (session == nullptr)
        return;

    const double bpm = session->getTempoBpm();
    if (bpm <= 0.0)
        return;

    const double secondsPerBeat = 60.0 / bpm;
    int beatsPerBar = 4;
    const auto ts = session->getTimeSignature();
    if (ts.containsChar ('/'))
        beatsPerBar = ts.upToFirstOccurrenceOf ("/", false, false).getIntValue();

    const double visibleSeconds = area.getWidth() / pixelsPerSecond;
    const double startBeat = std::floor (viewStartSeconds / secondsPerBeat);
    const double endBeat = std::ceil ((viewStartSeconds + visibleSeconds) / secondsPerBeat);

    const double barWidthPx = secondsPerBeat * beatsPerBar * pixelsPerSecond;
    const bool showLabels = barWidthPx >= 50.0;

    g.setFont (juce::FontOptions (11.0f));
    for (double beat = startBeat; beat <= endBeat; beat += 1.0)
    {
        const double timeSeconds = beat * secondsPerBeat;
        const int x = (int) ((timeSeconds - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        const bool isBar = ((int) beat % juce::jmax (1, beatsPerBar)) == 0;

        g.setColour (juce::Colours::white.withAlpha (isBar ? 0.45f : 0.18f));
        g.drawLine ((float) x, (float) area.getBottom() - 6.0f,
                    (float) x, (float) area.getBottom());

        if (isBar && showLabels)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            const auto label = session->getBarsAndBeatsText (timeSeconds);
            g.drawText (label, x + 4, area.getY(), 80, area.getHeight(), juce::Justification::centredLeft);
        }
    }

    g.setColour (juce::Colour (0xFF2F333A));
    g.drawLine ((float) area.getX(), (float) area.getBottom() - 1.0f,
                (float) area.getRight(), (float) area.getBottom() - 1.0f);
}

void PianoRollComponent::drawSustainLane (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xFF191B20));
    g.fillRect (area);

    if (session == nullptr || clipId == 0)
        return;

    const auto events = session->getSustainEventsForClip (clipId);
    if (events.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("Sustain (CC64)", area, juce::Justification::centredLeft);
        return;
    }

    const int baseY = area.getBottom();
    const double visibleStart = viewStartSeconds;
    const double visibleEnd = viewStartSeconds + (area.getWidth() / pixelsPerSecond);

    int lastValue = 0;
    double lastTime = visibleStart;

    for (const auto& ev : events)
    {
        if (ev.timeSeconds < visibleStart)
        {
            lastValue = ev.value;
            lastTime = visibleStart;
            continue;
        }

        if (ev.timeSeconds > visibleEnd)
            break;

        if (ev.timeSeconds > lastTime)
        {
            const int x1 = (int) ((lastTime - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
            const int x2 = (int) ((ev.timeSeconds - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
            const float height = (float) juce::jlimit (0, area.getHeight(), (int) std::round ((lastValue / 127.0) * area.getHeight()));
            const int y = baseY - (int) height;

            g.setColour (lastValue >= 64 ? juce::Colour (0xFF2EC27E) : juce::Colour (0xFF3A3F46));
            g.fillRect (juce::Rectangle<int> (x1, y, juce::jmax (1, x2 - x1), (int) height));
        }

        lastValue = ev.value;
        lastTime = ev.timeSeconds;
    }

    if (lastTime < visibleEnd)
    {
        const int x1 = (int) ((lastTime - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        const int x2 = (int) ((visibleEnd - viewStartSeconds) * pixelsPerSecond) + keyboardWidth;
        const float height = (float) juce::jlimit (0, area.getHeight(), (int) std::round ((lastValue / 127.0) * area.getHeight()));
        const int y = baseY - (int) height;
        g.setColour (lastValue >= 64 ? juce::Colour (0xFF2EC27E) : juce::Colour (0xFF3A3F46));
        g.fillRect (juce::Rectangle<int> (x1, y, juce::jmax (1, x2 - x1), (int) height));
    }

    g.setColour (juce::Colour (0xFF30343B));
    g.drawLine ((float) area.getX(), (float) area.getY(),
                (float) area.getRight(), (float) area.getY());
}

double PianoRollComponent::getGridSeconds() const
{
    if (session == nullptr)
        return 0.25;

    const double bpm = session->getTempoBpm();
    const double secondsPerBeat = (bpm > 0.0) ? (60.0 / bpm) : 0.5;
    return secondsPerBeat / 4.0;
}

double PianoRollComponent::quantizeSeconds (double seconds) const
{
    const double grid = getGridSeconds();
    if (grid <= 0.0)
        return seconds;

    return std::round (seconds / grid) * grid;
}

int PianoRollComponent::getNoteFromY (int y, juce::Rectangle<int> area) const
{
    const float noteHeight = getNoteHeight (area);
    const int noteIndex = (int) ((area.getBottom() - y) / noteHeight);
    const int noteNumber = lowestNote + noteIndex;
    return juce::jlimit (lowestNote, highestNote, noteNumber);
}

float PianoRollComponent::getNoteHeight (juce::Rectangle<int> area) const
{
    const int totalNotes = highestNote - lowestNote + 1;
    return (float) area.getHeight() / (float) totalNotes;
}

bool PianoRollComponent::isBlackKey (int noteNumber) const
{
    const int n = noteNumber % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

bool PianoRollComponent::isNoteSelected (const juce::ValueTree& state) const
{
    if (! state.isValid())
        return false;
    if (selectedNotes.isEmpty())
        return selectedNoteState.isValid() && selectedNoteState == state;

    for (const auto& sel : selectedNotes)
        if (sel == state)
            return true;

    return false;
}

juce::Rectangle<int> PianoRollComponent::getNoteGridArea() const
{
    auto area = getLocalBounds();
    area.removeFromLeft (keyboardWidth);
    if (showRuler)
        area.removeFromTop (rulerHeight);
    area.removeFromBottom (sustainLaneHeight);
    return area;
}
