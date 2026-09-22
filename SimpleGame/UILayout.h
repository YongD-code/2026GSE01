#pragma once

// The same rectangles are used to draw controls and to hit-test them.
struct UIRect
{
    float x, y, w, h;

    bool Contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};
