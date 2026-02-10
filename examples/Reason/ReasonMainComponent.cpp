#include "ReasonMainComponent.h"

#include <limits>
#include <thread>

namespace
{
int getRealchordsPort()
{
    const int port = juce::SystemStats::getEnvironmentVariable ("REALCHORDS_PORT", "8090").getIntValue();
    return port > 0 ? port : 8090;
}

juce::String getRealchordsHost()
{
    auto host = juce::SystemStats::getEnvironmentVariable ("REALCHORDS_HOST", "127.0.0.1");
    if (host.isEmpty())
        host = "127.0.0.1";
    return host;
}

juce::String getRealchordsBaseUrl()
{
    return "http://" + getRealchordsHost() + ":" + juce::String (getRealchordsPort());
}

juce::File findRealchordsProjectRoot()
{
    auto findFrom = [] (juce::File dir) -> juce::File
    {
        for (int i = 0; i < 12 && dir.exists(); ++i)
        {
            if (dir.getChildFile ("tools/realchords/realchords_batch_server.py").existsAsFile())
                return dir;
            dir = dir.getParentDirectory();
        }
        return {};
    };

    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto root = findFrom (exeDir);
    if (root.exists())
        return root;

    auto cwd = juce::File::getCurrentWorkingDirectory();
    root = findFrom (cwd);
    if (root.exists())
        return root;

    return {};
}

juce::String getRealchordsStartHint()
{
    const auto root = findRealchordsProjectRoot();
    if (root.exists())
    {
        const auto pythonPath = root.getChildFile ("tools/realchords/.venv/bin/python");
        const auto scriptPath = root.getChildFile ("tools/realchords/realchords_batch_server.py");
        return pythonPath.getFullPathName() + " " + scriptPath.getFullPathName();
    }

    return "tools/realchords/.venv/bin/python tools/realchords/realchords_batch_server.py";
}

}

class ReasonMainComponent::PianoRollResizer : public juce::Component
{
public:
    explicit PianoRollResizer (ReasonMainComponent& ownerRef)
        : owner (ownerRef)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        startHeight = owner.pianoRollHeight;
        startY = event.getScreenY();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const int delta = startY - event.getScreenY();
        owner.setPianoRollHeight (startHeight + delta);
    }

private:
    ReasonMainComponent& owner;
    int startHeight = 0;
    int startY = 0;
};

class ReasonMainComponent::TrackListResizer : public juce::Component
{
public:
    explicit TrackListResizer (ReasonMainComponent& ownerRef)
        : owner (ownerRef)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        startWidth = owner.trackListWidth;
        startX = event.getScreenX();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const int delta = event.getScreenX() - startX;
        owner.trackListWidth = juce::jlimit (180, 420, startWidth + delta);
        owner.resized();
    }

private:
    ReasonMainComponent& owner;
    int startWidth = 0;
    int startX = 0;
};

class ReasonMainComponent::InspectorResizer : public juce::Component
{
public:
    explicit InspectorResizer (ReasonMainComponent& ownerRef)
        : owner (ownerRef)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        startWidth = owner.inspectorWidth;
        startX = event.getScreenX();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const int delta = startX - event.getScreenX();
        owner.inspectorWidth = juce::jlimit (220, 560, startWidth + delta);
        owner.resized();
    }

private:
    ReasonMainComponent& owner;
    int startWidth = 0;
    int startX = 0;
};

class ReasonMainComponent::StartupOverlay : public juce::Component,
                                            private juce::Timer
{
public:
    StartupOverlay()
    {
        splashImage = juce::ImageFileFormat::loadFrom (BinaryData::Modulo_Loading_1920x1080_png,
                                                        BinaryData::Modulo_Loading_1920x1080_pngSize);

        addAndMakeVisible (newProjectButton);
        addAndMakeVisible (openProjectButton);

        auto style = [] (juce::TextButton& b, juce::Colour fill)
        {
            b.setColour (juce::TextButton::buttonColourId, fill);
            b.setColour (juce::TextButton::buttonOnColourId, fill.brighter (0.12f));
            b.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.95f));
            b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        };

        style (newProjectButton, juce::Colour (0xCC7F5A17));
        style (openProjectButton, juce::Colour (0xCC101010));

        newProjectButton.setButtonText ("New Blank Project");
        openProjectButton.setButtonText ("Open Existing Project");
        newProjectButton.setEnabled (false);
        openProjectButton.setEnabled (false);

        newProjectButton.onClick = [this]
        {
            if (onNewBlankProject != nullptr)
                onNewBlankProject();
        };

        openProjectButton.onClick = [this]
        {
            if (onOpenProject != nullptr)
                onOpenProject();
        };

        startedAtMs = juce::Time::getMillisecondCounter();
        startTimerHz (30);
    }

    std::function<void()> onNewBlankProject;
    std::function<void()> onOpenProject;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);

        if (splashImage.isValid())
            g.drawImageWithin (splashImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::centred);

        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, (float) getHeight() * 0.42f,
                                                 juce::Colours::black.withAlpha (0.78f), 0.0f, (float) getHeight(), false));
        g.fillRect (getLocalBounds());

        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.setFont (juce::FontOptions (12.5f));
        const auto hint = buttonsEnabled ? "Choose how to begin" : "Preparing session...";
        g.drawText (hint, 0, getHeight() - 142, getWidth(), 24, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (24);
        auto controls = area.removeFromBottom (96);
        controls = controls.withSizeKeepingCentre (juce::jmin (560, controls.getWidth()), controls.getHeight());
        controls.removeFromTop (8);

        auto left = controls.removeFromLeft (controls.getWidth() / 2).reduced (8, 6);
        auto right = controls.reduced (8, 6);
        newProjectButton.setBounds (left);
        openProjectButton.setBounds (right);
    }

private:
    void timerCallback() override
    {
        const auto elapsedMs = juce::Time::getMillisecondCounter() - startedAtMs;
        if (! buttonsEnabled && elapsedMs >= 900)
        {
            buttonsEnabled = true;
            newProjectButton.setEnabled (true);
            openProjectButton.setEnabled (true);
            repaint();
            stopTimer();
        }
    }

    juce::Image splashImage;
    juce::TextButton newProjectButton;
    juce::TextButton openProjectButton;
    uint32_t startedAtMs = 0;
    bool buttonsEnabled = false;
};

