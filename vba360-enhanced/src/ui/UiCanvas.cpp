#include "UiCanvas.h"
#include "UiFontData.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct FontInfo
{
  const short (*glyphs)[8];
  int glyphCount;
  const unsigned char *atlas;
  int atlasW;
  int lineHeight;
  short index[256]; // char code -> glyph row, -1 if missing
};

FontInfo g_fonts[UI_FONT_COUNT];
bool g_fontsReady = false;

void initFont(FontInfo &f, const short (*glyphs)[8], int count,
              const unsigned char *atlas, int atlasW, int lineHeight)
{
  f.glyphs = glyphs;
  f.glyphCount = count;
  f.atlas = atlas;
  f.atlasW = atlasW;
  f.lineHeight = lineHeight;
  for (int i = 0; i < 256; i++)
    f.index[i] = -1;
  for (int i = 0; i < count; i++)
    f.index[glyphs[i][0] & 0xFF] = (short)i;
}

void initFonts()
{
  if (g_fontsReady)
    return;
  initFont(g_fonts[UI_FONT_SMALL], kFontSmallGlyphs,
           sizeof(kFontSmallGlyphs) / sizeof(kFontSmallGlyphs[0]),
           kFontSmallAtlas, kFontSmallAtlasW, kFontSmallLineHeight);
  initFont(g_fonts[UI_FONT_BODY], kFontBodyGlyphs,
           sizeof(kFontBodyGlyphs) / sizeof(kFontBodyGlyphs[0]),
           kFontBodyAtlas, kFontBodyAtlasW, kFontBodyLineHeight);
  initFont(g_fonts[UI_FONT_TITLE], kFontTitleGlyphs,
           sizeof(kFontTitleGlyphs) / sizeof(kFontTitleGlyphs[0]),
           kFontTitleAtlas, kFontTitleAtlasW, kFontTitleLineHeight);
  g_fontsReady = true;
}

// 4-bit atlas sample scaled to 0..255
inline int atlasAlpha(const FontInfo &f, int x, int y)
{
  unsigned char b = f.atlas[(y * f.atlasW + x) >> 1];
  int v = (x & 1) ? (b & 0x0F) : (b >> 4);
  return v * 17;
}

int glyphAdvance(const FontInfo &f, unsigned char ch)
{
  int g = f.index[ch];
  if (g < 0)
    g = f.index['?'];
  return g < 0 ? 0 : f.glyphs[g][7];
}

} // namespace

UiCanvas::UiCanvas()
  : m_px(0), m_w(0), m_h(0), m_cx0(0), m_cy0(0), m_cx1(0), m_cy1(0)
{
  initFonts();
}

UiCanvas::~UiCanvas()
{
  free(m_px);
}

bool UiCanvas::create(int w, int h)
{
  free(m_px);
  m_px = (UiColor *)malloc(sizeof(UiColor) * w * h);
  if (!m_px) {
    m_w = m_h = 0;
    return false;
  }
  m_w = w;
  m_h = h;
  resetClip();
  return true;
}

void UiCanvas::setClip(int x, int y, int w, int h)
{
  m_cx0 = x < 0 ? 0 : x;
  m_cy0 = y < 0 ? 0 : y;
  m_cx1 = x + w > m_w ? m_w : x + w;
  m_cy1 = y + h > m_h ? m_h : y + h;
}

void UiCanvas::resetClip()
{
  m_cx0 = 0;
  m_cy0 = 0;
  m_cx1 = m_w;
  m_cy1 = m_h;
}

void UiCanvas::clear(UiColor c)
{
  for (int i = 0; i < m_w * m_h; i++)
    m_px[i] = c;
}

