#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace reason::trackcolours
{
    inline juce::Colour base (int index)
    {
        const float hue = std::fmod (0.08f * (float) index, 1.0f);
        return juce::Colour::fromHSV (hue, 0.28f, 0.25f, 1.0f);
    }

    inline juce::Colour highlight (int index)
    {
        const float hue = std::fmod (0.08f * (float) index, 1.0f);
        return juce::Colour::fromHSV (hue, 0.35f, 0.32f, 1.0f);
    }

    inline juce::Colour accent (int index)
    {
        const float hue = std::fmod (0.08f * (float) index, 1.0f);
        return juce::Colour::fromHSV (hue, 0.55f, 0.55f, 1.0f);
    }
}