ReasonMainComponent::ReasonMainComponent()
{
    addAndMakeVisible (transportBar);
    addAndMakeVisible (trackList);
    addAndMakeVisible (timeline);
    addAndMakeVisible (chordInspector);
    addAndMakeVisible (fxInspector);
    addAndMakeVisible (chordInspectorToggleButton);
    addAndMakeVisible (pianoRoll);
    addAndMakeVisible (verticalScrollBar);

    timeline.setSession (&session);
    timeline.setRowHeight (trackList.getRowHeight());
    trackList.setHeaderHeight (timeline.getHeaderHeight());

    const auto trackNames = session.getTrackNames();
    trackList.setTracks (trackNames);
    trackList.setTrackInstrumentNames (session.getTrackInstrumentNames());
    trackList.setTrackVolumes (session.getTrackVolumes());
    trackList.setTrackPans (session.getTrackPans());
    trackList.setTrackEffectCounts (session.getTrackEffectCounts());
    trackList.setTrackMuteStates (session.getTrackMuteStates());
    trackList.setTrackSoloStates (session.getTrackSoloStates());
    trackList.setSelectedIndex (session.getSelectedTrack());
    refreshFxInspector();
    refreshChordInspector();
    updateVerticalScrollBar();
    chordInspectorToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF6B4A18));
    chordInspectorToggleButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF8B6428));
    chordInspectorToggleButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFFFE6B3));
    chordInspectorToggleButton.onClick = [this]
    {
        chordInspectorCollapsed = ! chordInspectorCollapsed;
        updateChordToggleButton();
        resized();
    };
    updateChordToggleButton();
    fxInspectorVisible = false;
    fxInspector.setVisible (false);

    auto& laf = juce::LookAndFeel::getDefaultLookAndFeel();
    laf.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xFF17110A));
    laf.setColour (juce::PopupMenu::textColourId, juce::Colour (0xFFFFE6B3));
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFF6B4A18));
    laf.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xFFFFF1CF));
    laf.setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xFFE3C891));

    transportBar.setTempoText ("Tempo: --");
    transportBar.setBarsText ("Bar 1 | Beat 1");
    transportBar.setTimeSignatureText (session.getTimeSignature());
    transportBar.setKeySignatureText (session.getKeySignature());
    transportBar.setRecordActive (session.isRecording());
    transportBar.setMidiInputText ("MIDI: " + session.getSelectedMidiInputName());
    transportBar.setMetronomeActive (session.isMetronomeEnabled());
    lastKnownMidiInputName = session.getSelectedMidiInputName();

    transportBar.onPlay = [this]
    {
        session.togglePlay();
    };

    transportBar.onStop = [this]
    {
        session.stop();
    };

    transportBar.onRecord = [this]
    {
        const bool recording = session.toggleRecord();
        transportBar.setRecordActive (recording);
    };

    transportBar.onFileMenu = [this]
    {
        showFileMenu();
    };

    transportBar.onNewTrack = [this]
    {
        if (session.createTrack())
        {
            refreshSessionState();
            updateVerticalScrollBar();
        }
    };

    transportBar.onGenerateChords = [this]
    {
        generateChordOptionsForSelection();
    };

    transportBar.onMidiInput = [this]
    {
        showMidiInputMenu();
    };

    transportBar.onSettings = [this]
    {
        showSettingsMenu();
    };

    transportBar.onMetronomeChanged = [this] (bool enabled)
    {
        session.setMetronomeEnabled (enabled);
    };

    transportBar.onTempoChanged = [this] (const juce::String& text)
    {
        const auto trimmed = text.retainCharacters ("0123456789.");
        if (trimmed.isNotEmpty())
        {
            session.setTempoBpm (trimmed.getDoubleValue());
            timeline.repaint();
            pianoRoll.repaint();
        }
    };

    transportBar.onTimeSignatureChanged = [this] (const juce::String& text)
    {
        session.setTimeSignature (text);
    };

    transportBar.onKeySignatureChanged = [this] (const juce::String& text)
    {
        session.setKeySignature (text);
        transportBar.setKeySignatureText (session.getKeySignature());
    };

    trackList.onSelectionChanged = [this] (int index)
    {
        session.setSelectedTrack (index);
        refreshFxInspector();
        refreshChordInspector();
    };

    trackList.onVolumeChanged = [this] (int index, float value)
    {
        session.setTrackVolume (index, value);
    };

    trackList.onPanChanged = [this] (int index, float value)
    {
        session.setTrackPan (index, value);
    };

    trackList.onNameChanged = [this] (int index, const juce::String& name)
    {
        session.setTrackName (index, name);
        trackList.setTracks (session.getTrackNames());
    };

    trackList.onInstrumentClicked = [this] (int index)
    {
        showInstrumentMenu (index);
    };

    trackList.onFxClicked = [this] (int index)
    {
        fxInspectorVisible = true;
        resized();
        showFxMenu (index);
    };

    trackList.onMuteChanged = [this] (int index, bool muted)
    {
        session.setTrackMute (index, muted);
        trackList.setTrackMuteStates (session.getTrackMuteStates());
    };

    trackList.onSoloChanged = [this] (int index, bool solo)
    {
        session.setTrackSolo (index, solo);
        trackList.setTrackSoloStates (session.getTrackSoloStates());
    };

    trackList.onVerticalScrollChanged = [this] (int offset)
    {
        handleTrackScroll (offset);
    };

    trackList.onContextMenuRequested = [this] (int index, juce::Component* source)
    {
        showChordTrackMenu (index, source);
    };

    trackList.onTrackReordered = [this] (int sourceIndex, int targetIndex)
    {
        if (session.reorderTrack (sourceIndex, targetIndex))
        {
            refreshSessionState();
            trackList.setSelectedIndex (session.getSelectedTrack());
            refreshFxInspector();
            refreshChordInspector();
        }
    };

    fxInspector.onSlotSelected = [this] (uint64_t pluginId)
    {
        auto editor = session.createPluginEditor (pluginId);
        if (editor != nullptr)
            fxInspector.setEditor (std::move (editor));
        else
            fxInspector.clearEditor();
    };

    fxInspector.onSlotEnabledChanged = [this] (uint64_t pluginId, bool enabled)
    {
        session.setPluginEnabled (pluginId, enabled);
        refreshFxInspector();
    };

    fxInspector.onSlotRemoved = [this] (uint64_t pluginId)
    {
        session.removePlugin (pluginId);
        refreshFxInspector();
    };

    chordInspector.onChordCellAction = [this] (int measure, int beat, ChordInspectorComponent::ChordCellAction cellAction)
    {
        SessionController::ChordEditAction action = SessionController::ChordEditAction::block;
        if (cellAction == ChordInspectorComponent::ChordCellAction::arpeggio)
            action = SessionController::ChordEditAction::arpeggio;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::deleteChord)
            action = SessionController::ChordEditAction::deleteChord;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::doubleTime)
            action = SessionController::ChordEditAction::doubleTime;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::semitoneUp)
            action = SessionController::ChordEditAction::semitoneUp;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::semitoneDown)
            action = SessionController::ChordEditAction::semitoneDown;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::octaveUp)
            action = SessionController::ChordEditAction::octaveUp;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::octaveDown)
            action = SessionController::ChordEditAction::octaveDown;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::inversionUp)
            action = SessionController::ChordEditAction::inversionUp;
        else if (cellAction == ChordInspectorComponent::ChordCellAction::inversionDown)
            action = SessionController::ChordEditAction::inversionDown;

        if (session.applyChordEditAction (session.getSelectedTrack(), measure, beat, action))
        {
            timeline.repaint();
            refreshChordInspector();
            if (chordInspector.onChordCellSelected != nullptr)
                chordInspector.onChordCellSelected (measure, beat);
        }
    };

    chordInspector.onEmptyChordCellChosen = [this] (int measure, int beat, const juce::String& chordSymbol)
    {
        if (session.setChordAtCell (session.getSelectedTrack(), measure, beat, chordSymbol))
        {
            timeline.repaint();
            refreshChordInspector();
            if (chordInspector.onChordCellSelected != nullptr)
                chordInspector.onChordCellSelected (measure, beat);
        }
    };

    chordInspector.onChordCellSelected = [this] (int measure, int beat)
    {
        std::vector<ChordInspectorComponent::PreviewNote> preview;
        for (const auto& note : session.getChordCellPreviewNotesForTrack (session.getSelectedTrack(), measure, beat))
            preview.push_back ({ note.pitch, note.startBeats, note.lengthBeats });
        chordInspector.setPreviewNotes (preview);
    };

    timeline.onTrackSelected = [this] (int index)
    {
        trackList.setSelectedIndex (index);
        refreshFxInspector();
        refreshChordInspector();
    };

    timeline.onClipDoubleClicked = [this] (const SessionController::ClipInfo& clip)
    {
        if (clip.type == SessionController::ClipType::midi)
            openPianoRollForClip (clip.id);
    };

    timeline.onViewChanged = [this] (double viewStart, double pps)
    {
        if (pianoRollVisible)
            pianoRoll.setView (viewStart, pps);
    };

    timeline.onVerticalScrollChanged = [this] (int offset)
    {
        handleTrackScroll (offset);
    };

    pianoRoll.setSession (&session);
    pianoRoll.setVisible (false);
    pianoRoll.onViewChanged = [this] (double viewStart, double pps)
    {
        timeline.setView (viewStart, pps);
    };

    pianoRollResizer = std::make_unique<PianoRollResizer> (*this);
    addAndMakeVisible (*pianoRollResizer);
    pianoRollResizer->setVisible (false);

    trackListResizer = std::make_unique<TrackListResizer> (*this);
    addAndMakeVisible (*trackListResizer);
    inspectorResizer = std::make_unique<InspectorResizer> (*this);
    addAndMakeVisible (*inspectorResizer);

    verticalScrollBar.setRangeLimits (0.0, 1.0);
    verticalScrollBar.setCurrentRange (0.0, 1.0);
    verticalScrollBar.addListener (this);

    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    setWantsKeyboardFocus (true);
    grabKeyboardFocus();
    setSize (1100, 700);
    startTimerHz (30);

    setMainUiVisible (false);

    startupOverlay = std::make_unique<StartupOverlay>();
    startupOverlay->onNewBlankProject = [this]
    {
        dismissStartupOverlay();
    };
    startupOverlay->onOpenProject = [this]
    {
        beginProjectAction (ProjectAction::open);
    };
    addAndMakeVisible (*startupOverlay);
    startupOverlay->setBounds (getLocalBounds());
    startupOverlay->toFront (false);

    ensureRealchordsServer();
}

void ReasonMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF18191C));
}

void ReasonMainComponent::resized()
{
    auto r = getLocalBounds();

    auto topArea = r.removeFromTop (112);
    transportBar.setBounds (topArea);

    auto trackListArea = r.removeFromLeft (trackListWidth);
    trackList.setBounds (trackListArea);

    auto trackResizerArea = r.removeFromLeft (6);
    trackListResizer->setBounds (trackResizerArea);

    auto inspectorArea = r.removeFromRight (inspectorWidth);
    auto inspectorResizerArea = r.removeFromRight (6);
    inspectorResizer->setBounds (inspectorResizerArea);
    chordInspectorToggleButton.setBounds (inspectorArea.removeFromTop (26).reduced (3, 2));
    inspectorArea.removeFromTop (4);

    if (chordInspectorCollapsed)
    {
        chordInspector.setVisible (false);
    }
    else
    {
        chordInspector.setVisible (true);
        const int chordHeight = fxInspectorVisible ? 220 : inspectorArea.getHeight();
        chordInspector.setBounds (inspectorArea.removeFromTop (juce::jmax (0, chordHeight)));
        inspectorArea.removeFromTop (4);
    }

    if (fxInspectorVisible)
    {
        fxInspector.setVisible (true);
        fxInspector.setBounds (inspectorArea);
    }
    else
    {
        fxInspector.setVisible (false);
        fxInspector.setBounds ({});
    }

    auto scrollBarArea = r.removeFromRight (12);
    verticalScrollBar.setBounds (scrollBarArea.reduced (2, 2));

    if (pianoRollVisible)
    {
        const int resizerHeight = 6;
        auto pianoArea = r.removeFromBottom (pianoRollHeight);
        auto resizerArea = r.removeFromBottom (resizerHeight);
        timeline.setBounds (r);
        pianoRollResizer->setBounds (resizerArea);
        pianoRoll.setBounds (pianoArea);
    }
    else
    {
        timeline.setBounds (r);
    }

    trackList.setVisibleTrackHeightOverride (timeline.getVisibleTrackHeight());
    updateVerticalScrollBar();

    if (startupOverlay != nullptr && startupOverlay->isVisible())
    {
        startupOverlay->setBounds (getLocalBounds());
        startupOverlay->toFront (false);
    }
}

