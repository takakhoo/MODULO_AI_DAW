#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace reason::trackcolours
{
    inline float shadeNorm (int index)
    {
        const int wrapped = ((index % 8) + 8) % 8;
        return (float) wrapped / 7.0f;
    }

    inline juce::Colour base (int index)
    {
        const float n = shadeNorm (index);
        const float hue = 0.112f; // gold hue
        const float sat = juce::jmap (n, 0.42f, 0.66f);
        const float val = juce::jmap (n, 0.19f, 0.33f);
        return juce::Colour::fromHSV (hue, sat, val, 1.0f);
    }

    inline juce::Colour highlight (int index)
    {
        const float n = shadeNorm (index);
        const float hue = 0.112f;
        const float sat = juce::jmap (n, 0.48f, 0.72f);
        const float val = juce::jmap (n, 0.30f, 0.48f);
        return juce::Colour::fromHSV (hue, sat, val, 1.0f);
    }

    inline juce::Colour accent (int index)
    {
        const float n = shadeNorm (index);
        const float hue = 0.112f;
        const float sat = juce::jmap (n, 0.60f, 0.82f);
        const float val = juce::jmap (n, 0.56f, 0.82f);
        return juce::Colour::fromHSV (hue, sat, val, 1.0f);
    }
}
