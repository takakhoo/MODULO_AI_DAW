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
    bool deleteSelectedChordIfAny();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updatePickerButtonStyles();
    void applyChordFromPicker (const juce::String& qualitySuffix);
    void applyChordActionFromButton (ChordCellAction action);
    void drawPreview (juce::Graphics& g) const;
    juce::String getChordSymbolAtCell (int measure, int beat) const;
    static juce::String getNoteNameWithOctave (int midiNote);

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
    juce::String selectedRoot = "C";
    juce::String selectedQualitySuffix = "";
    juce::String previewChordSymbol = "-";
    juce::String previewCellText = "-";
    std::vector<PreviewNote> previewNotes;
    juce::Rectangle<int> previewBounds;
    juce::Rectangle<int> workshopBannerBounds;

    juce::OwnedArray<juce::TextButton> rootButtons;
    juce::StringArray rootButtonValues;
    juce::OwnedArray<juce::TextButton> qualityButtons;
    juce::OwnedArray<juce::TextButton> actionButtons;
    juce::StringArray qualitySuffixes;
};
