#pragma once

#include <JuceHeader.h>

#include "SessionController.h"

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent();

    void setSession (SessionController* newSession);
    void setClipId (uint64_t newClipId);
    uint64_t getClipId() const noexcept { return clipId; }

    void setView (double newViewStartSeconds, double newPixelsPerSecond);
    void setRulerVisible (bool shouldShow) { showRuler = shouldShow; }

    std::function<void (double viewStartSeconds, double pixelsPerSecond)> onViewChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    struct NoteRect
    {
        SessionController::MidiNoteInfo info;
        juce::Rectangle<int> bounds;
    };

    enum class DragMode
    {
        none,
        move,
        resize
    };

    std::vector<NoteRect> buildNoteRects() const;
    void drawKeyboard (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawGrid (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawNotes (juce::Graphics& g, juce::Rectangle<int> area, const std::vector<NoteRect>& notes) const;
    void drawChordLabels (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawRuler (juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawSustainLane (juce::Graphics& g, juce::Rectangle<int> area) const;

    double getGridSeconds() const;
    double quantizeSeconds (double seconds) const;
    int getNoteFromY (int y, juce::Rectangle<int> area) const;
    float getNoteHeight (juce::Rectangle<int> area) const;
    bool isBlackKey (int noteNumber) const;
    bool isNoteSelected (const juce::ValueTree& state) const;

    SessionController* session = nullptr;
    uint64_t clipId = 0;

    double viewStartSeconds = 0.0;
    double pixelsPerSecond = 100.0;
    const double minPixelsPerSecond = 20.0;
    const double maxPixelsPerSecond = 800.0;

    int keyboardWidth = 64;
    int rulerHeight = 24;
    int sustainLaneHeight = 44;
    bool showRuler = true;
    int lowestNote = 36;
    int highestNote = 84;

    juce::ValueTree selectedNoteState;
    juce::ValueTree draggingNoteState;
    juce::Array<juce::ValueTree> selectedNotes;
    DragMode dragMode = DragMode::none;

    double dragOffsetSeconds = 0.0;
    double dragStartSeconds = 0.0;
    double dragStartLength = 0.0;
    int dragStartNote = 0;

    double dragPreviewStart = 0.0;
    double dragPreviewLength = 0.0;
    int dragPreviewNote = 0;

    bool dragAllNotes = false;
    double dragAllDeltaSeconds = 0.0;
    int dragAllDeltaSemitones = 0;
    bool dragAllResizing = false;
    double dragAllResizeDelta = 0.0;
    std::vector<SessionController::MidiNoteInfo> dragAllBaseNotes;

    std::vector<SessionController::MidiNoteInfo> clipboardNotes;
    double clipboardStartSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollComponent)
};
