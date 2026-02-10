#include "SessionController.h"

#include <algorithm>
#include <cmath>
#include "PluginWindow.h"


namespace
{
constexpr float kMinTrackDb = -10.0f;
constexpr float kMaxTrackDb = 6.0f;
const juce::Identifier kInstrumentLabelId ("reasonInstrumentLabel");
constexpr double kMaxReasonableClipLengthSeconds = 60.0 * 20.0; // 20 minutes

bool isPlaceholderProgramName (const juce::String& name)
{
    auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return true;

    const auto lower = trimmed.toLowerCase();
    if (lower.contains ("untitled"))
        return true;
    if (lower == "default" || lower.startsWith ("default"))
        return true;
    if (lower == "init" || lower.startsWith ("init"))
        return true;
    if (lower == "program" || lower.startsWith ("program"))
        return true;

    return false;
}

juce::String fallbackExternalInstrumentName (const te::ExternalPlugin& external)
{
    auto candidate = external.desc.name.trim();
    if (! isPlaceholderProgramName (candidate))
        return candidate;

    candidate = external.desc.manufacturerName.trim();
    if (candidate.isNotEmpty() && ! isPlaceholderProgramName (candidate))
        return candidate + " Instrument";

    candidate = external.desc.pluginFormatName.trim();
    if (candidate.isNotEmpty() && ! isPlaceholderProgramName (candidate))
        return candidate + " Instrument";

    return {};
}

juce::String bestLabelForDescription (const juce::PluginDescription& desc)
{
    auto candidate = desc.name.trim();
    if (! isPlaceholderProgramName (candidate))
        return candidate;

    candidate = desc.manufacturerName.trim();
    if (candidate.isNotEmpty() && ! isPlaceholderProgramName (candidate))
        return candidate + " Instrument";

    candidate = desc.pluginFormatName.trim();
    if (candidate.isNotEmpty() && ! isPlaceholderProgramName (candidate))
        return candidate + " Instrument";

    candidate = desc.category.trim();
    if (candidate.isNotEmpty() && ! isPlaceholderProgramName (candidate))
        return candidate;

    return {};
}

juce::String boolToText (bool v) { return v ? "true" : "false"; }

double getBeatsPerBarAtTime (te::Edit& edit, te::TimePosition time)
{
    const auto sigText = edit.tempoSequence.getTimeSigAt (time).getStringTimeSig();
    auto parts = juce::StringArray::fromTokens (sigText, "/", "");
    if (parts.size() >= 2)
    {
        const int numerator = juce::jmax (1, parts[0].getIntValue());
        const int denominator = juce::jmax (1, parts[1].getIntValue());
        return numerator * (4.0 / (double) denominator);
    }

    return 4.0;
}

void enforceChordSustainEveryBar (te::Edit& edit, te::MidiClip& clip, juce::UndoManager& undo)
{
    auto& seq = clip.getSequence();
    std::vector<te::MidiControllerEvent*> sustainEvents;
    for (auto* c : seq.getControllerEvents())
    {
        if (c != nullptr && c->getType() == 64)
            sustainEvents.push_back (c);
    }
    for (auto* c : sustainEvents)
        seq.removeControllerEvent (*c, &undo);

    const auto clipStart = clip.getPosition().getStart();
    const auto clipRange = clip.getEditTimeRange();
    const auto clipStartBeatAbs = te::toBeats (clipStart, edit.tempoSequence);
    const auto clipEndBeatAbs = te::toBeats (clipRange.getEnd(), edit.tempoSequence);
    const double clipLengthBeats = juce::jmax (0.0, (clipEndBeatAbs - clipStartBeatAbs).inBeats());
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAtTime (edit, clipStart));
    const double liftBeforeBarEndBeats = 0.25;
    const int sustainOn = 127 << 7;
    const int sustainOff = 0;

    for (double barStart = 0.0; barStart < clipLengthBeats; barStart += beatsPerBar)
    {
        const double barEnd = juce::jmin (clipLengthBeats, barStart + beatsPerBar);
        const double offBeat = juce::jmax (barStart + 0.0625, barEnd - liftBeforeBarEndBeats);
        seq.addControllerEvent (te::BeatPosition::fromBeats (barStart), 64, sustainOn, &undo);
        seq.addControllerEvent (te::BeatPosition::fromBeats (offBeat), 64, sustainOff, &undo);
    }
}

bool parseBarBeatText (const juce::String& text, int& outBar, int& outBeat)
{
    auto digits = text.retainCharacters ("0123456789|");
    auto parts = juce::StringArray::fromTokens (digits, "|", "");
    if (parts.size() < 2)
        return false;
    outBar = parts[0].getIntValue();
    outBeat = parts[1].getIntValue();
    return outBar > 0 && outBeat > 0;
}

juce::Array<int> chordSymbolToPitches (juce::String symbol)
{
    symbol = symbol.trim();
    if (symbol.isEmpty())
        return {};

    auto upper = symbol.toUpperCase();
    const juce::String rootText = upper.substring (0, upper.length() >= 2 && (upper[1] == '#' || upper[1] == 'B') ? 2 : 1);
    juce::String qualityText = symbol.substring (rootText.length()).toLowerCase().trim();

    int rootSemitone = 0;
    if (rootText == "C") rootSemitone = 0;
    else if (rootText == "C#" || rootText == "DB") rootSemitone = 1;
    else if (rootText == "D") rootSemitone = 2;
    else if (rootText == "D#" || rootText == "EB") rootSemitone = 3;
    else if (rootText == "E" || rootText == "FB") rootSemitone = 4;
    else if (rootText == "F" || rootText == "E#") rootSemitone = 5;
    else if (rootText == "F#" || rootText == "GB") rootSemitone = 6;
    else if (rootText == "G") rootSemitone = 7;
    else if (rootText == "G#" || rootText == "AB") rootSemitone = 8;
    else if (rootText == "A") rootSemitone = 9;
    else if (rootText == "A#" || rootText == "BB") rootSemitone = 10;
    else if (rootText == "B" || rootText == "CB") rootSemitone = 11;

    juce::Array<int> intervals;
    if (qualityText.startsWith ("dim"))
        intervals.addArray ({ 0, 3, 6 });
    else if (qualityText.startsWith ("aug"))
        intervals.addArray ({ 0, 4, 8 });
    else if (qualityText.startsWith ("sus2"))
        intervals.addArray ({ 0, 2, 7 });
    else if (qualityText.startsWith ("sus4") || qualityText.startsWith ("sus"))
        intervals.addArray ({ 0, 5, 7 });
    else if (qualityText.startsWith ("maj"))
        intervals.addArray ({ 0, 4, 7 });
    else if (qualityText.startsWith ("m"))
        intervals.addArray ({ 0, 3, 7 });
    else
        intervals.addArray ({ 0, 4, 7 });

    if (qualityText.contains ("maj7"))
        intervals.add (11);
    else if (qualityText.contains ("7"))
        intervals.add (10);

    // Keep generated chords in a musical mid register.
    const int baseMidi = 48 + rootSemitone; // C3-based root
    juce::Array<int> pitches;
    for (auto i : intervals)
        pitches.add (baseMidi + i);
    pitches.sort();
    return pitches;
}

te::SettingID getKnownPluginListSettingId()
{
   #if JUCE_64BIT
    return te::SettingID::knownPluginList64;
   #else
    return te::SettingID::knownPluginList;
   #endif
}

void migrateLegacyReasonPluginListIfNeeded (te::Engine& engine)
{
    auto& pluginManager = engine.getPluginManager();
    auto hasExternalPlugins = [] (const juce::Array<juce::PluginDescription>& types)
    {
        for (const auto& type : types)
        {
            const auto format = type.pluginFormatName;
            if (format.equalsIgnoreCase ("AudioUnit")
                || format.equalsIgnoreCase ("VST3")
                || format.equalsIgnoreCase ("VST"))
                return true;
        }
        return false;
    };

    if (hasExternalPlugins (pluginManager.knownPluginList.getTypes()))
        return;

    const auto legacySettings = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                    .getChildFile ("Reason")
                                    .getChildFile ("Settings.xml");
    if (! legacySettings.existsAsFile())
        return;

    juce::PropertiesFile::Options options;
    options.millisecondsBeforeSaving = 0;
    options.storageFormat = juce::PropertiesFile::storeAsXML;

    juce::PropertiesFile legacyProps (legacySettings, options);

    const auto key = getKnownPluginListSettingId();
    std::unique_ptr<juce::XmlElement> legacyListXml (legacyProps.getXmlValue (te::PropertyStorage::settingToString (key)));
    if (legacyListXml == nullptr)
        return;

    juce::KnownPluginList migratedList;
    migratedList.recreateFromXml (*legacyListXml);
    if (! hasExternalPlugins (migratedList.getTypes()))
        return;

    pluginManager.knownPluginList.recreateFromXml (*legacyListXml);
    engine.getPropertyStorage().setXmlProperty (key, *legacyListXml);
    engine.getPropertyStorage().getPropertiesFile().saveIfNeeded();
}

juce::File getRecordDebugFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                   .getChildFile ("ReasonLogs");
    dir.createDirectory();
    return dir.getChildFile ("record_debug.log");
}

void logRecordDebug (const juce::String& message)
{
    const auto line = "[" + juce::Time::getCurrentTime().toString (true, true, true, true) + "] " + message;
    DBG (line);

    [[ maybe_unused ]] const bool ok = getRecordDebugFile().appendText (line + "\n", false, false, "\n");
}

juce::String summariseTrackArmed (const std::vector<bool>& trackArmed)
{
    juce::StringArray parts;
    for (size_t i = 0; i < trackArmed.size(); ++i)
        parts.add ("T" + juce::String ((int) i) + "=" + boolToText (trackArmed[i]));
    return parts.joinIntoString (", ");
}
}

std::unique_ptr<juce::Component> ExtendedUIBehaviour::createPluginWindow (te::PluginWindowState& state)
{
    return createReasonPluginWindow (state);
}

void ExtendedUIBehaviour::recreatePluginWindowContentAsync (te::Plugin& plugin)
{
    recreateReasonPluginWindowContentAsync (plugin);
}

SessionController::SessionController()
{
    migrateLegacyReasonPluginListIfNeeded (engine);

    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Reason");
    tempDir.createDirectory();
    auto editFile = tempDir.getNonexistentChildFile ("Untitled", ".tracktionedit", false);
    logRecordDebug ("SessionController ctor. Log file: " + getRecordDebugFile().getFullPathName());
    createNewEdit (editFile);
}

void SessionController::togglePlay()
{
    if (edit != nullptr)
        EngineHelpers::togglePlay (*edit);
}

void SessionController::stop()
{
    if (transport == nullptr)
        return;

    logRecordDebug ("stop() called. isRecording=" + boolToText (transport->isRecording()));
    const bool wasRecording = transport->isRecording();
    const auto previewSnapshot = recordingPreview;
    juce::Array<int> midiClipCountsBefore;
    if (edit != nullptr)
    {
        auto tracks = te::getAudioTracks (*edit);
        for (int i = 0; i < tracks.size(); ++i)
        {
            int midiClips = 0;
            for (auto* clip : tracks.getUnchecked (i)->getClips())
                if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                    ++midiClips;
            midiClipCountsBefore.add (midiClips);
        }
    }

    transport->stop (false, false);

    if (wasRecording)
    {
        commitRecordingPreviewFallback (previewSnapshot, midiClipCountsBefore, transport->getPosition().inSeconds());
        recordingPreview.clear();
    }

    if (edit != nullptr)
    {
        auto tracks = te::getAudioTracks (*edit);
        juce::StringArray perTrack;
        for (int i = 0; i < tracks.size(); ++i)
        {
            int midiClips = 0;
            for (auto* clip : tracks.getUnchecked (i)->getClips())
                if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                    ++midiClips;
            perTrack.add ("Track" + juce::String (i) + ":midiClips=" + juce::String (midiClips));
        }
        logRecordDebug ("stop() finished. " + perTrack.joinIntoString (" | "));
    }

    cursorTimeSeconds = transport->getPosition().inSeconds();
}

