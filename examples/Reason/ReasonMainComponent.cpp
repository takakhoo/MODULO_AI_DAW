#include "ReasonMainComponent.h"

ReasonMainComponent::ReasonMainComponent()
{
    addAndMakeVisible (transportBar);
    addAndMakeVisible (trackList);
    addAndMakeVisible (timeline);

    timeline.setSession (&session);
    timeline.setRowHeight (trackList.getRowHeight());

    const auto trackNames = session.getTrackNames();
    trackList.setTracks (trackNames);
    trackList.setTrackVolumes (session.getTrackVolumes());
    trackList.setSelectedIndex (session.getSelectedTrack());

    transportBar.setTempoText ("Tempo: --");

    transportBar.onPlay = [this]
    {
        session.togglePlay();
    };

    transportBar.onStop = [this]
    {
        session.stop();
    };

    transportBar.onImport = [this]
    {
        showImportMenu();
    };

    transportBar.onSettings = [this]
    {
        session.showAudioSettings();
    };

    trackList.onSelectionChanged = [this] (int index)
    {
        session.setSelectedTrack (index);
    };

    trackList.onVolumeChanged = [this] (int index, float value)
    {
        session.setTrackVolume (index, value);
    };

    timeline.onTrackSelected = [this] (int index)
    {
        trackList.setSelectedIndex (index);
    };

    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    setWantsKeyboardFocus (true);
    grabKeyboardFocus();
    setSize (1100, 700);
    startTimerHz (30);
}

void ReasonMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF18191C));
}

void ReasonMainComponent::resized()
{
    auto r = getLocalBounds();

    auto transportArea = r.removeFromTop (52);
    transportBar.setBounds (transportArea);

    auto trackListArea = r.removeFromLeft (260);
    trackList.setBounds (trackListArea);

    timeline.setBounds (r);
}

bool ReasonMainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        session.togglePlay();
        return true;
    }

    const juce::juce_wchar keyChar = key.getTextCharacter();
    if (keyChar == 'i' || keyChar == 'I')
    {
        timeline.addIdeaMarker (session.getInsertionTimeSeconds());
        return true;
    }

    return false;
}

void ReasonMainComponent::timerCallback()
{
    updateTimeDisplay();
}

void ReasonMainComponent::updateTimeDisplay()
{
    const double timeSeconds = session.getCurrentTimeSeconds();
    const int minutes = (int) (timeSeconds / 60.0);
    const double seconds = timeSeconds - (minutes * 60.0);

    transportBar.setTimeText (juce::String::formatted ("%02d:%04.1f", minutes, seconds));
}

void ReasonMainComponent::showImportMenu()
{
    juce::PopupMenu menu;
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
