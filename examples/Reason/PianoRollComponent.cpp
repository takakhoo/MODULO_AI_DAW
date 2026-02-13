#include "PianoRollComponent.h"
#include "ResizeCursors.h"

#include <cmath>
#include <limits>

PianoRollComponent::PianoRollComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    startTimerHz (30);

    velocitySlider.setSliderStyle (juce::Slider::LinearVertical);
    velocitySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    velocitySlider.setRange (1.0, 127.0, 1.0);
    velocitySlider.setValue (64.0);
    velocitySlider.setVisible (false);
    velocitySlider.onValueChange = [this] { applyVelocitySliderChange(); };
    addAndMakeVisible (velocitySlider);
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
    updateVelocitySliderFromSelection();
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
    if (velocitySlider.isVisible())
        area.removeFromLeft (velocityStripWidth);
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
        const int x = (int) ((playheadSeconds - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
        g.setColour (juce::Colour (0xFFEA5656));
        g.drawLine ((float) x, (float) noteArea.getY(), (float) x, (float) sustainArea.getBottom(), 1.2f);
    }
}

int PianoRollComponent::getNoteGridLeft() const
{
    return keyboardWidth + (velocitySlider.isVisible() ? velocityStripWidth : 0);
}

void PianoRollComponent::resized()
{
    if (velocitySlider.isVisible())
        velocitySlider.setBounds (keyboardWidth, 0, velocityStripWidth, getHeight());
}

void PianoRollComponent::updateVelocitySliderFromSelection()
{
    if (session == nullptr || clipId == 0)
    {
        velocitySlider.setVisible (false);
        return;
    }
    juce::Array<juce::ValueTree> toUse;
    if (! selectedNotes.isEmpty())
        toUse = selectedNotes;
    else if (selectedNoteState.isValid())
        toUse.add (selectedNoteState);
    if (toUse.isEmpty())
    {
        velocitySlider.setVisible (false);
        return;
    }
    const auto notes = session->getMidiNotesForClip (clipId);
    int sum = 0;
    int count = 0;
    for (const auto& info : notes)
    {
        for (const auto& vt : toUse)
            if (vt == info.state) { sum += info.velocity; ++count; break; }
    }
    const int avg = (count > 0) ? (sum / count) : 64;
    velocitySliderLastValue = (double) juce::jlimit (1, 127, avg);
    velocitySlider.setValue (velocitySliderLastValue, juce::dontSendNotification);
    velocitySlider.setVisible (true);
}