inline void UiCanvas::blendPixel(UiColor *p, UiColor c, int alpha)
{
  if (alpha <= 0)
    return;
  if (alpha >= 255) {
    *p = c | 0xFF000000;
    return;
  }
  // x / 255 without a division (the Xbox 360 CPU divides slowly; this runs
  // for every pixel of text and every translucent panel).
  UiColor d = *p;
  int inv = 255 - alpha;
  int r = (int)((c >> 16) & 0xFF) * alpha + (int)((d >> 16) & 0xFF) * inv + 128;
  int g = (int)((c >> 8) & 0xFF) * alpha + (int)((d >> 8) & 0xFF) * inv + 128;
  int b = (int)(c & 0xFF) * alpha + (int)(d & 0xFF) * inv + 128;
  r = (r + (r >> 8)) >> 8;
  g = (g + (g >> 8)) >> 8;
  b = (b + (b >> 8)) >> 8;
  *p = 0xFF000000 | (r << 16) | (g << 8) | b;
}

void UiCanvas::hspan(int x0, int x1, int y, UiColor c)
{
  if (y < m_cy0 || y >= m_cy1)
    return;
  if (x0 < m_cx0) x0 = m_cx0;
  if (x1 > m_cx1) x1 = m_cx1;
  int a = c >> 24;
  UiColor *row = m_px + y * m_w;
  if (a >= 255) {
    for (int x = x0; x < x1; x++)
      row[x] = c;
  } else {
    for (int x = x0; x < x1; x++)
      blendPixel(row + x, c, a);
  }
}

void UiCanvas::fillRect(int x, int y, int w, int h, UiColor c)
{
  for (int yy = y; yy < y + h; yy++)
    hspan(x, x + w, yy, c);
}

void UiCanvas::fillGradient(int x, int y, int w, int h, UiColor top, UiColor bottom)
{
  if (h <= 0)
    return;
  for (int i = 0; i < h; i++) {
    int t = h > 1 ? (i * 255) / (h - 1) : 0;
    int it = 255 - t;
    UiColor c = 0;
    for (int s = 0; s < 32; s += 8) {
      int v = (((top >> s) & 0xFF) * it + ((bottom >> s) & 0xFF) * t) / 255;
      c |= (UiColor)v << s;
    }
    hspan(x, x + w, y + i, c);
  }
}

void UiCanvas::fillRoundRect(int x, int y, int w, int h, int radius, UiColor c)
{
  fillRoundGradient(x, y, w, h, radius, c, c);
}

static UiColor lerpColor(UiColor a, UiColor b, int t, int n)
{
  if (a == b || n <= 0)
    return a;
  UiColor out = 0;
  for (int s = 0; s < 32; s += 8) {
    int ca = (a >> s) & 0xFF, cb = (b >> s) & 0xFF;
    out |= (UiColor)(ca + (cb - ca) * t / n) << s;
  }
  return out;
}

void UiCanvas::fillRoundGradient(int x, int y, int w, int h, int radius, UiColor top, UiColor bottom)
{
  if (radius * 2 > h) radius = h / 2;
  if (radius * 2 > w) radius = w / 2;
  if (radius <= 0) {
    if (top == bottom) fillRect(x, y, w, h, top);
    else fillGradient(x, y, w, h, top, bottom);
    return;
  }
  for (int i = 0; i < h; i++) {
    int py = y + i;
    if (py < m_cy0 || py >= m_cy1)
      continue;
    UiColor c = lerpColor(top, bottom, i, h - 1);
    int baseA = c >> 24;
    // distance into the corner band
    int dy = 0;
    if (i < radius) dy = radius - i;
    else if (i >= h - radius) dy = i - (h - radius - 1);
    if (dy == 0) {
      hspan(x, x + w, py, c);
      continue;
    }
    // anti-aliased corners: coverage per pixel in the corner columns
    float fy = (float)dy - 0.5f;
    for (int j = 0; j < radius; j++) {
      float fx = (float)(radius - j) - 0.5f;
      float dist = fx * fx + fy * fy;
      float r = (float)radius;
      int cov;
      if (dist <= (r - 1) * (r - 1)) cov = 255;
      else if (dist >= r * r) cov = 0;
      else {
        // linear falloff across the last pixel
        float d = r - 1.0f;
        float t = (dist - d * d) / (r * r - d * d);
        cov = (int)((1.0f - t) * 255.0f);
      }
      int a = cov * baseA / 255;
      int lx = x + j, rx = x + w - 1 - j;
      if (lx >= m_cx0 && lx < m_cx1) blendPixel(m_px + py * m_w + lx, c, a);
      if (rx >= m_cx0 && rx < m_cx1) blendPixel(m_px + py * m_w + rx, c, a);
    }
    hspan(x + radius, x + w - radius, py, c);
  }
}

