#pragma once

#include <JuceHeader.h>

class TransportBarComponent : public juce::Component,
                              private juce::Timer
{
public:
    TransportBarComponent();

    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onRecord;
    std::function<void()> onFileMenu;
    std::function<void()> onGenerateChords;
    std::function<void()> onMidiInput;
    std::function<void()> onSettings;
    std::function<void (bool)> onMetronomeChanged;
    std::function<void (const juce::String&)> onTempoChanged;
    std::function<void (const juce::String&)> onTimeSignatureChanged;
    std::function<void (const juce::String&)> onKeySignatureChanged;

    void setTimeText (const juce::String& text);
    void setBarsText (const juce::String& text);
    void setTempoText (const juce::String& text);
    void setTimeSignatureText (const juce::String& text);
    void setKeySignatureText (const juce::String& text);
    void setRecordActive (bool shouldShowActive);
    void setMidiInputText (const juce::String& text);
    void setMetronomeActive (bool shouldShowActive);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void showKeySignatureMenu();
    void styleActionButton (juce::TextButton& button, juce::Colour baseColour);

    juce::ShapeButton playButton { "Play", juce::Colours::transparentBlack,
                                   juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    juce::ShapeButton stopButton { "Stop", juce::Colours::transparentBlack,
                                   juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    juce::ShapeButton recordButton { "Record", juce::Colours::transparentBlack,
                                     juce::Colours::transparentBlack, juce::Colours::transparentBlack };
    juce::TextButton fileButton { "File" };
    juce::TextButton chordsButton { "Generate Chords" };
    juce::DrawableButton metronomeButton { "Metronome", juce::DrawableButton::ImageFitted };
    juce::TextButton midiInputButton { "MIDI In" };
    juce::DrawableButton settingsButton { "Settings", juce::DrawableButton::ImageFitted };

    juce::Label timeLabel { {}, "00:00.0" };
    juce::Label barsLabel { {}, "Bar 1 | Beat 1" };
    juce::Label tempoLabel { {}, "Tempo: --" };
    juce::Label timeSigLabel { {}, "4/4" };
    juce::Label keyLabel { {}, "Cmaj" };

    bool recordActive = false;
    bool recordBlinkOn = false;
    juce::Rectangle<int> displayPanelBounds;
    juce::Rectangle<int> moduloBounds;
    juce::Image chordsIcon;
};