bool SessionController::toggleRecord()
{
    if (edit == nullptr || transport == nullptr)
        return false;

    const bool wasRecording = transport->isRecording();
    logRecordDebug ("toggleRecord() called. wasRecording=" + boolToText (wasRecording)
                    + ", selectedTrack=" + juce::String (selectedTrackIndex)
                    + ", selectedMidiDeviceId=" + selectedMidiDeviceId
                    + ", armed={" + summariseTrackArmed (trackArmed) + "}");

    if (! wasRecording)
    {
        if (! prepareMidiRecording())
        {
            logRecordDebug ("toggleRecord() start aborted: prepareMidiRecording failed");
            return false;
        }

        transport->setPosition (te::TimePosition::fromSeconds (cursorTimeSeconds));
        lastRecordStartSeconds = transport->getPosition().inSeconds();
        transport->record (false);
        logRecordDebug ("toggleRecord() started. isRecording=" + boolToText (transport->isRecording()));
    }
    else
    {
        const auto previewSnapshot = recordingPreview;
        juce::Array<int> midiClipCountsBefore;
        if (edit != nullptr)
        {
            auto tracks = te::getAudioTracks (*edit);
            for (int i = 0; i < tracks.size(); ++i)
            {
                int midiClips = 0;
                for (auto* clip : tracks.getUnchecked (i)->getClips())
                    if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                        ++midiClips;
                midiClipCountsBefore.add (midiClips);
            }
        }

        transport->stop (false, false);
        commitRecordingPreviewFallback (previewSnapshot, midiClipCountsBefore, transport->getPosition().inSeconds());
        recordingPreview.clear();
        logRecordDebug ("toggleRecord() stopped. isRecording=" + boolToText (transport->isRecording()));
        if (edit != nullptr)
        {
            auto tracks = te::getAudioTracks (*edit);
            juce::StringArray perTrack;
            for (int i = 0; i < tracks.size(); ++i)
            {
                int midiClips = 0;
                for (auto* clip : tracks.getUnchecked (i)->getClips())
                    if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                        ++midiClips;
                perTrack.add ("Track" + juce::String (i) + ":midiClips=" + juce::String (midiClips));
            }
            logRecordDebug ("toggleRecord() clip snapshot after stop: " + perTrack.joinIntoString (" | "));
        }
    }

    return transport->isRecording();
}

bool SessionController::isRecording() const
{
    return transport != nullptr && transport->isRecording();
}

bool SessionController::createNewEdit (const juce::File& editFile)
{
    if (editFile == juce::File())
        return false;

    auto newEdit = te::createEmptyEdit (engine, editFile);
    if (newEdit == nullptr)
        return false;

    newEdit->playInStopEnabled = true;
    newEdit->ensureNumberOfAudioTracks (3);

    edit = std::move (newEdit);
    transport = &edit->getTransport();
    transport->stop (false, false);
    transport->setPosition (te::TimePosition());
    transport->ensureContextAllocated();

    cursorTimeSeconds = 0.0;
    selectedTrackIndex = 0;
    updateTrackNames();
    ensureTrackStateSize();
    if (! trackArmed.empty())
        trackArmed[0] = true;
    ensureMidiInputSelection();
    configureMidiInputRoutingForLivePlay();
    recordingPreview.clear();
    insertDefaultInstrumentIfAvailable (0);
    te::EditFileOperations (*edit).save (true, true, false);
    return true;
}

bool SessionController::openEdit (const juce::File& editFile)
{
    if (! editFile.existsAsFile())
        return false;

    auto newEdit = te::loadEditFromFile (engine, editFile);
    if (newEdit == nullptr)
        return false;

    newEdit->playInStopEnabled = true;

    edit = std::move (newEdit);
    transport = &edit->getTransport();
    transport->stop (false, false);
    transport->setPosition (te::TimePosition());
    transport->ensureContextAllocated();

    cursorTimeSeconds = 0.0;
    selectedTrackIndex = 0;
    updateTrackNames();
    ensureTrackStateSize();
    if (! trackArmed.empty())
        trackArmed[0] = true;
    ensureMidiInputSelection();
    configureMidiInputRoutingForLivePlay();
    recordingPreview.clear();
    return true;
}

bool SessionController::saveEdit()
{
    if (edit == nullptr)
        return false;

    return te::EditFileOperations (*edit).save (true, true, false);
}

bool SessionController::saveEditAs (const juce::File& editFile)
{
    if (edit == nullptr || editFile == juce::File())
        return false;

    return te::EditFileOperations (*edit).saveAs (editFile, true);
}

void SessionController::setSelectedTrack (int index)
{
    const int count = getTrackCount();
    selectedTrackIndex = juce::jlimit (0, juce::jmax (0, count - 1), index);

    ensureTrackStateSize();
    for (size_t i = 0; i < trackArmed.size(); ++i)
        trackArmed[i] = false;

    if (selectedTrackIndex >= 0 && selectedTrackIndex < (int) trackArmed.size())
        trackArmed[(size_t) selectedTrackIndex] = true;

    configureMidiInputRoutingForLivePlay();
}

void SessionController::setTrackArmed (int index, bool shouldArm)
{
    if (index < 0)
        return;

    ensureTrackStateSize();
    if (index >= (int) trackArmed.size())
        return;

    trackArmed[(size_t) index] = shouldArm;
    configureMidiInputRoutingForLivePlay();
}

bool SessionController::isTrackArmed (int index) const
{
    if (index < 0 || index >= (int) trackArmed.size())
        return false;

    return trackArmed[(size_t) index];
}

juce::Array<bool> SessionController::getTrackArmedStates() const
{
    juce::Array<bool> states;
    for (auto armed : trackArmed)
        states.add (armed);
    return states;
}

juce::StringArray SessionController::getMidiInputDeviceNames() const
{
    juce::StringArray names;
    auto devices = engine.getDeviceManager().getMidiInDevices();
    for (auto& device : devices)
        names.add (device->getName());
    return names;
}

int SessionController::getSelectedMidiInputIndex() const
{
    if (selectedMidiDeviceId.isEmpty())
        return -1;

    auto devices = engine.getDeviceManager().getMidiInDevices();
    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i]->getDeviceID() == selectedMidiDeviceId)
            return (int) i;

    return -1;
}

juce::String SessionController::getSelectedMidiInputName() const
{
    auto devices = engine.getDeviceManager().getMidiInDevices();
    for (auto& device : devices)
        if (device->getDeviceID() == selectedMidiDeviceId)
            return device->getName();

    if (! devices.empty())
        return devices.front()->getName();

    return "No MIDI Input";
}

void SessionController::setSelectedMidiInputIndex (int index)
{
    auto devices = engine.getDeviceManager().getMidiInDevices();
    if (index < 0 || index >= (int) devices.size())
    {
        selectedMidiDeviceId.clear();
        return;
    }

    selectedMidiDeviceId = devices[(size_t) index]->getDeviceID();
    configureMidiInputRoutingForLivePlay();
}

void SessionController::ensureMidiInputSelection()
{
    auto devices = engine.getDeviceManager().getMidiInDevices();
    if (devices.empty())
    {
        selectedMidiDeviceId.clear();
        return;
    }

    if (selectedMidiDeviceId.isEmpty())
    {
        selectedMidiDeviceId = devices.front()->getDeviceID();
        return;
    }

    for (auto& device : devices)
    {
        if (device->getDeviceID() == selectedMidiDeviceId)
            return;
    }

    selectedMidiDeviceId = devices.front()->getDeviceID();
}

void SessionController::refreshMidiLiveRouting()
{
    configureMidiInputRoutingForLivePlay();
}

int SessionController::getTrackCount() const
{
    if (edit == nullptr)
        return 0;

    return te::getAudioTracks (*edit).size();
}

juce::StringArray SessionController::getTrackNames() const
{
    juce::StringArray names;
    if (edit == nullptr)
        return names;

    auto tracks = te::getAudioTracks (*edit);

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks.getUnchecked (i);
        auto name = track->getName();

        if (name.isEmpty())
            name = "Track " + juce::String (i + 1);

        names.add (name);
    }

    return names;
}

juce::StringArray SessionController::getTrackInstrumentNames() const
{
    juce::StringArray names;
    if (edit == nullptr)
        return names;

    auto tracks = te::getAudioTracks (*edit);
    for (int i = 0; i < tracks.size(); ++i)
    {
        juce::String instrument = "No Instrument";
        auto* track = tracks.getUnchecked (i);
        const auto trackName = track->getName().trim();
        auto storedLabel = track->state.getProperty (kInstrumentLabelId).toString().trim();
        if (storedLabel.isNotEmpty() && (isPlaceholderProgramName (storedLabel)
                                         || storedLabel.equalsIgnoreCase (trackName)))
            storedLabel.clear();
        for (auto* plugin : track->pluginList)
        {
            if (plugin != nullptr && isInstrumentPlugin (*plugin))
            {
                juce::String display;
                if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin))
                {
                    auto programName = external->getCurrentProgramName();
                    if (! isPlaceholderProgramName (programName))
                        display = programName.trim();

                    if (isPlaceholderProgramName (display))
                        display = fallbackExternalInstrumentName (*external);

                    if (isPlaceholderProgramName (display))
                    {
                        auto pluginName = external->getName().trim();
                        if (! isPlaceholderProgramName (pluginName))
                            display = pluginName;
                    }
                }
                else
                {
                    display = plugin->getName();
                }

                if (display.equalsIgnoreCase (trackName))
                    display.clear();

                if (isPlaceholderProgramName (display) && storedLabel.isNotEmpty())
                    display = storedLabel;

                if (isPlaceholderProgramName (display))
                    display = plugin->getPluginType();

                if (isPlaceholderProgramName (display))
                    display = "Instrument";

                instrument = display;
                break;
            }
        }
        names.add (instrument);
    }

    return names;
}

juce::Array<int> SessionController::getTrackEffectCounts() const
{
    juce::Array<int> counts;
    if (edit == nullptr)
        return counts;

    auto tracks = te::getAudioTracks (*edit);
    for (int i = 0; i < tracks.size(); ++i)
    {
        int count = 0;
        auto* track = tracks.getUnchecked (i);
        for (auto* plugin : track->pluginList)
        {
            if (plugin == nullptr)
                continue;

            if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr)
                continue;

            if (isInstrumentPlugin (*plugin))
                continue;

            ++count;
        }
        counts.add (count);
    }

    return counts;
}

juce::Array<bool> SessionController::getTrackMuteStates() const
{
    juce::Array<bool> states;
    if (edit == nullptr)
        return states;

    auto tracks = te::getAudioTracks (*edit);
    for (auto* track : tracks)
        states.add (track != nullptr && track->isMuted (false));

    return states;
}

juce::Array<bool> SessionController::getTrackSoloStates() const
{
    juce::Array<bool> states;
    if (edit == nullptr)
        return states;

    auto tracks = te::getAudioTracks (*edit);
    for (auto* track : tracks)
        states.add (track != nullptr && track->isSolo (false));

    return states;
}

juce::String SessionController::getTrackName (int index) const
{
    if (auto* track = getAudioTrack (index))
    {
        auto name = track->getName();
        if (name.isEmpty())
            name = "Track " + juce::String (index + 1);
        return name;
    }

    return {};
}

juce::Array<float> SessionController::getTrackVolumes() const
{
    juce::Array<float> volumes;
    if (edit == nullptr)
        return volumes;

    auto tracks = te::getAudioTracks (*edit);

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks.getUnchecked (i);
        if (auto* volume = track->getVolumePlugin())
        {
            const float db = volume->getVolumeDb();
            const float normalized = (db - kMinTrackDb) / (kMaxTrackDb - kMinTrackDb);
            volumes.add (juce::jlimit (0.0f, 1.0f, normalized));
        }
        else
        {
            volumes.add (0.8f);
        }
    }

    return volumes;
}

juce::Array<float> SessionController::getTrackPans() const
{
    juce::Array<float> pans;
    if (edit == nullptr)
        return pans;

    auto tracks = te::getAudioTracks (*edit);
    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks.getUnchecked (i);
        if (auto* volume = track->getVolumePlugin())
            pans.add (juce::jlimit (-1.0f, 1.0f, volume->getPan()));
        else
            pans.add (0.0f);
    }

    return pans;
}

void SessionController::setTrackVolume (int index, float normalizedValue)
{
    if (auto* track = getAudioTrack (index))
    {
        if (auto* volume = track->getVolumePlugin())
        {
            const float norm = juce::jlimit (0.0f, 1.0f, normalizedValue);
            const float db = kMinTrackDb + (kMaxTrackDb - kMinTrackDb) * norm;
            volume->setVolumeDb (db);
        }
    }
}

void SessionController::setTrackPan (int index, float panValue)
{
    if (auto* track = getAudioTrack (index))
    {
        if (auto* volume = track->getVolumePlugin())
            volume->setPan (juce::jlimit (-1.0f, 1.0f, panValue));
    }
}

float SessionController::getTrackVolume (int index) const
{
    if (auto* track = getAudioTrack (index))
    {
        if (auto* volume = track->getVolumePlugin())
        {
            const float db = volume->getVolumeDb();
            const float normalized = (db - kMinTrackDb) / (kMaxTrackDb - kMinTrackDb);
            return juce::jlimit (0.0f, 1.0f, normalized);
        }
    }

    return 0.8f;
}

void SessionController::setTrackMute (int index, bool shouldMute)
{
    if (auto* track = getAudioTrack (index))
        track->setMute (shouldMute);
}

void SessionController::setTrackSolo (int index, bool shouldSolo)
{
    if (auto* track = getAudioTrack (index))
        track->setSolo (shouldSolo);
}

void SessionController::setTrackName (int index, const juce::String& name)
{
    if (auto* track = getAudioTrack (index))
    {
        auto trimmed = name.trim();
        if (trimmed.isEmpty())
            trimmed = "Track " + juce::String (index + 1);
        track->setName (trimmed);
    }
}

