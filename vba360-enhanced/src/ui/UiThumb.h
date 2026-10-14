// Small raw thumbnails (game previews and save-state pictures).
// Format: "VTMB", u16 width, u16 height (little endian), then RGB565 LE.
#ifndef UI_THUMB_H
#define UI_THUMB_H

#include <string>
#include <vector>

#include "UiCanvas.h"

struct UiThumb
{
  int w, h;
  std::vector<UiColor> px;   // 0x00RRGGBB
  UiThumb() : w(0), h(0) {}
  bool valid() const { return w > 0 && h > 0 && (int)px.size() == w * h; }
};

// src is 32-bit 0x00RRGGBB with pitch in pixels.
bool uiSaveThumb(const std::string &path, const UiColor *src, int w, int h, int pitch);
bool uiLoadThumb(const std::string &path, UiThumb &out);

#endif // UI_THUMB_H
