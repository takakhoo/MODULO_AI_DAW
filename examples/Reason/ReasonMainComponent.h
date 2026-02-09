#pragma once

#include <JuceHeader.h>

#include "SessionController.h"
#include "ChordInspectorComponent.h"
#include "FxInspectorComponent.h"
#include "PianoRollComponent.h"
#include "TimelineComponent.h"
#include "TrackListComponent.h"
#include "TransportBarComponent.h"

class ReasonMainComponent : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer,
                            private juce::ScrollBar::Listener
{
public:
    ReasonMainComponent();
    ~ReasonMainComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    class PianoRollResizer;
    class TrackListResizer;

    enum class ImportType
    {
        audio,
        midi
    };

    enum class ProjectAction
    {
        newEdit,
        open,
        save,
        saveAs
    };

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    void updateTimeDisplay();
    void showFileMenu();
    void showImportMenu();
    void beginImport (ImportType type);
    void showProjectMenu();
    void beginProjectAction (ProjectAction action);
    void showPluginsWindow();
    void showSettingsMenu();
    void showMidiInputMenu();
    void showInstrumentMenu (int trackIndex);
    void showFxMenu (int trackIndex);
    void generateChordOptionsForSelection();
    void handleChordOptionsResponse (uint64_t sourceClipId, const juce::var& response, const juce::String& rawResponse);
    void openPianoRollForClip (uint64_t clipId);
    void togglePianoRollForSelectedClip();
    void setPianoRollHeight (int newHeight);
    void refreshFxInspector();
    void refreshChordInspector();
    void refreshSessionState();
    void updateVerticalScrollBar();
    void handleTrackScroll (int newOffset);
    void showChordTrackMenu (int trackIndex, juce::Component* source);
    void ensureRealchordsServer();
    bool pingRealchordsServer (int timeoutMs);
    void startRealchordsServer();

    SessionController session;

    TransportBarComponent transportBar;
    TrackListComponent trackList;
    TimelineComponent timeline;
    ChordInspectorComponent chordInspector;
    FxInspectorComponent fxInspector;
    PianoRollComponent pianoRoll;
    std::unique_ptr<PianoRollResizer> pianoRollResizer;
    int pianoRollHeight = 220;
    bool pianoRollVisible = false;
    std::unique_ptr<TrackListResizer> trackListResizer;
    int trackListWidth = 260;
    juce::ScrollBar verticalScrollBar { true };
    int trackScrollOffset = 0;
    bool ignoreVerticalScrollCallback = false;
    int inspectorWidth = 280;
    bool isGeneratingChords = false;
    uint64_t clipClipboardId = 0;
    int instrumentRefreshCounter = 0;
    juce::String lastKnownMidiInputName;

    std::shared_ptr<juce::FileChooser> fileChooser;
    std::shared_ptr<juce::FileChooser> projectChooser;
    std::unique_ptr<juce::ChildProcess> realchordsProcess;
    bool realchordsLaunchAttempted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReasonMainComponent)
};
