#pragma once

#include <JuceHeader.h>
#include <optional>
#include <unordered_map>

#include "../common/Utilities.h"

namespace te = tracktion;

class ExtendedUIBehaviour : public te::UIBehaviour
{
public:
    ExtendedUIBehaviour() = default;
    ~ExtendedUIBehaviour() override = default;

    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& state) override;
    void recreatePluginWindowContentAsync (te::Plugin& plugin) override;
};

class SessionController
{
public:
    enum class ClipType
    {
        audio,
        midi
    };

    struct NotePreview
    {
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        int noteNumber = 0;
    };

    struct ClipInfo
    {
        uint64_t id = 0;
        int trackIndex = 0;
        ClipType type = ClipType::audio;
        juce::String name;
        juce::String sourcePath;
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        std::vector<NotePreview> notes;
    };

    struct MidiNoteInfo
    {
        juce::ValueTree state;
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        int noteNumber = 0;
        int velocity = 100;
    };

    struct SustainEvent
    {
        double timeSeconds = 0.0;
        int value = 0;
    };

    struct PluginChoice
    {
        juce::String type;
        juce::PluginDescription description;
    };

    struct PluginInfo
    {
        uint64_t id = 0;
        juce::String name;
        bool isInstrument = false;
        bool enabled = true;
    };

    struct RealchordsNoteEvent
    {
        int frame = 0;
        int pitch = 0;
        bool on = false;
    };

    struct ChordFrame
    {
        int frame = 0;
        juce::String symbol;
        juce::Array<int> pitches;
        bool on = false;
    };

    struct ChordOption
    {
        juce::String name;
        int frameCount = 0;
        std::vector<ChordFrame> frames;
    };

    struct ChordLabel
    {
        double startSeconds = 0.0;
        juce::String symbol;
    };

    enum class ChordPlaybackStyle
    {
        block,
        arpeggio
    };

    SessionController();

    te::Engine& getEngine() noexcept { return engine; }
    te::Edit& getEdit() noexcept { jassert (edit != nullptr); return *edit; }
    te::TransportControl& getTransport() noexcept { jassert (transport != nullptr); return *transport; }

    void togglePlay();
    void stop();
    bool toggleRecord();
    bool isRecording() const;

    bool createNewEdit (const juce::File& editFile);
    bool openEdit (const juce::File& editFile);
    bool saveEdit();
    bool saveEditAs (const juce::File& editFile);

    void setSelectedTrack (int index);
    int getSelectedTrack() const noexcept { return selectedTrackIndex; }

    void setTrackArmed (int index, bool shouldArm);
    bool isTrackArmed (int index) const;
    juce::Array<bool> getTrackArmedStates() const;

    juce::StringArray getMidiInputDeviceNames() const;
    int getSelectedMidiInputIndex() const;
    juce::String getSelectedMidiInputName() const;
    void setSelectedMidiInputIndex (int index);

    int getTrackCount() const;
    juce::StringArray getTrackNames() const;
    juce::StringArray getTrackInstrumentNames() const;
    juce::Array<float> getTrackVolumes() const;
    juce::Array<int> getTrackEffectCounts() const;
    juce::Array<bool> getTrackMuteStates() const;
    juce::Array<bool> getTrackSoloStates() const;
    juce::String getTrackName (int index) const;
    std::vector<PluginInfo> getTrackEffects (int trackIndex) const;

    void setTrackVolume (int index, float normalizedValue);
    float getTrackVolume (int index) const;
    void setTrackMute (int index, bool shouldMute);
    void setTrackSolo (int index, bool shouldSolo);
    void setTrackName (int index, const juce::String& name);

    double getCurrentTimeSeconds() const;
    double getTempoBpm() const;
    void setTempoBpm (double bpm);
    juce::String getTimeSignature() const;
    bool setTimeSignature (const juce::String& text);
    juce::String getKeySignature() const;
    bool setKeySignature (const juce::String& text);
    juce::String getBarsAndBeatsText (double seconds) const;
    bool isMetronomeEnabled() const;
    void setMetronomeEnabled (bool enabled);

    bool undo();
    bool redo();

    void setCursorTimeSeconds (double seconds);
    double getCursorTimeSeconds() const noexcept { return cursorTimeSeconds; }
    double getInsertionTimeSeconds() const;

    bool importAudio (int trackIndex, const juce::File& file, double startTimeSeconds);
    bool importMidi (int trackIndex, const juce::File& file, double startTimeSeconds);

    std::vector<ClipInfo> getClipsForTrack (int trackIndex) const;
    std::optional<ClipInfo> getClipInfo (uint64_t clipId) const;