double SessionController::getCurrentTimeSeconds() const
{
    if (transport == nullptr)
        return 0.0;

    return transport->getPosition().inSeconds();
}

double SessionController::getTempoBpm() const
{
    if (edit == nullptr)
        return 120.0;

    const auto& tempo = edit->tempoSequence.getTempoAt (te::TimePosition::fromSeconds (0.0));
    return tempo.getBpm();
}

void SessionController::setTempoBpm (double bpm)
{
    if (edit == nullptr)
        return;

    const double clamped = juce::jlimit (20.0, 300.0, bpm);
    auto& tempo = edit->tempoSequence.getTempoAt (te::TimePosition::fromSeconds (0.0));
    tempo.setBpm (clamped);
}

juce::String SessionController::getTimeSignature() const
{
    if (edit == nullptr)
        return "4/4";

    auto& timeSig = edit->tempoSequence.getTimeSigAt (te::TimePosition::fromSeconds (0.0));
    return timeSig.getStringTimeSig();
}

bool SessionController::setTimeSignature (const juce::String& text)
{
    if (edit == nullptr)
        return false;

    auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    auto& timeSig = edit->tempoSequence.getTimeSigAt (te::TimePosition::fromSeconds (0.0));
    timeSig.setStringTimeSig (trimmed);
    return true;
}

juce::String SessionController::getKeySignature() const
{
    if (edit == nullptr)
        return "Cmaj";

    auto& pitch = edit->pitchSequence.getPitchAt (te::TimePosition::fromSeconds (cursorTimeSeconds));
    const int root = pitch.getPitch();
    const auto scale = pitch.getScale();
    auto& mutableEngine = const_cast<te::Engine&> (engine);
    const auto rootName = te::Pitch::getPitchAsString (mutableEngine, root);
    const auto scaleName = te::Scale::getShortNameForType (scale);
    return rootName + scaleName;
}

bool SessionController::setKeySignature (const juce::String& text)
{
    if (edit == nullptr)
        return false;

    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    auto lower = trimmed.toLowerCase();
    const bool isMinor = lower.contains ("min");

    auto pitchName = trimmed;
    pitchName = pitchName.retainCharacters ("ABCDEFGabcdefg#b");
    if (pitchName.isEmpty())
        return false;

    const int pitchValue = te::Pitch::getPitchFromString (engine, pitchName);
    const auto scaleType = isMinor ? te::Scale::minor : te::Scale::major;

    auto& pitchSetting = edit->pitchSequence.getPitchAt (te::TimePosition::fromSeconds (0.0));
    pitchSetting.setPitch (pitchValue);
    pitchSetting.setScaleID (scaleType);
    return true;
}

bool SessionController::isMetronomeEnabled() const
{
    return edit != nullptr && edit->clickTrackEnabled;
}

void SessionController::setMetronomeEnabled (bool enabled)
{
    if (edit == nullptr)
        return;

    edit->clickTrackEnabled = enabled;
}

juce::String SessionController::getBarsAndBeatsText (double seconds) const
{
    if (edit == nullptr)
        return "Bar 1 | Beat 1";

    const auto barsBeats = edit->tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (seconds));
    const int bar = barsBeats.bars + 1;
    const int beat = barsBeats.getWholeBeats() + 1;
    return "Bar " + juce::String (bar) + " | Beat " + juce::String (beat);
}

bool SessionController::undo()
{
    if (edit == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    if (! undoManager.canUndo())
        return false;

    undoManager.undo();
    updateTrackNames();
    ensureTrackStateSize();
    return true;
}

bool SessionController::redo()
{
    if (edit == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    if (! undoManager.canRedo())
        return false;

    undoManager.redo();
    updateTrackNames();
    ensureTrackStateSize();
    return true;
}

void SessionController::setCursorTimeSeconds (double seconds)
{
    cursorTimeSeconds = juce::jmax (0.0, seconds);
    if (transport != nullptr)
        transport->setPosition (te::TimePosition::fromSeconds (cursorTimeSeconds));
}

bool SessionController::importAudio (int trackIndex, const juce::File& file, double startTimeSeconds)
{
    if (edit == nullptr)
        return false;

    if (! file.existsAsFile())
        return false;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return false;

    te::AudioFile audioFile (engine, file);
    if (! audioFile.isValid())
        return false;

    const auto length = te::TimeDuration::fromSeconds (audioFile.getLength());
    const auto start = te::TimePosition::fromSeconds (startTimeSeconds);

    auto clip = track->insertWaveClip (file.getFileNameWithoutExtension(), file,
                                       { { start, length }, {} }, false);
    if (clip == nullptr)
        return false;

    cursorTimeSeconds = startTimeSeconds;
    return true;
}

bool SessionController::importMidi (int trackIndex, const juce::File& file, double startTimeSeconds)
{
    if (edit == nullptr)
        return false;

    if (! file.existsAsFile())
        return false;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return false;

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return false;

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
        return false;

    midiFile.convertTimestampTicksToSeconds();

    juce::MidiMessageSequence sequence;
    for (int i = 0; i < midiFile.getNumTracks(); ++i)
    {
        if (auto* trackSeq = midiFile.getTrack (i))
            sequence.addSequence (*trackSeq, 0.0, 0.0, trackSeq->getEndTime());
    }

    sequence.updateMatchedPairs();

    double lengthSeconds = sequence.getEndTime();
    if (lengthSeconds <= 0.01)
        lengthSeconds = 1.0;

    const auto start = te::TimePosition::fromSeconds (startTimeSeconds);
    const auto length = te::TimeDuration::fromSeconds (lengthSeconds);

    auto clip = track->insertMIDIClip (file.getFileNameWithoutExtension(), { start, length }, nullptr);
    if (clip == nullptr)
        return false;

    te::mergeInMidiSequence (*clip, sequence, te::TimeDuration::fromSeconds (startTimeSeconds),
                             te::MidiList::NoteAutomationType::none);

    clip->setMidiChannel (te::MidiChannel (1));

    cursorTimeSeconds = startTimeSeconds;
    return true;
}

std::vector<SessionController::ClipInfo> SessionController::getClipsForTrack (int trackIndex) const
{
    std::vector<ClipInfo> clips;

    if (edit == nullptr)
        return clips;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return clips;

    for (auto* clip : track->getClips())
    {
        if (clip == nullptr)
            continue;

        ClipInfo info;
        info.id = clip->itemID.getRawID();
        info.trackIndex = trackIndex;
        info.name = clip->getName();

        auto range = clip->getEditTimeRange();
        info.startSeconds = range.getStart().inSeconds();
        info.lengthSeconds = range.getLength().inSeconds();
        if (! std::isfinite (info.lengthSeconds) || info.lengthSeconds <= 0.0 || info.lengthSeconds > kMaxReasonableClipLengthSeconds)
            info.lengthSeconds = 4.0;

        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            info.type = ClipType::midi;
            info.name = "MIDI: " + info.name;

            auto& sequence = midiClip->getSequence();
            for (auto* note : sequence.getNotes())
            {
                auto noteRange = note->getEditTimeRange (*midiClip);
                NotePreview noteInfo;
                noteInfo.startSeconds = noteRange.getStart().inSeconds();
                noteInfo.lengthSeconds = noteRange.getLength().inSeconds();
                noteInfo.noteNumber = note->getNoteNumber();
                info.notes.push_back (noteInfo);
            }
        }
        else
        {
            info.type = ClipType::audio;
            info.sourcePath = clip->getCurrentSourceFile().getFullPathName();
            info.name = "Audio: " + info.name;
        }

        clips.push_back (info);
    }

    if (auto preview = buildRecordingPreview (*track, trackIndex))
        clips.push_back (*preview);

    return clips;
}

std::optional<SessionController::ClipInfo> SessionController::buildRecordingPreview (te::AudioTrack& track, int trackIndex) const
{
    if (edit == nullptr || transport == nullptr)
        return std::nullopt;

    const auto trackId = track.itemID.getRawID();

    if (! transport->isRecording())
    {
        recordingPreview.erase (trackId);
        return std::nullopt;
    }

    bool hasActiveRecording = false;
    double startSeconds = 0.0;

    auto& preview = recordingPreview[trackId];

    for (auto* instance : edit->getAllInputDevices())
    {
        if (instance == nullptr)
            continue;

        if (! instance->isRecordingActive (track.itemID))
            continue;

        hasActiveRecording = true;
        startSeconds = instance->getPunchInTime (track.itemID).inSeconds();

        if (preview.startSeconds != startSeconds)
        {
            preview.startSeconds = startSeconds;
            preview.activeNotes.clear();
            preview.finishedNotes.clear();
            preview.sustainEvents.clear();
        }

        if (auto fifo = instance->getRecordingNotes (track.itemID))
        {
            juce::MidiMessage message;
            while (fifo->pop (message))
            {
                const double now = getCurrentTimeSeconds();
                double time = message.getTimeStamp();
                // Some MIDI backends report timestamps in a different clock domain
                // (e.g. host uptime/epoch). Normalize those to transport time.
                if (! std::isfinite (time)
                    || time < (preview.startSeconds - 5.0)
                    || time > (now + 30.0))
                {
                    time = now;
                }
                const int noteNumber = message.getNoteNumber();

                if (message.isNoteOn())
                {
                    preview.activeNotes[noteNumber] = time;
                }
                else if (message.isNoteOff())
                {
                    auto it = preview.activeNotes.find (noteNumber);
                    if (it != preview.activeNotes.end())
                    {
                        if (time < it->second)
                            time = it->second + 0.01;

                        NotePreview note;
                        note.startSeconds = it->second;
                        note.lengthSeconds = juce::jmax (0.01, time - it->second);
                        note.noteNumber = noteNumber;
                        preview.finishedNotes.push_back (note);
                        preview.activeNotes.erase (it);
                    }
                }
                else if (message.isController() && message.getControllerNumber() == 64)
                {
                    SustainEvent ev;
                    ev.timeSeconds = time;
                    ev.value = message.getControllerValue();
                    preview.sustainEvents.push_back (ev);
                }
            }
        }
    }

    if (! hasActiveRecording)
    {
        recordingPreview.erase (trackId);
        return std::nullopt;
    }

    ClipInfo info;
    info.id = 0;
    info.trackIndex = trackIndex;
    info.type = ClipType::midi;
    info.name = "Recording...";
    info.startSeconds = preview.startSeconds;

    const double currentTime = getCurrentTimeSeconds();
    info.lengthSeconds = juce::jmax (0.01, currentTime - preview.startSeconds);
    info.notes = preview.finishedNotes;

    for (const auto& active : preview.activeNotes)
    {
        NotePreview note;
        note.startSeconds = active.second;
        note.lengthSeconds = juce::jmax (0.01, currentTime - active.second);
        note.noteNumber = active.first;
        info.notes.push_back (note);
    }

    std::sort (preview.sustainEvents.begin(), preview.sustainEvents.end(),
               [] (const SustainEvent& a, const SustainEvent& b) { return a.timeSeconds < b.timeSeconds; });

    return info;
}

std::optional<SessionController::ClipInfo> SessionController::getClipInfo (uint64_t clipId) const
{
    if (edit == nullptr)
        return std::nullopt;

    auto tracks = te::getAudioTracks (*edit);
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks.getUnchecked (trackIndex);
        for (auto* clip : track->getClips())
        {
            if (clip == nullptr)
                continue;

            if (clip->itemID.getRawID() != clipId)
                continue;

            ClipInfo info;
            info.id = clip->itemID.getRawID();
            info.trackIndex = trackIndex;
            info.name = clip->getName();
            auto range = clip->getEditTimeRange();
            info.startSeconds = range.getStart().inSeconds();
            info.lengthSeconds = range.getLength().inSeconds();
            if (! std::isfinite (info.lengthSeconds) || info.lengthSeconds <= 0.0 || info.lengthSeconds > kMaxReasonableClipLengthSeconds)
                info.lengthSeconds = 4.0;

            if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
            {
                info.type = ClipType::midi;
                info.name = "MIDI: " + info.name;
            }
            else
            {
                info.type = ClipType::audio;
                info.name = "Audio: " + info.name;
            }

            return info;
        }
    }

    return std::nullopt;
}

bool SessionController::moveClip (uint64_t clipId, double newStartSeconds)
{
    if (edit == nullptr)
        return false;

    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Move Clip");

    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, newStartSeconds));
    clip->setStart (start, false, true);
    cursorTimeSeconds = start.inSeconds();
    return true;
}

bool SessionController::moveClipToTrack (uint64_t clipId, int targetTrackIndex, double newStartSeconds)
{
    if (edit == nullptr)
        return false;

    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return false;

    auto* targetTrack = getAudioTrack (targetTrackIndex);
    if (targetTrack == nullptr)
        return false;

    if (! clip->moveTo (*targetTrack))
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Move Clip");
    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, newStartSeconds));
    clip->setStart (start, false, true);
    cursorTimeSeconds = start.inSeconds();
    updateTrackNames();
    return true;
}

