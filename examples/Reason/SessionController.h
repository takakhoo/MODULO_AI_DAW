#pragma once

#include <JuceHeader.h>

#include "../common/Utilities.h"

namespace te = tracktion;

class ExtendedUIBehaviour : public te::UIBehaviour
{
public:
    ExtendedUIBehaviour() = default;
    ~ExtendedUIBehaviour() override = default;
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

    SessionController();

    te::Engine& getEngine() noexcept { return engine; }
    te::Edit& getEdit() noexcept { return edit; }
    te::TransportControl& getTransport() noexcept { return transport; }

    void togglePlay();
    void stop();

    void setSelectedTrack (int index);
    int getSelectedTrack() const noexcept { return selectedTrackIndex; }

    int getTrackCount() const;
    juce::StringArray getTrackNames() const;
    juce::Array<float> getTrackVolumes() const;

    void setTrackVolume (int index, float normalizedValue);
    float getTrackVolume (int index) const;

    double getCurrentTimeSeconds() const;

    void setCursorTimeSeconds (double seconds);
    double getCursorTimeSeconds() const noexcept { return cursorTimeSeconds; }
    double getInsertionTimeSeconds() const;

    bool importAudio (int trackIndex, const juce::File& file, double startTimeSeconds);
    bool importMidi (int trackIndex, const juce::File& file, double startTimeSeconds);

    std::vector<ClipInfo> getClipsForTrack (int trackIndex) const;

    bool moveClip (uint64_t clipId, double newStartSeconds);

    void showAudioSettings();

private:
    te::AudioTrack* getAudioTrack (int index) const;
    te::Clip* findClipById (uint64_t clipId) const;
    void updateTrackNames();

    te::Engine engine { "Reason", std::make_unique<ExtendedUIBehaviour>(), nullptr };
    te::Edit edit { engine, te::Edit::EditRole::forEditing };
    te::TransportControl& transport { edit.getTransport() };

    int selectedTrackIndex = 0;
    double cursorTimeSeconds = 0.0;
};