bool ReasonMainComponent::keyPressed (const juce::KeyPress& key)
{
    if (startupOverlay != nullptr && startupOverlay->isVisible())
        return false;

    if (key == juce::KeyPress::spaceKey)
    {
        if (session.isRecording())
        {
            session.stop();
            transportBar.setRecordActive (false);
            refreshSessionState();
        }
        else
        {
            session.togglePlay();
        }
        return true;
    }

    if (pianoRollVisible && pianoRoll.hasKeyboardFocus (true))
    {
        if (pianoRoll.keyPressed (key))
            return true;
    }

    if (key.getModifiers().isCommandDown())
    {
        const int keyCode = key.getKeyCode();
        if (keyCode == 'z' || keyCode == 'Z')
        {
            const bool hadPianoRollOpen = pianoRollVisible;
            const uint64_t previousPianoRollClipId = pianoRoll.getClipId();
            const bool wasFocusedInPianoRoll = pianoRoll.hasKeyboardFocus (true);
            if (key.getModifiers().isShiftDown())
                session.redo();
            else
                session.undo();
            refreshSessionState();
            updateVerticalScrollBar();
            if (hadPianoRollOpen && previousPianoRollClipId != 0)
            {
                openPianoRollForClip (previousPianoRollClipId);
                if (wasFocusedInPianoRoll)
                    pianoRoll.grabKeyboardFocus();
            }
            return true;
        }
        if (keyCode == 'd' || keyCode == 'D')
        {
            if (session.duplicateTrack (session.getSelectedTrack()))
            {
                refreshSessionState();
                updateVerticalScrollBar();
            }
            return true;
        }
        if (keyCode == 'c' || keyCode == 'C')
        {
            const auto clipId = timeline.getSelectedClipId();
            if (clipId != 0)
                clipClipboardId = clipId;
            return true;
        }
        if (keyCode == 'v' || keyCode == 'V')
        {
            if (clipClipboardId != 0)
            {
                const auto newClipId = session.duplicateClipToTrack (clipClipboardId,
                                                                     session.getSelectedTrack(),
                                                                     session.getCursorTimeSeconds());
                if (newClipId != 0)
                {
                    timeline.setSelectedClipId (newClipId);
                    refreshSessionState();
                }
            }
            return true;
        }
        if (keyCode == 'n' || keyCode == 'N')
        {
            beginProjectAction (ProjectAction::newEdit);
            return true;
        }
        if (keyCode == 'o' || keyCode == 'O')
        {
            beginProjectAction (ProjectAction::open);
            return true;
        }
        if (keyCode == 's' || keyCode == 'S')
        {
            if (key.getModifiers().isShiftDown())
                beginProjectAction (ProjectAction::saveAs);
            else
                beginProjectAction (ProjectAction::save);
            return true;
        }
    }

    const juce::juce_wchar keyChar = key.getTextCharacter();
    if (keyChar == 'i' || keyChar == 'I')
    {
        timeline.addIdeaMarker (session.getInsertionTimeSeconds());
        return true;
    }

    if (keyChar == 'e' || keyChar == 'E')
    {
        togglePianoRollForSelectedClip();
        return true;
    }

    if (keyChar == 'r' || keyChar == 'R')
    {
        if (! session.isRecording())
        {
            const bool recording = session.toggleRecord();
            transportBar.setRecordActive (recording);
        }
        return true;
    }

    if (keyChar == 'l' || keyChar == 'L')
    {
        const auto selectedClipId = timeline.getSelectedClipId();
        auto clipInfo = session.getClipInfo (selectedClipId);
        if (clipInfo && clipInfo->type == SessionController::ClipType::midi)
        {
            const double lengthSeconds = juce::jmax (0.01, clipInfo->lengthSeconds);
            const int targetTrack = clipInfo->trackIndex;
            uint64_t lastNewClipId = selectedClipId;
            for (int i = 0; i < 5; ++i)
            {
                const double start = clipInfo->startSeconds + (i + 1) * lengthSeconds;
                const auto newId = session.duplicateClipToTrack (selectedClipId, targetTrack, start);
                if (newId != 0)
                    lastNewClipId = newId;
            }
            timeline.setSelectedClipId (lastNewClipId);
            refreshSessionState();
        }
        return true;
    }

    if (keyChar == 'u' || keyChar == 'U')
    {
        if (session.transposeTrackOctave (session.getSelectedTrack(), +1))
        {
            refreshSessionState();
            return true;
        }
    }

    if (keyChar == 'd' || keyChar == 'D')
    {
        if (session.transposeTrackOctave (session.getSelectedTrack(), -1))
        {
            refreshSessionState();
            return true;
        }
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (chordInspector.deleteSelectedChordIfAny())
        {
            timeline.repaint();
            refreshChordInspector();
            return true;
        }

        const auto clipId = timeline.getSelectedClipId();
        if (clipId != 0)
        {
            if (session.deleteClip (clipId))
            {
                timeline.clearSelection();
                refreshSessionState();
                return true;
            }
        }
        else
        {
            const int trackIndex = session.getSelectedTrack();
            if (session.deleteTrack (trackIndex))
            {
                refreshSessionState();
                updateVerticalScrollBar();
                return true;
            }
        }
    }

    return false;
}

void ReasonMainComponent::timerCallback()
{
    updateTimeDisplay();
    transportBar.setRecordActive (session.isRecording());
    chordInspector.setCurrentTimeSeconds (session.getCurrentTimeSeconds());

    if (++instrumentRefreshCounter >= 15)
    {
        instrumentRefreshCounter = 0;
        trackList.setTrackInstrumentNames (session.getTrackInstrumentNames());
    }
}

void ReasonMainComponent::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved != &verticalScrollBar || ignoreVerticalScrollCallback)
        return;

    handleTrackScroll ((int) std::round (newRangeStart));
}

void ReasonMainComponent::updateTimeDisplay()
{
    session.ensureMidiInputSelection();
    const auto currentMidiInputName = session.getSelectedMidiInputName();
    if (currentMidiInputName != lastKnownMidiInputName)
    {
        lastKnownMidiInputName = currentMidiInputName;
        if (currentMidiInputName != "No MIDI Input")
            session.refreshMidiLiveRouting();
    }

    const double timeSeconds = session.getCurrentTimeSeconds();
    const int minutes = (int) (timeSeconds / 60.0);
    const double seconds = timeSeconds - (minutes * 60.0);

    transportBar.setTimeText (juce::String::formatted ("%02d:%06.3f", minutes, seconds));
    transportBar.setBarsText (session.getBarsAndBeatsText (timeSeconds));
    transportBar.setTempoText ("Tempo " + juce::String (session.getTempoBpm(), 1));
    transportBar.setTimeSignatureText (session.getTimeSignature());
    transportBar.setKeySignatureText (session.getKeySignature());
    transportBar.setMidiInputText ("MIDI: " + currentMidiInputName);
    transportBar.setMetronomeActive (session.isMetronomeEnabled());
}

void ReasonMainComponent::showImportMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("Import Audio...", [this] { beginImport (ImportType::audio); });
    menu.addItem ("Import MIDI...", [this] { beginImport (ImportType::midi); });

    menu.showMenuAsync ({});
}

void ReasonMainComponent::showFileMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("New...", [this] { beginProjectAction (ProjectAction::newEdit); });
    menu.addItem ("Open...", [this] { beginProjectAction (ProjectAction::open); });
    menu.addSeparator();
    menu.addItem ("Save", [this] { beginProjectAction (ProjectAction::save); });
    menu.addItem ("Save As...", [this] { beginProjectAction (ProjectAction::saveAs); });
    menu.addSeparator();
    menu.addItem ("Import Audio...", [this] { beginImport (ImportType::audio); });
    menu.addItem ("Import MIDI...", [this] { beginImport (ImportType::midi); });
    menu.showMenuAsync ({});
}

void ReasonMainComponent::beginImport (ImportType type)
{
    auto& engine = session.getEngine();
    const auto defaultDir = engine.getPropertyStorage().getDefaultLoadSaveDirectory ("reasonImport");

    juce::String wildcard;
    juce::String title;

    if (type == ImportType::audio)
    {
        title = "Import audio...";
        wildcard = engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats();
    }
    else
    {
        title = "Import MIDI...";
        wildcard = "*.mid;*.midi";
    }

    fileChooser = std::make_shared<juce::FileChooser> (title, defaultDir, wildcard);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, type] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();
                                  if (! file.existsAsFile())
                                      return;

                                  const int trackIndex = session.getSelectedTrack();
                                  const double startTime = session.getInsertionTimeSeconds();

                                  bool success = false;
                                  if (type == ImportType::audio)
                                      success = session.importAudio (trackIndex, file, startTime);
                                  else
                                      success = session.importMidi (trackIndex, file, startTime);

                                  if (success)
                                  {
                                      session.getEngine().getPropertyStorage().setDefaultLoadSaveDirectory ("reasonImport", file.getParentDirectory());
                                      timeline.repaint();
                                  }
                              });
}

