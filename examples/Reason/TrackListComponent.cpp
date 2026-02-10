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

void TrackListComponent::setTrackInstrumentNames (const juce::StringArray& instrumentNames)
{
    instruments = instrumentNames;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const auto name = i < instruments.size() ? instruments[i] : juce::String();
            strip->setInstrumentName (name);
        }
    }
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

void TrackListComponent::setTrackPans (const juce::Array<float>& pans)
{
    trackPans = pans;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const float p = i < trackPans.size() ? trackPans[i] : 0.0f;
            strip->setPanNormalized (p);
        }
    }
}

void TrackListComponent::setTrackEffectCounts (const juce::Array<int>& counts)
{
    trackEffectCounts = counts;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const int count = i < trackEffectCounts.size() ? trackEffectCounts[i] : 0;
            strip->setEffectCount (count);
        }
    }
}

void TrackListComponent::setTrackMuteStates (const juce::Array<bool>& muteStates)
{
    trackMuted = muteStates;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const bool muted = i < trackMuted.size() ? trackMuted[i] : false;
            strip->setMuted (muted);
        }
    }
}

void TrackListComponent::setTrackSoloStates (const juce::Array<bool>& soloStates)
{
    trackSoloed = soloStates;
    for (int i = 0; i < strips.size(); ++i)
    {
        if (auto* strip = strips[i])
        {
            const bool solo = i < trackSoloed.size() ? trackSoloed[i] : false;
            strip->setSolo (solo);
        }
    }
}

void TrackListComponent::setSelectedIndex (int index)
{
    selectedIndex = juce::jlimit (0, juce::jmax (0, strips.size() - 1), index);
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setSelected (i == selectedIndex);
}

void TrackListComponent::setHeaderHeight (int height)
{
    headerHeight = juce::jmax (0, height);
    resized();
    repaint();
}

void TrackListComponent::setVisibleTrackHeightOverride (int height)
{
    visibleHeightOverride = height;
    setScrollOffset (scrollOffset);
}

void TrackListComponent::setScrollOffset (int offset)
{
    const int visibleHeight = (visibleHeightOverride > 0)
        ? visibleHeightOverride
        : juce::jmax (0, getHeight() - headerHeight);
    const int maxOffset = juce::jmax (0, getContentHeight() - visibleHeight);
    scrollOffset = juce::jlimit (0, maxOffset, offset);
    resized();
    repaint();
}

int TrackListComponent::getTrackIndexAtY (int y) const noexcept
{
    const int contentY = y + scrollOffset - headerHeight;
    if (contentY < 0)
        return -1;

    const int index = contentY / rowHeight;
    return index >= 0 && index < tracks.size() ? index : -1;
}

void TrackListComponent::resized()
{
    for (int i = 0; i < strips.size(); ++i)
    {
        const int y = headerHeight + i * rowHeight - scrollOffset;
        strips[i]->setBounds (0, y, getWidth(), rowHeight);
    }
}

void TrackListComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF14110C));
    if (headerHeight <= 0)
        return;

    juce::Rectangle<int> header (0, 0, getWidth(), headerHeight);
    g.setColour (juce::Colour (0xFF3A2B14));
    g.fillRect (header);

    g.setColour (juce::Colour (0xFF5A431D));
    g.drawLine ((float) header.getX(), (float) header.getBottom() - 1.0f,
                (float) header.getRight(), (float) header.getBottom() - 1.0f);

    g.setColour (juce::Colour (0xFFFFE6B3).withAlpha (0.9f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("Tracks", header.reduced (10, 4), juce::Justification::centredLeft);

    if (dragReorderActive && dragReorderTargetIndex >= 0)
    {
        const int y = headerHeight + dragReorderTargetIndex * rowHeight - scrollOffset;
        g.setColour (juce::Colour (0xFFFFE6B3).withAlpha (0.9f));
        g.fillRect (0, y - 2, getWidth(), 3);
    }
}

void TrackListComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& details)
{
    if (event.mods.isCommandDown())
        return;

    if (details.deltaX != 0.0f)
        return;

    const int delta = (int) std::round (-details.deltaY * 60.0f);
    if (delta == 0)
        return;

    setScrollOffset (scrollOffset + delta);
    if (onVerticalScrollChanged)
        onVerticalScrollChanged (scrollOffset);
}

