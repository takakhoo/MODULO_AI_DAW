#pragma once

#include <JuceHeader.h>
#include <vector>

class ChordInspectorComponent : public juce::Component,
                                private juce::ListBoxModel
{
public:
    struct PreviewNote
    {
        int pitch = 60;
        double startBeats = 0.0;
        double lengthBeats = 0.25;
        int velocity = 100;
    };

    enum class ChordCellAction
    {
        block,
        arpeggio,
        deleteChord,
        doubleTime,
        semitoneUp,
        semitoneDown,
        octaveUp,
        octaveDown,
        inversionUp,
        inversionDown
    };

    ChordInspectorComponent();

    void setChords (const juce::String& trackName,
                    const juce::StringArray& chordSymbols,
                    const juce::Array<double>& chordStartTimesSeconds,
                    const juce::Array<int>& chordMeasures,
                    const juce::Array<int>& chordBeats,
                    int beatsPerBar,
                    const juce::String& staffText);
    void setPreviewNotes (const std::vector<PreviewNote>& notes);
    void setCurrentTimeSeconds (double timeSeconds);
    std::function<void (int measure, int beat, ChordCellAction action)> onChordCellAction;
    std::function<void (int measure, int beat, const juce::String& chordSymbol)> onEmptyChordCellChosen;
    std::function<void (int measure, int beat)> onChordCellSelected;
    std::function<void (int measure, int beat, int noteIndexInCell, int newPitch)> onChordPreviewNotePitchChange;
    std::function<void (int measure, int beat, int pitch, double startBeats, double lengthBeats)> onChordPreviewNoteAdd;
    std::function<void (int measure, int beat, int noteIndexInCell)> onChordPreviewNoteDelete;
    std::function<void (int measure, int beat, int noteIndexInCell, double newLengthBeats)> onChordPreviewNoteResize;
    std::function<void (int measure, int beat, int noteIndexInCell, double newStartBeats)> onChordPreviewNoteStartChange;
    std::function<void (int measure, int beat, double gridBeats)> onChordPreviewNotesQuantize;
    std::function<void (int measure, int beat, int noteIndexInCell, int newVelocity)> onChordPreviewNoteVelocityChange;
    std::function<void (int pitch)> onPreviewNotePlay;
    bool deleteSelectedChordIfAny();

    void setKeySignature (const juce::String& keySignature);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void updatePickerButtonStyles();
    void applyChordFromPicker (const juce::String& qualitySuffix);
    void applyChordActionFromButton (ChordCellAction action);
    void drawPreview (juce::Graphics& g) const;
    juce::String getChordSymbolAtCell (int measure, int beat) const;
    juce::String getNoteNameWithOctave (int midiNote) const;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& event) override;

    juce::Label titleLabel;
    juce::Label barHeaderLabel;
    juce::ListBox chordList;
    juce::Label staffLabel;
    juce::TextButton semitoneUpButton { "♯" };
    juce::TextButton semitoneDownButton { "♭" };
    juce::StringArray symbols;
    juce::Array<double> chordStarts;
    juce::Array<int> measures;
    juce::Array<int> beats;
    int beatsInBar = 4;
    int rowCount = 1;
    double currentTimeSeconds = 0.0;
    int activeMeasure = -1;
    int activeBeat = -1;
    int selectedMeasure = -1;
    int selectedBeat = -1;
public:
    int getSelectedMeasure() const noexcept { return selectedMeasure; }
    int getSelectedBeat() const noexcept { return selectedBeat; }
private:
    juce::String selectedRoot = "C";
    juce::String selectedQualitySuffix = "";
    juce::String previewChordSymbol = "-";
    juce::String previewCellText = "-";
    std::vector<PreviewNote> previewNotes;
    juce::Rectangle<int> previewBounds;
    juce::Rectangle<int> workshopBannerBounds;

    juce::String keySignatureForRoots = "Cmaj";
    juce::OwnedArray<juce::TextButton> rootButtons;
    juce::StringArray rootButtonValues;
    juce::OwnedArray<juce::TextButton> qualityButtons;
    juce::OwnedArray<juce::TextButton> actionButtons;
    juce::StringArray qualitySuffixes;

    struct PreviewNoteRect { int noteIndex = 0; juce::Rectangle<float> bounds; };
    juce::Rectangle<int> getPreviewNoteGridInner() const;
    std::vector<PreviewNoteRect> getPreviewNoteRects() const;
    int getPitchFromPreviewY (float yInInner) const;
    double getTimeFromPreviewX (float xInInner) const;
    double getGridBeats() const;
    double snapLengthBeatsToGridIfClose (double lengthBeats) const;
    double snapStartBeatsToGridIfClose (double startBeats) const;
    int draggingPreviewNoteIndex = -1;
    int lastPreviewDragPitch = -1;
    enum class PreviewDragMode { none, move, resize, moveStart };
    PreviewDragMode previewDragMode = PreviewDragMode::none;
    double dragStartPreviewTime = 0.0;
    double dragStartPreviewLength = 0.0;
    double dragPreviewLength = 0.0;
    double dragPreviewStart = 0.0;
    juce::Array<int> selectedPreviewNotes;

    juce::Slider velocitySlider;
    double velocitySliderLastValue = 64.0;
    void updateVelocitySliderFromSelection();
    void applyVelocitySliderChange();
};
