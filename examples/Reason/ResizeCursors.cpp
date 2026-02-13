#include "ResizeCursors.h"

namespace ReasonResizeCursors
{
static juce::MouseCursor createLeftBracketCursor()
{
    const int size = 20;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    img.clear (img.getBounds(), juce::Colour (0x00000000));

    const int stroke = 1;
    const int margin = 3;
    const int barWidth = 2;
    const int capLen = 4;
    const int x0 = margin;
    const int y0 = margin;
    const int y1 = size - margin;

    g.setColour (juce::Colour (0xFF000000));
    g.fillRect (x0 - 1, y0 - 1, barWidth + 2, y1 - y0 + 2 + capLen);
    g.fillRect (x0 - 1, y0 - 1, capLen + 2, stroke + 2);
    g.fillRect (x0 - 1, y1 - stroke - 1, capLen + 2, stroke + 2);

    g.setColour (juce::Colour (0xFFFFFFFF));
    g.fillRect (x0, y0, barWidth, y1 - y0);
    g.fillRect (x0, y0, capLen, stroke);
    g.fillRect (x0, y1 - stroke, capLen, stroke);

    return juce::MouseCursor (img, margin + 1, size / 2);
}

static juce::MouseCursor createRightBracketCursor()
{
    const int size = 20;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    img.clear (img.getBounds(), juce::Colour (0x00000000));

    const int stroke = 1;
    const int margin = 3;
    const int barWidth = 2;
    const int capLen = 4;
    const int x0 = size - margin - barWidth;
    const int y0 = margin;
    const int y1 = size - margin;

    g.setColour (juce::Colour (0xFF000000));
    g.fillRect (x0 - 1, y0 - 1, barWidth + 2, y1 - y0 + 2 + capLen);
    g.fillRect (x0 + barWidth - capLen - 1, y0 - 1, capLen + 2, stroke + 2);
    g.fillRect (x0 + barWidth - capLen - 1, y1 - stroke - 1, capLen + 2, stroke + 2);

    g.setColour (juce::Colour (0xFFFFFFFF));
    g.fillRect (x0, y0, barWidth, y1 - y0);
    g.fillRect (x0 + barWidth - capLen, y0, capLen, stroke);
    g.fillRect (x0 + barWidth - capLen, y1 - stroke, capLen, stroke);

    return juce::MouseCursor (img, x0 - 1, size / 2);
}

juce::MouseCursor getLeftBracketResizeCursor()
{
    static juce::MouseCursor c = createLeftBracketCursor();
    return c;
}

juce::MouseCursor getRightBracketResizeCursor()
{
    static juce::MouseCursor c = createRightBracketCursor();
    return c;
}
}