void UiCanvas::fillCircle(int cx, int cy, int radius, UiColor c)
{
  fillRoundRect(cx - radius, cy - radius, radius * 2, radius * 2, radius, c);
}

int UiCanvas::lineHeight(UiFontId font) const
{
  return g_fonts[font].lineHeight;
}

int UiCanvas::textWidth(UiFontId font, const char *text) const
{
  const FontInfo &f = g_fonts[font];
  int w = 0;
  for (const unsigned char *p = (const unsigned char *)text; *p; p++)
    w += glyphAdvance(f, *p);
  return w;
}

int UiCanvas::drawText(UiFontId font, int x, int y, const char *text, UiColor c)
{
  const FontInfo &f = g_fonts[font];
  int baseA = c >> 24;
  int pen = x;
  for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
    int gi = f.index[*p];
    if (gi < 0)
      gi = f.index['?'];
    if (gi < 0)
      continue;
    const short *g = f.glyphs[gi];
    int gx = g[1], gy = g[2], gw = g[3], gh = g[4];
    int ox = pen + g[5], oy = y + g[6];
    for (int yy = 0; yy < gh; yy++) {
      int py = oy + yy;
      if (py < m_cy0 || py >= m_cy1)
        continue;
      UiColor *row = m_px + py * m_w;
      for (int xx = 0; xx < gw; xx++) {
        int px = ox + xx;
        if (px < m_cx0 || px >= m_cx1)
          continue;
        int a = atlasAlpha(f, gx + xx, gy + yy);
        if (a) {
          if (baseA < 255) {
            a = a * baseA + 128;
            a = (a + (a >> 8)) >> 8;
          }
          blendPixel(row + px, c, a);
        }
      }
    }
    pen += g[7];
  }
  return pen - x;
}

int UiCanvas::drawTextFit(UiFontId font, int x, int y, int maxWidth,
                          const char *text, UiColor c, UiAlign align)
{
  int w = textWidth(font, text);
  if (w <= maxWidth) {
    int ox = x;
    if (align == UI_ALIGN_CENTER) ox = x + (maxWidth - w) / 2;
    else if (align == UI_ALIGN_RIGHT) ox = x + maxWidth - w;
    return drawText(font, ox, y, text, c);
  }
  // Truncate and append "..."
  const FontInfo &f = g_fonts[font];
  int dots = glyphAdvance(f, '.') * 3;
  char buf[512];
  int n = 0, acc = 0;
  for (const unsigned char *p = (const unsigned char *)text; *p && n < 500; p++) {
    int adv = glyphAdvance(f, *p);
    if (acc + adv + dots > maxWidth)
      break;
    buf[n++] = (char)*p;
    acc += adv;
  }
  buf[n++] = '.';
  buf[n++] = '.';
  buf[n++] = '.';
  buf[n] = 0;
  return drawText(font, x, y, buf, c);
}