void ReasonMainComponent::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("New...", [this] { beginProjectAction (ProjectAction::newEdit); });
    menu.addItem ("Open...", [this] { beginProjectAction (ProjectAction::open); });
    menu.addSeparator();
    menu.addItem ("Save", [this] { beginProjectAction (ProjectAction::save); });
    menu.addItem ("Save As...", [this] { beginProjectAction (ProjectAction::saveAs); });
    menu.showMenuAsync ({});
}

void ReasonMainComponent::beginProjectAction (ProjectAction action)
{
    if (action == ProjectAction::save)
    {
        if (! session.saveEdit())
            beginProjectAction (ProjectAction::saveAs);
        return;
    }

    auto& engine = session.getEngine();
    const auto defaultDir = engine.getPropertyStorage().getDefaultLoadSaveDirectory ("reasonProject");

    if (action == ProjectAction::saveAs)
    {
        projectChooser = std::make_shared<juce::FileChooser> ("Save Project...", defaultDir, "*.tracktionedit");
        projectChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                     [this] (const juce::FileChooser& chooser)
                                     {
                                         auto file = chooser.getResult();
                                         if (file == juce::File())
                                             return;

                                         if (file.getFileExtension().isEmpty())
                                             file = file.withFileExtension (".tracktionedit");

                                         if (session.saveEditAs (file))
                                         {
                                             session.getEngine().getPropertyStorage().setDefaultLoadSaveDirectory ("reasonProject", file.getParentDirectory());
                                         }
                                     });
        return;
    }

    const bool isNew = (action == ProjectAction::newEdit);
    const auto flags = isNew
        ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles)
        : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);

    const juce::String title = isNew ? "New Project..." : "Open Project...";
    projectChooser = std::make_shared<juce::FileChooser> (title, defaultDir, "*.tracktionedit");
    projectChooser->launchAsync (flags,
                                 [this, isNew] (const juce::FileChooser& chooser)
                                 {
                                     auto file = chooser.getResult();
                                     if (file == juce::File())
                                         return;

                                     if (isNew && file.getFileExtension().isEmpty())
                                         file = file.withFileExtension (".tracktionedit");

                                     const bool success = isNew ? session.createNewEdit (file)
                                                                : session.openEdit (file);
                                     if (success)
                                     {
                                         session.getEngine().getPropertyStorage().setDefaultLoadSaveDirectory ("reasonProject", file.getParentDirectory());
                                         refreshSessionState();
                                         if (startupOverlay != nullptr && startupOverlay->isVisible())
                                             dismissStartupOverlay();
                                     }
                                 });
}

void ReasonMainComponent::showPluginsWindow()
{
    auto& engine = session.getEngine();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugins";
    options.dialogBackgroundColour = juce::Colour (0xFF1A1C20);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;

    auto* component = new juce::PluginListComponent (engine.getPluginManager().pluginFormatManager,
                                                     engine.getPluginManager().knownPluginList,
                                                     engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                                     std::addressof (engine.getPropertyStorage().getPropertiesFile()),
                                                     true);
    component->setSize (800, 600);
    options.content.setOwned (component);
    options.launchAsync();
}

void ReasonMainComponent::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem ("Audio/MIDI Device Settings...", [this] { session.showAudioSettings(); });
    menu.addSeparator();
    menu.addItem ("Plugin Manager...", [this] { showPluginsWindow(); });
    menu.showMenuAsync ({});
}

void ReasonMainComponent::showMidiInputMenu()
{
    juce::PopupMenu menu;
    auto names = session.getMidiInputDeviceNames();
    const int selectedIndex = session.getSelectedMidiInputIndex();

    if (names.isEmpty())
    {
        menu.addItem ("No MIDI Inputs Found", false, false, nullptr);
    }
    else
    {
        for (int i = 0; i < names.size(); ++i)
        {
            const bool isSelected = (i == selectedIndex);
            juce::PopupMenu::Item item;
            item.text = names[i];
            item.isEnabled = true;
            item.isTicked = isSelected;
            item.action = [this, i]
            {
                session.setSelectedMidiInputIndex (i);
                transportBar.setMidiInputText ("MIDI: " + session.getSelectedMidiInputName());
            };
            menu.addItem (item);
        }
    }

    menu.showMenuAsync ({});
}

void ReasonMainComponent::showInstrumentMenu (int trackIndex)
{
    trackList.setSelectedIndex (trackIndex);
    session.setSelectedTrack (trackIndex);

    juce::PopupMenu menu;
    auto choices = session.getInstrumentChoices();

    if (choices.empty())
    {
        menu.addItem ("No instruments found", false, false, nullptr);
    }
    else
    {
        const auto currentNames = session.getTrackInstrumentNames();
        const auto current = (trackIndex >= 0 && trackIndex < currentNames.size())
            ? currentNames[trackIndex]
            : juce::String();
        std::sort (choices.begin(), choices.end(),
                   [] (const SessionController::PluginChoice& a, const SessionController::PluginChoice& b)
                   {
                       return a.description.name.compareIgnoreCase (b.description.name) < 0;
                   });

        for (size_t i = 0; i < choices.size(); ++i)
        {
            const auto& choice = choices[i];
            juce::String label = choice.description.name;
            if (label.isEmpty())
                label = choice.description.pluginFormatName;

            const bool isCurrent = label == current;
            juce::PopupMenu::Item item;
            item.itemID = (int) i + 1;
            item.text = label;
            item.isEnabled = true;
            item.isTicked = isCurrent;
            menu.addItem (item);
        }
    }

    menu.showMenuAsync ({}, [this, trackIndex, choices] (int result)
    {
        if (result <= 0 || result > (int) choices.size())
            return;

        const auto prevInstrumentNames = session.getTrackInstrumentNames();
        const auto prevInstrument = (trackIndex >= 0 && trackIndex < prevInstrumentNames.size())
            ? prevInstrumentNames[trackIndex]
            : juce::String();
        const auto prevTrackName = session.getTrackName (trackIndex);

        const auto& choice = choices[(size_t) result - 1];
        const auto pluginId = session.insertInstrument (trackIndex, choice);

        const auto newInstrumentNames = session.getTrackInstrumentNames();
        const auto newInstrument = (trackIndex >= 0 && trackIndex < newInstrumentNames.size())
            ? newInstrumentNames[trackIndex]
            : juce::String();

        const auto defaultName = "Track " + juce::String (trackIndex + 1);
        auto isPlaceholder = [] (const juce::String& text)
        {
            auto lower = text.trim().toLowerCase();
            if (lower.isEmpty())
                return true;
            if (lower.contains ("untitled"))
                return true;
            if (lower == "instrument" || lower == "no instrument")
                return true;
            if (lower == "default" || lower.startsWith ("default"))
                return true;
            if (lower == "init" || lower.startsWith ("init"))
                return true;
            if (lower == "program" || lower.startsWith ("program"))
                return true;
            return false;
        };

        if ((prevTrackName == defaultName || prevTrackName == prevInstrument) && ! isPlaceholder (newInstrument))
            session.setTrackName (trackIndex, newInstrument);

        trackList.setTracks (session.getTrackNames());
        trackList.setTrackInstrumentNames (newInstrumentNames);

        if (pluginId != 0)
            session.showPluginWindow (pluginId);
    });
}

