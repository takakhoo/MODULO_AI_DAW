#include "SessionController.h"

#include <algorithm>
#include <cmath>
#include "PluginWindow.h"


namespace
{
constexpr float kMinTrackDb = -60.0f;
constexpr float kMaxTrackDb = 6.0f;
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
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Reason");
    tempDir.createDirectory();
    auto editFile = tempDir.getNonexistentChildFile ("Untitled", ".tracktionedit", false);
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

    transport->stop (false, false);
    cursorTimeSeconds = transport->getPosition().inSeconds();
}

bool SessionController::toggleRecord()
{
    if (edit == nullptr || transport == nullptr)
        return false;

    const bool wasRecording = transport->isRecording();

    if (! wasRecording)
    {
        if (! prepareMidiRecording())
            return false;

        transport->setPosition (te::TimePosition::fromSeconds (cursorTimeSeconds));
    }

    EngineHelpers::toggleRecord (*edit);

    if (wasRecording)
        te::EditFileOperations (*edit).save (true, true, false);

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
}

void SessionController::setTrackArmed (int index, bool shouldArm)
{
    if (index < 0)
        return;

    ensureTrackStateSize();
    if (index >= (int) trackArmed.size())
        return;

    trackArmed[(size_t) index] = shouldArm;
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
        for (auto* plugin : track->pluginList)
        {
            if (plugin != nullptr && isInstrumentPlugin (*plugin))
            {
                instrument = plugin->getName();
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

    const auto& tempo = edit->tempoSequence.getTempoAt (te::TimePosition::fromSeconds (cursorTimeSeconds));
    return tempo.getBpm();
}

void SessionController::setTempoBpm (double bpm)
{
    if (edit == nullptr)
        return;

    const double clamped = juce::jlimit (20.0, 300.0, bpm);
    auto& tempo = edit->tempoSequence.getTempoAt (te::TimePosition::fromSeconds (cursorTimeSeconds));
    tempo.setBpm (clamped);
}

juce::String SessionController::getTimeSignature() const
{
    if (edit == nullptr)
        return "4/4";

    auto& timeSig = edit->tempoSequence.getTimeSigAt (te::TimePosition::fromSeconds (cursorTimeSeconds));
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

    return clips;
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
                                                 const std::vector<ChordOption>& options)
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
            const auto startBeat = te::BeatPosition::fromBeats (startBeats);
            const auto durBeat = te::BeatDuration::fromBeats (juce::jmax (0.25, lengthBeats));

            for (auto pitch : onsets[i].pitches)
                seq.addNote (pitch, startBeat, durBeat, 100, 0, &undo);

            auto label = juce::ValueTree ("CHORD_LABEL");
            label.setProperty ("beat", startBeats, nullptr);
            label.setProperty ("symbol", onsets[i].symbol, nullptr);
            labelsState.addChild (label, -1, nullptr);
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

bool SessionController::prepareMidiRecording()
{
    if (edit == nullptr)
        return false;

    ensureTrackStateSize();
    bool anyArmed = false;
    for (auto armed : trackArmed)
        anyArmed |= armed;

    if (! anyArmed)
    {
        engine.getUIBehaviour().showWarningMessage ("No tracks are armed for MIDI recording.");
        return false;
    }

    auto devices = engine.getDeviceManager().getMidiInDevices();
    if (devices.empty())
    {
        engine.getUIBehaviour().showWarningMessage ("No MIDI input devices available.");
        return false;
    }

    if (selectedMidiDeviceId.isEmpty())
        selectedMidiDeviceId = devices.front()->getDeviceID();

    for (auto& device : devices)
    {
        const bool enable = device->getDeviceID() == selectedMidiDeviceId;
        device->setMonitorMode (te::InputDevice::MonitorMode::automatic);
        device->setEnabled (enable);
    }

    edit->getTransport().ensureContextAllocated();

    auto tracks = te::getAudioTracks (*edit);
    for (auto* instance : edit->getAllInputDevices())
    {
        auto& inputDevice = instance->getInputDevice();
        if (inputDevice.getDeviceID() != selectedMidiDeviceId)
            continue;

        auto targets = instance->getTargets();
        for (const auto& target : targets)
        {
            bool keep = false;
            for (int i = 0; i < tracks.size(); ++i)
            {
                if (trackArmed[(size_t) i] && tracks.getUnchecked (i)->itemID == target)
                {
                    keep = true;
                    break;
                }
            }

            if (! keep)
                [[ maybe_unused ]] auto removed = instance->removeTarget (target, &edit->getUndoManager());
        }

        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! trackArmed[(size_t) i])
                continue;

            if (auto* track = tracks.getUnchecked (i))
            {
                [[ maybe_unused ]] auto res = instance->setTarget (track->itemID, true, &edit->getUndoManager(), std::nullopt);
                instance->setRecordingEnabled (track->itemID, true);
            }
        }
    }

    edit->restartPlayback();
    return true;
}

te::MidiNote* SessionController::findMidiNote (te::MidiClip& clip, const juce::ValueTree& noteState) const
{
    return clip.getSequence().getNoteFor (noteState);
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
    return plugin;
}
