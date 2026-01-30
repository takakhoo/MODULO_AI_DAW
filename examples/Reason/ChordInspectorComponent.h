#pragma once

#include <JuceHeader.h>

class ChordInspectorComponent : public juce::Component,
                                private juce::ListBoxModel
{
public:
    ChordInspectorComponent();

    void setChords (const juce::String& trackName,
                    const juce::StringArray& chordLines,
                    const juce::String& staffText);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    juce::Label titleLabel;
    juce::ListBox chordList;
    juce::Label staffLabel;
    juce::StringArray lines;
};