bool SessionController::resizeClipLength (uint64_t clipId, double newLengthSeconds)
{
    if (edit == nullptr)
        return false;

    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Resize Clip");
    const auto length = te::TimeDuration::fromSeconds (juce::jmax (0.01, newLengthSeconds));
    clip->setLength (length, true);
    return true;
}

bool SessionController::resizeClipRange (uint64_t clipId, double newStartSeconds, double newLengthSeconds)
{
    if (edit == nullptr)
        return false;

    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return false;

    const double safeStart = juce::jmax (0.0, newStartSeconds);
    const double safeLength = juce::jmax (0.01, newLengthSeconds);
    const double safeEnd = safeStart + safeLength;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Resize Clip");

    // Preserve sync so resizing behaves like trimming without shifting notes.
    clip->setStart (te::TimePosition::fromSeconds (safeStart), true, false);
    clip->setEnd (te::TimePosition::fromSeconds (safeEnd), true);
    return true;
}

uint64_t SessionController::duplicateClipToTrack (uint64_t clipId, int targetTrackIndex, double newStartSeconds)
{
    if (edit == nullptr)
        return 0;

    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return 0;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Paste Clip");

    auto newClip = te::duplicateClip (*clip);
    if (newClip == nullptr)
        return 0;

    if (auto* targetTrack = getAudioTrack (targetTrackIndex))
    {
        if (newClip->getTrack() != targetTrack)
            newClip->moveTo (*targetTrack);
    }

    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, newStartSeconds));
    newClip->setStart (start, false, true);
    cursorTimeSeconds = start.inSeconds();
    updateTrackNames();
    return newClip->itemID.getRawID();
}

bool SessionController::deleteClip (uint64_t clipId)
{
    if (edit == nullptr)
        return false;

    if (auto* clip = findClipById (clipId))
    {
        auto& undoManager = edit->getUndoManager();
        undoManager.beginNewTransaction ("Delete Clip");
        clip->removeFromParent();
        return true;
    }

    return false;
}

bool SessionController::deleteTrack (int trackIndex)
{
    if (edit == nullptr)
        return false;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Delete Track");
    edit->deleteTrack (track);

    selectedTrackIndex = juce::jlimit (0, juce::jmax (0, getTrackCount() - 1), selectedTrackIndex);
    updateTrackNames();
    ensureTrackStateSize();
    return true;
}

bool SessionController::duplicateTrack (int trackIndex)
{
    if (edit == nullptr)
        return false;

    auto* sourceTrack = getAudioTrack (trackIndex);
    if (sourceTrack == nullptr)
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Duplicate Track");

    te::TrackInsertPoint insertPoint (*sourceTrack, false);
    auto duplicated = edit->copyTrack (sourceTrack, insertPoint);
    if (duplicated == nullptr)
        return false;

    selectedTrackIndex = duplicated->getIndexInEditTrackList();
    updateTrackNames();
    ensureTrackStateSize();
    return true;
}

std::vector<SessionController::MidiNoteInfo> SessionController::getMidiNotesForClip (uint64_t clipId) const
{
    std::vector<MidiNoteInfo> notes;
    if (edit == nullptr)
        return notes;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return notes;

    auto& sequence = midiClip->getSequence();
    for (auto* note : sequence.getNotes())
    {
        if (note == nullptr)
            continue;

        auto timeRange = note->getEditTimeRange (*midiClip);
        MidiNoteInfo info;
        info.state = note->state;
        info.startSeconds = timeRange.getStart().inSeconds();
        info.lengthSeconds = timeRange.getLength().inSeconds();
        info.noteNumber = note->getNoteNumber();
        info.velocity = note->getVelocity();
        notes.push_back (info);
    }

    return notes;
}

std::vector<SessionController::SustainEvent> SessionController::getSustainEventsForClip (uint64_t clipId) const
{
    std::vector<SustainEvent> events;
    if (edit == nullptr)
        return events;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return events;

    auto& sequence = midiClip->getSequence();
    const auto& controllers = sequence.getControllerEvents();
    events.reserve ((size_t) controllers.size());

    for (auto* controller : controllers)
    {
        if (controller == nullptr)
            continue;

        if (controller->getType() != 64)
            continue;

        SustainEvent ev;
        ev.timeSeconds = controller->getEditTime (*midiClip).inSeconds();
        ev.value = controller->getControllerValue();
        events.push_back (ev);
    }

    std::sort (events.begin(), events.end(),
               [] (const SustainEvent& a, const SustainEvent& b) { return a.timeSeconds < b.timeSeconds; });
    return events;
}

bool SessionController::addMidiNote (uint64_t clipId, int noteNumber, double startSeconds, double lengthSeconds)
{
    if (edit == nullptr)
        return false;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return false;

    const auto clipStart = midiClip->getEditTimeRange().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto noteStartBeatAbs = te::toBeats (te::TimePosition::fromSeconds (startSeconds), edit->tempoSequence);
    const auto noteEndBeatAbs = te::toBeats (te::TimePosition::fromSeconds (startSeconds + lengthSeconds), edit->tempoSequence);
    const double relativeStartBeats = juce::jmax (0.0, (noteStartBeatAbs - clipStartBeat).inBeats());
    const auto startBeat = te::BeatPosition::fromBeats (relativeStartBeats);
    const double lengthBeatsValue = juce::jmax (0.001, (noteEndBeatAbs - noteStartBeatAbs).inBeats());
    const auto lengthBeats = te::BeatDuration::fromBeats (lengthBeatsValue);

    auto& sequence = midiClip->getSequence();
    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Add MIDI Note");
    sequence.addNote (noteNumber, startBeat, lengthBeats, 100, 0, &undo);
    return true;
}

bool SessionController::updateMidiNote (uint64_t clipId, const juce::ValueTree& noteState,
                                        double startSeconds, double lengthSeconds, int noteNumber)
{
    if (edit == nullptr)
        return false;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return false;

    auto* note = findMidiNote (*midiClip, noteState);
    if (note == nullptr)
        return false;

    const auto clipStart = midiClip->getEditTimeRange().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto noteStartBeatAbs = te::toBeats (te::TimePosition::fromSeconds (startSeconds), edit->tempoSequence);
    const auto noteEndBeatAbs = te::toBeats (te::TimePosition::fromSeconds (startSeconds + lengthSeconds), edit->tempoSequence);
    const double relativeStartBeats = juce::jmax (0.0, (noteStartBeatAbs - clipStartBeat).inBeats());
    const auto startBeat = te::BeatPosition::fromBeats (relativeStartBeats);
    const double lengthBeatsValue = juce::jmax (0.001, (noteEndBeatAbs - noteStartBeatAbs).inBeats());
    const auto lengthBeats = te::BeatDuration::fromBeats (lengthBeatsValue);

    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Edit MIDI Note");
    note->setStartAndLength (startBeat, lengthBeats, &undo);
    note->setNoteNumber (noteNumber, &undo);
    return true;
}

bool SessionController::resizeMidiNote (uint64_t clipId, const juce::ValueTree& noteState, double lengthSeconds)
{
    if (edit == nullptr)
        return false;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return false;

    auto* note = findMidiNote (*midiClip, noteState);
    if (note == nullptr)
        return false;

    const auto clipStart = midiClip->getEditTimeRange().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto noteStartTime = note->getEditTimeRange (*midiClip).getStart();
    const auto noteStartBeatAbs = te::toBeats (noteStartTime, edit->tempoSequence);
    const auto noteEndBeatAbs = te::toBeats (te::TimePosition::fromSeconds (noteStartTime.inSeconds() + lengthSeconds), edit->tempoSequence);
    const double relativeStartBeats = juce::jmax (0.0, (noteStartBeatAbs - clipStartBeat).inBeats());
    const auto startBeat = te::BeatPosition::fromBeats (relativeStartBeats);
    const double lengthBeatsValue = juce::jmax (0.001, (noteEndBeatAbs - noteStartBeatAbs).inBeats());
    const auto lengthBeats = te::BeatDuration::fromBeats (lengthBeatsValue);

    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Resize MIDI Note");
    note->setStartAndLength (startBeat, lengthBeats, &undo);
    return true;
}

bool SessionController::deleteMidiNote (uint64_t clipId, const juce::ValueTree& noteState)
{
    if (edit == nullptr)
        return false;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return false;

    auto* note = findMidiNote (*midiClip, noteState);
    if (note == nullptr)
        return false;

    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Delete MIDI Note");
    midiClip->getSequence().removeNote (*note, &undo);
    return true;
}

bool SessionController::buildRealchordsNoteEvents (uint64_t clipId,
                                                   std::vector<RealchordsNoteEvent>& events,
                                                   int& endFrame) const
{
    events.clear();
    endFrame = 0;

    if (edit == nullptr)
        return false;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return false;

    const auto clipRange = midiClip->getEditTimeRange();
    const auto clipStartBeat = te::toBeats (clipRange.getStart(), edit->tempoSequence);
    const auto clipEndBeat = te::toBeats (clipRange.getEnd(), edit->tempoSequence);
    const double clipBeats = juce::jmax (0.0, (clipEndBeat - clipStartBeat).inBeats());
    const double framesPerBeat = 4.0;
    endFrame = (int) std::ceil (clipBeats * framesPerBeat);

    auto& sequence = midiClip->getSequence();
    for (auto* note : sequence.getNotes())
    {
        if (note == nullptr)
            continue;

        const auto noteRange = note->getEditTimeRange (*midiClip);
        const auto noteStartBeatAbs = te::toBeats (noteRange.getStart(), edit->tempoSequence);
        const auto noteEndBeatAbs = te::toBeats (noteRange.getEnd(), edit->tempoSequence);
        const double startBeats = juce::jmax (0.0, (noteStartBeatAbs - clipStartBeat).inBeats());
        const double endBeats = juce::jmax (startBeats, (noteEndBeatAbs - clipStartBeat).inBeats());

        int startFrame = (int) std::round (startBeats * framesPerBeat);
        int endFrameNote = (int) std::round (endBeats * framesPerBeat);
        if (endFrameNote <= startFrame)
            endFrameNote = startFrame + 1;

        events.push_back ({ startFrame, note->getNoteNumber(), true });
        events.push_back ({ endFrameNote, note->getNoteNumber(), false });
        endFrame = std::max (endFrame, endFrameNote + 1);
    }

    std::sort (events.begin(), events.end(),
               [] (const RealchordsNoteEvent& a, const RealchordsNoteEvent& b)
               {
                   if (a.frame != b.frame)
                       return a.frame < b.frame;
                   return a.on && ! b.on;
               });

    return true;
}

