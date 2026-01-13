#include "SessionController.h"

namespace
{
constexpr float kMinTrackDb = -60.0f;
constexpr float kMaxTrackDb = 6.0f;
}

SessionController::SessionController()
{
    edit.ensureNumberOfAudioTracks (3);
    updateTrackNames();
}

void SessionController::togglePlay()
{
    EngineHelpers::togglePlay (edit);
}

void SessionController::stop()
{
    transport.stop (false, false);
    cursorTimeSeconds = transport.getPosition().inSeconds();
}

void SessionController::setSelectedTrack (int index)
{
    const int count = getTrackCount();
    selectedTrackIndex = juce::jlimit (0, juce::jmax (0, count - 1), index);
}

int SessionController::getTrackCount() const
{
    return te::getAudioTracks (edit).size();
}

juce::StringArray SessionController::getTrackNames() const
{
    juce::StringArray names;
    auto tracks = te::getAudioTracks (edit);

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

juce::Array<float> SessionController::getTrackVolumes() const
{
    juce::Array<float> volumes;
    auto tracks = te::getAudioTracks (edit);

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

double SessionController::getCurrentTimeSeconds() const
{
    return transport.getPosition().inSeconds();
}

void SessionController::setCursorTimeSeconds (double seconds)
{
    cursorTimeSeconds = juce::jmax (0.0, seconds);
    transport.setPosition (te::TimePosition::fromSeconds (cursorTimeSeconds));
}

bool SessionController::importAudio (int trackIndex, const juce::File& file, double startTimeSeconds)
{
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

bool SessionController::moveClip (uint64_t clipId, double newStartSeconds)
{
    auto* clip = findClipById (clipId);
    if (clip == nullptr)
        return false;

    auto& undoManager = edit.getUndoManager();
    undoManager.beginNewTransaction ("Move Clip");

    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, newStartSeconds));
    clip->setStart (start, false, true);
    cursorTimeSeconds = start.inSeconds();
    return true;
}

void SessionController::showAudioSettings()
{
    EngineHelpers::showAudioDeviceSettings (engine);
}

te::AudioTrack* SessionController::getAudioTrack (int index) const
{
    auto tracks = te::getAudioTracks (edit);
    if (index < 0 || index >= tracks.size())
        return nullptr;

    return tracks.getUnchecked (index);
}

te::Clip* SessionController::findClipById (uint64_t clipId) const
{
    if (clipId == 0)
        return nullptr;

    const auto id = te::EditItemID::fromRawID (clipId);
    return te::findClipForID (edit, id);
}

double SessionController::getInsertionTimeSeconds() const
{
    if (transport.isPlaying())
        return transport.getPosition().inSeconds();

    return cursorTimeSeconds;
}

void SessionController::updateTrackNames()
{
    auto tracks = te::getAudioTracks (edit);
    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks.getUnchecked (i);
        if (track->getName().isEmpty())
            track->setName ("Track " + juce::String (i + 1));
    }
}
