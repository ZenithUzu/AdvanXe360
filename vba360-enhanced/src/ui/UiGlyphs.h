// Xbox 360 controller button images for the button hints (see UiGlyphs.cpp).
#ifndef UI_GLYPHS_H
#define UI_GLYPHS_H

#include <string.h>
#include "UiCanvas.h"

struct UiGlyph
{
  const char *name;   // "A", "B", "X", "Y", "BACK", "START", "LB", "RB", "LT", "RT", "LS", "RS", "DPAD"
  int w, h;
  const UiColor *px;
};

// The image for a button name, or 0.
const UiGlyph *uiGlyph(const char *name);

#endif
