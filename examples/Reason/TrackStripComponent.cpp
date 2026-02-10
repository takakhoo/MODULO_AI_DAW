#include "TrackStripComponent.h"
#include "TrackColors.h"

constexpr float kMinTrackDb = -10.0f;
constexpr float kMaxTrackDb = 6.0f;

static float normalizedToDb (float normalizedValue)
{
    const float norm = juce::jlimit (0.0f, 1.0f, normalizedValue);
    return kMinTrackDb + (kMaxTrackDb - kMinTrackDb) * norm;
}

static juce::String formatDb (float db)
{
    return juce::String (db, 1) + " dB";
}

static bool isPlaceholderInstrumentLabel (const juce::String& name)
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

TrackStripComponent::TrackStripComponent (int index)
    : trackIndex (index)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (gainLabel);
    addAndMakeVisible (gainValueLabel);
    addAndMakeVisible (panSlider);
    addAndMakeVisible (panLabel);
    addAndMakeVisible (instrumentButton);
    addAndMakeVisible (fxButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

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
    volumeSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFFD9DEE6));
    volumeSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF3E444D));
    volumeSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFF2F5FA));
    volumeSlider.setLookAndFeel (&gainSliderLaf);
    volumeSlider.onValueChange = [this]
    {
        if (onVolumeChanged)
            onVolumeChanged (trackIndex, (float) volumeSlider.getValue());
        updateGainReadout ((float) volumeSlider.getValue());
        repaint();
    };

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredLeft);
    gainLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    gainLabel.setFont (juce::FontOptions (11.0f, juce::Font::plain));

    gainValueLabel.setJustificationType (juce::Justification::centredRight);
    gainValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    gainValueLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    updateGainReadout ((float) volumeSlider.getValue());

    panLabel.setText ("Pan", juce::dontSendNotification);
    panLabel.setJustificationType (juce::Justification::centredLeft);
    panLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    panLabel.setFont (juce::FontOptions (11.0f, juce::Font::plain));

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.001);
    panSlider.setValue (0.0, juce::dontSendNotification);
    panSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFFE0B15A));
    panSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF3C3424));
    panSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFFFE4A6));
    panSlider.setLookAndFeel (&panSliderLaf);
    panSlider.onValueChange = [this]
    {
        if (onPanChanged)
            onPanChanged (trackIndex, (float) panSlider.getValue());
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
}

TrackStripComponent::~TrackStripComponent()
{
    volumeSlider.setLookAndFeel (nullptr);
    panSlider.setLookAndFeel (nullptr);
}

void TrackStripComponent::GainSliderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                   float sliderPos, float minSliderPos, float maxSliderPos,
                                                                   const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float trackHeight = juce::jlimit (6.0f, 12.0f, height * 0.6f);
    const float trackY = (float) y + ((float) height - trackHeight) * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, trackY, (float) width, trackHeight);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    const float fillWidth = juce::jlimit (0.0f, (float) width, sliderPos - (float) x);
    if (fillWidth > 0.0f)
    {
        auto fill = track.withWidth (fillWidth);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (fill, trackHeight * 0.5f);
    }

    const float thumbRadius = trackHeight * 0.65f;
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (sliderPos - thumbRadius,
                   track.getCentreY() - thumbRadius,
                   thumbRadius * 2.0f,
                   thumbRadius * 2.0f);
}

void TrackStripComponent::PanSliderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                  float sliderPos, float minSliderPos, float maxSliderPos,
                                                                  const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float trackHeight = juce::jlimit (5.0f, 10.0f, height * 0.52f);
    const float trackY = (float) y + ((float) height - trackHeight) * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, trackY, (float) width, trackHeight);
    const float centerX = track.getCentreX();

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    const float left = juce::jmin (centerX, sliderPos);
    const float right = juce::jmax (centerX, sliderPos);
    if (right - left > 0.5f)
    {
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (juce::Rectangle<float> (left, trackY, right - left, trackHeight), trackHeight * 0.5f);
    }

    g.setColour (juce::Colour (0xFFEFD29B).withAlpha (0.7f));
    g.drawLine (centerX, track.getY() - 1.0f, centerX, track.getBottom() + 1.0f, 1.1f);

    const float thumbRadius = trackHeight * 0.7f;
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (sliderPos - thumbRadius,
                   track.getCentreY() - thumbRadius,
                   thumbRadius * 2.0f,
                   thumbRadius * 2.0f);
    g.setColour (juce::Colour (0xFF4F3914).withAlpha (0.85f));
    g.drawEllipse (sliderPos - thumbRadius,
                   track.getCentreY() - thumbRadius,
                   thumbRadius * 2.0f,
                   thumbRadius * 2.0f,
                   1.0f);
}

