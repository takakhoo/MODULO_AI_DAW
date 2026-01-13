#pragma once

#include <JuceHeader.h>

#include "SessionController.h"
#include "TimelineComponent.h"
#include "TrackListComponent.h"
#include "TransportBarComponent.h"

class ReasonMainComponent : public juce::Component,
                            private juce::Timer
{
public:
    ReasonMainComponent();
    ~ReasonMainComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    enum class ImportType
    {
        audio,
        midi
    };

    void timerCallback() override;
    void updateTimeDisplay();
    void showImportMenu();
    void beginImport (ImportType type);

    SessionController session;

    TransportBarComponent transportBar;
    TrackListComponent trackList;
    TimelineComponent timeline;

    std::shared_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReasonMainComponent)
};
