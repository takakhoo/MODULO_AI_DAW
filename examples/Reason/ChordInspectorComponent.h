#pragma once

#include <JuceHeader.h>

class ChordInspectorComponent : public juce::Component,
                                private juce::ListBoxModel
{
public:
    ChordInspectorComponent();

    void setChords (const juce::String& trackName,
                    const juce::StringArray& chordLines,
                    const juce::Array<double>& chordStartTimesSeconds,
                    const juce::String& staffText);
    void setCurrentTimeSeconds (double timeSeconds);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    juce::Label titleLabel;
    juce::ListBox chordList;
    juce::Label staffLabel;
    juce::StringArray lines;
    juce::Array<double> chordStarts;
    double currentTimeSeconds = 0.0;
    int activeRow = -1;
};
