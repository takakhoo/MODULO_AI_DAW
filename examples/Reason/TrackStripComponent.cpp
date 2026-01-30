#include "TrackStripComponent.h"
#include "TrackColors.h"

static juce::Colour volumeColourFor (float value)
{
    const float v = juce::jlimit (0.0f, 1.0f, value);
    const float hue = juce::jmap (v, 0.55f, 0.08f);
    const float sat = juce::jmap (v, 0.35f, 0.7f);
    const float bright = juce::jmap (v, 0.35f, 0.9f);
    return juce::Colour::fromHSV (hue, sat, bright, 1.0f);
}

TrackStripComponent::TrackStripComponent (int index)
    : trackIndex (index)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (instrumentButton);
    addAndMakeVisible (fxButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);
    addAndMakeVisible (aiButton);

    addAndMakeVisible (numberLabel);
    numberLabel.setJustificationType (juce::Justification::centred);
    numberLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    numberLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xFF2D3A4A));
    numberLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    numberLabel.setText (juce::String (trackIndex + 1), juce::dontSendNotification);

    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    nameLabel.setEditable (false, true, true);
    nameLabel.onEditorHide = [this]
    {
        if (onNameChanged)
            onNameChanged (trackIndex, nameLabel.getText().trim());
    };

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange (0.0, 1.0, 0.001);
    volumeSlider.setColour (juce::Slider::trackColourId, volumeColourFor (0.8f));
    volumeSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF20252C));
    volumeSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFBFEDE4));
    volumeSlider.onValueChange = [this]
    {
        if (onVolumeChanged)
            onVolumeChanged (trackIndex, (float) volumeSlider.getValue());
        volumeSlider.setColour (juce::Slider::trackColourId, volumeColourFor ((float) volumeSlider.getValue()));
        repaint();
    };

    instrumentButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF27313C));
    instrumentButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    instrumentButton.onClick = [this]
    {
        if (onSelected)
            onSelected (trackIndex);
        if (onInstrumentClicked)
            onInstrumentClicked (trackIndex);
    };

    fxButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    fxButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    fxButton.onClick = [this]
    {
        if (onSelected)
            onSelected (trackIndex);
        if (onFxClicked)
            onFxClicked (trackIndex);
    };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    muteButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    muteButton.onClick = [this]
    {
        if (onSelected)
            onSelected (trackIndex);
        if (onMuteChanged)
            onMuteChanged (trackIndex, muteButton.getToggleState());
    };

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    soloButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    soloButton.onClick = [this]
    {
        if (onSelected)
            onSelected (trackIndex);
        if (onSoloChanged)
            onSoloChanged (trackIndex, soloButton.getToggleState());
    };

    aiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    aiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void TrackStripComponent::setTrackName (const juce::String& name)
{
    nameLabel.setText (name, juce::dontSendNotification);
}

void TrackStripComponent::setInstrumentName (const juce::String& name)
{
    auto label = name.isNotEmpty() ? name : "Instrument";
    instrumentButton.setButtonText (label);
    instrumentButton.setTooltip (label);
}

void TrackStripComponent::setTrackNumber (int number)
{
    numberLabel.setText (juce::String (number), juce::dontSendNotification);
    numberLabel.setColour (juce::Label::backgroundColourId,
                           reason::trackcolours::accent (trackIndex).withAlpha (0.9f));
}

void TrackStripComponent::setSelected (bool shouldSelect)
{
    isSelected = shouldSelect;
    repaint();
}

void TrackStripComponent::setVolumeNormalized (float value)
{
    volumeSlider.setValue (juce::jlimit (0.0f, 1.0f, value), juce::dontSendNotification);
    volumeSlider.setColour (juce::Slider::trackColourId, volumeColourFor (value));
}

void TrackStripComponent::setEffectCount (int count)
{
    const int clamped = juce::jmax (0, count);
    if (clamped > 0)
    {
        fxButton.setButtonText ("FX " + juce::String (clamped));
        fxButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3B4C2A));
    }
    else
    {
        fxButton.setButtonText ("FX");
        fxButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
    }
}

void TrackStripComponent::setMuted (bool shouldMute)
{
    muteButton.setToggleState (shouldMute, juce::dontSendNotification);
    const auto colour = shouldMute ? juce::Colour (0xFF1D6C67) : juce::Colour (0xFF2A2F3A);
    muteButton.setColour (juce::TextButton::buttonColourId, colour);
    muteButton.setColour (juce::TextButton::textColourOnId, shouldMute ? juce::Colours::black : juce::Colours::white);
    muteButton.setColour (juce::TextButton::textColourOffId, shouldMute ? juce::Colours::black : juce::Colours::white);
}

void TrackStripComponent::setSolo (bool shouldSolo)
{
    soloButton.setToggleState (shouldSolo, juce::dontSendNotification);
    const auto colour = shouldSolo ? juce::Colour (0xFFE1B948) : juce::Colour (0xFF2A2F3A);
    soloButton.setColour (juce::TextButton::buttonColourId, colour);
    soloButton.setColour (juce::TextButton::textColourOnId, shouldSolo ? juce::Colours::black : juce::Colours::white);
    soloButton.setColour (juce::TextButton::textColourOffId, shouldSolo ? juce::Colours::black : juce::Colours::white);
}

void TrackStripComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto bounds = r;
    const auto base = reason::trackcolours::base (trackIndex);
    const auto highlight = reason::trackcolours::highlight (trackIndex);
    const auto selectedBoost = isSelected ? 0.12f : 0.0f;
    auto top = highlight.withMultipliedBrightness (1.0f + selectedBoost);
    auto bottom = base.withMultipliedBrightness (1.0f + selectedBoost);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) r.getBottom(), false));
    g.fillRect (r);

    g.setColour (juce::Colour (0xFF2E3239));
    g.drawRect (bounds);

    if (isSelected)
    {
        auto accent = reason::trackcolours::accent (trackIndex);
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (bounds);
        g.setColour (accent);
        g.drawRect (bounds, 2);
        g.fillRect (bounds.removeFromLeft (4));
    }
}

void TrackStripComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    auto top = r.removeFromTop (26);
    auto numberArea = top.removeFromLeft (30);
    numberLabel.setBounds (numberArea);

    auto buttons = top.removeFromRight (150);
    nameLabel.setBounds (top);
    muteButton.setBounds (buttons.removeFromLeft (36).reduced (0, 2));
    soloButton.setBounds (buttons.removeFromLeft (36).reduced (0, 2));
    fxButton.setBounds (buttons.removeFromLeft (46).reduced (0, 2));
    aiButton.setBounds (buttons.removeFromLeft (32).reduced (0, 2));

    r.removeFromTop (6);
    instrumentButton.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    volumeSlider.setBounds (r.removeFromTop (16).reduced (12, 0));
}

void TrackStripComponent::mouseDown (const juce::MouseEvent&)
{
    if (onSelected)
        onSelected (trackIndex);
}

void TrackStripComponent::mouseUp (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        if (onContextMenuRequested)
            onContextMenuRequested (trackIndex, this);
    }
}