bool SessionController::createChordOptionTracks (uint64_t sourceClipId,
                                                 const std::vector<ChordOption>& options,
                                                 ChordPlaybackStyle playbackStyle)
{
    if (edit == nullptr || options.empty())
        return false;

    auto* sourceClip = findMidiClipById (sourceClipId);
    if (sourceClip == nullptr)
        return false;

    auto* sourceTrack = dynamic_cast<te::AudioTrack*> (sourceClip->getTrack());

    const auto clipRange = sourceClip->getEditTimeRange();
    const double clipStartSeconds = clipRange.getStart().inSeconds();

    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Create Chord Options");

    for (size_t optionIndex = 0; optionIndex < options.size(); ++optionIndex)
    {
        const auto& option = options[optionIndex];

        te::TrackInsertPoint insertPoint = te::TrackInsertPoint::getEndOfTracks (*edit);
        auto newTrackPtr = edit->insertNewAudioTrack (insertPoint, nullptr);
        auto* track = newTrackPtr.get();
        if (track == nullptr)
            continue;

        juce::String trackName = option.name;
        if (trackName.isEmpty())
            trackName = "Chords Option " + juce::String ((int) optionIndex + 1);
        track->setName (trackName);
        track->setMute (false);
        track->setSolo (false);

        const double framesPerBeat = 4.0;
        const auto clipLengthSeconds = te::TimeDuration::fromSeconds (clipRange.getLength().inSeconds());
        const auto clipStart = te::TimePosition::fromSeconds (clipStartSeconds);

        auto clip = track->insertMIDIClip (trackName, { clipStart, clipLengthSeconds }, nullptr);
        if (clip == nullptr)
            continue;

        auto& seq = clip->getSequence();

        std::vector<ChordFrame> onsets;
        for (const auto& frame : option.frames)
        {
            if (frame.on && frame.symbol.isNotEmpty() && frame.pitches.size() > 0)
                onsets.push_back (frame);
        }

        std::sort (onsets.begin(), onsets.end(),
                   [] (const ChordFrame& a, const ChordFrame& b)
                   {
                       return a.frame < b.frame;
                   });

        auto labelsState = clip->state.getOrCreateChildWithName ("CHORD_LABELS", nullptr);
        labelsState.removeAllChildren (nullptr);

        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int startFrame = onsets[i].frame;
            const int endFrame = (i + 1 < onsets.size()) ? onsets[i + 1].frame : option.frameCount;
            if (endFrame <= startFrame)
                continue;

            const double startBeats = (startFrame / framesPerBeat);
            const double lengthBeats = (endFrame - startFrame) / framesPerBeat;
            juce::Array<int> orderedPitches (onsets[i].pitches);
            orderedPitches.sort();

            if (playbackStyle == ChordPlaybackStyle::arpeggio)
            {
                const int noteCount = orderedPitches.size();
                const double eighthStep = 0.5;     // eighth-note grid
                const double sixteenthStep = 0.25; // sixteenth-note grid
                const double minTail = 0.125;
                const double neededForEighth = (juce::jmax (0, noteCount - 1) * eighthStep) + minTail;
                const double stepBeats = (lengthBeats >= neededForEighth) ? eighthStep : sixteenthStep;

                for (int noteIndex = 0; noteIndex < orderedPitches.size(); ++noteIndex)
                {
                    const double noteStartBeats = startBeats + (stepBeats * noteIndex);

                    // Keep arpeggio starts locked to 1/8 or 1/16 grid and avoid
                    // placing notes after the chord window.
                    const double elapsed = stepBeats * noteIndex;
                    if (elapsed >= lengthBeats)
                        break;

                    const double remaining = lengthBeats - elapsed;
                    const double noteLengthBeats = juce::jmax (minTail, juce::jmin (stepBeats * 0.95, remaining));
                    seq.addNote (orderedPitches[noteIndex],
                                 te::BeatPosition::fromBeats (noteStartBeats),
                                 te::BeatDuration::fromBeats (noteLengthBeats),
                                 100, 0, &undo);
                }
            }
            else
            {
                const auto startBeat = te::BeatPosition::fromBeats (startBeats);
                const auto durBeat = te::BeatDuration::fromBeats (juce::jmax (0.25, lengthBeats));
                for (auto pitch : orderedPitches)
                    seq.addNote (pitch, startBeat, durBeat, 100, 0, &undo);
            }

            auto label = juce::ValueTree ("CHORD_LABEL");
            label.setProperty ("beat", startBeats, nullptr);
            label.setProperty ("symbol", onsets[i].symbol, nullptr);
            labelsState.addChild (label, -1, nullptr);
        }

        // Add sustain pedal every bar: down on bar start, up just before bar end.
        const auto clipStartBeatAbs = te::toBeats (clipStart, edit->tempoSequence);
        const auto clipEndBeatAbs = te::toBeats (clipStart + clipLengthSeconds, edit->tempoSequence);
        const double clipLengthBeats = juce::jmax (0.0, (clipEndBeatAbs - clipStartBeatAbs).inBeats());
        const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAtTime (*edit, clipStart));
        const double liftBeforeBarEndBeats = 0.25; // 16th-note before bar end
        const int sustainOn = 127 << 7;
        const int sustainOff = 0;

        for (double barStart = 0.0; barStart < clipLengthBeats; barStart += beatsPerBar)
        {
            const double barEnd = juce::jmin (clipLengthBeats, barStart + beatsPerBar);
            const double offBeat = juce::jmax (barStart + 0.0625, barEnd - liftBeforeBarEndBeats);

            seq.addControllerEvent (te::BeatPosition::fromBeats (barStart), 64, sustainOn, &undo);
            seq.addControllerEvent (te::BeatPosition::fromBeats (offBeat), 64, sustainOff, &undo);
        }

        clip->setMidiChannel (te::MidiChannel (1));

        if (sourceTrack != nullptr)
        {
            if (duplicateInstrumentPlugin (*track, *sourceTrack) == nullptr)
                insertDefaultInstrumentIfAvailable (track->getIndexInEditTrackList());
        }
        else
        {
            insertDefaultInstrumentIfAvailable (track->getIndexInEditTrackList());
        }
    }

    updateTrackNames();
    ensureTrackStateSize();
    return true;
}

std::vector<SessionController::ChordLabel> SessionController::getChordLabelsForClip (uint64_t clipId) const
{
    std::vector<ChordLabel> labels;
    if (edit == nullptr)
        return labels;

    auto* midiClip = findMidiClipById (clipId);
    if (midiClip == nullptr)
        return labels;

    auto labelsState = midiClip->state.getChildWithName ("CHORD_LABELS");
    if (! labelsState.isValid())
        return labels;

    const auto clipStart = midiClip->getPosition().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);

    for (int i = 0; i < labelsState.getNumChildren(); ++i)
    {
        auto child = labelsState.getChild (i);
        const double beatOffset = (double) child.getProperty ("beat");
        const auto beatPos = clipStartBeat + te::BeatDuration::fromBeats (beatOffset);
        const double timeSeconds = edit->tempoSequence.toTime (beatPos).inSeconds();
        ChordLabel label;
        label.startSeconds = timeSeconds;
        label.symbol = child.getProperty ("symbol").toString();
        labels.push_back (label);
    }

    std::sort (labels.begin(), labels.end(),
               [] (const ChordLabel& a, const ChordLabel& b)
               {
                   return a.startSeconds < b.startSeconds;
               });

    return labels;
}

std::vector<SessionController::ChordLabel> SessionController::getChordLabelsForTrack (int trackIndex) const
{
    if (edit == nullptr)
        return {};

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return {};

    for (auto* clip : track->getClips())
    {
        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            auto labels = getChordLabelsForClip (midiClip->itemID.getRawID());
            if (! labels.empty())
                return labels;
        }
    }

    return {};
}

std::vector<SessionController::ChordPreviewNote> SessionController::getChordCellPreviewNotesForTrack (int trackIndex, int measure, int beat) const
{
    std::vector<ChordPreviewNote> result;
    if (edit == nullptr || measure < 1 || beat < 1)
        return result;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return result;

    te::MidiClip* chordClip = nullptr;
    for (auto* clip : track->getClips())
    {
        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            auto labelsState = midiClip->state.getChildWithName ("CHORD_LABELS");
            if (labelsState.isValid() && labelsState.getNumChildren() > 0)
            {
                chordClip = midiClip;
                break;
            }
        }
    }
    if (chordClip == nullptr)
        return result;

    auto labelsState = chordClip->state.getChildWithName ("CHORD_LABELS");
    if (! labelsState.isValid())
        return result;

    const auto clipStart = chordClip->getPosition().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto clipRange = chordClip->getEditTimeRange();
    const auto clipEndBeat = te::toBeats (clipRange.getEnd(), edit->tempoSequence);
    const double clipLengthBeats = juce::jmax (0.0, (clipEndBeat - clipStartBeat).inBeats());

    struct LabelEntry { double startBeat = 0.0; };
    std::vector<LabelEntry> entries;
    entries.reserve ((size_t) labelsState.getNumChildren());
    for (int i = 0; i < labelsState.getNumChildren(); ++i)
        entries.push_back ({ (double) labelsState.getChild (i).getProperty ("beat") });

    std::sort (entries.begin(), entries.end(), [] (const LabelEntry& a, const LabelEntry& b) { return a.startBeat < b.startBeat; });

    int targetIndex = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto beatPos = clipStartBeat + te::BeatDuration::fromBeats (entries[(size_t) i].startBeat);
        const auto timeSeconds = edit->tempoSequence.toTime (beatPos).inSeconds();
        int bar = 0, barBeat = 0;
        if (! parseBarBeatText (getBarsAndBeatsText (timeSeconds), bar, barBeat))
            continue;
        if (bar == measure && barBeat == beat)
        {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex < 0)
        return result;

    const double startBeat = entries[(size_t) targetIndex].startBeat;
    const double endBeat = (targetIndex + 1 < (int) entries.size()) ? entries[(size_t) targetIndex + 1].startBeat : clipLengthBeats;
    if (endBeat <= startBeat + 0.001)
        return result;

    auto& seq = chordClip->getSequence();
    for (auto* note : seq.getNotes())
    {
        if (note == nullptr)
            continue;
        const double nStart = note->getStartBeat().inBeats();
        const double nEnd = nStart + note->getLengthBeats().inBeats();
        if (nStart < endBeat && nEnd > startBeat)
        {
            const double localStart = juce::jmax (0.0, nStart - startBeat);
            const double localEnd = juce::jmin (endBeat - startBeat, nEnd - startBeat);
            result.push_back ({ note->getNoteNumber(), localStart, juce::jmax (0.0625, localEnd - localStart) });
        }
    }

    std::sort (result.begin(), result.end(), [] (const ChordPreviewNote& a, const ChordPreviewNote& b)
    {
        if (a.startBeats < b.startBeats)
            return a.startBeats < b.startBeats;
        if (a.startBeats > b.startBeats)
            return false;
        return a.pitch < b.pitch;
    });
    return result;
}