void ReasonMainComponent::showFxMenu (int trackIndex)
{
    trackList.setSelectedIndex (trackIndex);
    session.setSelectedTrack (trackIndex);

    juce::PopupMenu menu;
    auto choices = session.getEffectChoices();

    if (choices.empty())
    {
        menu.addItem ("No effects found", false, false, nullptr);
        menu.showMenuAsync ({});
        return;
    }

    std::sort (choices.begin(), choices.end(),
               [] (const SessionController::PluginChoice& a, const SessionController::PluginChoice& b)
               {
                   return a.description.name.compareIgnoreCase (b.description.name) < 0;
               });

    for (size_t i = 0; i < choices.size(); ++i)
    {
        const auto& choice = choices[i];
        juce::String label = choice.description.name;
        if (label.isEmpty())
            label = choice.description.pluginFormatName;

        juce::PopupMenu::Item item;
        item.itemID = (int) i + 1;
        item.text = label;
        item.isEnabled = true;
        menu.addItem (item);
    }

    menu.showMenuAsync ({}, [this, trackIndex, choices] (int result)
    {
        if (result <= 0 || result > (int) choices.size())
            return;

        const auto& choice = choices[(size_t) result - 1];
        const auto pluginId = session.insertEffect (trackIndex, choice);
        trackList.setTrackEffectCounts (session.getTrackEffectCounts());
        fxInspectorVisible = true;
        resized();
        refreshFxInspector();

        if (pluginId != 0)
        {
            fxInspector.setSelectedSlot (pluginId);
            auto editor = session.createPluginEditor (pluginId);
            if (editor != nullptr)
                fxInspector.setEditor (std::move (editor));
            else
                fxInspector.clearEditor();
        }
    });
}

void ReasonMainComponent::generateChordOptionsForSelection()
{
    if (isGeneratingChords)
        return;

    ensureRealchordsServer();

    uint64_t clipId = timeline.getSelectedClipId();
    auto clipInfo = session.getClipInfo (clipId);
    if (! clipInfo || clipInfo->type != SessionController::ClipType::midi)
    {
        const auto clips = session.getClipsForTrack (session.getSelectedTrack());
        const double insertionTime = session.getInsertionTimeSeconds();
        double bestScore = std::numeric_limits<double>::max();
        uint64_t bestClipId = 0;

        for (const auto& clip : clips)
        {
            if (clip.type != SessionController::ClipType::midi)
                continue;

            // Prefer clips that contain the insertion point, then nearest-start clips.
            const double clipStart = clip.startSeconds;
            const double clipEnd = clip.startSeconds + clip.lengthSeconds;
            const bool containsInsertion = insertionTime >= clipStart && insertionTime <= clipEnd;
            const double distanceToStart = std::abs (insertionTime - clipStart);
            const double score = containsInsertion ? (distanceToStart * 0.1) : (distanceToStart + 1000.0);

            if (score < bestScore)
            {
                bestScore = score;
                bestClipId = clip.id;
            }
        }

        clipId = bestClipId;
    }

    if (clipId == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "Select a MIDI clip (or record one) before generating chord options.");
        return;
    }

    juce::AlertWindow prompt ("Generate Chords",
                              "Choose how many options and playback style.",
                              juce::AlertWindow::NoIcon);
    prompt.addTextEditor ("count", "5", "Options");
    prompt.addComboBox ("style",
                        juce::StringArray { "Block Chords", "Arpeggio" },
                        "Playback");
    prompt.addButton ("Generate", 1, juce::KeyPress (juce::KeyPress::returnKey));
    prompt.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (prompt.runModalLoop() != 1)
        return;

    int optionCount = prompt.getTextEditorContents ("count").getIntValue();
    optionCount = juce::jlimit (1, 16, optionCount);
    int styleChoice = 1;
    if (auto* styleBox = prompt.getComboBoxComponent ("style"))
        styleChoice = styleBox->getSelectedId();
    const auto playbackStyle = (styleChoice == 2)
        ? SessionController::ChordPlaybackStyle::arpeggio
        : SessionController::ChordPlaybackStyle::block;

    std::vector<SessionController::RealchordsNoteEvent> events;
    int endFrame = 0;
    if (! session.buildRealchordsNoteEvents (clipId, events, endFrame))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "Unable to extract MIDI note events from the selected clip.");
        return;
    }

    juce::Array<juce::var> noteArray;
    for (const auto& ev : events)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("on", ev.on);
        obj->setProperty ("pitch", ev.pitch);
        obj->setProperty ("frame", ev.frame);
        noteArray.add (juce::var (obj));
    }

    auto* payloadObj = new juce::DynamicObject();
    payloadObj->setProperty ("model", "GAPT");
    payloadObj->setProperty ("options", optionCount);
    payloadObj->setProperty ("temperature", 0.9);
    payloadObj->setProperty ("endFrame", endFrame);
    payloadObj->setProperty ("lookahead", 16);
    payloadObj->setProperty ("commitahead", 16);
    payloadObj->setProperty ("silenceTill", 32);
    payloadObj->setProperty ("notes", juce::var (noteArray));
    const juce::String payload = juce::JSON::toString (juce::var (payloadObj));

    isGeneratingChords = true;

    const auto baseUrl = getRealchordsBaseUrl();
    const auto startHint = getRealchordsStartHint();

    std::thread ([this, payload, clipId, playbackStyle, baseUrl, startHint]
    {
        juce::URL url (baseUrl + "/generate");
        url = url.withPOSTData (payload);
        int statusCode = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (60000)
                           .withNumRedirectsToFollow (2)
                           .withStatusCode (&statusCode)
                           .withExtraHeaders ("Content-Type: application/json\r\n");

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (options));
        if (stream == nullptr)
        {
            juce::MessageManager::callAsync ([this, baseUrl, startHint]
            {
                isGeneratingChords = false;
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                       "Generate Chords",
                                                       "Could not reach the Realchords batch server at " + baseUrl
                                                           + ".\n\nStart it with:\n" + startHint);
            });
            return;
        }

        const juce::String response = stream->readEntireStreamAsString();
        const juce::var parsed = juce::JSON::parse (response);

        juce::MessageManager::callAsync ([this, clipId, parsed, response, statusCode, playbackStyle]
        {
            isGeneratingChords = false;
            if (statusCode != 0 && statusCode != 200)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                       "Generate Chords",
                                                       "Realchords server returned HTTP " + juce::String (statusCode) + ".\n\nResponse:\n" + response);
                return;
            }
            handleChordOptionsResponse (clipId, parsed, response, playbackStyle);
        });
    }).detach();
}

