// Xbox One style achievement pop-up: see UiPopupXbox.h.
//
// The CodePen's timeline (as fractions of the whole):
//   0-5%    the circle grows from 10% and fades in; two rings pulse out of it
//   11-24%  it slides left and turns lighter; the pill unfolds (100 -> 355 px)
//   20-25%  the text fades in, rising into place; the trophy appears
//   79-84%  the text rises out and fades
//   85-89%  the pill folds and the circle comes back to the middle
//   89-99%  the circle pulses (110%), then shrinks away
// The original runs 10 seconds; this one 8 (a game is running under it).
#include "UiPopupXbox.h"

#include <math.h>
#include <string.h>
#include <vector>

namespace {

const int kFrames = 480;
const int kPillH = 100, kCircleR = 50, kMinPill = 355, kMaxPill = 720;
const UiColor kGreen = 0xFF39960C, kGreenLight = 0xFF42AE0E, kRing1 = 0xFF40A90E, kRing2 = 0xFF32830A;

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float ease(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }   // CSS "ease", near enough
float span(float t, float a, float b) { return ease((t - a) / (b - a)); }

UiColor mix(UiColor a, UiColor b, float k)
{
  int r = (int)(((a >> 16) & 255) + (((b >> 16) & 255) - (int)((a >> 16) & 255)) * k);
  int g = (int)(((a >> 8) & 255) + (((b >> 8) & 255) - (int)((a >> 8) & 255)) * k);
  int bl = (int)((a & 255) + ((b & 255) - (int)(a & 255)) * k);
  return 0xFF000000 | (r << 16) | (g << 8) | bl;
}

UiColor withAlpha(UiColor c, float a) { return ((UiColor)(clamp01(a) * 255) << 24) | (c & 0xFFFFFF); }

int pillWidth(const char *line1, const char *line2)
{
  UiCanvas probe;
  probe.create(4, 4);
  int t1 = probe.textWidth(UI_FONT_BODY, line1), t2 = probe.textWidth(UI_FONT_SMALL, line2);
  int w = (t1 > t2 ? t1 : t2) + 2 * kCircleR + 34 + 40;   // the circle at the left end, margins
  return w < kMinPill ? kMinPill : w > kMaxPill ? kMaxPill : w;
}

// A trophy, white, squeezed sideways by sx (0..1: it turns around).
void trophy(UiCanvas &c, int x, int y, float sx, UiColor col, UiColor hole)
{
  if (sx < 0.12f) sx = 0.12f;
  int w = (int)(30 * sx), hw = (int)(17 * sx);
  c.fillCircle(x - hw, y - 9, 8, col);           // the handles
  c.fillCircle(x + hw, y - 9, 8, col);
  c.fillCircle(x - hw, y - 9, 4, hole);
  c.fillCircle(x + hw, y - 9, 4, hole);
  c.fillRoundRect(x - w / 2, y - 19, w, 20, 3, col);   // the cup
  c.fillCircle(x, y, w / 2, col);
  c.fillRect(x - (int)(4 * sx) - 1, y + 4, (int)(8 * sx) + 2, 9, col);   // the stem
  c.fillRoundRect(x - (int)(13 * sx), y + 12, (int)(26 * sx) + 1, 7, 2, col);   // the base
}

// The whole scene at time t. mask: everything in white (its coverage), to
// work out each pixel's transparency afterwards.
void scene(UiCanvas &c, float t, int frame, const char *line1, const char *line2, bool mask)
{
  const int W = c.width(), H = c.height(), cx = W / 2, cy = H / 2;
  const int pillW = W - 60;
  UiColor white = 0xFFFFFFFF;

  // Circle: grows in, slides left, comes back, pulses, shrinks away.
  float grow = 0.1f + 0.9f * span(t, 0.0f, 0.05f);
  float pulse = t < 0.89f ? 1 : t < 0.93f ? 1 + 0.1f * span(t, 0.89f, 0.93f) : t < 0.96f ? 1.1f - 0.1f * span(t, 0.93f, 0.96f)
                                                                                         : 1 - span(t, 0.96f, 0.99f);
  float scale = grow * pulse;
  float slide = span(t, 0.11f, 0.24f) * (1 - span(t, 0.85f, 0.89f));
  int offset = (int)((pillW / 2 - 47) * slide);
  int ccx = cx - offset;
  UiColor circleCol = mix(kGreen, kGreenLight, slide);

  // Rings pulsing out of it at the start (twice, the second a little later).
  for (int k = 0; k < 2; k++) {
    float rt = (t - 0.04f - k * 0.025f) / 0.07f;
    if (rt > 0 && rt < 1) {
      int r = (int)(kCircleR * (1 + 0.35f * rt));
      c.fillCircle(ccx, cy, r, mask ? withAlpha(white, 0.7f * (1 - rt)) : withAlpha(k ? kRing2 : kRing1, 0.7f * (1 - rt)));
    }
  }

  // The pill: unfolds from the circle's size to fit the text, folds back.
  float unfold = span(t, 0.11f, 0.24f) * (1 - span(t, 0.85f, 0.89f));
  if (t > 0.11f && t < 0.90f) {
    int w = (int)(kPillH + (pillW - kPillH) * unfold);
    c.fillRoundRect(cx - w / 2, cy - kPillH / 2, w, kPillH, kPillH / 2, mask ? white : kGreen);

    // The text, inside the pill (to the right of the circle): rises in, rises out.
    float in = span(t, 0.20f, 0.25f), out = span(t, 0.79f, 0.84f);
    float a = in * (1 - out);
    if (a > 0) {
      int tx = ccx + kCircleR + 16, tw = cx + w / 2 - 36 - tx;
      int dy = (int)(22 * (1 - in)) - (int)(30 * out);
      c.setClip(cx - w / 2, cy - kPillH / 2, w, kPillH);
      c.drawTextFit(UI_FONT_BODY, tx, cy - 32 + dy, tw, line1, mask ? white : withAlpha(white, a));
      c.drawTextFit(UI_FONT_SMALL, tx, cy + 4 + dy, tw, line2, mask ? white : withAlpha(0xFFE6F5DC, a));
      c.resetClip();
    }
  }

  // The circle itself, over the pill's left end.
  int r = (int)(kCircleR * scale);
  if (r > 0) {
    float fade = clamp01(t / 0.05f) * (1 - span(t, 0.97f, 0.99f));
    c.fillCircle(ccx, cy, r, mask ? withAlpha(white, fade) : withAlpha(circleCol, fade));
    // The trophy: turning while the pill is open, still when it's shut.
    float shown = scale * fade;
    if (!mask && shown > 0.3f) {
      float turn = (t > 0.20f && t < 0.90f) ? (float)cos(frame * 6.2831853 / 360.0) : 1.0f;   // a turn every 6 s
      float sx = (float)fabs(turn) * scale;
      trophy(c, ccx, cy, sx, withAlpha(white, shown), circleCol);
    }
  }
}

} // namespace