bool SessionController::applyChordEditAction (int trackIndex, int measure, int beat, ChordEditAction action)
{
    if (edit == nullptr || measure < 1 || beat < 1)
        return false;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return false;

    te::MidiClip* chordClip = nullptr;
    for (auto* clip : track->getClips())
    {
        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            auto labelsState = midiClip->state.getChildWithName ("CHORD_LABELS");
            if (labelsState.isValid() && labelsState.getNumChildren() > 0)
            {
                chordClip = midiClip;
                break;
            }
        }
    }

    if (chordClip == nullptr)
        return false;

    auto labelsState = chordClip->state.getChildWithName ("CHORD_LABELS");
    if (! labelsState.isValid())
        return false;

    const auto clipStart = chordClip->getPosition().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto clipRange = chordClip->getEditTimeRange();
    const auto clipEndBeat = te::toBeats (clipRange.getEnd(), edit->tempoSequence);
    const double clipLengthBeats = juce::jmax (0.0, (clipEndBeat - clipStartBeat).inBeats());

    auto parseBarBeat = [] (const juce::String& text, int& outBar, int& outBeat) -> bool
    {
        auto digits = text.retainCharacters ("0123456789|");
        auto parts = juce::StringArray::fromTokens (digits, "|", "");
        if (parts.size() < 2)
            return false;
        outBar = parts[0].getIntValue();
        outBeat = parts[1].getIntValue();
        return outBar > 0 && outBeat > 0;
    };

    struct LabelEntry
    {
        int childIndex = -1;
        double startBeat = 0.0;
    };
    std::vector<LabelEntry> entries;
    entries.reserve ((size_t) labelsState.getNumChildren());
    for (int i = 0; i < labelsState.getNumChildren(); ++i)
    {
        auto child = labelsState.getChild (i);
        entries.push_back ({ i, (double) child.getProperty ("beat") });
    }

    std::sort (entries.begin(), entries.end(),
               [] (const LabelEntry& a, const LabelEntry& b) { return a.startBeat < b.startBeat; });

    int targetSortedIndex = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto beatPos = clipStartBeat + te::BeatDuration::fromBeats (entries[(size_t) i].startBeat);
        const auto timeSeconds = edit->tempoSequence.toTime (beatPos).inSeconds();
        int rowMeasure = 0, rowBeat = 0;
        if (! parseBarBeat (getBarsAndBeatsText (timeSeconds), rowMeasure, rowBeat))
            continue;

        if (rowMeasure == measure && rowBeat == beat)
        {
            targetSortedIndex = i;
            break;
        }
    }

    if (targetSortedIndex < 0)
        return false;

    const double startBeat = entries[(size_t) targetSortedIndex].startBeat;
    const double endBeat = (targetSortedIndex + 1 < (int) entries.size())
        ? entries[(size_t) targetSortedIndex + 1].startBeat
        : clipLengthBeats;

    if (endBeat <= startBeat + 0.001)
        return false;

    auto& seq = chordClip->getSequence();
    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Edit Chord Cell");
    auto finalize = [&]() -> bool
    {
        enforceChordSustainEveryBar (*edit, *chordClip, undo);
        return true;
    };

    std::vector<te::MidiNote*> notesToRemove;
    juce::Array<int> pitches;
    int velocity = 100;
    struct ExistingNote { double start = 0.0; double length = 0.0; int pitch = 60; int vel = 100; };
    std::vector<ExistingNote> existingNotes;

    for (auto* note : seq.getNotes())
    {
        if (note == nullptr)
            continue;

        const double noteStart = note->getStartBeat().inBeats();
        const double noteEnd = noteStart + note->getLengthBeats().inBeats();
        const bool overlaps = noteStart < endBeat && noteEnd > startBeat;
        if (! overlaps)
            continue;

        notesToRemove.push_back (note);
        if (! pitches.contains (note->getNoteNumber()))
            pitches.add (note->getNoteNumber());
        velocity = juce::jmax (velocity, note->getVelocity());
        existingNotes.push_back ({ noteStart, note->getLengthBeats().inBeats(), note->getNoteNumber(), note->getVelocity() });
    }

    for (auto* note : notesToRemove)
        seq.removeNote (*note, &undo);

    if (action == ChordEditAction::deleteChord)
    {
        labelsState.removeChild (entries[(size_t) targetSortedIndex].childIndex, &undo);
        return finalize();
    }

    if (pitches.isEmpty())
        return finalize();

    pitches.sort();
    const double lengthBeats = endBeat - startBeat;

    if (action == ChordEditAction::semitoneUp || action == ChordEditAction::semitoneDown
        || action == ChordEditAction::octaveUp || action == ChordEditAction::octaveDown)
    {
        int shift = 0;
        if (action == ChordEditAction::semitoneUp) shift = 1;
        if (action == ChordEditAction::semitoneDown) shift = -1;
        if (action == ChordEditAction::octaveUp) shift = 12;
        if (action == ChordEditAction::octaveDown) shift = -12;
        for (const auto& n : existingNotes)
        {
            const int shiftedPitch = juce::jlimit (0, 127, n.pitch + shift);
            seq.addNote (shiftedPitch,
                         te::BeatPosition::fromBeats (n.start),
                         te::BeatDuration::fromBeats (juce::jmax (0.0625, n.length)),
                         n.vel, 0, &undo);
        }
        return finalize();
    }

    if (action == ChordEditAction::inversionUp || action == ChordEditAction::inversionDown)
    {
        if (existingNotes.empty())
            return finalize();

        int targetIndex = 0;
        if (action == ChordEditAction::inversionUp)
        {
            for (int i = 1; i < (int) existingNotes.size(); ++i)
            {
                const auto& best = existingNotes[(size_t) targetIndex];
                const auto& cur = existingNotes[(size_t) i];
                if (cur.pitch < best.pitch || (cur.pitch == best.pitch && cur.start < best.start))
                    targetIndex = i;
            }
            existingNotes[(size_t) targetIndex].pitch = juce::jlimit (0, 127, existingNotes[(size_t) targetIndex].pitch + 12);
        }
        else
        {
            for (int i = 1; i < (int) existingNotes.size(); ++i)
            {
                const auto& best = existingNotes[(size_t) targetIndex];
                const auto& cur = existingNotes[(size_t) i];
                if (cur.pitch > best.pitch || (cur.pitch == best.pitch && cur.start > best.start))
                    targetIndex = i;
            }
            existingNotes[(size_t) targetIndex].pitch = juce::jlimit (0, 127, existingNotes[(size_t) targetIndex].pitch - 12);
        }

        for (const auto& n : existingNotes)
        {
            seq.addNote (n.pitch,
                         te::BeatPosition::fromBeats (n.start),
                         te::BeatDuration::fromBeats (juce::jmax (0.0625, n.length)),
                         n.vel, 0, &undo);
        }
        return finalize();
    }

    if (action == ChordEditAction::arpeggio)
    {
        const int noteCount = pitches.size();
        const double eighthStep = 0.5;
        const double sixteenthStep = 0.25;
        const double minTail = 0.125;
        const double neededForEighth = (juce::jmax (0, noteCount - 1) * eighthStep) + minTail;
        const double stepBeats = (lengthBeats >= neededForEighth) ? eighthStep : sixteenthStep;

        for (int i = 0; i < pitches.size(); ++i)
        {
            const double noteStart = startBeat + (stepBeats * i);
            const double elapsed = stepBeats * i;
            if (elapsed >= lengthBeats)
                break;

            const double remaining = lengthBeats - elapsed;
            const double noteLength = juce::jmax (minTail, juce::jmin (stepBeats * 0.95, remaining));
            seq.addNote (pitches[i],
                         te::BeatPosition::fromBeats (noteStart),
                         te::BeatDuration::fromBeats (noteLength),
                         velocity, 0, &undo);
        }
        return finalize();
    }

    if (action == ChordEditAction::doubleTime)
    {
        const double half = lengthBeats * 0.5;

        auto isArpeggiatedCell = [&]() -> bool
        {
            if (existingNotes.size() <= 1 || pitches.isEmpty())
                return false;

            struct StartGroup
            {
                double start = 0.0;
                juce::Array<int> groupPitches;
            };

            std::vector<StartGroup> groups;
            for (const auto& n : existingNotes)
            {
                bool merged = false;
                for (auto& g : groups)
                {
                    if (std::abs (g.start - n.start) < 0.0001)
                    {
                        if (! g.groupPitches.contains (n.pitch))
                            g.groupPitches.add (n.pitch);
                        merged = true;
                        break;
                    }
                }
                if (! merged)
                {
                    StartGroup g;
                    g.start = n.start;
                    g.groupPitches.add (n.pitch);
                    groups.push_back (std::move (g));
                }
            }

            // Blocked chords have every pitch at each start instant.
            for (const auto& g : groups)
                if (g.groupPitches.size() < pitches.size())
                    return true;

            return false;
        };

        if (isArpeggiatedCell())
        {
            // Preserve the arpeggio shape and repeat it twice in the same cell (double-time feel).
            double patternSpan = 0.0;
            for (const auto& n : existingNotes)
            {
                const double relStart = juce::jmax (0.0, n.start - startBeat);
                patternSpan = juce::jmax (patternSpan, relStart + juce::jmax (0.0625, n.length));
            }
            if (patternSpan <= 0.0001)
                patternSpan = half;

            const double scale = juce::jmax (0.05, half / patternSpan);
            const double minLen = 0.0625;

            for (const auto& n : existingNotes)
            {
                const double relStart = juce::jmax (0.0, n.start - startBeat) * scale;
                const double relLen = juce::jmax (minLen, n.length * scale);

                for (int repeat = 0; repeat < 2; ++repeat)
                {
                    const double chunkStart = startBeat + (repeat * half);
                    double noteStart = chunkStart + relStart;
                    double noteEnd = juce::jmin (chunkStart + half, noteStart + relLen);
                    if (noteEnd <= noteStart + 0.0001)
                        continue;

                    seq.addNote (juce::jlimit (0, 127, n.pitch),
                                 te::BeatPosition::fromBeats (noteStart),
                                 te::BeatDuration::fromBeats (juce::jmax (minLen, noteEnd - noteStart)),
                                 n.vel, 0, &undo);
                }
            }
        }
        else
        {
            const double noteLength = juce::jmax (0.125, juce::jmin (half * 0.95, lengthBeats * 0.49));
            for (auto pitch : pitches)
            {
                seq.addNote (pitch,
                             te::BeatPosition::fromBeats (startBeat),
                             te::BeatDuration::fromBeats (noteLength),
                             velocity, 0, &undo);
                seq.addNote (pitch,
                             te::BeatPosition::fromBeats (startBeat + half),
                             te::BeatDuration::fromBeats (noteLength),
                             velocity, 0, &undo);
            }
        }
        return finalize();
    }

    // Block
    const auto start = te::BeatPosition::fromBeats (startBeat);
    const auto dur = te::BeatDuration::fromBeats (juce::jmax (0.125, lengthBeats));
    for (auto pitch : pitches)
        seq.addNote (pitch, start, dur, velocity, 0, &undo);

    return finalize();
}

bool SessionController::setChordAtCell (int trackIndex, int measure, int beat, const juce::String& chordSymbol)
{
    if (edit == nullptr || measure < 1 || beat < 1 || chordSymbol.trim().isEmpty())
        return false;

    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return false;

    te::MidiClip* chordClip = nullptr;
    for (auto* clip : track->getClips())
    {
        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            auto labelsState = midiClip->state.getChildWithName ("CHORD_LABELS");
            if (labelsState.isValid())
            {
                chordClip = midiClip;
                break;
            }
        }
    }

    if (chordClip == nullptr)
        return false;

    auto labelsState = chordClip->state.getOrCreateChildWithName ("CHORD_LABELS", nullptr);

    const auto clipStart = chordClip->getPosition().getStart();
    const auto clipStartBeat = te::toBeats (clipStart, edit->tempoSequence);
    const auto clipRange = chordClip->getEditTimeRange();
    const auto clipEndBeat = te::toBeats (clipRange.getEnd(), edit->tempoSequence);
    const double clipLengthBeats = juce::jmax (0.0, (clipEndBeat - clipStartBeat).inBeats());
    if (clipLengthBeats <= 0.001)
        return false;

    double targetStartBeat = -1.0;
    for (double beatOffset = 0.0; beatOffset <= clipLengthBeats + 0.0001; beatOffset += 1.0)
    {
        const auto absBeat = clipStartBeat + te::BeatDuration::fromBeats (beatOffset);
        const auto timeSeconds = edit->tempoSequence.toTime (absBeat).inSeconds();
        int bar = 0, barBeat = 0;
        if (! parseBarBeatText (getBarsAndBeatsText (timeSeconds), bar, barBeat))
            continue;
        if (bar == measure && barBeat == beat)
        {
            targetStartBeat = beatOffset;
            break;
        }
    }

    if (targetStartBeat < 0.0)
        return false;

    double nextLabelBeat = clipLengthBeats;
    for (int i = 0; i < labelsState.getNumChildren(); ++i)
    {
        const auto child = labelsState.getChild (i);
        const double b = (double) child.getProperty ("beat");
        if (b > targetStartBeat + 0.0001)
            nextLabelBeat = juce::jmin (nextLabelBeat, b);
    }

    double targetEndBeat = juce::jmin (clipLengthBeats, nextLabelBeat);
    auto& seq = chordClip->getSequence();

    // Preserve the original occupied span of this cell when rewriting root/quality.
    double occupiedEndBeat = targetStartBeat;
    for (auto* note : seq.getNotes())
    {
        if (note == nullptr)
            continue;

        const double noteStart = note->getStartBeat().inBeats();
        if (noteStart < targetStartBeat - 0.0001 || noteStart >= targetEndBeat + 0.0001)
            continue;

        const double noteEnd = noteStart + note->getLengthBeats().inBeats();
        occupiedEndBeat = juce::jmax (occupiedEndBeat, juce::jmin (noteEnd, targetEndBeat));
    }

    if (occupiedEndBeat > targetStartBeat + 0.001)
        targetEndBeat = occupiedEndBeat;
    else if (targetEndBeat <= targetStartBeat + 0.001)
        targetEndBeat = juce::jmin (clipLengthBeats, targetStartBeat + 1.0);

    auto& undo = edit->getUndoManager();
    undo.beginNewTransaction ("Set Chord At Cell");

    std::vector<te::MidiNote*> notesToRemove;
    for (auto* note : seq.getNotes())
    {
        if (note == nullptr)
            continue;
        const double noteStart = note->getStartBeat().inBeats();
        const double noteEnd = noteStart + note->getLengthBeats().inBeats();
        if (noteStart < targetEndBeat && noteEnd > targetStartBeat)
            notesToRemove.push_back (note);
    }
    for (auto* note : notesToRemove)
        seq.removeNote (*note, &undo);

    juce::ValueTree labelToUpdate;
    for (int i = labelsState.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = labelsState.getChild (i);
        const double b = (double) child.getProperty ("beat");
        if (std::abs (b - targetStartBeat) < 0.0001)
        {
            if (! labelToUpdate.isValid())
                labelToUpdate = child;
            else
                labelsState.removeChild (i, &undo);
        }
    }

    if (! labelToUpdate.isValid())
    {
        labelToUpdate = juce::ValueTree ("CHORD_LABEL");
        labelToUpdate.setProperty ("beat", targetStartBeat, nullptr);
        labelsState.addChild (labelToUpdate, -1, &undo);
    }
    labelToUpdate.setProperty ("symbol", chordSymbol.trim(), &undo);

    auto pitches = chordSymbolToPitches (chordSymbol);
    const double durBeats = juce::jmax (0.125, targetEndBeat - targetStartBeat);
    for (auto p : pitches)
    {
        seq.addNote (p,
                     te::BeatPosition::fromBeats (targetStartBeat),
                     te::BeatDuration::fromBeats (durBeats),
                     100, 0, &undo);
    }

    enforceChordSustainEveryBar (*edit, *chordClip, undo);
    return true;
}

std::vector<SessionController::PluginChoice> SessionController::getInstrumentChoices() const
{
    std::vector<PluginChoice> choices;
    auto types = engine.getPluginManager().knownPluginList.getTypes();
    for (const auto& desc : types)
    {
        if (desc.isInstrument || desc.category.containsIgnoreCase ("synth") || desc.category.containsIgnoreCase ("instrument"))
        {
            auto instrumentDesc = desc;
            instrumentDesc.isInstrument = true;
            choices.push_back ({ te::ExternalPlugin::xmlTypeName, instrumentDesc });
        }
    }

    auto fallback = te::PluginManager::createBuiltInPluginDescription<te::FourOscPlugin> (true);
    fallback.name = "Built-in: " + fallback.name;
    choices.push_back ({ te::FourOscPlugin::xmlTypeName, fallback });

    return choices;
}