void TrackStripComponent::setTrackName (const juce::String& name)
{
    nameLabel.setText (name, juce::dontSendNotification);
}

void TrackStripComponent::setInstrumentName (const juce::String& name)
{
    auto label = isPlaceholderInstrumentLabel (name) ? "Instrument" : name;
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
    updateGainReadout (value);
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

void TrackStripComponent::setPanNormalized (float value)
{
    panSlider.setValue (juce::jlimit (-1.0f, 1.0f, value), juce::dontSendNotification);
}

void TrackStripComponent::setMuted (bool shouldMute)
{
    muteButton.setToggleState (shouldMute, juce::dontSendNotification);
    const auto baseColour = juce::Colour (0xFF6EC7FF);
    const auto colour = shouldMute ? baseColour : baseColour.withMultipliedBrightness (0.55f);
    muteButton.setColour (juce::TextButton::buttonColourId, colour);
    muteButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    muteButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
}

void TrackStripComponent::setSolo (bool shouldSolo)
{
    soloButton.setToggleState (shouldSolo, juce::dontSendNotification);
    const auto baseColour = juce::Colour (0xFFF8E81C);
    const auto colour = shouldSolo ? baseColour : baseColour.withMultipliedBrightness (0.6f);
    soloButton.setColour (juce::TextButton::buttonColourId, colour);
    soloButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    soloButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
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

    if (! gainSliderBounds.isEmpty())
    {
        const float zeroNorm = (0.0f - kMinTrackDb) / (kMaxTrackDb - kMinTrackDb);
        const int markerX = gainSliderBounds.getX()
                            + (int) std::round (gainSliderBounds.getWidth() * zeroNorm);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine ((float) markerX, (float) gainSliderBounds.getY(),
                    (float) markerX, (float) gainSliderBounds.getBottom(), 1.5f);
    }
}

void TrackStripComponent::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    auto top = r.removeFromTop (24);
    auto numberArea = top.removeFromLeft (30);
    numberLabel.setBounds (numberArea);
    nameLabel.setBounds (top);

    r.removeFromTop (4);
    auto controls = r.removeFromTop (20);
    auto rightButtons = controls.removeFromRight (124);
    muteButton.setBounds (rightButtons.removeFromLeft (36).reduced (0, 1));
    soloButton.setBounds (rightButtons.removeFromLeft (36).reduced (0, 1));
    fxButton.setBounds (rightButtons.removeFromLeft (52).reduced (0, 1));

    const int instrumentWidth = juce::jmin (controls.getWidth(), 118);
    instrumentButton.setBounds (controls.withSizeKeepingCentre (instrumentWidth, controls.getHeight()));

    r.removeFromTop (4);
    auto gainRow = r.removeFromTop (18);
    gainLabel.setBounds (gainRow.removeFromLeft (34));
    gainValueLabel.setBounds (gainRow.removeFromRight (64));
    volumeSlider.setBounds (gainRow.reduced (2, 0));
    gainSliderBounds = volumeSlider.getBounds();

    r.removeFromTop (2);
    auto panRow = r.removeFromTop (18);
    panLabel.setBounds (panRow.removeFromLeft (34));
    panSlider.setBounds (panRow.reduced (2, 0));
    panSliderBounds = panSlider.getBounds();
}

void TrackStripComponent::mouseDown (const juce::MouseEvent&)
{
    isDragReorderActive = true;
    if (onSelected)
        onSelected (trackIndex);
}

void TrackStripComponent::mouseUp (const juce::MouseEvent& event)
{
    if (isDragReorderActive && onReorderDrag != nullptr)
    {
        const auto parentEvent = event.getEventRelativeTo (getParentComponent());
        onReorderDrag (trackIndex, (int) std::round (parentEvent.position.y), true);
    }
    isDragReorderActive = false;

    if (event.mods.isPopupMenu())
    {
        if (onContextMenuRequested)
            onContextMenuRequested (trackIndex, this);
    }
}

void TrackStripComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragReorderActive || onReorderDrag == nullptr)
        return;

    const auto parentEvent = event.getEventRelativeTo (getParentComponent());
    onReorderDrag (trackIndex, (int) std::round (parentEvent.position.y), false);
}

void TrackStripComponent::updateGainReadout (float normalizedValue)
{
    const float db = normalizedToDb (normalizedValue);
    gainValueLabel.setText (formatDb (db), juce::dontSendNotification);
}
