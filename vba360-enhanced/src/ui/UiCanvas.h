// Software 2D canvas used by the menu UI. Portable C++03 (builds with
// Visual Studio 2008 / Xbox 360 SDK and with g++ for testing on PC).
//
// Pixels are 32-bit native-endian 0xAARRGGBB, the same layout as the
// D3DFMT_LIN_A8R8G8B8 texture the Xbox 360 host uploads it to.
#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include <vector>

typedef unsigned int UiColor;

enum UiFontId { UI_FONT_SMALL = 0, UI_FONT_BODY = 1, UI_FONT_TITLE = 2, UI_FONT_COUNT = 3 };

enum UiAlign { UI_ALIGN_LEFT = 0, UI_ALIGN_CENTER = 1, UI_ALIGN_RIGHT = 2 };

class UiCanvas
{
public:
  UiCanvas();
  ~UiCanvas();

  // Allocates an owned w*h buffer.
  bool create(int w, int h);
  int width() const { return m_w; }
  int height() const { return m_h; }
  const UiColor *pixels() const { return m_px; }
  UiColor *pixelsRW() { return m_px; }

  void setClip(int x, int y, int w, int h);
  void resetClip();

  void clear(UiColor c);
  // Alpha-blended fills (alpha taken from the color's top byte).
  void fillRect(int x, int y, int w, int h, UiColor c);
  void fillRoundRect(int x, int y, int w, int h, int radius, UiColor c);
  // The same with a vertical gradient (alpha too).
  void fillRoundGradient(int x, int y, int w, int h, int radius, UiColor top, UiColor bottom);
  // 1-pixel outline of the same rounded rectangle.
  void strokeRoundRect(int x, int y, int w, int h, int radius, UiColor c);
  void fillCircle(int cx, int cy, int radius, UiColor c);
  // Vertical gradient between two colors.
  void fillGradient(int x, int y, int w, int h, UiColor top, UiColor bottom);

  // Draws Latin-1 text with its top-left at (x, y). Returns the advance width.
  int drawText(UiFontId font, int x, int y, const char *text, UiColor c);
  // Draws text truncated with "..." to fit maxWidth, honoring alignment
  // inside the box [x, x + maxWidth).
  int drawTextFit(UiFontId font, int x, int y, int maxWidth, const char *text,
                  UiColor c, UiAlign align = UI_ALIGN_LEFT);
  int textWidth(UiFontId font, const char *text) const;
  int lineHeight(UiFontId font) const;

  // Scales a 32-bit source image (pitch in pixels) into the destination rect.
  // brightness 0..256 darkens the image (256 = unchanged).
  void blitScaled(const UiColor *src, int sw, int sh, int spitch,
                  int dx, int dy, int dw, int dh, int brightness = 256);
  // Copies a whole canvas-sized image (width*height pixels), ignoring the clip.
  void copyFrom(const UiColor *src);
  // Draws a w x h image 1:1 at (x, y) using each pixel's own alpha.
  void blitAlpha(const UiColor *src, int w, int h, int x, int y);
  // A picture drawn as a card turned in depth (cover flow): column by
  // column from dx to dx + dw, its left edge topL..topL+hL tall and its right
  // edge topR..topR+hR, in between a straight line. reflect: percent of the
  // height mirrored under it, fading out (0 = none).
  // sky / under (optional, one int per canvas column): for drawing nearest
  // first. sky[x] is the highest row already covered above the floor (rows
  // from there down are skipped, then it is raised); under[x] how many rows of
  // reflection are already drawn below it.
  void blitCard(const UiColor *src, int sw, int sh, int pitch, int dx, int dw,
                int topL, int hL, int topR, int hR, int bright, int reflect,
                int *sky = 0, int *under = 0);

private:
  UiColor *m_px;
  int m_w, m_h;
  int m_cx0, m_cy0, m_cx1, m_cy1; // clip rect, exclusive max

  void blendPixel(UiColor *p, UiColor c, int alpha);
  void hspan(int x0, int x1, int y, UiColor c);

  UiCanvas(const UiCanvas &);
  UiCanvas &operator=(const UiCanvas &);
};

// Color helpers
inline UiColor uiRgba(int r, int g, int b, int a)
{
  return ((UiColor)a << 24) | ((UiColor)r << 16) | ((UiColor)g << 8) | (UiColor)b;
}
inline UiColor uiWithAlpha(UiColor c, int a)
{
  return (c & 0x00FFFFFF) | ((UiColor)a << 24);
}

// An image (0xAARRGGBB) scaled to dw x dh (bilinear), its brightness times
// bright/256. Slow: make it once and keep it, then draw it with blitAlpha.
void uiScaleImage(const UiColor *src, int sw, int sh, int dw, int dh, int bright, std::vector<UiColor> &out);

#endif // UI_CANVAS_H