void ReasonMainComponent::handleChordOptionsResponse (uint64_t sourceClipId, const juce::var& response, const juce::String& rawResponse,
                                                      SessionController::ChordPlaybackStyle playbackStyle)
{
    if (! response.isObject())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "Chord generation failed (invalid response). Make sure the Realchords server is running.\n\nResponse:\n" + rawResponse);
        return;
    }

    const auto* obj = response.getDynamicObject();
    const auto optionsVar = obj->getProperty ("options");
    if (! optionsVar.isArray())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "Chord generation failed (no options returned).");
        return;
    }

    std::vector<SessionController::ChordOption> options;
    const auto& optionsArray = *optionsVar.getArray();
    for (int i = 0; i < optionsArray.size(); ++i)
    {
        const auto& optionVar = optionsArray.getReference (i);
        if (! optionVar.isObject())
            continue;

        const auto* optionObj = optionVar.getDynamicObject();
        SessionController::ChordOption option;
        option.name = "Chords Option " + juce::String (i + 1);
        option.frameCount = (int) optionObj->getProperty ("frameCount");

        const auto chordsVar = optionObj->getProperty ("chords");
        if (chordsVar.isArray())
        {
            const auto& chordsArray = *chordsVar.getArray();
            for (const auto& chordVar : chordsArray)
            {
                if (! chordVar.isObject())
                    continue;
                const auto* chordObj = chordVar.getDynamicObject();
                SessionController::ChordFrame frame;
                frame.frame = (int) chordObj->getProperty ("frame");
                frame.symbol = chordObj->getProperty ("symbol").toString();
                frame.on = (bool) chordObj->getProperty ("on");

                const auto pitchesVar = chordObj->getProperty ("pitches");
                if (pitchesVar.isArray())
                {
                    const auto& pitches = *pitchesVar.getArray();
                    for (const auto& pitchVar : pitches)
                        frame.pitches.add ((int) pitchVar);
                }
                option.frames.push_back (frame);
            }
        }
        options.push_back (std::move (option));
    }

    if (options.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "No chord options were generated.");
        return;
    }

    if (session.createChordOptionTracks (sourceClipId, options, playbackStyle))
    {
        refreshSessionState();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                               "Generate Chords",
                                               "Failed to create chord option tracks in the edit.");
    }
}

void ReasonMainComponent::ensureRealchordsServer()
{
    if (pingRealchordsServer (400))
        return;

    if (! realchordsLaunchAttempted)
    {
        realchordsLaunchAttempted = true;
        startRealchordsServer();
    }
}

bool ReasonMainComponent::pingRealchordsServer (int timeoutMs)
{
    juce::URL url (getRealchordsBaseUrl() + "/health");
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (timeoutMs)
                       .withNumRedirectsToFollow (1);
    std::unique_ptr<juce::InputStream> stream (url.createInputStream (options));
    if (stream == nullptr)
        return false;

    const juce::String response = stream->readEntireStreamAsString();
    return response.contains ("\"ok\"");
}

void ReasonMainComponent::startRealchordsServer()
{
    if (realchordsProcess != nullptr && realchordsProcess->isRunning())
        return;

    juce::File projectRoot = findRealchordsProjectRoot();
    if (! projectRoot.exists())
        return;

    const juce::String pythonPath = projectRoot.getChildFile ("tools/realchords/.venv/bin/python").getFullPathName();
    const juce::String scriptPath = projectRoot.getChildFile ("tools/realchords/realchords_batch_server.py").getFullPathName();

    const juce::String command = "REALCHORDS_HOST=" + getRealchordsHost()
                                 + " REALCHORDS_PORT=" + juce::String (getRealchordsPort())
                                 + " \"" + pythonPath + "\" \"" + scriptPath + "\"";
    realchordsProcess = std::make_unique<juce::ChildProcess>();
    realchordsProcess->start (command);
}

void ReasonMainComponent::openPianoRollForClip (uint64_t clipId)
{
    auto clipInfo = session.getClipInfo (clipId);
    if (! clipInfo || clipInfo->type != SessionController::ClipType::midi)
        return;

    pianoRollVisible = true;
    pianoRoll.setVisible (true);
    pianoRollResizer->setVisible (true);
    pianoRoll.setClipId (clipId);
    pianoRoll.setView (timeline.getViewStartSeconds(), timeline.getPixelsPerSecond());
    pianoRoll.grabKeyboardFocus();
    resized();
}

void ReasonMainComponent::togglePianoRollForSelectedClip()
{
    const auto clipId = timeline.getSelectedClipId();
    if (clipId == 0)
        return;

    if (pianoRollVisible && pianoRoll.getClipId() == clipId)
    {
        pianoRollVisible = false;
        pianoRoll.setVisible (false);
        pianoRollResizer->setVisible (false);
        resized();
        return;
    }

    openPianoRollForClip (clipId);
}

void ReasonMainComponent::setPianoRollHeight (int newHeight)
{
    if (! pianoRollVisible)
        return;

    const int minHeight = 140;
    const int maxHeight = juce::jmax (minHeight, getHeight() - 220);
    pianoRollHeight = juce::jlimit (minHeight, maxHeight, newHeight);
    resized();
}

void ReasonMainComponent::refreshFxInspector()
{
    const int trackIndex = session.getSelectedTrack();
    fxInspector.setTrackName (session.getTrackName (trackIndex));

    std::vector<FxInspectorComponent::Slot> slots;
    for (const auto& plugin : session.getTrackEffects (trackIndex))
        slots.push_back ({ plugin.id, plugin.name, plugin.enabled });

    fxInspector.setSlots (slots);

    if (slots.empty())
        fxInspector.clearEditor();
}

void ReasonMainComponent::refreshChordInspector()
{
    const int trackIndex = session.getSelectedTrack();
    const auto trackName = session.getTrackName (trackIndex);

    auto labels = session.getChordLabelsForTrack (trackIndex);
    juce::StringArray symbols;
    juce::Array<double> starts;
    juce::Array<int> measures;
    juce::Array<int> beats;
    for (const auto& label : labels)
    {
        const auto timeText = session.getBarsAndBeatsText (label.startSeconds);
        auto parts = juce::StringArray::fromTokens (timeText.retainCharacters ("0123456789|"), "|", "");
        symbols.add (label.symbol);
        starts.add (label.startSeconds);
        measures.add (parts.size() >= 1 ? juce::jmax (1, parts[0].getIntValue()) : 1);
        beats.add (parts.size() >= 2 ? juce::jmax (1, parts[1].getIntValue()) : 1);
    }

    juce::String staff;
    if (! symbols.isEmpty())
        staff = "| " + symbols.joinIntoString (" | ") + " |";

    int beatsPerBar = 4;
    const auto timeSig = session.getTimeSignature();
    const auto slash = timeSig.indexOfChar ('/');
    if (slash > 0)
        beatsPerBar = juce::jlimit (1, 16, timeSig.substring (0, slash).getIntValue());

    chordInspector.setChords (trackName, symbols, starts, measures, beats, beatsPerBar, staff);
    chordInspector.setCurrentTimeSeconds (session.getCurrentTimeSeconds());
}