std::vector<SessionController::PluginChoice> SessionController::getEffectChoices() const
{
    std::vector<PluginChoice> choices;

    auto addBuiltIn = [&choices] (auto desc, const juce::String& typeName)
    {
        desc.name = "Built-in: " + desc.name;
        choices.push_back ({ typeName, desc });
    };

    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::ChorusPlugin>(), te::ChorusPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::CompressorPlugin>(), te::CompressorPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::DelayPlugin>(), te::DelayPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::EqualiserPlugin>(), te::EqualiserPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::ImpulseResponsePlugin>(), te::ImpulseResponsePlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::LatencyPlugin>(), te::LatencyPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::LowPassPlugin>(), te::LowPassPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::MidiModifierPlugin>(), te::MidiModifierPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::MidiPatchBayPlugin>(), te::MidiPatchBayPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::PatchBayPlugin>(), te::PatchBayPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::PhaserPlugin>(), te::PhaserPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::PitchShiftPlugin>(), te::PitchShiftPlugin::xmlTypeName);
    addBuiltIn (te::PluginManager::createBuiltInPluginDescription<te::ReverbPlugin>(), te::ReverbPlugin::xmlTypeName);

    auto isSafeExternalEffect = [] (const juce::PluginDescription& desc) -> bool
    {
        if (desc.isInstrument)
            return false;

        const auto name = desc.name.toLowerCase();
        const auto category = desc.category.toLowerCase();

        const std::vector<juce::String> allowKeywords = {
            "reverb", "delay", "echo", "chorus", "phaser", "flanger", "compress",
            "limiter", "expander", "gate", "eq", "equal", "filter", "distortion",
            "overdrive", "satur", "pitch", "tremolo"
        };

        const std::vector<juce::String> denyKeywords = {
            "panner", "matrix", "mixer", "spatial", "soundfield", "head", "vector",
            "3d", "network", "net", "midi", "patch", "router", "utility"
        };

        for (const auto& deny : denyKeywords)
            if (name.contains (deny) || category.contains (deny))
                return false;

        for (const auto& allow : allowKeywords)
            if (name.contains (allow) || category.contains (allow))
                return true;

        return false;
    };

    auto types = engine.getPluginManager().knownPluginList.getTypes();
    for (const auto& desc : types)
    {
        if (isSafeExternalEffect (desc))
            choices.push_back ({ te::ExternalPlugin::xmlTypeName, desc });
    }

    return choices;
}

std::vector<SessionController::PluginInfo> SessionController::getTrackPlugins (int trackIndex) const
{
    std::vector<PluginInfo> plugins;
    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr)
        return plugins;

    for (auto* plugin : track->pluginList)
    {
        if (plugin == nullptr)
            continue;

        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr)
            continue;

        PluginInfo info;
        info.id = plugin->itemID.getRawID();
        info.name = plugin->getName();
        info.isInstrument = isInstrumentPlugin (*plugin);
        info.enabled = plugin->isEnabled();
        plugins.push_back (info);
    }

    return plugins;
}

std::vector<SessionController::PluginInfo> SessionController::getTrackEffects (int trackIndex) const
{
    std::vector<PluginInfo> effects;
    for (auto info : getTrackPlugins (trackIndex))
    {
        if (! info.isInstrument)
            effects.push_back (info);
    }

    return effects;
}

uint64_t SessionController::insertInstrument (int trackIndex, const PluginChoice& choice)
{
    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr || edit == nullptr)
        return 0;

    removeInstrumentPlugins (*track);

    auto plugin = edit->getPluginCache().createNewPlugin (choice.type, choice.description);
    if (plugin == nullptr)
    {
        engine.getUIBehaviour().showWarningMessage ("Instrument could not be loaded.");
        return 0;
    }

    const int insertIndex = getInsertIndexBeforeVolume (*track);
    track->pluginList.insertPlugin (plugin, insertIndex, nullptr);

    juce::String label = bestLabelForDescription (choice.description);
    if (isPlaceholderProgramName (label))
        label.clear();

    if (label.isEmpty())
        label = plugin->getName().trim();

    if (isPlaceholderProgramName (label))
        label = choice.description.name.trim();

    if (isPlaceholderProgramName (label))
        label.clear();

    if (label.isNotEmpty())
        track->state.setProperty (kInstrumentLabelId, label, nullptr);
    else
        track->state.removeProperty (kInstrumentLabelId, nullptr);

    return plugin->itemID.getRawID();
}

uint64_t SessionController::insertEffect (int trackIndex, const PluginChoice& choice)
{
    auto* track = getAudioTrack (trackIndex);
    if (track == nullptr || edit == nullptr)
        return 0;

    auto plugin = edit->getPluginCache().createNewPlugin (choice.type, choice.description);
    if (plugin == nullptr)
    {
        engine.getUIBehaviour().showWarningMessage ("Effect could not be loaded.");
        return 0;
    }

    const int insertIndex = getInsertIndexBeforeVolume (*track);
    track->pluginList.insertPlugin (plugin, insertIndex, nullptr);
    return plugin->itemID.getRawID();
}

bool SessionController::showPluginWindow (uint64_t pluginId)
{
    if (edit == nullptr)
        return false;

    auto plugin = edit->getPluginCache().getPluginFor (te::EditItemID::fromRawID (pluginId));
    if (plugin == nullptr || plugin->windowState == nullptr)
        return false;

    plugin->windowState->showWindowExplicitly();
    return true;
}

bool SessionController::setPluginEnabled (uint64_t pluginId, bool enabled)
{
    if (edit == nullptr)
        return false;

    auto plugin = edit->getPluginCache().getPluginFor (te::EditItemID::fromRawID (pluginId));
    if (plugin == nullptr)
        return false;

    plugin->setEnabled (enabled);
    return true;
}

bool SessionController::removePlugin (uint64_t pluginId)
{
    if (edit == nullptr)
        return false;

    auto plugin = edit->getPluginCache().getPluginFor (te::EditItemID::fromRawID (pluginId));
    if (plugin == nullptr)
        return false;

    plugin->deleteFromParent();
    return true;
}

std::unique_ptr<te::Plugin::EditorComponent> SessionController::createPluginEditor (uint64_t pluginId)
{
    if (edit == nullptr)
        return {};

    auto plugin = edit->getPluginCache().getPluginFor (te::EditItemID::fromRawID (pluginId));
    if (plugin == nullptr)
        return {};

    return plugin->createEditor();
}

void SessionController::showAudioSettings()
{
    EngineHelpers::showAudioDeviceSettings (engine);
}

te::AudioTrack* SessionController::getAudioTrack (int index) const
{
    if (edit == nullptr)
        return nullptr;

    auto tracks = te::getAudioTracks (*edit);
    if (index < 0 || index >= tracks.size())
        return nullptr;

    return tracks.getUnchecked (index);
}

te::MidiClip* SessionController::findMidiClipById (uint64_t clipId) const
{
    if (edit == nullptr)
        return nullptr;

    auto* clip = findClipById (clipId);
    return dynamic_cast<te::MidiClip*> (clip);
}

te::Clip* SessionController::findClipById (uint64_t clipId) const
{
    if (clipId == 0)
        return nullptr;

    const auto id = te::EditItemID::fromRawID (clipId);
    return te::findClipForID (*edit, id);
}

double SessionController::getInsertionTimeSeconds() const
{
    if (transport != nullptr && transport->isPlaying())
        return transport->getPosition().inSeconds();

    return cursorTimeSeconds;
}

void SessionController::updateTrackNames()
{
    if (edit == nullptr)
        return;

    auto tracks = te::getAudioTracks (*edit);
    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks.getUnchecked (i);
        if (track->getName().isEmpty())
            track->setName ("Track " + juce::String (i + 1));
    }
}

void SessionController::ensureTrackStateSize()
{
    const int count = getTrackCount();
    if (count <= 0)
    {
        trackArmed.clear();
        return;
    }

    trackArmed.resize ((size_t) count, false);
}

int SessionController::getInsertIndexBeforeVolume (te::AudioTrack& track) const
{
    auto plugins = track.pluginList.getPlugins();
    for (int i = 0; i < plugins.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugins[i].get()) != nullptr)
            return i;

    return plugins.size();
}

bool SessionController::isInstrumentPlugin (te::Plugin& plugin) const
{
    if (plugin.isSynth())
        return true;

    if (auto* external = dynamic_cast<const te::ExternalPlugin*> (&plugin))
    {
        if (external->desc.isInstrument)
            return true;

        const auto category = external->desc.category;
        if (category.containsIgnoreCase ("synth") || category.containsIgnoreCase ("instrument"))
            return true;
    }

    return false;
}

void SessionController::removeInstrumentPlugins (te::AudioTrack& track)
{
    juce::Array<te::Plugin*> toRemove;
    for (auto* plugin : track.pluginList)
        if (plugin != nullptr && isInstrumentPlugin (*plugin))
            toRemove.add (plugin);

    for (auto* plugin : toRemove)
        plugin->deleteFromParent();
}

bool SessionController::insertDefaultInstrumentIfAvailable (int trackIndex)
{
    auto choices = getInstrumentChoices();
    if (choices.empty())
        return false;

    const auto it = std::find_if (choices.begin(), choices.end(),
                                  [] (const PluginChoice& choice)
                                  {
                                      return choice.description.pluginFormatName == "AudioUnit";
                                  });

    const PluginChoice& choice = (it != choices.end()) ? *it : choices.front();
    return insertInstrument (trackIndex, choice) != 0;
}

void SessionController::configureMidiInputRoutingForLivePlay()
{
    if (edit == nullptr)
        return;

    ensureTrackStateSize();
    ensureMidiInputSelection();

    auto devices = engine.getDeviceManager().getMidiInDevices();
    if (devices.empty())
    {
        logRecordDebug ("configureMidiInputRoutingForLivePlay: no MIDI devices");
        return;
    }

    te::MidiInputDevice* selectedDevice = nullptr;
    for (auto& device : devices)
    {
        if (device->getDeviceID() == selectedMidiDeviceId)
        {
            selectedDevice = device.get();
            break;
        }
    }

    if (selectedDevice == nullptr)
    {
        selectedDevice = devices.front().get();
        selectedMidiDeviceId = selectedDevice->getDeviceID();
    }

    if (! selectedDevice->isEnabled())
        selectedDevice->setEnabled (true);

    devices = engine.getDeviceManager().getMidiInDevices();
    selectedDevice = nullptr;
    for (auto& device : devices)
    {
        if (device->getDeviceID() == selectedMidiDeviceId)
        {
            selectedDevice = device.get();
            break;
        }
    }

    if (selectedDevice == nullptr && ! devices.empty())
    {
        selectedDevice = devices.front().get();
        selectedMidiDeviceId = selectedDevice->getDeviceID();
    }

    if (selectedDevice == nullptr)
    {
        logRecordDebug ("configureMidiInputRoutingForLivePlay: selected device null");
        return;
    }

    for (auto& device : devices)
    {
        if (device.get() == selectedDevice)
            device->setMonitorMode (te::InputDevice::MonitorMode::on);
        else
            device->setMonitorMode (te::InputDevice::MonitorMode::automatic);

        device->recordingEnabled = (device.get() == selectedDevice);
    }

    auto tracks = te::getAudioTracks (*edit);
    if (tracks.isEmpty())
        return;

    // If the monitored track has no instrument, live MIDI can appear "silent".
    // Ensure one exists for immediate play-through on fresh/empty tracks.
    if (selectedTrackIndex >= 0 && selectedTrackIndex < tracks.size())
    {
        if (auto* selectedTrack = tracks.getUnchecked (selectedTrackIndex))
        {
            bool hasInstrument = false;
            for (auto* plugin : selectedTrack->pluginList)
            {
                if (plugin != nullptr && isInstrumentPlugin (*plugin))
                {
                    hasInstrument = true;
                    break;
                }
            }

            if (! hasInstrument)
                [[ maybe_unused ]] const bool inserted = insertDefaultInstrumentIfAvailable (selectedTrackIndex);
        }
    }

    auto* instance = findMidiInputInstanceForSelectedDevice();
    if (instance == nullptr)
    {
        logRecordDebug ("configureMidiInputRoutingForLivePlay: no input instance for selected device id=" + selectedMidiDeviceId);
        return;
    }

    std::unordered_set<te::EditItemID> desiredTargets;
    bool anyArmed = false;
    for (int i = 0; i < tracks.size(); ++i)
    {
        if ((size_t) i < trackArmed.size() && trackArmed[(size_t) i])
        {
            desiredTargets.insert (tracks.getUnchecked (i)->itemID);
            anyArmed = true;
        }
    }

    if (! anyArmed && selectedTrackIndex >= 0 && selectedTrackIndex < tracks.size())
        desiredTargets.insert (tracks.getUnchecked (selectedTrackIndex)->itemID);

    auto existingTargets = instance->getTargets();
    for (const auto& target : existingTargets)
        if (desiredTargets.find (target) == desiredTargets.end())
            [[ maybe_unused ]] auto removed = instance->removeTarget (target, nullptr);

    for (const auto& target : desiredTargets)
    {
        [[ maybe_unused ]] auto result = instance->setTarget (target, true, nullptr);
        instance->setRecordingEnabled (target, true);
    }

    edit->getTransport().ensureContextAllocated (true);
    logRecordDebug ("configureMidiInputRoutingForLivePlay: device=" + selectedMidiDeviceId
                    + ", targets=" + juce::String (desiredTargets.size())
                    + ", armedAny=" + boolToText (anyArmed)
                    + ", selectedTrack=" + juce::String (selectedTrackIndex));
}