void TrackListComponent::rebuildStrips()
{
    strips.clear();

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* strip = strips.add (new TrackStripComponent (i));
        strip->setTrackNumber (i + 1);
        strip->setTrackName (tracks[i]);
        strip->setSelected (i == selectedIndex);

        const auto instrumentName = i < instruments.size() ? instruments[i] : juce::String();
        strip->setInstrumentName (instrumentName);

        const float v = i < trackVolumes.size() ? trackVolumes[i] : 0.8f;
        strip->setVolumeNormalized (v);

        const float p = i < trackPans.size() ? trackPans[i] : 0.0f;
        strip->setPanNormalized (p);

        const int effectCount = i < trackEffectCounts.size() ? trackEffectCounts[i] : 0;
        strip->setEffectCount (effectCount);

        const bool muted = i < trackMuted.size() ? trackMuted[i] : false;
        strip->setMuted (muted);

        const bool solo = i < trackSoloed.size() ? trackSoloed[i] : false;
        strip->setSolo (solo);

        strip->onSelected = [this] (int index)
        {
            setSelectedIndex (index);
            if (onSelectionChanged)
                onSelectionChanged (index);
        };

        strip->onNameChanged = [this] (int index, const juce::String& newName)
        {
            if (onNameChanged)
                onNameChanged (index, newName);
        };

        strip->onVolumeChanged = [this] (int index, float value)
        {
            if (onVolumeChanged)
                onVolumeChanged (index, value);
        };

        strip->onPanChanged = [this] (int index, float value)
        {
            if (onPanChanged)
                onPanChanged (index, value);
        };

        strip->onInstrumentClicked = [this] (int index)
        {
            if (onInstrumentClicked)
                onInstrumentClicked (index);
        };

        strip->onFxClicked = [this] (int index)
        {
            if (onFxClicked)
                onFxClicked (index);
        };

        strip->onMuteChanged = [this] (int index, bool shouldMute)
        {
            if (onMuteChanged)
                onMuteChanged (index, shouldMute);
        };

        strip->onSoloChanged = [this] (int index, bool shouldSolo)
        {
            if (onSoloChanged)
                onSoloChanged (index, shouldSolo);
        };

        strip->onContextMenuRequested = [this] (int index, juce::Component* source)
        {
            if (onContextMenuRequested)
                onContextMenuRequested (index, source);
        };

        strip->onReorderDrag = [this] (int sourceIndex, int pointerY, bool finished)
        {
            if (! dragReorderActive)
            {
                dragReorderActive = true;
                dragReorderSourceIndex = sourceIndex;
                dragReorderTargetIndex = sourceIndex;
            }

            int target = getTrackIndexAtY (pointerY);
            if (target < 0)
            {
                if (pointerY < headerHeight)
                    target = 0;
                else
                    target = juce::jmax (0, tracks.size() - 1);
            }
            dragReorderTargetIndex = juce::jlimit (0, juce::jmax (0, tracks.size() - 1), target);
            repaint();

            if (finished)
            {
                const int src = dragReorderSourceIndex;
                const int dst = dragReorderTargetIndex;
                dragReorderActive = false;
                dragReorderSourceIndex = -1;
                dragReorderTargetIndex = -1;
                repaint();

                if (src >= 0 && dst >= 0 && src != dst && onTrackReordered != nullptr)
                    onTrackReordered (src, dst);
            }
        };

        addAndMakeVisible (strip);
    }

    resized();
    repaint();
}
