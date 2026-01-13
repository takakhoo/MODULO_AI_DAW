#include "TrackListComponent.h"

TrackListComponent::TrackListComponent()
{
    setOpaque (true);
}

void TrackListComponent::setTracks (const juce::StringArray& trackNames)
{
    tracks = trackNames;
    rebuildStrips();
}

void TrackListComponent::setTrackVolumes (const juce::Array<float>& volumes)
{
    trackVolumes = volumes;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const float v = i < trackVolumes.size() ? trackVolumes[i] : 0.8f;
            strip->setVolumeNormalized (v);
        }
    }
}

void TrackListComponent::setSelectedIndex (int index)
{
    selectedIndex = juce::jlimit (0, juce::jmax (0, strips.size() - 1), index);
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setSelected (i == selectedIndex);
}

void TrackListComponent::resized()
{
    auto r = getLocalBounds();

    for (int i = 0; i < strips.size(); ++i)
    {
        auto row = r.removeFromTop (rowHeight);
        strips[i]->setBounds (row);
    }
}

void TrackListComponent::rebuildStrips()
{
    strips.clear();

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* strip = strips.add (new TrackStripComponent (i));
        strip->setTrackName (tracks[i]);
        strip->setSelected (i == selectedIndex);

        const float v = i < trackVolumes.size() ? trackVolumes[i] : 0.8f;
        strip->setVolumeNormalized (v);

        strip->onSelected = [this] (int index)
        {
            setSelectedIndex (index);
            if (onSelectionChanged)
                onSelectionChanged (index);
        };

        strip->onVolumeChanged = [this] (int index, float value)
        {
            if (onVolumeChanged)
                onVolumeChanged (index, value);
        };

        addAndMakeVisible (strip);
    }

    resized();
    repaint();
}
