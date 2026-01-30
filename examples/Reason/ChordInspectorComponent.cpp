#include "ChordInspectorComponent.h"

ChordInspectorComponent::ChordInspectorComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Chords", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::FontOptions (13.5f, juce::Font::bold));

    addAndMakeVisible (chordList);
    chordList.setModel (this);
    chordList.setRowHeight (22);

    addAndMakeVisible (staffLabel);
    staffLabel.setJustificationType (juce::Justification::centredLeft);
    staffLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
    staffLabel.setFont (juce::FontOptions (12.0f));
    staffLabel.setText ("—", juce::dontSendNotification);
}

void ChordInspectorComponent::setChords (const juce::String& trackName,
                                         const juce::StringArray& chordLines,
                                         const juce::String& staffText)
{
    lines = chordLines;
    titleLabel.setText (trackName.isNotEmpty() ? ("Chords — " + trackName) : "Chords",
                        juce::dontSendNotification);
    chordList.updateContent();
    chordList.repaint();

    staffLabel.setText (staffText.isNotEmpty() ? staffText : "—", juce::dontSendNotification);
}

void ChordInspectorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1A1C20));
    g.setColour (juce::Colour (0xFF2B2F36));
    g.drawRect (getLocalBounds());
}

void ChordInspectorComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (20);
    titleLabel.setBounds (header);

    r.removeFromTop (6);
    auto staffArea = r.removeFromBottom (36);
    staffLabel.setBounds (staffArea);

    chordList.setBounds (r);
}

int ChordInspectorComponent::getNumRows()
{
    return lines.size();
}

void ChordInspectorComponent::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xFF27384D));

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (12.0f));
    if (juce::isPositiveAndBelow (rowNumber, lines.size()))
        g.drawText (lines[rowNumber], 8, 0, width - 12, height, juce::Justification::centredLeft);
}