int uiXboxPopupFrames() { return kFrames; }

void uiXboxPopupSize(const char *line1, const char *line2, int &w, int &h)
{
  w = pillWidth(line1, line2) + 60;   // room for the rings around the circle
  h = kPillH + 50;
}

void uiDrawXboxPopup(UiCanvas &c, int frame, const char *line1, const char *line2)
{
  const int W = c.width(), H = c.height();
  float t = (float)frame / kFrames;
  // Drawn twice: in color over black, and in white (how much each pixel is
  // covered). Color / coverage gives the real color, coverage its alpha, so
  // the round edges blend into the game instead of having a dark rim.
  static UiCanvas mask;
  if (mask.width() != W || mask.height() != H)
    mask.create(W, H);
  c.clear(0xFF000000);
  mask.clear(0xFF000000);
  scene(c, t, frame, line1, line2, false);
  scene(mask, t, frame, line1, line2, true);
  UiColor *p = c.pixels();
  const UiColor *m = mask.pixels();
  for (int i = 0; i < W * H; i++) {
    unsigned a = m[i] & 0xFF;
    if (a == 255)   // most of it: solid, nothing to work out (divisions are slow on the console)
      continue;
    if (!a) {
      p[i] = 0;
      continue;
    }
    unsigned r = ((p[i] >> 16) & 255) * 255 / a, g = ((p[i] >> 8) & 255) * 255 / a, b = (p[i] & 255) * 255 / a;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    p[i] = (a << 24) | (r << 16) | (g << 8) | b;
  }
}