bool SessionController::prepareMidiRecording()
{
    if (edit == nullptr)
        return false;

    ensureTrackStateSize();
    ensureMidiInputSelection();
    bool anyArmed = false;
    for (auto armed : trackArmed)
        anyArmed |= armed;

    if (! anyArmed)
    {
        logRecordDebug ("prepareMidiRecording: failed - no armed tracks");
        engine.getUIBehaviour().showWarningMessage ("No tracks are armed for MIDI recording.");
        return false;
    }

    auto devices = engine.getDeviceManager().getMidiInDevices();
    if (devices.empty())
    {
        logRecordDebug ("prepareMidiRecording: failed - no MIDI devices");
        engine.getUIBehaviour().showWarningMessage ("No MIDI input devices available.");
        return false;
    }

    if (selectedMidiDeviceId.isEmpty())
        selectedMidiDeviceId = devices.front()->getDeviceID();

    te::MidiInputDevice* selectedDevice = nullptr;
    for (auto& device : devices)
    {
        if (device->getDeviceID() == selectedMidiDeviceId)
        {
            selectedDevice = device.get();
            break;
        }
    }

    if (selectedDevice == nullptr)
    {
        selectedDevice = devices.front().get();
        selectedMidiDeviceId = selectedDevice->getDeviceID();
    }

    if (! selectedDevice->isEnabled())
    {
        selectedDevice->setEnabled (true);
        devices = engine.getDeviceManager().getMidiInDevices();
        selectedDevice = nullptr;

        for (auto& device : devices)
        {
            if (device->getDeviceID() == selectedMidiDeviceId)
            {
                selectedDevice = device.get();
                break;
            }
        }

        if (selectedDevice == nullptr && ! devices.empty())
        {
            selectedDevice = devices.front().get();
            selectedMidiDeviceId = selectedDevice->getDeviceID();
        }
    }


    for (auto& device : devices)
    {
        if (device.get() == selectedDevice)
            device->setMonitorMode (te::InputDevice::MonitorMode::on);
        else
            device->setMonitorMode (te::InputDevice::MonitorMode::automatic);

        device->recordingEnabled = device.get() == selectedDevice;
    }

    edit->getTransport().ensureContextAllocated (true);

    auto tracks = te::getAudioTracks (*edit);
    auto* instance = findMidiInputInstanceForSelectedDevice();

    if (instance == nullptr)
    {
        logRecordDebug ("prepareMidiRecording: failed - no instance for device id=" + selectedMidiDeviceId);
        engine.getUIBehaviour().showWarningMessage ("Could not attach the selected MIDI input to this edit.");
        return false;
    }

    std::unordered_set<te::EditItemID> armedTargets;
    for (int i = 0; i < tracks.size(); ++i)
        if (trackArmed[(size_t) i])
            armedTargets.insert (tracks.getUnchecked (i)->itemID);

    auto targets = instance->getTargets();
    for (const auto& target : targets)
        if (armedTargets.find (target) == armedTargets.end())
            [[ maybe_unused ]] auto removed = instance->removeTarget (target, &edit->getUndoManager());

    bool assigned = false;
    juce::String lastTargetError;
    for (int i = 0; i < tracks.size(); ++i)
    {
        if (! trackArmed[(size_t) i])
            continue;

        if (auto* track = tracks.getUnchecked (i))
        {
            auto targetResult = instance->setTarget (track->itemID, true, &edit->getUndoManager());
            if (! targetResult)
            {
                lastTargetError = targetResult.error();
                continue;
            }

            instance->setRecordingEnabled (track->itemID, true);
            assigned = true;
        }
    }

    if (! assigned)
    {
        auto message = juce::String ("Could not arm any track targets for MIDI recording.");
        if (lastTargetError.isNotEmpty())
            message << "\n" << lastTargetError;
        logRecordDebug ("prepareMidiRecording: failed - no targets assigned. lastError=" + lastTargetError);
        engine.getUIBehaviour().showWarningMessage (message);
        return false;
    }

    edit->getTransport().ensureContextAllocated (true);
    edit->restartPlayback();
    logRecordDebug ("prepareMidiRecording: success. selectedDevice=" + selectedMidiDeviceId
                    + ", armed={" + summariseTrackArmed (trackArmed) + "}");
    return true;
}

te::MidiNote* SessionController::findMidiNote (te::MidiClip& clip, const juce::ValueTree& noteState) const
{
    return clip.getSequence().getNoteFor (noteState);
}

te::InputDeviceInstance* SessionController::findMidiInputInstanceForSelectedDevice() const
{
    if (edit == nullptr)
        return nullptr;

    for (auto* instance : edit->getAllInputDevices())
    {
        if (instance == nullptr)
            continue;

        auto& input = instance->getInputDevice();
        if (input.getDeviceType() != te::InputDevice::physicalMidiDevice)
            continue;

        if (selectedMidiDeviceId.isEmpty() || input.getDeviceID() == selectedMidiDeviceId)
            return instance;
    }

    logRecordDebug ("findMidiInputInstanceForSelectedDevice: no match for id=" + selectedMidiDeviceId);
    return nullptr;
}

void SessionController::commitRecordingPreviewFallback (const std::unordered_map<uint64_t, RecordingPreviewState>& snapshot,
                                                        const juce::Array<int>& midiClipCountsBefore,
                                                        double stopTimeSeconds)
{
    if (edit == nullptr || snapshot.empty())
        return;

    auto tracks = te::getAudioTracks (*edit);
    if (tracks.isEmpty())
        return;

    auto countMidiClipsOnTrack = [] (te::AudioTrack& track)
    {
        int midiClips = 0;
        for (auto* clip : track.getClips())
            if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                ++midiClips;
        return midiClips;
    };

    auto& undo = edit->getUndoManager();
    bool addedAnyFallbackClips = false;

    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks.getUnchecked (trackIndex);
        if (track == nullptr)
            continue;

        const auto trackId = track->itemID.getRawID();
        auto it = snapshot.find (trackId);
        if (it == snapshot.end())
            continue;

        const int beforeCount = trackIndex < midiClipCountsBefore.size() ? midiClipCountsBefore[trackIndex] : 0;
        const int afterCount = countMidiClipsOnTrack (*track);
        if (afterCount > beforeCount)
            continue; // Tracktion created a clip; don't duplicate.

        auto notes = it->second.finishedNotes;
        notes.reserve (notes.size() + it->second.activeNotes.size());
        for (const auto& active : it->second.activeNotes)
        {
            NotePreview n;
            n.startSeconds = active.second;
            n.lengthSeconds = juce::jmax (0.01, stopTimeSeconds - active.second);
            n.noteNumber = active.first;
            notes.push_back (n);
        }

        if (notes.empty())
            continue;

        const double previewStart = juce::jmax (0.0, it->second.startSeconds);
        const double fallbackStart = juce::jmax (0.0, lastRecordStartSeconds);
        double minEventStart = std::numeric_limits<double>::max();
        for (const auto& n : notes)
            minEventStart = juce::jmin (minEventStart, n.startSeconds);
        for (const auto& s : it->second.sustainEvents)
            minEventStart = juce::jmin (minEventStart, s.timeSeconds);

        if (! std::isfinite (minEventStart))
            minEventStart = std::numeric_limits<double>::max();

        const bool previewValid = std::isfinite (previewStart) && previewStart >= 0.0 && previewStart <= (stopTimeSeconds + 30.0);
        const bool minEventValid = std::isfinite (minEventStart) && minEventStart >= 0.0 && minEventStart <= (stopTimeSeconds + 30.0);

        // Preserve absolute record timing: choose a clip start close to where events actually happened.
        double clipStart = 0.0;
        if (previewValid && minEventValid)
            clipStart = juce::jmin (previewStart, minEventStart);
        else if (minEventValid)
            clipStart = minEventStart;
        else if (previewValid)
            clipStart = previewStart;
        else
            clipStart = fallbackStart;

        if (! std::isfinite (clipStart) || clipStart < 0.0)
            clipStart = 0.0;

        std::vector<NotePreview> absoluteNotes;
        absoluteNotes.reserve (notes.size());
        for (const auto& n : notes)
        {
            NotePreview nn = n;
            nn.startSeconds = std::isfinite (nn.startSeconds) ? juce::jmax (0.0, nn.startSeconds) : clipStart;
            nn.lengthSeconds = juce::jmax (0.01, nn.lengthSeconds);
            absoluteNotes.push_back (nn);
        }

        double clipEnd = clipStart + 0.25;
        for (const auto& n : absoluteNotes)
            clipEnd = juce::jmax (clipEnd, n.startSeconds + n.lengthSeconds);
        for (const auto& s : it->second.sustainEvents)
        {
            const double st = std::isfinite (s.timeSeconds) ? juce::jmax (0.0, s.timeSeconds) : clipStart;
            clipEnd = juce::jmax (clipEnd, st + 0.01);
        }

        if (clipEnd <= clipStart)
            clipEnd = clipStart + 0.25;

        // Guard against pathological timestamps producing invisible/off-world clips.
        const double maxReasonableEnd = juce::jmax (clipStart + 0.25, stopTimeSeconds + 8.0);
        if (! std::isfinite (clipEnd) || clipEnd > maxReasonableEnd)
            clipEnd = maxReasonableEnd;

        undo.beginNewTransaction ("Commit MIDI Recording Fallback");
        auto clip = track->insertMIDIClip ("Recorded MIDI",
                                           { te::TimePosition::fromSeconds (clipStart),
                                             te::TimeDuration::fromSeconds (clipEnd - clipStart) },
                                           nullptr);
        if (clip == nullptr)
            continue;

        const auto clipStartBeat = te::toBeats (te::TimePosition::fromSeconds (clipStart), edit->tempoSequence);
        auto& seq = clip->getSequence();

        for (const auto& n : absoluteNotes)
        {
            const auto noteStartBeatAbs = te::toBeats (te::TimePosition::fromSeconds (n.startSeconds), edit->tempoSequence);
            const auto noteEndBeatAbs = te::toBeats (te::TimePosition::fromSeconds (n.startSeconds + juce::jmax (0.01, n.lengthSeconds)),
                                                     edit->tempoSequence);
            const auto relStart = te::BeatPosition::fromBeats (juce::jmax (0.0, (noteStartBeatAbs - clipStartBeat).inBeats()));
            const auto len = te::BeatDuration::fromBeats (juce::jmax (0.001, (noteEndBeatAbs - noteStartBeatAbs).inBeats()));
            seq.addNote (n.noteNumber, relStart, len, 100, 0, &undo);
        }

        for (const auto& s : it->second.sustainEvents)
        {
            const double sustainTime = std::isfinite (s.timeSeconds) ? juce::jmax (0.0, s.timeSeconds) : clipStart;
            const auto sustainBeatAbs = te::toBeats (te::TimePosition::fromSeconds (sustainTime), edit->tempoSequence);
            const auto relBeat = te::BeatPosition::fromBeats (juce::jmax (0.0, (sustainBeatAbs - clipStartBeat).inBeats()));
            const int ccValue = juce::jlimit (0, 127, s.value) << 7;
            seq.addControllerEvent (relBeat, 64, ccValue, &undo);
        }

        clip->setMidiChannel (te::MidiChannel (1));
        addedAnyFallbackClips = true;
        cursorTimeSeconds = clipStart;
        logRecordDebug ("fallback committed MIDI clip on track " + juce::String (trackIndex)
                        + " with notes=" + juce::String ((int) absoluteNotes.size())
                        + ", sustainEvents=" + juce::String ((int) it->second.sustainEvents.size())
                        + ", previewStart=" + juce::String (previewStart, 3)
                        + ", requestedStart=" + juce::String (fallbackStart, 3)
                        + ", clipStart=" + juce::String (clipStart, 3)
                        + ", clipEnd=" + juce::String (clipEnd, 3));
    }

    if (addedAnyFallbackClips)
        te::EditFileOperations (*edit).save (true, true, false);
}

te::Plugin::Ptr SessionController::duplicateInstrumentPlugin (te::AudioTrack& destination,
                                                              const te::AudioTrack& source)
{
    if (edit == nullptr)
        return {};

    te::Plugin::Ptr sourceInstrument;
    for (auto* plugin : source.pluginList)
    {
        if (plugin != nullptr && isInstrumentPlugin (*plugin))
        {
            sourceInstrument = plugin;
            break;
        }
    }

    if (sourceInstrument == nullptr)
        return {};

    auto stateCopy = sourceInstrument->state.createCopy();
    auto plugin = edit->getPluginCache().createNewPlugin (stateCopy);
    if (plugin == nullptr)
        return {};

    const int insertIndex = getInsertIndexBeforeVolume (destination);
    destination.pluginList.insertPlugin (plugin, insertIndex, nullptr);
    plugin->setEnabled (sourceInstrument->isEnabled());

    auto label = source.state.getProperty (kInstrumentLabelId).toString().trim();
    if (! label.isEmpty())
        destination.state.setProperty (kInstrumentLabelId, label, nullptr);
    return plugin;
}