    bool moveClip (uint64_t clipId, double newStartSeconds);
    bool moveClipToTrack (uint64_t clipId, int targetTrackIndex, double newStartSeconds);
    bool resizeClipLength (uint64_t clipId, double newLengthSeconds);
    bool resizeClipRange (uint64_t clipId, double newStartSeconds, double newLengthSeconds);
    uint64_t duplicateClipToTrack (uint64_t clipId, int targetTrackIndex, double newStartSeconds);
    bool deleteClip (uint64_t clipId);
    bool deleteTrack (int trackIndex);
    bool duplicateTrack (int trackIndex);

    std::vector<MidiNoteInfo> getMidiNotesForClip (uint64_t clipId) const;
    std::vector<SustainEvent> getSustainEventsForClip (uint64_t clipId) const;
    bool addMidiNote (uint64_t clipId, int noteNumber, double startSeconds, double lengthSeconds);
    bool updateMidiNote (uint64_t clipId, const juce::ValueTree& noteState,
                         double startSeconds, double lengthSeconds, int noteNumber);
    bool resizeMidiNote (uint64_t clipId, const juce::ValueTree& noteState, double lengthSeconds);
    bool deleteMidiNote (uint64_t clipId, const juce::ValueTree& noteState);

    bool buildRealchordsNoteEvents (uint64_t clipId, std::vector<RealchordsNoteEvent>& events,
                                    int& endFrame) const;
    bool createChordOptionTracks (uint64_t sourceClipId, const std::vector<ChordOption>& options,
                                  ChordPlaybackStyle playbackStyle = ChordPlaybackStyle::block);
    std::vector<ChordLabel> getChordLabelsForClip (uint64_t clipId) const;
    std::vector<ChordLabel> getChordLabelsForTrack (int trackIndex) const;

    std::vector<PluginChoice> getInstrumentChoices() const;
    std::vector<PluginChoice> getEffectChoices() const;
    std::vector<PluginInfo> getTrackPlugins (int trackIndex) const;
    uint64_t insertInstrument (int trackIndex, const PluginChoice& choice);
    uint64_t insertEffect (int trackIndex, const PluginChoice& choice);
    bool showPluginWindow (uint64_t pluginId);
    bool setPluginEnabled (uint64_t pluginId, bool enabled);
    bool removePlugin (uint64_t pluginId);
    std::unique_ptr<te::Plugin::EditorComponent> createPluginEditor (uint64_t pluginId);

    void showAudioSettings();
    void ensureMidiInputSelection();
    void refreshMidiLiveRouting();

private:
    struct RecordingPreviewState
    {
        double startSeconds = 0.0;
        std::unordered_map<int, double> activeNotes;
        std::vector<NotePreview> finishedNotes;
        std::vector<SustainEvent> sustainEvents;
    };

    te::AudioTrack* getAudioTrack (int index) const;
    te::Clip* findClipById (uint64_t clipId) const;
    te::MidiClip* findMidiClipById (uint64_t clipId) const;
    void updateTrackNames();
    void ensureTrackStateSize();
    int getInsertIndexBeforeVolume (te::AudioTrack& track) const;
    bool isInstrumentPlugin (te::Plugin& plugin) const;
    void removeInstrumentPlugins (te::AudioTrack& track);
    bool insertDefaultInstrumentIfAvailable (int trackIndex);
    void configureMidiInputRoutingForLivePlay();
    bool prepareMidiRecording();
    te::InputDeviceInstance* findMidiInputInstanceForSelectedDevice() const;
    void commitRecordingPreviewFallback (const std::unordered_map<uint64_t, RecordingPreviewState>& snapshot,
                                         const juce::Array<int>& midiClipCountsBefore,
                                         double stopTimeSeconds);
    void ensureMidiRecordingActive();
    te::MidiNote* findMidiNote (te::MidiClip& clip, const juce::ValueTree& noteState) const;
    te::Plugin::Ptr duplicateInstrumentPlugin (te::AudioTrack& destination, const te::AudioTrack& source);
    std::optional<ClipInfo> buildRecordingPreview (te::AudioTrack& track, int trackIndex) const;

    te::Engine engine { "Modulo", std::make_unique<ExtendedUIBehaviour>(), nullptr };
    std::unique_ptr<te::Edit> edit;
    te::TransportControl* transport = nullptr;

    int selectedTrackIndex = 0;
    double cursorTimeSeconds = 0.0;
    double lastRecordStartSeconds = 0.0;
    juce::String selectedMidiDeviceId;
    std::vector<bool> trackArmed;
    mutable std::unordered_map<uint64_t, RecordingPreviewState> recordingPreview;
};
