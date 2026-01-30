#pragma once

#include <JuceHeader.h>

class TransportBarComponent : public juce::Component
{
public:
    TransportBarComponent();

    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onRecord;
    std::function<void()> onImport;
    std::function<void()> onProject;
    std::function<void()> onPlugins;
    std::function<void()> onGenerateChords;
    std::function<void()> onMidiInput;
    std::function<void()> onSettings;
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

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton recordButton { "Record" };
    juce::TextButton projectButton { "Project" };
    juce::TextButton importButton { "Import..." };
    juce::TextButton pluginsButton { "Plugins" };
    juce::TextButton chordsButton { "Chords x5" };
    juce::TextButton midiInputButton { "MIDI In" };
    juce::TextButton settingsButton { "Settings" };

    juce::Label timeLabel { {}, "00:00.0" };
    juce::Label barsLabel { {}, "Bar 1 | Beat 1" };
    juce::Label tempoLabel { {}, "Tempo: --" };
    juce::Label timeSigLabel { {}, "4/4" };
    juce::Label keyLabel { {}, "Cmaj" };
};
