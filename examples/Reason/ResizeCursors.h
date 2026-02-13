#pragma once

#include <JuceHeader.h>

namespace ReasonResizeCursors
{
/** Returns a cursor shaped like a left square bracket [ for resizing the left edge of a note. */
juce::MouseCursor getLeftBracketResizeCursor();

/** Returns a cursor shaped like a right square bracket ] for resizing the right edge of a note. */
juce::MouseCursor getRightBracketResizeCursor();
}
