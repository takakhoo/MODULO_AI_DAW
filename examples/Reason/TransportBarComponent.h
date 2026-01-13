#pragma once

#include <JuceHeader.h>

class TransportBarComponent : public juce::Component
{
public:
    TransportBarComponent();

    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onImport;
    std::function<void()> onSettings;

    void setTimeText (const juce::String& text);
    void setTempoText (const juce::String& text);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton importButton { "Import..." };
    juce::TextButton settingsButton { "Settings" };

    juce::Label timeLabel { {}, "00:00.0" };
    juce::Label tempoLabel { {}, "Tempo: --" };
};