void UiCanvas::blitScaled(const UiColor *src, int sw, int sh, int spitch,
                          int dx, int dy, int dw, int dh, int brightness)
{
  if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
    return;
  // Visible part only, and the source column stepped in 16.16 fixed point
  // instead of a division per pixel.
  int x0 = dx < m_cx0 ? m_cx0 - dx : 0;
  int x1 = dx + dw > m_cx1 ? m_cx1 - dx : dw;
  if (x0 >= x1)
    return;
  unsigned stepX = (unsigned)(((unsigned long long)sw << 16) / (unsigned)dw);
  for (int y = 0; y < dh; y++) {
    int py = dy + y;
    if (py < m_cy0 || py >= m_cy1)
      continue;
    const UiColor *srow = src + (y * sh / dh) * spitch;
    UiColor *drow = m_px + py * m_w + dx;
    unsigned sx = stepX * (unsigned)x0;
    if (brightness == 256) {
      for (int x = x0; x < x1; x++, sx += stepX)
        drow[x] = srow[sx >> 16] | 0xFF000000;
    } else {
      for (int x = x0; x < x1; x++, sx += stepX) {
        UiColor s = srow[sx >> 16];
        int r = ((s >> 16) & 0xFF) * brightness >> 8;
        int g = ((s >> 8) & 0xFF) * brightness >> 8;
        int b = (s & 0xFF) * brightness >> 8;
        drow[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
      }
    }
  }
}

void UiCanvas::blitAlpha(const UiColor *src, int w, int h, int x, int y)
{
  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < m_cy0 || py >= m_cy1)
      continue;
    for (int i = 0; i < w; i++) {
      int px = x + i;
      if (px < m_cx0 || px >= m_cx1)
        continue;
      UiColor s = src[j * w + i];
      blendPixel(m_px + py * m_w + px, s, (int)(s >> 24));
    }
  }
}