void ReasonMainComponent::refreshSessionState()
{
    trackList.setTracks (session.getTrackNames());
    trackList.setTrackInstrumentNames (session.getTrackInstrumentNames());
    trackList.setTrackVolumes (session.getTrackVolumes());
    trackList.setTrackPans (session.getTrackPans());
    trackList.setTrackEffectCounts (session.getTrackEffectCounts());
    trackList.setTrackMuteStates (session.getTrackMuteStates());
    trackList.setTrackSoloStates (session.getTrackSoloStates());
    trackList.setSelectedIndex (session.getSelectedTrack());
    transportBar.setMidiInputText ("MIDI: " + session.getSelectedMidiInputName());
    transportBar.setRecordActive (session.isRecording());
    timeline.repaint();
    updateTimeDisplay();
    refreshFxInspector();
    refreshChordInspector();
    updateVerticalScrollBar();

    pianoRollVisible = false;
    pianoRoll.setVisible (false);
    if (pianoRollResizer != nullptr)
        pianoRollResizer->setVisible (false);
    pianoRoll.setClipId (0);
}

void ReasonMainComponent::updateVerticalScrollBar()
{
    const int contentHeight = timeline.getContentHeight();
    const int visibleHeight = timeline.getVisibleTrackHeight();
    const int maxOffset = juce::jmax (0, contentHeight - visibleHeight);
    const int clampedOffset = juce::jlimit (0, maxOffset, trackScrollOffset);

    ignoreVerticalScrollCallback = true;
    verticalScrollBar.setRangeLimits (0.0, (double) juce::jmax (1, maxOffset));
    verticalScrollBar.setCurrentRange ((double) clampedOffset, (double) juce::jmax (1, visibleHeight));
    ignoreVerticalScrollCallback = false;

    trackList.setVisibleTrackHeightOverride (visibleHeight);
    handleTrackScroll (clampedOffset);
}

void ReasonMainComponent::handleTrackScroll (int newOffset)
{
    const int maxOffset = juce::jmax (0, timeline.getContentHeight() - timeline.getVisibleTrackHeight());
    trackScrollOffset = juce::jlimit (0, maxOffset, newOffset);
    timeline.setScrollOffset (trackScrollOffset);
    trackList.setScrollOffset (trackScrollOffset);

    ignoreVerticalScrollCallback = true;
    verticalScrollBar.setCurrentRange ((double) trackScrollOffset,
                                       (double) juce::jmax (1, timeline.getVisibleTrackHeight()));
    ignoreVerticalScrollCallback = false;
}

void ReasonMainComponent::showChordTrackMenu (int trackIndex, juce::Component* source)
{
    trackList.setSelectedIndex (trackIndex);
    session.setSelectedTrack (trackIndex);
    refreshChordInspector();

    juce::PopupMenu menu;
    auto labels = session.getChordLabelsForTrack (trackIndex);
    if (labels.empty())
    {
        menu.addItem ("No chords found", false, false, nullptr);
    }
    else
    {
        for (const auto& label : labels)
        {
            const auto timeText = session.getBarsAndBeatsText (label.startSeconds);
            menu.addItem (timeText + "  " + label.symbol, false, false, nullptr);
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (source));
}

void ReasonMainComponent::setMainUiVisible (bool shouldBeVisible)
{
    transportBar.setVisible (shouldBeVisible);
    trackList.setVisible (shouldBeVisible);
    timeline.setVisible (shouldBeVisible);
    chordInspectorToggleButton.setVisible (shouldBeVisible);
    chordInspector.setVisible (shouldBeVisible && ! chordInspectorCollapsed);
    fxInspector.setVisible (shouldBeVisible && fxInspectorVisible);
    verticalScrollBar.setVisible (shouldBeVisible);
    trackListResizer->setVisible (shouldBeVisible);
    inspectorResizer->setVisible (shouldBeVisible);

    const bool shouldShowPiano = shouldBeVisible && pianoRollVisible;
    pianoRoll.setVisible (shouldShowPiano);
    if (pianoRollResizer != nullptr)
        pianoRollResizer->setVisible (shouldShowPiano);
}

void ReasonMainComponent::updateChordToggleButton()
{
    chordInspectorToggleButton.setButtonText (chordInspectorCollapsed ? "Chords >" : "Chords v");
}

void ReasonMainComponent::dismissStartupOverlay()
{
    if (startupOverlay == nullptr)
        return;

    setMainUiVisible (true);
    startupOverlay->setVisible (false);
    grabKeyboardFocus();
    refreshSessionState();
    resized();
}

bool ReasonMainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() > 0;
}

void ReasonMainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty())
        return;

    const juce::Point<int> dropPos (x, y);
    int trackIndex = session.getSelectedTrack();
    double startTime = session.getInsertionTimeSeconds();

    if (timeline.getBounds().contains (dropPos))
    {
        const auto local = dropPos - timeline.getPosition();
        const int hovered = timeline.getTrackIndexAtY (local.y);
        if (hovered >= 0)
            trackIndex = hovered;
        startTime = timeline.getTimeAtX (local.x);
    }
    else if (trackList.getBounds().contains (dropPos))
    {
        const auto local = dropPos - trackList.getPosition();
        const int hovered = trackList.getTrackIndexAtY (local.y);
        if (hovered >= 0)
            trackIndex = hovered;
    }

    session.setSelectedTrack (trackIndex);
    trackList.setSelectedIndex (trackIndex);

    auto& formatManager = session.getEngine().getAudioFileFormatManager().readFormatManager;

    for (const auto& path : files)
    {
        juce::File file (path);
        if (! file.existsAsFile())
            continue;

        const auto ext = file.getFileExtension().toLowerCase();
        const auto extNoDot = ext.startsWithChar ('.') ? ext.substring (1) : ext;
        const bool isMidi = (ext == ".mid" || ext == ".midi");
        bool success = false;

        if (isMidi)
        {
            success = session.importMidi (trackIndex, file, startTime);
        }
        else if (formatManager.findFormatForFileExtension (extNoDot) != nullptr)
        {
            success = session.importAudio (trackIndex, file, startTime);
        }
        else
        {
            success = session.importAudio (trackIndex, file, startTime);
            if (! success)
                success = session.importMidi (trackIndex, file, startTime);
        }

        if (success)
            timeline.repaint();
    }
}
