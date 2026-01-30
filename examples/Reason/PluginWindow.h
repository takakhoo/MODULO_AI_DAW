#pragma once

#include <JuceHeader.h>

#include "../common/Utilities.h"

namespace te = tracktion;

std::unique_ptr<juce::Component> createReasonPluginWindow (te::PluginWindowState& state);
void recreateReasonPluginWindowContentAsync (te::Plugin& plugin);