void PianoRollComponent::applyVelocitySliderChange()
{
    if (session == nullptr || clipId == 0)
        return;
    juce::Array<juce::ValueTree> toUse;
    if (! selectedNotes.isEmpty())
        toUse = selectedNotes;
    else if (selectedNoteState.isValid())
        toUse.add (selectedNoteState);
    if (toUse.isEmpty())
        return;
    const double newVal = velocitySlider.getValue();
    const int delta = (int) std::round (newVal - velocitySliderLastValue);
    velocitySliderLastValue = newVal;
    const auto notes = session->getMidiNotesForClip (clipId);
    for (const auto& info : notes)
    {
        for (const auto& vt : toUse)
        {
            if (vt != info.state)
                continue;
            const int newVel = juce::jlimit (1, 127, info.velocity + delta);
            session->setMidiNoteVelocity (clipId, info.state, newVel);
            break;
        }
    }
    repaint();
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
    local.removeFromLeft (getNoteGridLeft());
    const bool inRuler = showRuler && event.y < rulerHeight;
    if (showRuler)
        local.removeFromTop (rulerHeight);
    auto sustainArea = local.removeFromBottom (sustainLaneHeight);
    auto noteArea = local;

    if (event.x >= getNoteGridLeft())
    {
        const double clickTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
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
    dragAllResizeLeft = false;
    dragAllResizeDelta = 0.0;
    dragAllResizeLeftDeltaStart = 0.0;
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

        if (auto clipInfo = session->getClipInfo (clipId))
            session->playPreviewNote (clipInfo->trackIndex, note.info.noteNumber, 65);

        const int resizeHandle = 6;
        const bool onLeftEdge  = (event.x >= note.bounds.getX() && event.x < note.bounds.getX() + resizeHandle);
        const bool onRightEdge = (event.x > note.bounds.getRight() - resizeHandle && event.x <= note.bounds.getRight());

        if (onRightEdge)
        {
            dragMode = DragMode::resize;
        }
        else if (onLeftEdge)
        {
            dragMode = DragMode::resizeLeft;
        }
        else
        {
            dragMode = DragMode::move;
            const double clickTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
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
        else if (dragMode == DragMode::resizeLeft && selectedNotes.size() > 1)
        {
            dragAllResizeLeft = true;
            dragAllBaseNotes = session->getMidiNotesForClip (clipId);
        }

        updateVelocitySliderFromSelection();
        repaint();
        return;
    }

    if (event.x < getNoteGridLeft())
        return;

    if (! event.mods.isCtrlDown() && ! event.mods.isCommandDown())
    {
        isLassoSelecting = true;
        lassoAdditive = event.mods.isCommandDown() || event.mods.isShiftDown();
        lassoStartPoint = event.getPosition();
        lassoSelection = juce::Rectangle<int> (lassoStartPoint, lassoStartPoint).getIntersection (noteArea);
        if (! lassoAdditive)
            selectedNotes.clear();
        selectedNoteState = {};
        updateVelocitySliderFromSelection();
        repaint();
        return;
    }

    const double clickTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
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
    local.removeFromLeft (getNoteGridLeft());
    if (showRuler)
        local.removeFromTop (rulerHeight);
    local.removeFromBottom (sustainLaneHeight);
    const auto noteArea = local;

    if (dragMode == DragMode::move)
    {
        const double dragTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
        const double startTime = juce::jmax (0.0, dragTime - dragOffsetSeconds);
        const int noteNumber = getNoteFromY (event.y, noteArea);

        dragPreviewStart = startTime;
        dragPreviewNote = noteNumber;
        dragPreviewLength = dragStartLength;

        if (noteNumber != lastPlayedPitchDuringDrag)
        {
            lastPlayedPitchDuringDrag = noteNumber;
            if (auto clipInfo = session->getClipInfo (clipId))
                session->playPreviewNote (clipInfo->trackIndex, noteNumber, 65);
        }

        if (dragAllNotes)
        {
            dragAllDeltaSeconds = startTime - dragStartSeconds;
            dragAllDeltaSemitones = noteNumber - dragStartNote;
        }
    }
    else if (dragMode == DragMode::resize)
    {
        const double dragTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
        const double newLength = dragTime - dragStartSeconds;
        dragPreviewLength = juce::jmax (0.01, newLength);
        dragPreviewStart = dragStartSeconds;
        dragPreviewNote = dragStartNote;

        if (dragAllResizing)
            dragAllResizeDelta = dragPreviewLength - dragStartLength;
    }
    else if (dragMode == DragMode::resizeLeft)
    {
        const double dragTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
        const double noteEnd = dragStartSeconds + dragStartLength;
        const double newStart = juce::jmax (0.0, juce::jmin (noteEnd - 0.01, dragTime));
        dragPreviewStart = newStart;
        dragPreviewLength = noteEnd - newStart;
        dragPreviewNote = dragStartNote;

        if (dragAllResizeLeft)
            dragAllResizeLeftDeltaStart = dragPreviewStart - dragStartSeconds;
    }

    repaint();
}

void PianoRollComponent::mouseMove (const juce::MouseEvent& event)
{
    if (session == nullptr || clipId == 0)
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    auto noteArea = getNoteGridArea();
    if (event.x < noteArea.getX() || event.y < noteArea.getY()
        || event.x >= noteArea.getRight() || event.y >= noteArea.getBottom())
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    const int resizeHandleWidth = 6;
    const auto noteRects = buildNoteRects();

    for (const auto& note : noteRects)
    {
        if (! note.bounds.contains (event.getPosition()))
            continue;

        const int mx = event.getPosition().x;
        const bool onLeftEdge  = (mx >= note.bounds.getX() && mx < note.bounds.getX() + resizeHandleWidth);
        const bool onRightEdge = (mx > note.bounds.getRight() - resizeHandleWidth && mx <= note.bounds.getRight());

        if (onLeftEdge)
        {
            setMouseCursor (ReasonResizeCursors::getLeftBracketResizeCursor());
            return;
        }
        if (onRightEdge)
        {
            setMouseCursor (ReasonResizeCursors::getRightBracketResizeCursor());
            return;
        }

        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
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
        updateVelocitySliderFromSelection();
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
                double newLength = juce::jmax (0.01, note.lengthSeconds + dragAllResizeDelta);
                newLength = snapLengthToGridIfClose (newLength);
                session->resizeMidiNote (clipId, note.state, newLength);
            }
        }
        else
        {
            const double snappedLength = snapLengthToGridIfClose (dragPreviewLength);
            session->resizeMidiNote (clipId, draggingNoteState, snappedLength);
        }
    }
    else if (dragMode == DragMode::resizeLeft && draggingNoteState.isValid())
    {
        if (dragAllResizeLeft)
        {
            for (const auto& note : dragAllBaseNotes)
            {
                if (! isNoteSelected (note.state))
                    continue;
                double newStart = juce::jmax (0.0, note.startSeconds + dragAllResizeLeftDeltaStart);
                double newLength = juce::jmax (0.01, (note.startSeconds + note.lengthSeconds) - newStart);
                newLength = snapLengthToGridIfClose (newLength);
                newStart = (note.startSeconds + note.lengthSeconds) - newLength;
                session->updateMidiNote (clipId, note.state, newStart, newLength, note.noteNumber);
            }
        }
        else
        {
            double snappedLength = snapLengthToGridIfClose (dragPreviewLength);
            double newStart = (dragStartSeconds + dragStartLength) - snappedLength;
            session->updateMidiNote (clipId, draggingNoteState, newStart, snappedLength, dragPreviewNote);
        }
    }

    dragMode = DragMode::none;
    draggingNoteState = {};
    dragAllNotes = false;
    dragAllResizing = false;
    lastPlayedPitchDuringDrag = -1;
    repaint();
}

void PianoRollComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details)
{
    const float delta = (details.deltaX != 0.0f) ? details.deltaX : details.deltaY;

    if (event.mods.isCommandDown())
    {
        const double zoomFactor = std::pow (1.1, delta * 10.0);
        const double anchorTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
        const double newPps = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, pixelsPerSecond * zoomFactor);
        pixelsPerSecond = newPps;
        viewStartSeconds = juce::jmax (0.0, anchorTime - (event.position.x - getNoteGridLeft()) / pixelsPerSecond);
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

    const double anchorTime = viewStartSeconds + (event.position.x - getNoteGridLeft()) / pixelsPerSecond;
    pixelsPerSecond = juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, pixelsPerSecond * (double) scaleFactor);
    viewStartSeconds = juce::jmax (0.0, anchorTime - (event.position.x - getNoteGridLeft()) / pixelsPerSecond);

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

    const int code = key.getKeyCode();
    if (! key.getModifiers().isAnyModifierKeyDown() && (code == 'q' || code == 'Q'))
    {
        juce::Array<juce::ValueTree> targets;
        if (! selectedNotes.isEmpty())
            targets = selectedNotes;
        else if (selectedNoteState.isValid())
            targets.add (selectedNoteState);

        if (targets.isEmpty())
            return true;

        if (session->quantizeMidiNotes (clipId, targets, getGridSeconds()))
            repaint();
        return true;
    }

    return false;
}

std::vector<PianoRollComponent::NoteRect> PianoRollComponent::buildNoteRects() const
{
    std::vector<NoteRect> rects;
    if (session == nullptr || clipId == 0)
        return rects;

    auto notes = session->getMidiNotesForClip (clipId);
    auto noteArea = getLocalBounds().withTrimmedLeft (getNoteGridLeft());
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
        if (dragAllResizeLeft && isNoteSelected (note.state))
        {
            startSeconds = note.startSeconds + dragAllResizeLeftDeltaStart;
            lengthSeconds = juce::jmax (0.01, note.lengthSeconds - dragAllResizeLeftDeltaStart);
        }

        const int x = (int) ((startSeconds - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
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
        auto base = juce::Colour (0xFF1A0F05).interpolatedWith (juce::Colour (0xFFFFE8A8), velNorm);
        if (selected)
            base = base.interpolatedWith (juce::Colour (0xFFFFE6AE), 0.4f);

        g.setColour (base);
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xFF2A1C09).withAlpha (0.9f));
        g.drawRect (bounds, selected ? 2 : 1);

        if (showNoteLabels && session != nullptr && bounds.getHeight() >= 8)
        {
            g.setColour (juce::Colour (0xFF1C1408));
            const float fontSize = juce::jlimit (6.0f, 10.0f, (float) bounds.getHeight() - 2.0f);
            g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
            g.drawText (session->getNoteNameForPitch (note.info.noteNumber),
                        bounds.reduced (2, 0),
                        juce::Justification::centredLeft,
                        true);
        }
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
        const int x = (int) ((label.startSeconds - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
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
        const int x = (int) ((timeSeconds - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
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
            const int x1 = (int) ((lastTime - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
            const int x2 = (int) ((ev.timeSeconds - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
            const float height = (float) juce::jlimit (0, area.getHeight(), (int) std::round ((lastValue / 127.0) * area.getHeight()));
            const int y = baseY - (int) height;

            g.setColour (lastValue >= 64 ? juce::Colour (0xFF7A5A22) : juce::Colour (0xFF3A2F1A));
            g.fillRect (juce::Rectangle<int> (x1, y, juce::jmax (1, x2 - x1), (int) height));
        }

        lastValue = ev.value;
        lastTime = ev.timeSeconds;
    }

    if (lastTime < visibleEnd)
    {
        const int x1 = (int) ((lastTime - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
        const int x2 = (int) ((visibleEnd - viewStartSeconds) * pixelsPerSecond) + getNoteGridLeft();
        const float height = (float) juce::jlimit (0, area.getHeight(), (int) std::round ((lastValue / 127.0) * area.getHeight()));
        const int y = baseY - (int) height;
        g.setColour (lastValue >= 64 ? juce::Colour (0xFF7A5A22) : juce::Colour (0xFF3A2F1A));
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

double PianoRollComponent::snapLengthToGridIfClose (double lengthSeconds) const
{
    const double grid = getGridSeconds();
    if (grid <= 0.0 || lengthSeconds <= 0.0)
        return lengthSeconds;

    const double rounded = std::round (lengthSeconds / grid) * grid;
    const double snappedLength = juce::jmax (0.01, rounded);
    const double threshold = juce::jmax (0.02, grid * 0.15);
    if (std::fabs (lengthSeconds - snappedLength) <= threshold)
        return snappedLength;
    return lengthSeconds;
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
    area.removeFromLeft (getNoteGridLeft());
    if (showRuler)
        area.removeFromTop (rulerHeight);
    area.removeFromBottom (sustainLaneHeight);
    return area;
}