void uiScaleImage(const UiColor *src, int sw, int sh, int dw, int dh, int bright, std::vector<UiColor> &out)
{
  out.assign(dw > 0 && dh > 0 ? dw * dh : 0, 0);
  if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
    return;
  for (int j = 0; j < dh; j++) {
    int fy = (int)(((long long)j * 2 + 1) * sh * 128 / dh) - 128;   // 8.8 source row
    if (fy < 0) fy = 0;
    int y0 = fy >> 8, ty = fy & 255, y1 = y0 + 1 < sh ? y0 + 1 : y0;
    for (int i = 0; i < dw; i++) {
      int fx = (int)(((long long)i * 2 + 1) * sw * 128 / dw) - 128;
      if (fx < 0) fx = 0;
      int x0 = fx >> 8, tx = fx & 255, x1 = x0 + 1 < sw ? x0 + 1 : x0;
      UiColor p[4] = { src[y0 * sw + x0], src[y0 * sw + x1], src[y1 * sw + x0], src[y1 * sw + x1] };
      int wgt[4] = { (256 - tx) * (256 - ty), tx * (256 - ty), (256 - tx) * ty, tx * ty };
      // Weighted by alpha so transparent edges don't darken the colors.
      int a = 0, r = 0, g = 0, b = 0;
      for (int k = 0; k < 4; k++) {
        int pa = (int)(p[k] >> 24) * (wgt[k] >> 8);
        a += pa;
        r += ((p[k] >> 16) & 0xFF) * (pa >> 8);
        g += ((p[k] >> 8) & 0xFF) * (pa >> 8);
        b += (p[k] & 0xFF) * (pa >> 8);
      }
      int div = a >> 8;
      if (div <= 0)
        continue;
      r = r / div * bright >> 8; g = g / div * bright >> 8; b = b / div * bright >> 8;
      if (r > 255) r = 255;
      if (g > 255) g = 255;
      if (b > 255) b = 255;
      out[j * dw + i] = ((UiColor)(div > 255 ? 255 : div) << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

void UiCanvas::strokeRoundRect(int x, int y, int w, int h, int radius, UiColor c)
{
  if (w <= 0 || h <= 0)
    return;
  if (radius * 2 > h) radius = h / 2;
  if (radius * 2 > w) radius = w / 2;
  // inset(i): how far in from the side row i starts
  int prev = w / 2;
  for (int i = 0; i < h; i++) {
    int dy = i < radius ? radius - i : i >= h - radius ? i - (h - radius - 1) : 0;
    int in = 0;
    if (dy) {
      float fy = (float)dy - 0.5f, r = (float)radius;
      in = radius - (int)(sqrtf(r * r - fy * fy > 0 ? r * r - fy * fy : 0) + 0.5f);
    }
    int ndy = i + 1 < radius ? radius - i - 1 : i + 1 >= h - radius ? i + 1 - (h - radius - 1) : 0;
    int next = 0;
    if (ndy && i + 1 < h) {
      float fy = (float)ndy - 0.5f, r = (float)radius;
      next = radius - (int)(sqrtf(r * r - fy * fy > 0 ? r * r - fy * fy : 0) + 0.5f);
    } else if (i + 1 >= h) {
      next = w / 2;
    }
    int py = y + i;
    if (i == 0 || i == h - 1) {
      hspan(x + in, x + w - in, py, c);
    } else {
      int toward = i < h / 2 ? prev : next;    // the row nearer the top / bottom edge
      int e = toward > in + 1 ? toward : in + 1;
      hspan(x + in, x + e, py, c);
      hspan(x + w - e, x + w - in, py, c);
    }
    prev = in;
  }
}

static inline UiColor dimColor(UiColor p, int bright)
{
  if (bright >= 256)
    return p | 0xFF000000;
  UiColor rb = ((p & 0xFF00FF) * (UiColor)bright >> 8) & 0xFF00FF;
  UiColor g = ((p & 0x00FF00) * (UiColor)bright >> 8) & 0x00FF00;
  return 0xFF000000 | rb | g;
}

void UiCanvas::blitCard(const UiColor *src, int sw, int sh, int pitch, int dx, int dw,
                        int topL, int hL, int topR, int hR, int bright, int reflect)
{
  if (dw <= 0 || sw <= 0 || sh <= 0)
    return;
  // Per column: where it starts, how tall it is and how fast it walks down
  // the picture. Then drawn row by row: writing down columns of a 1280-wide
  // canvas misses the cache at every pixel, which the Xbox 360 CPU pays dearly.
  static std::vector<int> top, hh, vs, rh, body, r0;
  static std::vector<const UiColor *> col;
  top.resize(dw); hh.resize(dw); vs.resize(dw); rh.resize(dw); col.resize(dw); body.resize(dw); r0.resize(dw);
  int i0 = dx < m_cx0 ? m_cx0 - dx : 0, i1 = dx + dw > m_cx1 ? m_cx1 - dx : dw;
  if (i0 >= i1)
    return;
  int ymin = 1 << 30, ymax = -(1 << 30);
  for (int i = i0; i < i1; i++) {
    top[i] = topL + (topR - topL) * i / dw;
    hh[i] = hL + (hR - hL) * i / dw;
    vs[i] = hh[i] > 0 ? (sh << 16) / hh[i] : 0;
    rh[i] = hh[i] * reflect / 100;
    col[i] = src + (i * sw + sw / 2) / dw;
    body[i] = hh[i];
    r0[i] = 0;
    if (top[i] < ymin) ymin = top[i];
    if (top[i] + hh[i] + 1 + rh[i] > ymax) ymax = top[i] + hh[i] + 1 + rh[i];
  }
  if (ymin < m_cy0) ymin = m_cy0;
  if (ymax > m_cy1) ymax = m_cy1;
  for (int y = ymin; y < ymax; y++) {
    UiColor *row = m_px + y * m_w + dx;
    for (int i = i0; i < i1; i++) {
      int j = y - top[i];
      if (j < hh[i]) {
        if (j >= 0 && j < body[i])
          row[i] = dimColor(col[i][((j * vs[i]) >> 16) * pitch], bright);
      } else if (rh[i] > 0) {   // the mirror image under it, fading out
        int r = j - hh[i] - 1;
        if (r >= r0[i] && r < rh[i]) {
          UiColor p = col[i][(((hh[i] - 1 - r) * vs[i]) >> 16) * pitch];
          blendPixel(row + i, p | 0xFF000000, (0x58 * (rh[i] - r) / rh[i]) * bright >> 8);
        }
      }
    }
  }
}

void UiCanvas::copyFrom(const UiColor *src)
{
  if (src)
    memcpy(m_px, src, (size_t)m_w * m_h * sizeof(UiColor));
}
