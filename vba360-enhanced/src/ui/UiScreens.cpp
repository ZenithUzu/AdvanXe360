#include "UiScreens.h"
#include "UiGlyphs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>

const UiColor UI_COL_TEXT     = 0xFFE9ECF4;
const UiColor UI_COL_TEXT_DIM = 0xFF98A2B8;
// Set by uiApplyTheme().
UiColor UI_COL_ACCENT    = 0xFF7B6CFF;
UiColor UI_COL_PANEL     = 0xFF161B2A;
UiColor UI_COL_ROW_SEL   = 0x407B6CFF;
UiColor UI_COL_BG_TOP    = 0xFF0E1220;
UiColor UI_COL_BG_BOTTOM = 0xFF181E31;

// Menu color themes: accent, background gradient (top, bottom), panels.
// The text stays light on dark in all of them so it reads the same.
const UiTheme uiThemes[UI_THEME_COUNT] = {
  { { "Predeterminado", "Default" },          0xFF7B6CFF, 0xFF0E1220, 0xFF181E31, 0xFF161B2A },
  { { "Xbox 360", "Xbox 360" },               0xFF6CC24A, 0xFF0B140C, 0xFF142417, 0xFF132016 },
  { { "Game Boy", "Game Boy" },               0xFF9BBC0F, 0xFF101A0C, 0xFF1F2B14, 0xFF1A2412 },
  { { "Game Boy Color", "Game Boy Color" },   0xFFE0457B, 0xFF170C1A, 0xFF2A1430, 0xFF221428 },
  { { "Game Boy Advance", "Game Boy Advance" }, 0xFF6F64F0, 0xFF0F0D24, 0xFF1D1940, 0xFF181534 },
  { { "Rojo", "Red" },                        0xFFE5534B, 0xFF170A0C, 0xFF2A1215, 0xFF221114 },
  // Sky, water and glass of the late-2000s look (Windows Vista/7, Wii, Xbox 360 "blades" era).
  { { "Frutiger Aero", "Frutiger Aero" },     0xFF27A7E8, 0xFF1F7FC6, 0xFF07304F, 0xFF0B3C62, THEME_AERO },
};
int UI_THEME_STYLE = THEME_FLAT;

int UI_PANEL_OPACITY = 100;

void uiApplyTheme(int index, UiColor accent, int panelOpacity)
{
  if (index < 0 || index >= UI_THEME_COUNT)
    index = 0;
  const UiTheme &t = uiThemes[index];
  UI_COL_ACCENT = accent ? accent : t.accent;
  UI_COL_ROW_SEL = uiWithAlpha(UI_COL_ACCENT, 0x40);
  UI_PANEL_OPACITY = panelOpacity;
  UI_COL_BG_TOP = t.bgTop;
  UI_COL_BG_BOTTOM = t.bgBottom;
  UI_COL_PANEL = t.panel;
  UI_THEME_STYLE = t.style;
}

// Vista-era glossy pill: a white rim, a bright upper half that breaks
// sharply at the middle, light coming back up from the bottom and a soft
// blue glow around it.
static void drawGloss(UiCanvas &c, int x, int y, int w, int h, int r, UiColor base)
{
  if (w <= 4 || h <= 4)
    return;
  if (r > h / 2) r = h / 2;
  int ir = r > 1 ? r - 1 : 0;
  c.fillRoundRect(x - 3, y - 3, w + 6, h + 6, r + 3, 0x308CD7FF);                // glow
  c.fillRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, 0xE0FFFFFF);                // rim
  c.fillRoundRect(x, y, w, h, r, base | 0xFF000000);
  c.fillRoundRect(x + 1, y + 1, w - 2, h / 2, ir, 0x48FFFFFF);                   // upper half
  c.fillGradient(x + r / 2, y + 1, w - r, h / 2 - 1, 0x40FFFFFF, 0x08FFFFFF);
  c.fillGradient(x + r / 2, y + h / 2, w - r, h - h / 2 - 2, 0x00FFFFFF, 0x30FFFFFF);
  c.fillRect(x + r, y + 1, w - 2 * r, 1, 0xF0FFFFFF);                             // top light
  c.fillRect(x + r, y + h - 2, w - 2 * r, 1, 0x59003078);                         // bottom shade
}

void uiDrawRowSelection(UiCanvas &c, int x, int y, int w, int h)
{
  if (UI_THEME_STYLE == THEME_AERO) {
    drawGloss(c, x, y, w, h, 12, UI_COL_ACCENT);
    return;
  }
  c.fillRoundRect(x, y, w, h, 10, UI_COL_ROW_SEL);
  c.fillRoundRect(x, y + 8, 4, h - 16, 2, UI_COL_ACCENT);
}

void uiDrawSelection(UiCanvas &c, int x, int y, int w, int h, int radius, bool strong)
{
  if (UI_THEME_STYLE == THEME_AERO) {
    if (strong) {
      drawGloss(c, x, y, w, h, radius, UI_COL_ACCENT);
    } else {   // a frame around something (a box art): glow and rim only
      c.fillRoundRect(x - 3, y - 3, w + 6, h + 6, radius + 3, 0x408CD7FF);
      c.fillRoundRect(x, y, w, h, radius, 0xC0FFFFFF);
    }
    return;
  }
  c.fillRoundRect(x, y, w, h, radius, strong ? UI_COL_ACCENT : UI_COL_ROW_SEL);
}

void uiDrawPanel(UiCanvas &c, int x, int y, int w, int h, int radius, int alpha)
{
  int solid = alpha >= 0xC0;                 // dialogs and the in-game menu: keep them readable
  alpha = alpha * UI_PANEL_OPACITY / 100;   // Interface -> Panel transparency
  if (alpha > 0xFF) alpha = 0xFF;
  if (solid && alpha < 0xB0) alpha = 0xB0;
  if (UI_THEME_STYLE != THEME_AERO) {
    c.fillRoundRect(x, y, w, h, radius, uiWithAlpha(UI_COL_PANEL, alpha));
    return;
  }
  // Glass: tinted, a soft reflection over the upper part (below the round
  // corners, so it never pokes out of them) and a thin bright outline. No
  // drop shadow: through see-through glass it shows as a second, offset box.
  int a = alpha >= 0xE0 ? alpha : alpha * 3 / 4;   // dialogs stay solid enough to read
  int r = radius < h / 2 ? radius : h / 2;
  c.fillRoundRect(x, y, w, h, radius, uiWithAlpha(UI_COL_PANEL, a));
  c.fillRoundGradient(x, y, w, h, radius, 0x1EFFFFFF, 0x00FFFFFF);   // lighter at the top
  c.strokeRoundRect(x, y, w, h, radius, 0x80FFFFFF);
  c.fillRect(x + r, y + 1, w - 2 * r, 1, 0x40FFFFFF);
}

namespace {

// A thin anti-aliased circle outline.
void drawRing(UiCanvas &c, int cx, int cy, int r, UiColor col)
{
  int a = (int)(col >> 24);
  for (int y = -r - 1; y <= r + 1; y++) {
    for (int x = -r - 1; x <= r + 1; x++) {
      int d2 = x * x + y * y;
      int in = (r - 1) * (r - 1), out = (r + 1) * (r + 1);
      if (d2 < in || d2 > out)
        continue;
      int edge = d2 < r * r ? d2 - in : out - d2;           // fade at both sides
      int span = d2 < r * r ? r * r - in : out - r * r;
      int aa = span > 0 ? a * edge / span : a;
      c.fillRect(cx + x, cy + y, 1, 1, uiWithAlpha(col, aa));
    }
  }
}

// A soap bubble: clear body, white rim, a bright spot up-left and a cyan
// glint down-right.
void drawBubble(UiCanvas &c, int x, int y, int r)
{
  c.fillCircle(x, y, r, 0x0EFFFFFF);
  c.fillCircle(x + r / 6, y + r / 5, r * 3 / 4, 0x1000A0FF);
  drawRing(c, x, y, r, 0xA8FFFFFF);
  c.fillCircle(x - r * 35 / 100, y - r * 40 / 100, r * 32 / 100, 0x40FFFFFF);
  c.fillCircle(x - r * 35 / 100, y - r * 40 / 100, r * 20 / 100, 0xD0FFFFFF);
  c.fillCircle(x + r * 42 / 100, y + r * 45 / 100, r * 15 / 100 + 1, 0x7078DCFF);
}

} // namespace

// Water: the background seen through moving water. Same idea as the
// snes360-enhanced menus (the Xbox SDK "XuiEffect" displacement): a soft,
// tileable cloud of noise slides sideways and each pixel of the background is
// fetched from a little off its place by the noise's value, so the picture
// wobbles. Our own noise and software version (the GPU one needs shaders).
static const signed char *waterNoise()
{
  enum { N = 128 };
  static signed char tex[N * N];
  static bool made = false;
  if (made)
    return tex;
  static float acc[N * N];
  memset(acc, 0, sizeof(acc));
  unsigned int seed = 777;
  float total = 0.0f, weight = 1.0f;
  for (int cells = 2; cells <= 16; cells *= 2, weight *= 0.5f) {   // octaves: big blobs, then finer
    float lat[17 * 17];
    for (int i = 0; i < cells * cells; i++) {
      seed = seed * 1103515245u + 12345u;
      lat[i] = (float)((seed >> 9) & 1023) / 1023.0f;
    }
    int step = N / cells;
    for (int y = 0; y < N; y++)
      for (int x = 0; x < N; x++) {
        int cx = x / step, cy = y / step;
        float fx = (float)(x % step) / step, fy = (float)(y % step) / step;
        fx = fx * fx * (3 - 2 * fx);
        fy = fy * fy * (3 - 2 * fy);
        float a = lat[cy * cells + cx], b = lat[cy * cells + (cx + 1) % cells];
        float c = lat[((cy + 1) % cells) * cells + cx], d = lat[((cy + 1) % cells) * cells + (cx + 1) % cells];
        acc[y * N + x] += weight * ((a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy);
      }
    total += weight;
  }
  for (int i = 0; i < N * N; i++) {
    int v = (int)((acc[i] / total - 0.5f) * 2.0f * 127.0f * 1.6f);   // spread to about -127..127
    tex[i] = (signed char)(v > 127 ? 127 : v < -127 ? -127 : v);
  }
  made = true;
  return tex;
}

void uiDrawWater(UiCanvas &c, const UiColor *bg, int frame, int strength)
{
  enum { N = 128, G = 8, MAXGX = 1280 / G + 2, MAXGY = 720 / G + 2 };
  const int W = c.width(), H = c.height();
  if (W > 1280 || H > 720) {
    c.copyFrom(bg);
    return;
  }
  const signed char *t = waterNoise();
  // snes360-enhanced: shift = noise * 0.4 of the picture (here up to ~4% of
  // it each way), the noise sliding 1/1000 of its width per frame.
  const int amount = strength == 0 ? 22 : strength == 1 ? 40 : 60;   // 1/1000 of the screen
  const int maxX = W * amount / 1000 * 95 / 100, maxY = H * amount / 1000 * 95 / 100;
  const int scroll = frame * N * 256 / 1000;                         // 8.8 texels
  static int ox[MAXGY][MAXGX], oy[MAXGY][MAXGX];                       // 8.8 pixels, every G pixels
  const int gx = W / G + 2, gy = H / G + 2;
  for (int j = 0; j < gy; j++)
    for (int i = 0; i < gx; i++) {
      int u = i * G * N * 256 / W + scroll, v = j * G * N * 256 / H;   // 8.8 texels
      int u0 = (u >> 8) & (N - 1), v0 = (v >> 8) & (N - 1), fu = u & 255, fv = v & 255;
      int u1 = (u0 + 1) & (N - 1), v1 = (v0 + 1) & (N - 1);
      int top = t[v0 * N + u0] * (256 - fu) + t[v0 * N + u1] * fu;
      int bot = t[v1 * N + u0] * (256 - fu) + t[v1 * N + u1] * fu;
      int n = (top * (256 - fv) + bot * fv) >> 16;                     // -127..127
      ox[j][i] = n * maxX * 256 / 127;
      oy[j][i] = n * maxY * 256 / 127;
    }
  UiColor *dst = c.pixelsRW();
  // Two rows at a time: the second reuses the first one's shift (one row
  // down in the picture), so the shift is worked out once per 2x2 pixels.
  for (int y = 0; y < H; y += 2) {
    int j = y / G, fy = (y % G) * 256 / G;
    UiColor *out = dst + y * W;
    UiColor *out2 = y + 1 < H ? out + W : out;
    const bool rowSafe = y - maxY - 2 >= 0 && y + maxY + 3 < H;
    for (int i = 0; i * G < W; i++) {
      // offsets at this cell's two left corners' rows, blended down to y, then across
      int ax = (ox[j][i] * (256 - fy) + ox[j + 1][i] * fy) >> 8;
      int ay = (oy[j][i] * (256 - fy) + oy[j + 1][i] * fy) >> 8;
      int bx = (ox[j][i + 1] * (256 - fy) + ox[j + 1][i + 1] * fy) >> 8;
      int by = (oy[j][i + 1] * (256 - fy) + oy[j + 1][i + 1] * fy) >> 8;
      const int dx = (bx - ax) / G, dy = (by - ay) / G;
      const int x0 = i * G, x1 = x0 + G < W ? x0 + G : W;
      int fx = ax + (x0 << 8), fy2 = ay + (y << 8);   // 8.8 source position
      if (rowSafe && x0 - maxX - 2 >= 0 && x1 + maxX + 2 < W && ((x1 - x0) & 1) == 0) {
        // Away from the edges: no clamping, two pixels per step (the shift
        // changes far less than a pixel between them).
        for (int x = x0; x < x1; x += 2, fx += 2 * (dx + 256), fy2 += 2 * dy) {
          const UiColor *sp = bg + (fy2 >> 8) * W + (fx >> 8);
          out[x] = sp[0];
          out[x + 1] = sp[1];
          out2[x] = sp[W];
          out2[x + 1] = sp[W + 1];
        }
        continue;
      }
      for (int x = x0; x < x1; x++, fx += dx + 256, fy2 += dy) {
        int sx = fx >> 8, sy = fy2 >> 8;
        sx = sx < 0 ? 0 : sx >= W ? W - 1 : sx;
        sy = sy < 0 ? 0 : sy >= H ? H - 1 : sy;
        out[x] = bg[sy * W + sx];
        out2[x] = bg[(sy + 1 < H ? sy + 1 : sy) * W + sx];
      }
    }
  }
}

void uiDrawEffects(UiCanvas &c, const UiConfig &cfg, int frame)
{
  if (UI_THEME_STYLE != THEME_AERO)
    return;   // the moving effects belong to the Frutiger Aero theme
  const int W = c.width(), H = c.height();
  const float t = (float)frame;
  if (cfg.bgEffect & UI_EFFECT_BUBBLES) {
    // Bubbles rise at their own speed, sway a little and start over below.
    static const short bub[][4] = {   // x, radius, speed (1/10 px per frame), phase
      { 90, 18, 9, 0 }, { 600, 7, 16, 4 }, { 950, 30, 6, 7 }, { 330, 26, 7, 5 },
      { 1170, 22, 8, 9 }, { 210, 9, 14, 3 }, { 720, 20, 8, 2 }, { 470, 12, 12, 1 },
      { 1060, 14, 11, 8 }, { 830, 10, 13, 6 }, { 1240, 8, 15, 10 }, { 40, 11, 12, 11 },
      { 520, 16, 10, 12 }, { 1110, 6, 17, 13 }, { 160, 13, 11, 14 }, { 400, 7, 16, 15 },
      { 660, 24, 7, 16 }, { 890, 9, 14, 17 }, { 1010, 17, 9, 18 }, { 280, 15, 10, 19 },
      { 760, 6, 18, 20 } };
    const int total = (int)(sizeof(bub) / sizeof(bub[0]));
    const int count = cfg.bubbleAmount == 0 ? 7 : cfg.bubbleAmount == 1 ? 14 : total;
    for (int i = 0; i < count; i++) {
      int r = bub[i][1];
      int span = H + r * 4;
      int y = H + r * 2 - (int)((t * bub[i][2] / 10 + bub[i][3] * 97) ) % span;
      int x = bub[i][0] * W / 1280 + (int)(r * 0.8f * sinf(t * 0.03f + bub[i][3]));
      drawBubble(c, x, y, r);
    }
  }
}

namespace {

const UiColor COL_DIVIDER     = 0x30FFFFFF;
const UiColor COL_FAV         = 0xFFF5C451;
const UiColor COL_DIR         = 0xFF8FA8FF;

// Small triangle used for "< value >" and submenu arrows.
void drawArrow(UiCanvas &c, int x, int y, int size, bool right, UiColor col)
{
  for (int row = -size; row <= size; row++) {
    int len = size - (row < 0 ? -row : row);
    if (right)
      c.fillRect(x, y + row, len, 1, col);
    else
      c.fillRect(x + size - len, y + row, len, 1, col);
  }
}

// Folder glyph: a tab and a body.
void drawFolderIcon(UiCanvas &c, int x, int y, UiColor col)
{
  c.fillRoundRect(x, y + 3, 12, 6, 2, col);
  c.fillRoundRect(x, y + 6, 26, 18, 3, col);
}

void drawBadge(UiCanvas &c, int x, int y, const char *text, UiColor bg, UiColor fg)
{
  int w = c.textWidth(UI_FONT_SMALL, text) + 14;
  c.fillRoundRect(x, y, w, 24, 6, bg);
  c.drawText(UI_FONT_SMALL, x + 7, y + 1, text, fg);
}

const char *romKind(const std::string &name);

// What stands for a game: its box art, else (Interface -> Game list preview)
// the picture of the last time it was played. Both null: nothing yet.
const UiApp::Cover *gameArt(UiApp &app, const std::string &rom, const UiThumb *&thumb)
{
  int mode = app.cfg().previewMode;
  thumb = 0;
  if (mode != PREVIEW_LAST) {
    const UiApp::Cover *cv = app.cover(rom, !app.inputHeld());
    if (cv)
      return cv;
    // Covers are read once the list stops; until then don't flash the
    // last-played picture where the cover is about to appear.
    if (!app.coverChecked(rom)) {
      app.drawAgain();
      return 0;
    }
    if (mode == PREVIEW_COVER)
      return 0;
  }
  thumb = app.thumb(app.gameThumbPath(rom));
  return 0;
}

const char *romKind(const std::string &name)
{
  std::string e = uiExtension(name);
  if (e == ".gba" || e == ".agb" || e == ".bin" || e == ".mb") return "GBA";
  if (e == ".gbc" || e == ".cgb") return "GBC";
  if (e == ".gb" || e == ".dmg" || e == ".sgb") return "GB";
  if (e == ".zip") return "ZIP";
  if (e == ".7z") return "7Z";
  return "";
}

std::string formatSize(unsigned int bytes)
{
  char buf[32];
  if (bytes >= 1024 * 1024)
    sprintf(buf, "%.1f MB", bytes / (1024.0 * 1024.0));
  else if (bytes >= 1024)
    sprintf(buf, "%u KB", bytes / 1024);
  else
    sprintf(buf, "%u B", bytes);
  return buf;
}

// Finds a patch next to the ROM (same rules as the loader).
std::string findPatch(UiApp &app, const std::string &rom)
{
  static const char *exts[] = { ".bps", ".ups", ".ips" };
  std::string base = uiStripExtension(rom);
  for (int i = 0; i < 3; i++)
    if (app.host().fileExists(base + exts[i]))
      return uiFileName(base + exts[i]);
  return "";
}

void drawScrollbar(UiCanvas &c, int x, int y, int h, int first, int visible, int total)
{
  if (total <= visible)
    return;
  c.fillRoundRect(x, y, 4, h, 2, 0x20FFFFFF);
  int barH = h * visible / total;
  if (barH < 24) barH = 24;
  int barY = y + (h - barH) * first / (total - visible);
  c.fillRoundRect(x, barY, 4, barH, 2, 0x90FFFFFF);
}

} // namespace

// --- UiListState -------------------------------------------------------------

bool UiListState::move(int delta, int count, bool wrap)
{
  if (count <= 0)
    return false;
  int old = sel;
  int n = sel + delta;
  if (wrap && (delta == 1 || delta == -1)) {
    if (n < 0) n = count - 1;
    if (n >= count) n = 0;
  }
  if (n < 0) n = 0;
  if (n >= count) n = count - 1;
  sel = n;
  return sel != old;
}

void UiListState::clamp(int count)
{
  if (sel >= count) sel = count - 1;
  if (sel < 0) sel = 0;
  if (scroll > sel) scroll = sel;
  if (scroll < 0) scroll = 0;
}

void UiListState::ensureVisible(int visibleRows)
{
  if (visibleRows < 1) visibleRows = 1;
  if (sel < scroll) scroll = sel;
  if (sel >= scroll + visibleRows) scroll = sel - visibleRows + 1;
  if (scroll < 0) scroll = 0;
}

// --- Names -------------------------------------------------------------------

const char *uiButtonName(UiApp &app, int button)
{
  switch (button) {
  case PAD_UP: return app.tr("Cruceta arriba", "D-pad up");
  case PAD_DOWN: return app.tr("Cruceta abajo", "D-pad down");
  case PAD_LEFT: return app.tr("Cruceta izquierda", "D-pad left");
  case PAD_RIGHT: return app.tr("Cruceta derecha", "D-pad right");
  case PAD_A: return "A";
  case PAD_B: return "B";
  case PAD_X: return "X";
  case PAD_Y: return "Y";
  case PAD_LB: return "LB";
  case PAD_RB: return "RB";
  case PAD_LT: return "LT";
  case PAD_RT: return "RT";
  case PAD_START: return "START";
  case PAD_BACK: return "BACK";
  case PAD_LS: return app.tr("Clic stick izquierdo", "Left stick click");
  case PAD_RS: return app.tr("Clic stick derecho", "Right stick click");
  default: return app.tr("Sin asignar", "Unassigned");
  }
}

const char *uiActionName(UiApp &app, int action)
{
  switch (action) {
  case ACT_GBA_A: return app.tr("Bot\xf3n A", "A button");
  case ACT_GBA_B: return app.tr("Bot\xf3n B", "B button");
  case ACT_GBA_L: return app.tr("Bot\xf3n L", "L button");
  case ACT_GBA_R: return app.tr("Bot\xf3n R", "R button");
  case ACT_GBA_START: return "Start";
  case ACT_GBA_SELECT: return "Select";
  case ACT_GBA_UP: return app.tr("Arriba", "Up");
  case ACT_GBA_DOWN: return app.tr("Abajo", "Down");
  case ACT_GBA_LEFT: return app.tr("Izquierda", "Left");
  case ACT_GBA_RIGHT: return app.tr("Derecha", "Right");
  case ACT_TURBO_A: return app.tr("Turbo A", "Turbo A");
  case ACT_TURBO_B: return app.tr("Turbo B", "Turbo B");
  case ACT_FAST_FORWARD: return app.tr("Avance r\xe1pido", "Fast forward");
  case ACT_MENU: return app.tr("Abrir men\xfa", "Open menu");
  case ACT_QUICK_SAVE: return app.tr("Guardado r\xe1pido", "Quick save");
  case ACT_QUICK_LOAD: return app.tr("Carga r\xe1pida", "Quick load");
  case ACT_REWIND: return app.tr("Rebobinar", "Rewind");
  case ACT_SHAKE: return app.tr("Sacudir (salto de Kirby)", "Shake (Kirby's jump)");
  default: return "";
  }
}

// --- UiHomeScreen ------------------------------------------------------------

UiHomeScreen::UiHomeScreen(UiApp &app, const std::string &selectRom)
  : m_tab(TAB_GAMES), m_dirError(false)
{
  std::string start = selectRom.empty() ? app.cfg().lastDir : uiParentDir(selectRom);
  openDir(app, start, selectRom);
  if (m_dirError && !start.empty())
    openDir(app, "", "");
}

// Lower case, Latin-1 letters too (\xd1 -> \xf1); names in UTF-8 (copied over
// FTP) read as Latin-1 first, so searching what the keyboard types finds them.
static std::string lowerAscii(const std::string &s)
{
  std::string r = uiUtf8ToLatin1(s);
  for (size_t i = 0; i < r.size(); i++) {
    unsigned char c = (unsigned char)r[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
      r[i] = (char)(c + 0x20);
  }
  return r;
}

// First letter used to jump (A..Z, '#' for digits / symbols).
static char rowInitial(const std::string &name)
{
  for (size_t i = 0; i < name.size(); i++) {
    char ch = name[i];
    if (ch >= 'a' && ch <= 'z') return (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return ch;
    if (ch != ' ' && ch != '[' && ch != '(' && ch != '_' && ch != '-' && ch != '.')
      return '#';
  }
  return '#';
}

struct RowSorter
{
  int mode;
  const std::vector<std::string> *recents;
  int recentIndex(const std::string &path) const
  {
    for (size_t i = 0; i < recents->size(); i++)
      if ((*recents)[i] == path)
        return (int)i;
    return 0x7FFFFFFF;
  }
  // Case-insensitive name order without building lower-case copies.
  static int nameCmp(const std::string &a, const std::string &b)
  {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
      unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[i];
      if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
      if (ca != cb)
        return ca < cb ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
  }
  bool operator()(const UiDirEntry &a, const UiDirEntry &b) const
  {
    if (a.isDir != b.isDir)
      return a.isDir;   // folders always first
    if (!a.isDir && mode == SORT_RECENT) {
      int ra = recentIndex(a.path), rb = recentIndex(b.path);
      if (ra != rb)
        return ra < rb;
    }
    int c = nameCmp(a.name, b.name);
    return (!a.isDir && mode == SORT_NAME_DESC) ? c > 0 : c < 0;
  }
};

int UiHomeScreen::rowHeight() const
{
  return m_tab == TAB_GAMES ? 52 : 68;
}

void UiHomeScreen::openDir(UiApp &app, const std::string &dir, const std::string &selectName)
{
  m_entries.clear();
  m_dirError = false;
  if (dir.empty()) {
    app.host().listDrives(m_entries);
  } else {
    std::vector<UiDirEntry> all;
    if (!app.host().listDir(dir, all)) {
      m_dirError = true;
    }
    for (size_t i = 0; i < all.size(); i++) {
      if (all[i].isDir || !app.cfg().hideNonGames || uiIsRomFile(all[i].name))
        m_entries.push_back(all[i]);
    }
  }
  if (m_dir != dir)
    app.setListFilter("");   // a search only applies to the folder it was made in
  m_dir = dir;
  if (!m_dirError)
    app.cfg().lastDir = dir;
  if (!dir.empty()) {
    RowSorter sorter;
    sorter.mode = app.cfg().listSort;
    sorter.recents = &app.recents().items();
    std::stable_sort(m_entries.begin(), m_entries.end(), sorter);
  }
  m_list[TAB_GAMES] = UiListState();
  if (!selectName.empty()) {
    for (size_t i = 0; i < m_entries.size(); i++) {
      if (m_entries[i].name == selectName || m_entries[i].path == selectName) {
        m_list[TAB_GAMES].sel = (int)i;
        break;
      }
    }
  }
  buildRows(app);
}

void UiHomeScreen::onResume(UiApp &app)
{
  if (m_tab == TAB_GAMES && m_filter != app.listFilter()) {
    // Back from the search keyboard: start at the first match.
    buildRows(app);
    m_list[TAB_GAMES] = UiListState();
    m_list[TAB_GAMES].clamp((int)m_rows.size());
    return;
  }
  refresh(app);
}

void UiHomeScreen::refresh(UiApp &app)
{
  buildRows(app);
}

void UiHomeScreen::buildRows(UiApp &app)
{
  m_rows.clear();
  if (m_tab == TAB_GAMES) {
    m_filter = app.listFilter();
    std::string needle = lowerAscii(m_filter);
    for (size_t i = 0; i < m_entries.size(); i++) {
      if (!needle.empty() && lowerAscii(m_entries[i].name).find(needle) == std::string::npos)
        continue;
      Row r;
      r.title = m_entries[i].name;
      r.path = m_entries[i].path;
      r.isDir = m_entries[i].isDir;
      r.size = m_entries[i].size;
      r.menuId = 0;
      m_rows.push_back(r);
    }
  } else if (m_tab == TAB_RECENT || m_tab == TAB_FAVORITES) {
    const std::vector<std::string> &items =
      m_tab == TAB_RECENT ? app.recents().items() : app.favorites().items();
    for (size_t i = 0; i < items.size(); i++) {
      Row r;
      r.title = uiStripExtension(uiFileName(items[i]));
      r.detail = uiParentDir(items[i]);
      r.path = items[i];
      r.isDir = false;
      r.size = 0;
      r.menuId = 0;
      m_rows.push_back(r);
    }
  } else {
    struct Cat { int id; const char *es, *en, *des, *den; };
    static const Cat cats[] = {
      { MENU_SETTINGS_VIDEO, "Video", "Video", "Filtros, pantalla y FPS", "Filters, screen and FPS" },
      { MENU_SETTINGS_AUDIO, "Audio", "Audio", "Sonido de los juegos y m\xfasica del men\xfa", "Game sound and menu music" },
      { MENU_SETTINGS_EMU, "Emulaci\xf3n", "Emulation", "Velocidad, BIOS y trucos", "Speed, BIOS and cheats" },
      { MENU_SETTINGS_SAVES, "Guardado", "Saving", "Carpetas y guardado autom\xe1tico", "Folders and auto save" },
      { MENU_CONTROLS, "Controles", "Controls", "Botones y sensores (stick derecho)", "Buttons and sensors (right stick)" },
      { MENU_CLOCK, "Reloj de los juegos", "Game clock", "Hora de la consola o personalizada", "Console time or a custom one" },
      { MENU_SETTINGS_UI, "Interfaz e idioma", "Interface & language", "Idioma, pantalla de inicio y lista", "Language, start screen and list" },
      { MENU_SETTINGS_LOOK, "Apariencia", "Appearance", "Tema, fondo, transparencia y efectos", "Theme, background, transparency and effects" },
      { MENU_SETTINGS_KINECT, "Kinect", "Kinect", "Control con la mano y Game Boy Camera", "Hand control and Game Boy Camera" },
      { MENU_SETTINGS_RA, "RetroAchievements", "RetroAchievements", "En pruebas: conexi\xf3n segura", "Testing: secure connection" },
      { MENU_ABOUT, "Acerca de", "About", "Versi\xf3n y cr\xe9""ditos", "Version and credits" },
      { -1, "Salir al Dashboard", "Exit to Dashboard", "Cierra el emulador", "Close the emulator" }
    };
    for (size_t i = 0; i < sizeof(cats) / sizeof(cats[0]); i++) {
      Row r;
      r.title = app.tr(cats[i].es, cats[i].en);
      r.detail = app.tr(cats[i].des, cats[i].den);
      r.isDir = false;
      r.size = 0;
      r.menuId = cats[i].id;
      m_rows.push_back(r);
    }
  }
  if (m_tab == TAB_GAMES) {
    app.setListMatches((int)m_rows.size());   // games and folders that match
  }
  m_list[m_tab].clamp((int)m_rows.size());
}

void UiHomeScreen::activate(UiApp &app)
{
  UiListState &ls = m_list[m_tab];
  if (ls.sel < 0 || ls.sel >= (int)m_rows.size())
    return;
  const Row &r = m_rows[ls.sel];
  if (m_tab == TAB_SETTINGS) {
    if (r.menuId == -1)
      app.openConfirm(app.tr("\xbfSalir al Dashboard?", "Exit to the Dashboard?"), 1, "");
    else
      app.openMenu(r.menuId);
    return;
  }
  if (r.isDir) {
    openDir(app, r.path, "");
    return;
  }
  if (m_tab != TAB_GAMES && !app.host().fileExists(r.path)) {
    app.toast(app.tr("No se encontr\xf3 el archivo", "File not found"));
    return;
  }
  app.launch(r.path);
}

void UiHomeScreen::update(UiApp &app, const UiInputState &in)
{
  if (in.pressed & PAD_BIT(PAD_LB)) {
    m_tab = (m_tab + TAB_COUNT - 1) % TAB_COUNT;
    buildRows(app);
    return;
  }
  if (in.pressed & PAD_BIT(PAD_RB)) {
    m_tab = (m_tab + 1) % TAB_COUNT;
    buildRows(app);
    return;
  }

  UiListState &ls = m_list[m_tab];
  int count = (int)m_rows.size();
  int visible = (UI_FOOTER_Y - 12 - (UI_HEADER_H + 20)) / rowHeight();
  {
    if (in.repeat & PAD_BIT(PAD_UP)) ls.move(-1, count, true);
    if (in.repeat & PAD_BIT(PAD_DOWN)) ls.move(1, count, true);
    if (in.repeat & PAD_BIT(PAD_LEFT)) ls.move(-visible, count, false);
    if (in.repeat & PAD_BIT(PAD_RIGHT)) ls.move(visible, count, false);
  }
  if (m_tab == TAB_GAMES && !m_dir.empty()) {
    // Games: LT / RT jump to the previous / next first letter.
    if (in.repeat & PAD_BIT(PAD_LT)) jumpLetter(-1);
    if (in.repeat & PAD_BIT(PAD_RT)) jumpLetter(1);
  } else {
    if (in.repeat & PAD_BIT(PAD_LT)) ls.move(-count, count, false);
    if (in.repeat & PAD_BIT(PAD_RT)) ls.move(count, count, false);
  }
  ls.ensureVisible(visible);

  if (m_tab == TAB_GAMES && !m_dir.empty()) {
    if (in.pressed & PAD_BIT(PAD_BACK)) {
      app.openSearch();
      return;
    }
    if (in.pressed & PAD_BIT(PAD_START)) {
      cycleSort(app);
      return;
    }
  }

  if (in.pressed & PAD_BIT(PAD_A)) {
    activate(app);
    return;
  }
  if (in.pressed & PAD_BIT(PAD_B)) {
    if (m_tab == TAB_GAMES && !app.listFilter().empty()) {
      // First B clears the search.
      std::string keep = ls.sel >= 0 && ls.sel < count ? m_rows[ls.sel].path : "";
      app.setListFilter("");
      buildRows(app);
      for (size_t i = 0; i < m_rows.size(); i++)
        if (m_rows[i].path == keep) { m_list[TAB_GAMES].sel = (int)i; break; }
      m_list[TAB_GAMES].ensureVisible(visible);
    } else if (m_tab == TAB_GAMES && !m_dir.empty()) {
      std::string child = m_dir;
      openDir(app, uiParentDir(m_dir), child);
    } else if (m_tab != TAB_GAMES) {
      m_tab = TAB_GAMES;
      buildRows(app);
    } else if (app.screenCount() > 1) {
      app.pop();   // drive list: back to the library under it
    }
    return;
  }
  if ((in.pressed & PAD_BIT(PAD_Y)) && m_tab == TAB_GAMES && ls.sel >= 0 && ls.sel < count &&
      m_rows[ls.sel].isDir) {
    app.cfg().libraryDir = m_rows[ls.sel].path;
    app.saveAll();
    app.toast(app.tr("Carpeta de la biblioteca: ", "Library folder: ") + m_rows[ls.sel].title);
    return;
  }

  bool hasRom = ls.sel >= 0 && ls.sel < count && m_tab != TAB_SETTINGS && !m_rows[ls.sel].isDir;
  if (!hasRom)
    return;
  const std::string rom = m_rows[ls.sel].path;
  if (in.pressed & PAD_BIT(PAD_X)) {
    app.toggleFavorite(rom);
    buildRows(app);
  } else if (in.pressed & PAD_BIT(PAD_Y)) {
    app.openMenu(MENU_GAME_OPTIONS, rom);
  } else if ((in.pressed & PAD_BIT(PAD_BACK)) && m_tab == TAB_RECENT) {
    app.recents().remove(rom);
    app.saveAll();
    buildRows(app);
  }
}

void UiHomeScreen::jumpLetter(int dir)
{
  UiListState &ls = m_list[TAB_GAMES];
  int count = (int)m_rows.size();
  if (count == 0)
    return;
  int cur = ls.sel < 0 ? 0 : (ls.sel >= count ? count - 1 : ls.sel);
  // Folders count as one group, then a group per first letter.
  #define ROW_KEY(i) (m_rows[i].isDir ? '\x01' : rowInitial(m_rows[i].title))
  char key = ROW_KEY(cur);
  int i = cur;
  if (dir > 0) {
    while (i < count && ROW_KEY(i) == key) i++;
    if (i >= count) return;
  } else {
    // Start of this group; if already there, start of the previous one.
    while (i > 0 && ROW_KEY(i - 1) == key) i--;
    if (i == cur) {
      if (i == 0) return;
      i--;
      key = ROW_KEY(i);
      while (i > 0 && ROW_KEY(i - 1) == key) i--;
    }
  }
  #undef ROW_KEY
  ls.sel = i;
}

void UiHomeScreen::cycleSort(UiApp &app)
{
  std::string keep;
  UiListState &ls = m_list[TAB_GAMES];
  if (ls.sel >= 0 && ls.sel < (int)m_rows.size())
    keep = m_rows[ls.sel].path;
  app.cfg().listSort = (app.cfg().listSort + 1) % SORT_COUNT;
  app.saveAll();
  RowSorter sorter;
  sorter.mode = app.cfg().listSort;
  sorter.recents = &app.recents().items();
  std::stable_sort(m_entries.begin(), m_entries.end(), sorter);
  buildRows(app);
  for (size_t i = 0; i < m_rows.size(); i++)
    if (m_rows[i].path == keep) { ls.sel = (int)i; break; }
  static const char *es[SORT_COUNT] = { "Orden: A-Z", "Orden: Z-A", "Orden: jugados recientemente" };
  static const char *en[SORT_COUNT] = { "Order: A-Z", "Order: Z-A", "Order: recently played" };
  app.toast(app.tr(es[app.cfg().listSort], en[app.cfg().listSort]));
}

void UiHomeScreen::drawList(UiApp &app, UiCanvas &c, int x, int y, int w, int h)
{
  UiListState &ls = m_list[m_tab];
  int rh = rowHeight();
  int visible = h / rh;
  ls.ensureVisible(visible);

  if (m_rows.empty()) {
    const char *msg;
    if (m_tab == TAB_GAMES && !m_filter.empty())
      msg = app.tr("Ning\xfan juego coincide con la b\xfasqueda (B: quitarla)",
                   "No game matches the search (B: clear it)");
    else if (m_tab == TAB_GAMES)
      msg = m_dirError ? app.tr("No se pudo abrir esta carpeta", "This folder could not be opened")
                       : app.tr("No hay juegos en esta carpeta", "No games in this folder");
    else if (m_tab == TAB_RECENT)
      msg = app.tr("A\xfan no has jugado nada", "You have not played anything yet");
    else
      msg = app.tr("Pulsa X sobre un juego para a\xf1""adirlo", "Press X on a game to add it");
    c.drawTextFit(UI_FONT_BODY, x, y + 40, w, msg, UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
    return;
  }

  c.setClip(x - 8, y, w + 16, h);
  for (int i = 0; i < visible + 1; i++) {
    int idx = ls.scroll + i;
    if (idx >= (int)m_rows.size())
      break;
    const Row &r = m_rows[idx];
    int ry = y + i * rh;
    bool sel = idx == ls.sel;
    if (sel)
      uiDrawRowSelection(c, x, ry + 2, w - 16, rh - 4);
    int tx = x + 20;
    if (m_tab == TAB_GAMES) {
      if (r.isDir) {
        drawFolderIcon(c, tx, ry + 13, COL_DIR);
        tx += 40;
      } else {
        // Fixed-width type column so names line up.
        const char *k = romKind(r.title);
        int bw = c.textWidth(UI_FONT_SMALL, k) + 14;
        drawBadge(c, tx + (52 - bw) / 2, ry + 14, k, 0x30FFFFFF, UI_COL_TEXT);
        tx += 52 + 12;
      }
      int right = 0;
      if (!r.isDir && app.favorites().contains(r.path)) {
        c.fillCircle(x + w - 40, ry + rh / 2, 6, COL_FAV);
        right = 32;
      }
      std::string name = r.isDir ? r.title : uiStripExtension(r.title);
      c.drawTextFit(UI_FONT_BODY, tx, ry + 12, x + w - 36 - right - tx, name.c_str(),
                    sel ? UI_COL_TEXT : 0xFFD0D5E2);
    } else {
      c.drawTextFit(UI_FONT_BODY, tx, ry + 8, w - 60, r.title.c_str(),
                    sel ? UI_COL_TEXT : 0xFFD0D5E2);
      c.drawTextFit(UI_FONT_SMALL, tx, ry + 38, w - 60, r.detail.c_str(), UI_COL_TEXT_DIM);
      if (m_tab == TAB_SETTINGS && r.menuId > 0)
        drawArrow(c, x + w - 44, ry + rh / 2, 7, true, sel ? UI_COL_TEXT : UI_COL_TEXT_DIM);
    }
  }
  c.resetClip();
  drawScrollbar(c, x + w - 6, y, h, ls.scroll, visible, (int)m_rows.size());
}

void UiHomeScreen::drawInfo(UiApp &app, UiCanvas &c, int x, int y, int w, int h)
{
  uiDrawPanel(c, x, y, w, h, 16, 0x60);
  UiListState &ls = m_list[m_tab];
  bool valid = ls.sel >= 0 && ls.sel < (int)m_rows.size();
  int pad = 20;
  int tw = w - pad * 2, th = tw * 2 / 3;
  int ty = y + pad;

  if (m_tab == TAB_SETTINGS) {
    c.drawTextFit(UI_FONT_TITLE, x + pad, ty, tw, UI_APP_NAME, UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x + pad, ty + 46, tw, app.tr("basado en VBA360", "based on VBA360"), UI_COL_ACCENT);
    if (valid) {
      c.drawTextFit(UI_FONT_BODY, x + pad, ty + 100, tw, m_rows[ls.sel].title.c_str(), UI_COL_TEXT);
      c.drawTextFit(UI_FONT_SMALL, x + pad, ty + 134, tw, m_rows[ls.sel].detail.c_str(), UI_COL_TEXT_DIM);
    }
    return;
  }
  if (!valid) {
    c.fillRoundRect(x + pad, ty, tw, th, 10, 0x40000000);
    return;
  }
  const Row &r = m_rows[ls.sel];
  if (r.isDir) {
    c.fillRoundRect(x + pad, ty, tw, th, 10, 0x40000000);
    drawFolderIcon(c, x + w / 2 - 13, ty + th / 2 - 12, COL_DIR);
    c.drawTextFit(UI_FONT_BODY, x + pad, ty + th + 16, tw, r.title.c_str(), UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x + pad, ty + th + 50, tw,
                  m_dir.empty() ? app.tr("Unidad", "Drive") : app.tr("Carpeta", "Folder"),
                  UI_COL_TEXT_DIM);
    return;
  }

  // New covers are read only once the list stops moving.
  const UiThumb *t = 0;
  const UiApp::Cover *cv = gameArt(app, r.path, t);
  int boxH = th;
  if (cv) {
    // Box art: as wide as the panel, as tall as its proportions need
    // (leaving room for the name and details below).
    int maxH = h - pad * 2 - 140;
    if (maxH > UiApp::COVER_MAX_H) maxH = UiApp::COVER_MAX_H;
    int dw = tw, dh = cv->h * tw / cv->w;
    if (dh > maxH) { dh = maxH; dw = cv->w * maxH / cv->h; }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int dx = x + pad + (tw - dw) / 2;
    c.fillRoundRect(dx - 3, ty - 3, dw + 6, dh + 6, 8, 0x50000000);
    c.blitScaled(&cv->px[0], cv->w, cv->h, cv->w, dx, ty, dw, dh);
    boxH = dh;
  } else if (t) {
    c.fillRoundRect(x + pad - 2, ty - 2, tw + 4, th + 4, 10, 0x40000000);
    c.blitScaled(&t->px[0], t->w, t->h, t->w, x + pad, ty, tw, th);
  } else {
    c.fillRoundRect(x + pad, ty, tw, th, 10, 0x40000000);
    const char *k = romKind(r.path);
    c.drawTextFit(UI_FONT_TITLE, x + pad, ty + th / 2 - 20, tw, k, 0x50FFFFFF, UI_ALIGN_CENTER);
  }
  int ly = ty + boxH + 16;
  std::string name = uiStripExtension(uiFileName(r.path));
  c.drawTextFit(UI_FONT_BODY, x + pad, ly, tw, name.c_str(), UI_COL_TEXT);
  ly += 40;

  char line[256];
  if (r.size) {
    const char *k = romKind(r.path);
    int bw = c.textWidth(UI_FONT_SMALL, k) + 14;
    drawBadge(c, x + pad, ly, k, 0x30FFFFFF, UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x + pad + bw + 10, ly, tw - bw - 10, formatSize(r.size).c_str(), UI_COL_TEXT_DIM);
    ly += 28;
  }
  std::string patch = findPatch(app, r.path);
  if (!patch.empty()) {
    sprintf(line, "%s %s", app.tr("Parche:", "Patch:"), patch.c_str());
    c.drawTextFit(UI_FONT_SMALL, x + pad, ly, tw, line, 0xFF8EE3A8);
    ly += 28;
  }
  if (app.favorites().contains(r.path)) {
    c.fillCircle(x + pad + 6, ly + 11, 6, COL_FAV);
    c.drawText(UI_FONT_SMALL, x + pad + 20, ly, app.tr("En favoritos", "In favorites"), COL_FAV);
  }
}

// --- Library: consoles, then their games as box art --------------------------

namespace {

enum { SYS_GBA, SYS_GBC, SYS_GB, SYS_OTHER, SYS_COUNT };
const char *const kSysName[SYS_COUNT] = { "Game Boy Advance", "Game Boy Color", "Game Boy", "" };
const char *const kSysYear[SYS_COUNT] = { "Nintendo \xb7 2001", "Nintendo \xb7 1998", "Nintendo \xb7 1989", "" };

// Which console a ROM belongs to, by its file type (zip / 7z go apart).
int romSystem(const std::string &path)
{
  std::string e = uiExtension(path);
  if (e == ".gbc" || e == ".cgb") return SYS_GBC;
  if (e == ".gb" || e == ".dmg" || e == ".sgb") return SYS_GB;
  if (e == ".zip" || e == ".7z") return SYS_OTHER;
  return SYS_GBA;
}

std::string romTitle(const std::string &path) { return uiStripExtension(uiFileName(path)); }

bool nameLess(const std::string &a, const std::string &b)
{
  return lowerAscii(uiFileName(a)) < lowerAscii(uiFileName(b));
}

// "Game Boy Advance - USA - Not played yet" from the file name and recents.
std::string romMeta(UiApp &app, const std::string &rom, int sys)
{
  std::string m = sys == SYS_OTHER ? std::string(app.tr("Comprimido", "Archive")) : kSysName[sys];
  std::string n = lowerAscii(uiFileName(rom));
  static const char *const reg[][3] = {
    { "usa", "USA", "USA" }, { "europe", "Europa", "Europe" }, { "japan", "Jap\xf3n", "Japan" },
    { "spain", "Espa\xf1""a", "Spain" }, { "world", "Mundo", "World" }, { 0, 0, 0 } };
  size_t p = n.find('(');
  if (p != std::string::npos)
    for (int i = 0; reg[i][0]; i++)
      if (n.find(reg[i][0], p) != std::string::npos) {
        m += std::string("  \xb7  ") + app.tr(reg[i][1], reg[i][2]);
        break;
      }
  m += std::string("  \xb7  ") + (app.recents().contains(rom) ? app.tr("Jugado", "Played")
                                                               : app.tr("Sin jugar", "Not played yet"));
  return m;
}

// Box art cropped to fill a cartridge label, keeping the upper part (logos).
// skipLeft: percent of the art's left side left out (the GBA box side strip).
void drawLabelArt(UiCanvas &c, const UiApp::Cover *cv, int x, int y, int w, int h, int bright, int skipLeft)
{
  int left = cv->w * skipLeft / 100, aw = cv->w - left;
  int sx = left, sy = 0, sw = aw, sh = cv->h;
  if (aw * h > cv->h * w) { sw = cv->h * w / h; sx = left + (aw - sw) / 2; }
  else { sh = aw * h / w; sy = (cv->h - sh) / 2; }
  if (sw > 0 && sh > 0)
    c.blitScaled(&cv->px[sy * cv->w + sx], sw, sh, cv->w, x, y, w, h, bright);
}

UiColor dim(UiColor col, int bright)
{
  return 0xFF000000 | ((((col >> 16) & 0xFF) * bright >> 8) << 16) |
         ((((col >> 8) & 0xFF) * bright >> 8) << 8) | ((col & 0xFF) * bright >> 8);
}

int cartHeight(int sys, int w) { return (sys == SYS_GB || sys == SYS_GBC) ? w * 114 / 100 : w * 62 / 100; }

// A console's cartridge seen from the front; its label is the box art of
// the game last chosen there (or the console's name).
void drawCart(UiCanvas &c, int sys, int x, int y, int w, const UiApp::Cover *cv, const char *label, int bright)
{
  bool tall = sys == SYS_GB || sys == SYS_GBC;
  int h = cartHeight(sys, w), rad = w / 16;
  UiColor plastic = sys == SYS_GB ? 0xFFB9B9B3 : sys == SYS_GBC ? 0xFF2C2A36 : sys == SYS_OTHER ? 0xFF5A6070 : 0xFF4B4E58;
  plastic = dim(plastic, bright);
  c.fillRoundRect(x + 6, y + 10, w, h, rad, 0x60000000);                 // shadow
  c.fillRoundRect(x, y, w, h, rad, dim(sys == SYS_GB ? 0xFF8F8F89 : 0xFF1A1A1F, bright));
  c.fillRoundRect(x + 2, y, w - 4, h - 3, rad, plastic);
  c.fillRect(x + rad, y + 2, w - rad * 2, 2, 0x30FFFFFF);
  int lx, ly, lw, lh;
  if (tall) {   // label up top, grooves near the bottom
    for (int i = 0; i < 4; i++)
      c.fillRect(x + w / 8, y + h * (84 + i * 3) / 100, w * 3 / 4, 1, 0x40000000);
    lx = x + w / 10; ly = y + h * 9 / 100; lw = w * 8 / 10; lh = h * 64 / 100;
  } else {      // grip along the top, label below it
    for (int i = 0; i < 3; i++)
      c.fillRect(x + w / 10, y + h * (5 + i * 3) / 100, w * 8 / 10, 1, 0x40000000);
    lx = x + w * 8 / 100; ly = y + h * 22 / 100; lw = w * 84 / 100; lh = h * 68 / 100;
  }
  c.fillRoundRect(lx - 3, ly - 3, lw + 6, lh + 6, 5, 0xFF16161A);
  if (cv) {
    drawLabelArt(c, cv, lx, ly, lw, lh, bright, tall ? 0 : 13);
  } else {
    c.fillGradient(lx, ly, lw, lh, dim(0xFFE9E4D8, bright), dim(0xFFC9C2B2, bright));
    c.drawTextFit(w > 200 ? UI_FONT_BODY : UI_FONT_SMALL, lx + 6, ly + lh / 2 - 14, lw - 12, label,
                  0xFF2A2A30, UI_ALIGN_CENTER);
  }
}

// The square of a picture a box shows: the middle of the box art, or, for a
// game without one, the console's plain box (made once per console and
// theme color, so the cover flow and shelves can treat both the same).
struct BoxPic { const UiColor *px; int w, h, pitch; };

BoxPic boxPic(const UiApp::Cover *cv, const std::string &rom)
{
  BoxPic b;
  if (cv) {
    int side = cv->w < cv->h ? cv->w : cv->h;
    b.px = &cv->px[((cv->h - side) / 2) * cv->w + (cv->w - side) / 2];
    b.w = b.h = side;
    b.pitch = cv->w;
    return b;
  }
  enum { S = 200 };
  static UiCanvas plain[4];
  static UiColor made[4] = { 0, 0, 0, 0 };
  const char *kind = romKind(rom);
  int k = !strcmp(kind, "GBC") ? 1 : !strcmp(kind, "GB") ? 2 : !strcmp(kind, "GBA") ? 0 : 3;
  UiColor key = UI_COL_ACCENT ^ (UI_COL_PANEL << 1);
  if (made[k] != key || !plain[k].pixels()) {
    if (!plain[k].pixels())
      plain[k].create(S, S);
    UiCanvas &c = plain[k];
    c.fillGradient(0, 0, S, S, dim(UI_COL_ACCENT, 150), dim(UI_COL_PANEL, 230));
    c.fillGradient(0, 0, S, S / 2, 0x26FFFFFF, 0x00FFFFFF);      // a sheen on the upper half
    c.fillRect(0, 0, S, 2, 0x50FFFFFF);
    c.drawTextFit(UI_FONT_TITLE, 0, S / 2 - 26, S, kind[0] ? kind : "?", 0xB0FFFFFF, UI_ALIGN_CENTER);
    made[k] = key;
  }
  b.px = plain[k].pixels();
  b.w = b.h = b.pitch = S;
  return b;
}

// The box art, read when the list stops (and drawn again until it is).
const UiApp::Cover *boxArt(UiApp &app, const std::string &rom)
{
  const UiApp::Cover *cv = app.cover(rom, !app.inputHeld());
  if (!cv && !app.coverChecked(rom))
    app.drawAgain();
  return cv;
}

// A small heart (favorites), centered on (cx, cy).
void drawHeart(UiCanvas &c, int cx, int cy, int r, UiColor col)
{
  c.fillCircle(cx - r / 2, cy - r / 4, r / 2 + 1, col);
  c.fillCircle(cx + r / 2, cy - r / 4, r / 2 + 1, col);
  for (int row = 0; row <= r; row++) {   // the point
    int half = r - row;
    c.fillRect(cx - half, cy - r / 4 + row, half * 2 + 1, 1, col);
  }
}

enum { GRID_COLS = 6, GRID_ROWS = 2 };
enum { GRID_GAP = 20, GRID_PAD = 8, GRID_NAME_H = 30, GRID_STRIP_H = 58 };

// The cards' size and spacing in a w x h area (drawGrid and the hand cursor).
void gridLayout(int w, int h, int &cardW, int &cardH, int &rowH)
{
  cardW = (w - GRID_GAP * (GRID_COLS - 1)) / GRID_COLS;
  cardH = GRID_PAD + (cardW - GRID_PAD * 2) + GRID_PAD + GRID_NAME_H;
  rowH = (h - GRID_STRIP_H - cardH * GRID_ROWS) / GRID_ROWS + cardH;
}

// What the hand points at in the Library's grid of games: a game's index,
// one of these, or -1.
enum { HAND_BACK = 1 << 20, HAND_UP, HAND_DOWN };

int gridHandTarget(int px, int py, int x, int y, int w, int h, int scroll, int count)
{
  int dx = px - (x + 20), dy = py - 52;
  if (dx * dx + dy * dy < 40 * 40)
    return HAND_BACK;   // the round back button by the title
  int cardW, cardH, rowH;
  gridLayout(w, h, cardW, cardH, rowH);
  if (py < y)
    return scroll > 0 ? HAND_UP : -1;
  if (py > y + GRID_ROWS * rowH - GRID_GAP)
    return (scroll + GRID_ROWS) * GRID_COLS < count ? HAND_DOWN : -1;
  int col = (px - x) / (cardW + GRID_GAP), row = (py - y) / rowH;
  if (px < x || col >= GRID_COLS || (px - x) % (cardW + GRID_GAP) >= cardW || (py - y) % rowH >= cardH)
    return -1;
  int idx = (scroll + row) * GRID_COLS + col;
  return idx < count ? idx : -1;
}

// The games as cards: box art on a rounded panel with the name under it.
// The chosen one rises, gets an accent ring and full brightness.
// scrollPx: how far down the list the view is, in pixels (rows * rowH when
// still; in between while it slides).
void drawGrid(UiApp &app, UiCanvas &c, int x, int y, int w, int h,
              const std::vector<std::string> &roms, const UiListState &ls, int sys, int scrollPx)
{
  const int count = (int)roms.size();
  const int gap = GRID_GAP, pad = GRID_PAD, stripH = GRID_STRIP_H;
  int cardW, cardH, rowH;
  gridLayout(w, h, cardW, cardH, rowH);
  const int art = cardW - pad * 2;
  int first = scrollPx / rowH;
  c.setClip(x - 10, y - 10, w + 20, GRID_ROWS * rowH - gap + 20);   // cards slide in and out of the grid
  for (int r = 0; r <= GRID_ROWS; r++) {
    for (int col = 0; col < GRID_COLS; col++) {
      int idx = (first + r) * GRID_COLS + col;
      if (idx >= count)
        break;
      bool sel = idx == ls.sel;
      int cx = x + col * (cardW + gap), cy = y + (first + r) * rowH - scrollPx + (sel ? 0 : 6);
      if (sel) {   // a soft glow and a ring in the accent color
        c.fillRoundRect(cx - 6, cy - 6, cardW + 12, cardH + 12, 20, uiWithAlpha(UI_COL_ACCENT, 0x38));
        uiDrawPanel(c, cx, cy, cardW, cardH, 14, 0xE8);
        c.strokeRoundRect(cx - 1, cy - 1, cardW + 2, cardH + 2, 15, UI_COL_ACCENT);
        c.strokeRoundRect(cx - 2, cy - 2, cardW + 4, cardH + 4, 16, UI_COL_ACCENT);
      } else {
        uiDrawPanel(c, cx, cy, cardW, cardH, 14, 0xB8);
      }
      const UiApp::Cover *cv = boxArt(app, roms[idx]);
      BoxPic b = boxPic(cv, roms[idx]);
      c.blitCard(b.px, b.w, b.h, b.pitch, cx + pad, art, cy + pad, art, cy + pad, art, sel ? 256 : 190, 0);
      if (app.favorites().contains(roms[idx])) {   // heart badge on the art's corner
        int hx = cx + pad + art - 16, hy = cy + pad + 16;
        c.fillCircle(hx, hy, 13, uiWithAlpha(UI_COL_ACCENT, 0xF0));
        drawHeart(c, hx, hy + 1, 7, 0xFFFFFFFF);
      }
      c.drawTextFit(UI_FONT_SMALL, cx + 8, cy + pad + art + 6, cardW - 16, romTitle(roms[idx]).c_str(),
                    sel ? UI_COL_TEXT : UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
    }
  }
  c.resetClip();
  drawScrollbar(c, x + w + 14, y, GRID_ROWS * rowH - gap, ls.scroll, GRID_ROWS,
                (count + GRID_COLS - 1) / GRID_COLS);
  if (ls.sel < 0 || ls.sel >= count)
    return;
  int sy = y + h - stripH + 14;
  c.fillRect(x, sy - 8, w, 1, 0x30FFFFFF);
  c.drawTextFit(UI_FONT_BODY, x, sy, w * 6 / 10, romTitle(roms[ls.sel]).c_str(), UI_COL_TEXT);
  c.drawTextFit(UI_FONT_SMALL, x + w * 6 / 10, sy + 6, w * 4 / 10, romMeta(app, roms[ls.sel], sys).c_str(),
                UI_COL_TEXT_DIM, UI_ALIGN_RIGHT);
}

// What the last look through the folders found, kept between Library screens:
// coming back from a game or the browser rebuilt it by walking every Roms
// folder and reading carts\ again, which was slow on the console. Read again
// when the emulator starts or the library folder changes.
// shortcut: ROMs copied over FTP while the emulator runs show up after a restart.
struct LibraryCache
{
  bool valid;
  std::string root;
  std::vector<std::string> games[SYS_COUNT];
  std::vector<UiColor> cartPx[SYS_COUNT];   // carts\<gba|gbc|gb|zip>.png (empty: built-in)
  int cartW[SYS_COUNT], cartH[SYS_COUNT];
  LibraryCache() : valid(false) {}
};
LibraryCache g_library;

void scanDir(UiApp &app, const std::string &dir, int depth, int &budget)
{
  std::vector<UiDirEntry> e;
  if (budget <= 0 || !app.host().listDir(dir, e))
    return;
  for (size_t i = 0; i < e.size() && budget > 0; i++, budget--) {
    if (e[i].isDir) {
      if (depth < 3)
        scanDir(app, e[i].path, depth + 1, budget);
    } else if (uiIsRomFile(e[i].name) && lowerAscii(e[i].name).find("bios") == std::string::npos) {
      int sys = romSystem(e[i].path);
      if (sys == SYS_OTHER) {   // a zip / 7z goes with the console of the game inside
        std::string inner = app.host().archiveRomName(e[i].path);
        if (!inner.empty())
          sys = romSystem(inner);
      }
      g_library.games[sys].push_back(e[i].path);
    }
  }
}

// Fills g_library: the games of the library folder (or of every drive's
// Roms folder) by console, and the player's own cartridge pictures.
void readLibrary(UiApp &app)
{
  for (int s = 0; s < SYS_COUNT; s++)
    g_library.games[s].clear();
  g_library.root = app.cfg().libraryDir;
  std::vector<std::string> roots;
  if (!g_library.root.empty()) {
    roots.push_back(g_library.root);
  } else {
    std::vector<UiDirEntry> drives;
    app.host().listDrives(drives);
    for (size_t i = 0; i < drives.size(); i++)
      roots.push_back(uiJoinPath(drives[i].path, "Roms") + "\\");
  }
  int budget = 20000;   // shortcut: stops after 20000 files/folders; a whole drive as the library could miss some
  for (size_t i = 0; i < roots.size(); i++)
    scanDir(app, roots[i], 0, budget);
  static const char *const cartFile[SYS_COUNT] = { "gba.png", "gbc.png", "gb.png", "zip.png" };
  std::string carts = uiJoinPath(app.host().emulatorDir(), "carts");
  for (int s = 0; s < SYS_COUNT; s++) {
    bool wide = s == SYS_GBA || s == SYS_OTHER;
    std::vector<UiColor> &px = g_library.cartPx[s];
    px.clear();
    g_library.cartW[s] = g_library.cartH[s] = 0;
    if (!app.host().loadImageFit(uiJoinPath(carts, cartFile[s]), wide ? 440 : 260, wide ? 300 : 320,
                                 px, g_library.cartW[s], g_library.cartH[s]))
      px.clear();
    std::sort(g_library.games[s].begin(), g_library.games[s].end(), nameLess);
  }
  g_library.valid = true;
}

void ensureLibrary(UiApp &app)
{
  if (!g_library.valid || g_library.root != app.cfg().libraryDir)
    readLibrary(app);
}

} // namespace

UiLibraryScreen::UiLibraryScreen(UiApp &app, const std::string &selectRom)
  : m_sys(0), m_inSystem(false), m_rescan(false), m_favOnly(false), m_viewDirty(true), m_scrollPx(-1), m_scrollSys(-1)
{
  scan(app);
  if (selectRom.empty())
    return;
  for (size_t s = 0; s < m_systems.size(); s++) {
    const std::vector<std::string> &g = m_games[m_systems[s]];
    for (size_t i = 0; i < g.size(); i++)
      if (g[i] == selectRom) {
        m_sys = (int)s;
        m_inSystem = true;
        m_list[m_systems[s]].sel = (int)i;
      }
  }
}

void UiLibraryScreen::scan(UiApp &app)
{
  ensureLibrary(app);
  m_root = g_library.root;
  for (int s = 0; s < SYS_COUNT; s++)
    m_games[s] = g_library.games[s];
  m_systems.clear();
  for (int s = 0; s < SYS_COUNT; s++) {
    m_list[s].clamp((int)m_games[s].size());
    if (!m_games[s].empty())
      m_systems.push_back(s);
  }
  if (m_sys >= (int)m_systems.size())
    m_sys = 0;
  if (m_systems.empty())
    m_inSystem = false;
}

void uiLibraryRoms(UiApp &app, std::vector<std::string> &out)
{
  ensureLibrary(app);
  out.clear();
  for (int s = 0; s < SYS_COUNT; s++)
    out.insert(out.end(), g_library.games[s].begin(), g_library.games[s].end());
}

// The games of the console that pass RB (favorites) and LB (search), keeping
// the chosen one chosen.
void UiLibraryScreen::buildView(UiApp &app, int sys)
{
  UiListState &ls = m_list[sys];
  std::string keep = ls.sel >= 0 && ls.sel < (int)m_view.size() ? m_view[ls.sel] : "";
  std::string needle = lowerAscii(app.listFilter());
  const std::vector<std::string> &g = m_games[sys];
  m_view.clear();
  for (size_t i = 0; i < g.size(); i++) {
    if (m_favOnly && !app.favorites().contains(g[i]))
      continue;
    if (!needle.empty() && lowerAscii(uiFileName(g[i])).find(needle) == std::string::npos)
      continue;
    if (g[i] == keep)
      ls.sel = (int)m_view.size();
    m_view.push_back(g[i]);
  }
  ls.clamp((int)m_view.size());
  m_viewDirty = false;
  m_viewFilter = app.listFilter();
  app.setListMatches((int)m_view.size());   // the search keyboard shows it
}

void UiLibraryScreen::onResume(UiApp &app)
{
  m_viewDirty = true;   // favorites or the search may have changed
  if (m_rescan) {
    m_rescan = false;
    scan(app);   // read again only if the library folder changed (Y in the browser)
  }
}

void UiLibraryScreen::update(UiApp &app, const UiInputState &in)
{
  if (in.pressed & PAD_BIT(PAD_BACK) || (!m_inSystem && (in.pressed & PAD_BIT(PAD_Y)))) {
    m_rescan = true;   // files may change in the browser
    app.push(new UiHomeScreen(app, ""));
    return;
  }
  if (m_systems.empty())
    return;
  if (!m_inSystem) {
    if (app.handActive()) {   // hand: left / right cartridge to move, the middle one to open
      int sw = app.handSwipe();   // a swipe moves the cartridges with the hand
      if (sw == UiApp::SWIPE_LEFT && m_sys < (int)m_systems.size() - 1) m_sys++;
      if (sw == UiApp::SWIPE_RIGHT && m_sys > 0) m_sys--;
      int t = app.handX() < UI_W / 2 - 200 ? 0 : app.handX() > UI_W / 2 + 200 ? 2 : 1;
      if (app.handDwell(t)) {
        if (t == 0 && m_sys > 0) m_sys--;
        else if (t == 2 && m_sys < (int)m_systems.size() - 1) m_sys++;
        else if (t == 1) { m_inSystem = true; m_viewDirty = true; return; }
      }
    }
    if ((in.repeat & PAD_BIT(PAD_LEFT)) && m_sys > 0) m_sys--;
    if ((in.repeat & PAD_BIT(PAD_RIGHT)) && m_sys < (int)m_systems.size() - 1) m_sys++;
    if (in.pressed & PAD_BIT(PAD_A)) {
      m_inSystem = true;
      m_viewDirty = true;
    }
    return;
  }
  int sys = m_systems[m_sys];
  if (m_viewDirty)
    buildView(app, sys);
  const std::vector<std::string> &g = m_view;
  UiListState &ls = m_list[sys];
  int count = (int)g.size();
  if (in.pressed & PAD_BIT(PAD_LB)) {   // search this console's games
    app.openSearch();
    return;
  }
  if (in.pressed & PAD_BIT(PAD_RB)) {   // only favorites / all
    m_favOnly = !m_favOnly;
    buildView(app, sys);
    count = (int)g.size();
  }
  if ((in.repeat & PAD_BIT(PAD_LEFT)) && ls.sel > 0) ls.sel--;
  if ((in.repeat & PAD_BIT(PAD_RIGHT)) && ls.sel < count - 1) ls.sel++;
  const int step = GRID_COLS;
  if (app.handActive()) {   // hand: pointing chooses, staying still opens
    int top = UI_HEADER_H + 20;
    int t = gridHandTarget(app.handX(), app.handY(), UI_MARGIN_X, top, UI_W - UI_MARGIN_X * 2 - 20,
                           UI_FOOTER_Y - 12 - top, ls.scroll, count);
    // A swipe turns a page (two rows); the list moves with the hand.
    int sw = app.handSwipe(), page = step * GRID_ROWS;
    if (sw == UiApp::SWIPE_UP || sw == UiApp::SWIPE_LEFT) {
      ls.sel = std::min(ls.sel + page, count - 1);
      ls.scroll = std::min(ls.scroll + GRID_ROWS, std::max(0, (count + step - 1) / step - GRID_ROWS));
      t = -1;
    } else if (sw == UiApp::SWIPE_DOWN || sw == UiApp::SWIPE_RIGHT) {
      ls.sel = std::max(ls.sel - page, 0);
      ls.scroll = std::max(ls.scroll - GRID_ROWS, 0);
      t = -1;
    }
    if (t >= 0 && t < count)
      ls.sel = t;
    if (app.handDwell(t)) {
      if (t == HAND_BACK) {
        m_inSystem = false;
        return;
      }
      if (t == HAND_UP && ls.sel >= step) ls.sel -= step;
      else if (t == HAND_DOWN) ls.sel = std::min(ls.sel + step, count - 1);
      else if (t >= 0 && t < count) {
        app.launch(g[t]);
        return;
      }
    }
  }
  if ((in.repeat & PAD_BIT(PAD_UP)) && ls.sel >= step) ls.sel -= step;
  if ((in.repeat & PAD_BIT(PAD_DOWN)) && ls.sel < count - 1) ls.sel = std::min(ls.sel + step, count - 1);
  // LT / RT: previous / next first letter.
  if (count > 0 && (in.repeat & (PAD_BIT(PAD_LT) | PAD_BIT(PAD_RT)))) {
    int dir = (in.repeat & PAD_BIT(PAD_RT)) ? 1 : -1;
    char key = rowInitial(uiFileName(g[ls.sel]));
    int i = ls.sel;
    while (i + dir >= 0 && i + dir < count && rowInitial(uiFileName(g[i + dir])) == key) i += dir;
    if (i + dir >= 0 && i + dir < count) i += dir;
    if (dir < 0) {   // to the first game of that letter
      key = rowInitial(uiFileName(g[i]));
      while (i > 0 && rowInitial(uiFileName(g[i - 1])) == key) i--;
    }
    ls.sel = i;
  }
  int row = ls.sel / GRID_COLS;
  if (row < ls.scroll) ls.scroll = row;
  if (row >= ls.scroll + GRID_ROWS) ls.scroll = row - GRID_ROWS + 1;

  if (in.pressed & PAD_BIT(PAD_B)) {
    if (!app.listFilter().empty()) {   // first B clears the search
      app.setListFilter("");
      m_viewDirty = true;
    } else {
      m_inSystem = false;
    }
  } else if (count > 0 && (in.pressed & PAD_BIT(PAD_A))) {
    app.launch(g[ls.sel]);
  } else if (count > 0 && (in.pressed & PAD_BIT(PAD_X))) {
    app.toggleFavorite(g[ls.sel]);
    m_viewDirty = m_favOnly;
  } else if (count > 0 && (in.pressed & PAD_BIT(PAD_Y))) {
    app.openMenu(MENU_GAME_OPTIONS, g[ls.sel]);
  }
}

void UiLibraryScreen::draw(UiApp &app, UiCanvas &c)
{
  app.drawBackground(c);
  const int x = UI_MARGIN_X, w = UI_W - UI_MARGIN_X * 2;
  if (m_systems.empty()) {
    c.drawText(UI_FONT_TITLE, x, 30, UI_APP_NAME, UI_COL_TEXT);
    std::string where = m_root.empty() ? std::string(app.tr("No hay juegos en las carpetas Roms.", "No games in the Roms folders."))
                                       : std::string(app.tr("No hay juegos en ", "No games in ")) + m_root;
    c.drawTextFit(UI_FONT_BODY, x, 290, w, where.c_str(), UI_COL_TEXT, UI_ALIGN_CENTER);
    c.drawTextFit(UI_FONT_SMALL, x, 340, w,
                  app.tr("Pon tus juegos en una carpeta Roms (disco duro o USB), o pulsa BACK y Y sobre otra carpeta.",
                         "Put your games in a Roms folder (hard drive or USB), or press BACK and Y on another folder."),
                  UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
    const char *hints[] = { app.tr("BACK:Explorador y ajustes", "BACK:Browser and settings") };
    app.drawHints(c, hints, 1);
    return;
  }
  int sys = m_systems[m_sys];
  if (!m_inSystem) {
    // The consoles, each as its cartridge, side by side.
    c.drawText(UI_FONT_TITLE, x, 30, UI_APP_NAME, UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x, 86, w,
                  m_root.empty() ? app.tr("Carpetas Roms", "Roms folders") : m_root.c_str(), UI_COL_TEXT_DIM);
    const int cx = UI_W / 2, floor = 470;
    int n = (int)m_systems.size();
    for (int pass = 2; pass >= 0; pass--)
      for (int side = -1; side <= 1; side += 2) {
        int k = pass == 0 ? 0 : side * pass;
        if (pass == 0 && side > 0)
          continue;
        int i = m_sys + k;
        if (i < 0 || i >= n)
          continue;
        int s = m_systems[i];
        int pct = pass == 0 ? 100 : pass == 1 ? 55 : 40, bright = pass == 0 ? 256 : pass == 1 ? 170 : 110;
        int cxk = cx + side * (pass == 0 ? 0 : pass == 1 ? 320 : 530);
        // The player's picture from carts\, else the built-in generic one.
        UiGlyph own = { "", g_library.cartW[s], g_library.cartH[s],
                        g_library.cartPx[s].empty() ? 0 : &g_library.cartPx[s][0] };
        const UiGlyph *img = own.px ? &own : uiCartImage(s == SYS_GBA ? "GBA" : s == SYS_GBC ? "GBC" : s == SYS_GB ? "GB" : "");
        if (img) {
          int cw = img->w * pct / 100, ch = img->h * pct / 100;
          c.fillRoundRect(cxk - cw / 2 + 8, floor - ch + 14, cw - 4, ch - 6, 14, 0x50000000);   // shadow
          // Scaled once per console and size, then only copied: rescaling every
          // frame cost as much as the rest of the screen.
          static std::vector<UiColor> scaled[SYS_COUNT][3];
          static const UiColor *from[SYS_COUNT][3];
          std::vector<UiColor> &sc = scaled[s][pass];
          if (sc.size() != (size_t)(cw * ch) || from[s][pass] != img->px) {
            uiScaleImage(img->px, img->w, img->h, cw, ch, bright, sc);
            from[s][pass] = img->px;
          }
          c.blitAlpha(&sc[0], cw, ch, cxk - cw / 2, floor - ch);
        } else {     // zip / 7z: a plain cartridge
          int cw = 420 * pct / 100;
          drawCart(c, s, cxk - cw / 2, floor - cartHeight(s, cw), cw, 0, "ZIP / 7Z", bright);
        }
      }
    const char *name = sys == SYS_OTHER ? app.tr("Comprimidos (.zip / .7z)", "Archives (.zip / .7z)") : kSysName[sys];
    c.drawTextFit(UI_FONT_TITLE, x, floor + 34, w, name, UI_COL_TEXT, UI_ALIGN_CENTER);
    char buf[48];
    int ng = (int)m_games[sys].size();
    sprintf(buf, ng == 1 ? app.tr("%d juego", "%d game") : app.tr("%d juegos", "%d games"), ng);
    c.drawTextFit(UI_FONT_SMALL, x, floor + 84, w, buf, UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
    for (int i = 0; i < n; i++)   // which console of how many
      c.fillRoundRect(cx - n * 14 + i * 28 + (i == m_sys ? 0 : 4), floor + 122, i == m_sys ? 22 : 14, 8, 4,
                      i == m_sys ? UI_COL_ACCENT : 0x50FFFFFF);
    const char *hints[] = { app.tr("A:Abrir", "A:Open"), app.tr("BACK:Explorador y ajustes", "BACK:Browser and settings") };
    app.drawHints(c, hints, 2);
    return;
  }

  // One console's games: back, the console, and where you are in the list.
  if (m_viewDirty || m_viewFilter != app.listFilter())   // the search filters as you type
    buildView(app, sys);
  const std::vector<std::string> &g = m_view;
  const UiListState &ls = m_list[sys];
  c.fillCircle(x + 20, 52, 20, 0x30FFFFFF);
  drawArrow(c, x + 14, 52, 7, false, UI_COL_TEXT);
  std::string sub = m_favOnly ? app.tr("Solo favoritos", "Favorites only") : "";
  if (!app.listFilter().empty())
    sub += std::string(sub.empty() ? "" : " \xb7 ") + app.tr("Buscando: ", "Searching: ") + app.listFilter();
  if (sub.empty())
    sub = kSysYear[sys];
  c.drawTextFit(UI_FONT_SMALL, x + 56, 26, 700, sub.c_str(), m_favOnly || !app.listFilter().empty() ? UI_COL_ACCENT : UI_COL_TEXT_DIM);
  c.drawTextFit(UI_FONT_TITLE, x + 56, 44, 700,
                sys == SYS_OTHER ? app.tr("Comprimidos", "Archives") : kSysName[sys], UI_COL_TEXT);
  char pos[32];
  sprintf(pos, "%d / %d", g.empty() ? 0 : ls.sel + 1, (int)g.size());
  int pw = c.textWidth(UI_FONT_BODY, pos);
  c.drawText(UI_FONT_BODY, x + w - pw, 44, pos, UI_COL_TEXT_DIM);
  char initial[2] = { g.empty() ? '#' : (char)toupper((unsigned char)rowInitial(uiFileName(g[ls.sel]))), 0 };
  if (initial[0] == '#' || initial[0] < 'A' || initial[0] > 'Z') initial[0] = '#';
  uiDrawSelection(c, x + w - pw - 58, 40, 42, 42, 10, true);
  c.drawTextFit(UI_FONT_BODY, x + w - pw - 58, 45, 42, initial, 0xFFFFFFFF, UI_ALIGN_CENTER);

  int top = UI_HEADER_H + 20, h = UI_FOOTER_Y - 12 - top;
  // The view slides to the row it should show instead of jumping there.
  int cardW, cardH, rowH;
  gridLayout(w - 20, h, cardW, cardH, rowH);
  int target = ls.scroll * rowH;
  if (m_scrollPx < 0 || m_scrollSys != sys) {
    m_scrollPx = target;
    m_scrollSys = sys;
  } else if (m_scrollPx != target) {
    int d = target - m_scrollPx, step = d / 4;
    m_scrollPx += step ? step : (d > 0 ? 1 : -1);
    app.drawAgain();
  }
  drawGrid(app, c, x, top, w - 20, h, g, ls, sys, m_scrollPx);
  if (g.empty())
    c.drawTextFit(UI_FONT_BODY, x, top + h / 2 - 20, w, m_favOnly && app.listFilter().empty()
                  ? app.tr("Sin favoritos aqu\xed: marca juegos con X (RB para ver todos)", "No favorites here: mark games with X (RB shows all)")
                  : app.tr("Ning\xfan juego coincide (B borra la b\xfasqueda)", "No game matches (B clears the search)"),
                  UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
  const char *hints[] = {
    app.tr("A:Jugar", "A:Play"), app.tr("B:Consolas", "B:Consoles"),
    !g.empty() && app.favorites().contains(g[ls.sel]) ? app.tr("X:Quitar", "X:Unfavorite") : app.tr("X:Favorito", "X:Favorite"),
    app.tr("Y:Opciones", "Y:Options"), app.tr("LB:Buscar", "LB:Search"),
    m_favOnly ? app.tr("RB:Todos", "RB:All") : app.tr("RB:Favoritos", "RB:Favorites"),
    app.tr("LT/RT:Letra", "LT/RT:Letter"), app.tr("BACK:Men\xfa", "BACK:Menu") };
  app.drawHints(c, hints, 8);
}

void UiHomeScreen::draw(UiApp &app, UiCanvas &c)
{
  if (m_tab == TAB_GAMES && m_filter != app.listFilter()) {
    // The search keyboard is filtering as the user types: start at the first match.
    buildRows(app);
    m_list[TAB_GAMES] = UiListState();
    m_list[TAB_GAMES].clamp((int)m_rows.size());
  }
  app.drawBackground(c);

  // Tabs in the header
  static const char *es[TAB_COUNT] = { "Juegos", "Recientes", "Favoritos", "Ajustes" };
  static const char *en[TAB_COUNT] = { "Games", "Recent", "Favorites", "Settings" };
  c.drawText(UI_FONT_TITLE, UI_MARGIN_X, 30, UI_APP_NAME, UI_COL_TEXT);
  int tx = 320;
  const UiGlyph *lb = uiGlyph("LB"), *rb = uiGlyph("RB");
  c.blitAlpha(lb->px, lb->w, lb->h, tx, 53 - lb->h / 2);
  tx += lb->w + 10;
  for (int i = 0; i < TAB_COUNT; i++) {
    const char *label = app.tr(es[i], en[i]);
    int w = c.textWidth(UI_FONT_BODY, label) + 36;
    if (i == m_tab)
      uiDrawSelection(c, tx, 32, w, 42, 21, true);
    c.drawText(UI_FONT_BODY, tx + 18, 38, label, i == m_tab ? 0xFFFFFFFF : UI_COL_TEXT_DIM);
    tx += w + 8;
  }
  c.blitAlpha(rb->px, rb->w, rb->h, tx + 4, 53 - rb->h / 2);

  // Breadcrumb / tab description
  std::string sub;
  if (m_tab == TAB_GAMES)
  {
    sub = m_dir.empty() ? std::string(app.tr("Elige una unidad", "Choose a drive")) : m_dir;
    if (!m_dir.empty()) {
      static const char *es[SORT_COUNT] = { "A-Z", "Z-A", "Recientes" };
      static const char *en[SORT_COUNT] = { "A-Z", "Z-A", "Recent" };
      int so = app.cfg().listSort;
      if (so < 0 || so >= SORT_COUNT) so = 0;
      std::string pre = "[";
      pre += app.tr(es[so], en[so]);
      if (!m_filter.empty()) {
        pre += "  \xb7  ";
        pre += app.tr("Buscando: ", "Search: ");
        pre += m_filter;
      }
      pre += "]  ";
      sub = pre + sub;
    }
  }
  else if (m_tab == TAB_RECENT)
    sub = app.tr("\xdaltimos juegos abiertos", "Recently played games");
  else if (m_tab == TAB_FAVORITES)
    sub = app.tr("Tus juegos marcados con X", "Games you marked with X");
  else
    sub = app.tr("Configuraci\xf3n del emulador", "Emulator configuration");
  c.drawTextFit(UI_FONT_SMALL, UI_MARGIN_X, 86, 760, sub.c_str(), UI_COL_TEXT_DIM);
  c.fillRect(UI_MARGIN_X, UI_HEADER_H, UI_W - UI_MARGIN_X * 2, 1, COL_DIVIDER);

  int top = UI_HEADER_H + 20;
  int listH = UI_FOOTER_Y - 12 - top;
  {
    drawList(app, c, UI_MARGIN_X, top, 760, listH);
    drawInfo(app, c, 848, top, UI_W - UI_MARGIN_X - 848, listH);
  }

  // Footer hints
  std::vector<const char *> hints;
  UiListState &ls = m_list[m_tab];
  bool onRom = ls.sel >= 0 && ls.sel < (int)m_rows.size() && m_tab != TAB_SETTINGS &&
               !m_rows[ls.sel].isDir;
  if (m_tab == TAB_SETTINGS)
    hints.push_back(app.tr("A:Abrir", "A:Open"));
  else if (onRom)
    hints.push_back(app.tr("A:Jugar", "A:Play"));
  else if (!m_rows.empty())
    hints.push_back(app.tr("A:Abrir", "A:Open"));
  if (m_tab == TAB_GAMES && !m_filter.empty())
    hints.push_back(app.tr("B:Quitar b\xfasqueda", "B:Clear search"));
  else if (m_tab == TAB_GAMES && !m_dir.empty())
    hints.push_back(app.tr("B:Subir", "B:Up"));
  if (onRom) {
    hints.push_back(app.favorites().contains(m_rows[ls.sel].path)
                    ? app.tr("X:Quitar favorito", "X:Unfavorite")
                    : app.tr("X:Favorito", "X:Favorite"));
    hints.push_back(m_tab == TAB_GAMES ? app.tr("Y:Opciones", "Y:Options")
                                       : app.tr("Y:Opciones del juego", "Y:Game options"));
    if (m_tab == TAB_RECENT)
      hints.push_back(app.tr("BACK:Quitar", "BACK:Remove"));
  }
  if (m_tab == TAB_GAMES && !m_dir.empty()) {
    hints.push_back(app.tr("BACK:Buscar", "BACK:Search"));
    hints.push_back(app.tr("START:Orden", "START:Sort"));
    hints.push_back(app.tr("LT/RT:Letra", "LT/RT:Letter"));
  }
  hints.push_back(app.tr("LB/RB:Pesta\xf1""as", "LB/RB:Tabs"));
  app.drawHints(c, &hints[0], (int)hints.size());
}

// --- UiMenuScreen ----------------------------------------------------------

UiMenuScreen::UiMenuScreen(int id, bool overlay)
  : menuId(id), showPreview(false), m_overlay(overlay)
{
}

void UiMenuScreen::clear()
{
  items.clear();
  hints.clear();
}

UiMenuItem &UiMenuScreen::add(int id, int kind, const std::string &label,
                              const std::string &value, const std::string &help)
{
  UiMenuItem it;
  it.id = id;
  it.kind = kind;
  it.label = label;
  it.value = value;
  it.help = help;
  items.push_back(it);
  return items.back();
}

void UiMenuScreen::addHeader(const std::string &label)
{
  add(0, ITEM_HEADER, label);
}

void UiMenuScreen::addInfo(const std::string &label)
{
  add(0, ITEM_INFO, label);
}

int UiMenuScreen::selectedId() const
{
  if (list.sel < 0 || list.sel >= (int)items.size())
    return 0;
  return items[list.sel].id;
}

const UiMenuItem *UiMenuScreen::findItem(int id) const
{
  for (size_t i = 0; i < items.size(); i++)
    if (items[i].id == id && items[i].kind != ITEM_HEADER && items[i].kind != ITEM_INFO)
      return &items[i];
  return 0;
}

void UiMenuScreen::selectId(int id)
{
  for (size_t i = 0; i < items.size(); i++) {
    if (items[i].id == id && items[i].kind != ITEM_HEADER && items[i].kind != ITEM_INFO) {
      list.sel = (int)i;
      return;
    }
  }
  list.clamp((int)items.size());
  skipUnselectable(1);
}

void UiMenuScreen::skipUnselectable(int dir)
{
  int n = (int)items.size();
  for (int guard = 0; guard < n; guard++) {
    if (list.sel < 0 || list.sel >= n)
      break;
    int k = items[list.sel].kind;
    if (k != ITEM_HEADER && k != ITEM_INFO)
      return;
    list.sel += dir;
    if (list.sel >= n) { list.sel = n - 1; dir = -1; }
    if (list.sel < 0) { list.sel = 0; dir = 1; }
  }
}

void UiMenuScreen::onResume(UiApp &app)
{
  int id = selectedId();
  app.rebuildMenus();
  selectId(id);
}

void UiMenuScreen::update(UiApp &app, const UiInputState &in)
{
  int n = (int)items.size();
  if (in.repeat & PAD_BIT(PAD_UP)) {
    int start = list.sel;
    do {
      list.move(-1, n, true);
    } while (list.sel != start && (items[list.sel].kind == ITEM_HEADER || items[list.sel].kind == ITEM_INFO));
  }
  if (in.repeat & PAD_BIT(PAD_DOWN)) {
    int start = list.sel;
    do {
      list.move(1, n, true);
    } while (list.sel != start && (items[list.sel].kind == ITEM_HEADER || items[list.sel].kind == ITEM_INFO));
  }

  if (in.pressed & PAD_BIT(PAD_B)) {
    app.onMenuClosed(*this);
    return;
  }
  if (list.sel < 0 || list.sel >= n)
    return;
  UiMenuItem &it = items[list.sel];
  if (it.kind == ITEM_HEADER || it.kind == ITEM_INFO)
    return;
  int id = it.id;

  if (it.kind == ITEM_OPTION) {
    if (in.repeat & PAD_BIT(PAD_LEFT)) { app.onMenuChange(*this, id, -1); return; }
    if (in.repeat & PAD_BIT(PAD_RIGHT)) { app.onMenuChange(*this, id, 1); return; }
  }
  if (in.pressed & PAD_BIT(PAD_A)) {
    if (!it.enabled)
      return;
    // Options open their list of values (in Controls, A captures the new
    // button / stick direction instead; openPicker knows which).
    if (it.kind == ITEM_OPTION)
      app.openPicker(*this, id);
    else
      app.onMenuActivate(*this, id);
    return;
  }
  static const int extra[] = { PAD_X, PAD_Y, PAD_BACK, PAD_START };
  for (int i = 0; i < 4; i++) {
    if (in.pressed & PAD_BIT(extra[i])) {
      app.onMenuSecondary(*this, id, extra[i]);
      return;
    }
  }
}

void UiMenuScreen::draw(UiApp &app, UiCanvas &c)
{
  int x, y, w, h;
  if (m_overlay) {
    // Floating panel over the (dimmed) game picture.
    w = showPreview ? 1000 : 760;
    h = 560;
    x = (UI_W - w) / 2;
    y = 56;
    uiDrawPanel(c, x, y, w, h, 20, 0xF0);
    c.drawTextFit(UI_FONT_TITLE, x + 32, y + 20, w - 64, title.c_str(), UI_COL_TEXT);
    if (!subtitle.empty())
      c.drawTextFit(UI_FONT_SMALL, x + 32, y + 64, w - 64, subtitle.c_str(), UI_COL_TEXT_DIM);
    c.fillRect(x + 32, y + 96, w - 64, 1, COL_DIVIDER);
    y += 108;
    h -= 108 + 70;
    x += 20;
    w = (showPreview ? 600 : w) - 40;
  } else {
    app.drawBackground(c);
    app.drawHeader(c, title.c_str(), subtitle.c_str());
    x = UI_MARGIN_X;
    y = UI_HEADER_H + 20;
    w = showPreview ? 760 : UI_W - UI_MARGIN_X * 2;
    h = UI_FOOTER_Y - 70 - y;
  }

  const int rh = 50;
  int visible = h / rh;
  list.ensureVisible(visible);
  // Text and section titles right above the selected row can't be selected
  // themselves, so scrolling up must reveal them with it (otherwise the
  // About screen's first lines never came back).
  {
    int top = list.sel;
    while (top > 0 && top - 1 < (int)items.size() &&
           (items[top - 1].kind == ITEM_HEADER || items[top - 1].kind == ITEM_INFO))
      top--;
    if (top < list.scroll && list.sel - top < visible)
      list.scroll = top;
  }
  c.setClip(x, y, w + 8, h);
  for (int i = 0; i <= visible; i++) {
    int idx = list.scroll + i;
    if (idx >= (int)items.size())
      break;
    const UiMenuItem &it = items[idx];
    int ry = y + i * rh;
    bool sel = idx == list.sel;
    if (it.kind == ITEM_HEADER) {
      c.drawText(UI_FONT_SMALL, x + 20, ry + 18, it.label.c_str(), UI_COL_ACCENT);
      continue;
    }
    if (it.kind == ITEM_INFO) {
      c.drawTextFit(UI_FONT_BODY, x + 20, ry + 10, w - 40, it.label.c_str(), UI_COL_TEXT_DIM);
      continue;
    }
    if (sel) {
      uiDrawRowSelection(c, x, ry + 2, w, rh - 4);
    }
    UiColor tc = !it.enabled ? 0xFF5E6678 : (sel ? UI_COL_TEXT : 0xFFD0D5E2);
    int valueW = 0;
    if (it.kind == ITEM_OPTION) {
      int vw = c.textWidth(UI_FONT_BODY, it.value.c_str());
      int right = x + w - 24;
      UiColor vc = sel ? 0xFFFFFFFF : UI_COL_TEXT_DIM;
      UiColor ac = UI_THEME_STYLE == THEME_AERO ? 0xFFFFFFFF : UI_COL_ACCENT;   // on the glossy pill
      if (sel) drawArrow(c, right - 8, ry + rh / 2, 7, true, ac);
      c.drawText(UI_FONT_BODY, right - 18 - vw, ry + 10, it.value.c_str(), vc);
      if (sel) drawArrow(c, right - 36 - vw, ry + rh / 2, 7, false, ac);
      valueW = vw + 60;
    } else if (it.kind == ITEM_SUBMENU) {
      drawArrow(c, x + w - 30, ry + rh / 2, 7, true, sel ? UI_COL_TEXT : UI_COL_TEXT_DIM);
      if (!it.value.empty()) {
        int vw = c.textWidth(UI_FONT_SMALL, it.value.c_str());
        c.drawText(UI_FONT_SMALL, x + w - 48 - vw, ry + 14, it.value.c_str(), sel ? UI_COL_TEXT : UI_COL_TEXT_DIM);
        valueW = vw + 60;
      } else {
        valueW = 40;
      }
    } else if (!it.value.empty()) {
      int vw = c.textWidth(UI_FONT_SMALL, it.value.c_str());
      c.drawText(UI_FONT_SMALL, x + w - 24 - vw, ry + 14, it.value.c_str(), sel ? UI_COL_TEXT : UI_COL_TEXT_DIM);
      valueW = vw + 36;
    }
    c.drawTextFit(UI_FONT_BODY, x + 20, ry + 10, w - 40 - valueW, it.label.c_str(), tc);
  }
  c.resetClip();
  drawScrollbar(c, x + w + 2, y, h, list.scroll, visible, (int)items.size());

  // Side preview (save-state thumbnails, game picture)
  if (showPreview) {
    int px = m_overlay ? x + w + 30 : 848;
    int pw = m_overlay ? 340 : UI_W - UI_MARGIN_X - 848;
    int ph = pw * 2 / 3;
    std::string thumbPath;
    if (list.sel >= 0 && list.sel < (int)items.size())
      thumbPath = items[list.sel].thumb;
    c.fillRoundRect(px, y, pw, ph, 10, 0x40000000);
    const UiThumb *t = thumbPath.empty() ? 0 : app.thumb(thumbPath);
    if (t) {
      c.blitScaled(&t->px[0], t->w, t->h, t->w, px, y, pw, ph);
    } else if (!thumbPath.empty()) {
      c.drawTextFit(UI_FONT_SMALL, px, y + ph / 2 - 10, pw, app.tr("Sin imagen", "No picture"),
                    UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
    } else {
      int gw, gh, gp;
      const UiColor *frame = app.host().gameFrame(gw, gh, gp);
      if (frame)
        c.blitScaled(frame, gw, gh, gp, px, y, pw, ph);
    }
  }

  // Help line for the selected item
  if (list.sel >= 0 && list.sel < (int)items.size() && !items[list.sel].help.empty()) {
    int hy = m_overlay ? 56 + 560 - 60 : UI_FOOTER_Y - 52;
    int hx = m_overlay ? (UI_W - (showPreview ? 1000 : 760)) / 2 + 32 : UI_MARGIN_X;
    int hw = m_overlay ? (showPreview ? 1000 : 760) - 64 : UI_W - UI_MARGIN_X * 2;
    c.drawTextFit(UI_FONT_SMALL, hx, hy, hw, items[list.sel].help.c_str(), UI_COL_TEXT_DIM);
  }

  std::vector<const char *> h2;
  if (list.sel >= 0 && list.sel < (int)items.size()) {
    int k = items[list.sel].kind;
    if (k == ITEM_OPTION) h2.push_back(app.tr("A:Cambiar", "A:Change"));
    else if (k == ITEM_ACTION || k == ITEM_SUBMENU) h2.push_back(app.tr("A:Elegir", "A:Select"));
  }
  for (size_t i = 0; i < hints.size(); i++)
    h2.push_back(hints[i].c_str());
  h2.push_back(m_overlay && menuId == MENU_QUICK ? app.tr("B:Continuar", "B:Resume")
                                                 : app.tr("B:Volver", "B:Back"));
  app.drawHints(c, &h2[0], (int)h2.size());
}

// --- UiKeyboardScreen --------------------------------------------------------

namespace {
const char *kKeys[4][6] = {
  { "0", "1", "2", "3", "4", "5" },
  { "6", "7", "8", "9", "A", "B" },
  { "C", "D", "E", "F", ":", "-" },
  { 0, 0, 0, 0, 0, 0 }   // bottom row: space / delete / OK (drawn specially)
};
// Text (names, search): the Spanish alphabet, \xd1 (N with tilde) after N.
const char *const kFullKeys[4] = { "ABCDEFGHIJK", "LMN\xd1OPQRSTU", "VWXYZ-'.&!?", "1234567890," };
// User names and passwords (LT: the next layer).
const char *const kAccountKeys[3][4] = {
  { "abcdefghij", "klmnopqrst", "uvwxyz-_.@", "1234567890" },
  { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-_.@", "1234567890" },
  { "!#$%&*+=?^", "()[]{}<>/\\", "~`'\",;:|-_", "1234567890" } };
}

UiKeyboardScreen::UiKeyboardScreen(const std::string &title, const std::string &text, int purpose, bool full,
                                   bool search, bool account, bool secret)
  : m_title(title), m_text(text), m_purpose(purpose), m_row(0), m_col(0), m_full(full || account),
    m_search(search), m_orig(text), m_account(account), m_secret(secret), m_layer(0)
{
}

static std::string trimRight(const std::string &s)
{
  std::string t = s;
  while (!t.empty() && t[t.size() - 1] == ' ')
    t.erase(t.size() - 1);
  return t;
}

const char *UiKeyboardScreen::keyLabel(int r, int c) const
{
  if (!m_full)
    return kKeys[r][c];
  static char buf[2];
  buf[0] = m_account ? kAccountKeys[m_layer][r][c] : kFullKeys[r][c];
  buf[1] = 0;
  return buf;
}

void UiKeyboardScreen::update(UiApp &app, const UiInputState &in)
{
  int rows = keyRows() + 1;   // + the space / delete / OK row
  int last = keyRows();
  int cols = m_row == last ? 3 : keyCols();
  if (in.repeat & PAD_BIT(PAD_UP)) {
    int r = (m_row + rows - 1) % rows;
    if (m_row == last) m_col = m_col * keyCols() / 3 + 1;      // keep roughly the same spot
    else if (r == last) m_col = m_col * 3 / keyCols();
    m_row = r;
  }
  if (in.repeat & PAD_BIT(PAD_DOWN)) {
    int r = (m_row + 1) % rows;
    if (m_row == last) m_col = m_col * keyCols() / 3 + 1;
    else if (r == last) m_col = m_col * 3 / keyCols();
    m_row = r;
  }
  cols = m_row == last ? 3 : keyCols();
  if (m_col >= cols) m_col = cols - 1;
  if (in.repeat & PAD_BIT(PAD_LEFT)) m_col = (m_col + cols - 1) % cols;
  if (in.repeat & PAD_BIT(PAD_RIGHT)) m_col = (m_col + 1) % cols;

  bool del = (in.repeat & PAD_BIT(PAD_X)) != 0;
  bool space = (in.pressed & PAD_BIT(PAD_Y)) != 0;
  bool ok = (in.pressed & PAD_BIT(PAD_START)) != 0;
  if (m_account && (in.pressed & PAD_BIT(PAD_LT)))
    m_layer = (m_layer + 1) % 3;
  std::string before = m_text;
  if (in.pressed & PAD_BIT(PAD_BACK))
    m_text.clear();   // erase everything
  if (in.pressed & PAD_BIT(PAD_A)) {
    if (m_row < last) {
      if (m_text.size() < 40)
        m_text += keyLabel(m_row, m_col);
    } else if (m_col == 0) space = true;
    else if (m_col == 1) del = true;
    else ok = true;
  }
  if (space && m_text.size() < 40) m_text += ' ';
  if (del && !m_text.empty()) m_text.erase(m_text.size() - 1);
  if (m_search && m_text != before)
    app.setListFilter(trimRight(m_text));   // the list behind filters as you type
  if (ok) {
    std::string text = m_text;
    int purpose = m_purpose;
    app.pop();
    app.onKeyboardDone(purpose, text, true);
    return;
  }
  if (in.pressed & PAD_BIT(PAD_B)) {
    int purpose = m_purpose;
    if (m_search)
      app.setListFilter(m_orig);   // cancel: the search it had before
    app.pop();
    app.onKeyboardDone(purpose, "", false);
  }
}

// Search: a wide, short keyboard at the bottom so the filtered list stays
// visible above it.
void UiKeyboardScreen::drawSearch(UiApp &app, UiCanvas &c)
{
  int w = UI_W - UI_MARGIN_X * 2, h = 322;
  int x = UI_MARGIN_X, y = UI_FOOTER_Y - 12 - h;
  uiDrawPanel(c, x, y, w, h, 20, 0xF4);   // no dark veil under it: it doubled the cost of every frame
  c.fillRoundRect(x, y, w, 4, 2, UI_COL_ACCENT);

  // Magnifier + text field + number of matches
  int fx = x + 24, fy = y + 18, fw = w - 48 - 220, fh = 50;
  c.fillRoundRect(fx, fy, fw, fh, 25, 0x50000000);
  c.fillCircle(fx + 28, fy + 22, 11, UI_COL_ACCENT);
  c.fillCircle(fx + 28, fy + 22, 7, 0xFF000000 | (UI_COL_PANEL & 0xFFFFFF));
  c.fillRect(fx + 35, fy + 30, 4, 12, UI_COL_ACCENT);
  c.fillRect(fx + 37, fy + 34, 6, 4, UI_COL_ACCENT);
  int tx = fx + 56;
  if (m_text.empty()) {
    c.drawTextFit(UI_FONT_BODY, tx, fy + 11, fw - 80,
                  app.tr("Escribe parte del nombre del juego", "Type part of the game's name"), UI_COL_TEXT_DIM);
    if ((app.frameCount() / 30) % 2 == 0)
      c.fillRect(tx, fy + 11, 2, 28, UI_COL_ACCENT);
  } else {
    int tw = c.drawTextFit(UI_FONT_BODY, tx, fy + 11, fw - 80, m_text.c_str(), UI_COL_TEXT);
    if ((app.frameCount() / 30) % 2 == 0)
      c.fillRect(tx + 2 + tw, fy + 11, 2, 28, UI_COL_ACCENT);
  }
  char buf[64];
  int n = app.listMatches();
  if (trimRight(m_text).empty())
    sprintf(buf, "%s", app.tr("Todos los juegos", "All games"));
  else if (n == 1)
    sprintf(buf, "%s", app.tr("1 resultado", "1 result"));
  else
    sprintf(buf, app.tr("%d resultados", "%d results"), n);
  c.drawTextFit(UI_FONT_BODY, fx + fw + 16, fy + 11, 220 - 16, buf,
                n == 0 && !trimRight(m_text).empty() ? 0xFFFF9A8A : UI_COL_ACCENT, UI_ALIGN_CENTER);

  int nc = keyCols(), last = keyRows();
  int gap = 6, kh = 40;
  int kw = (w - 48 - gap * (nc - 1)) / nc;
  int gx = x + 24, gy = fy + fh + 18;
  for (int r = 0; r <= last; r++) {
    int cols = r == last ? 3 : nc;
    int bw = r == last ? (kw * nc + gap * (nc - 1) - gap * 2) / 3 : kw;
    for (int col = 0; col < cols; col++) {
      int bx = gx + col * (bw + gap), by = gy + r * (kh + gap);
      bool sel = r == m_row && col == m_col;
      bool go = r == last && col == 2;
      if (sel)
        uiDrawSelection(c, bx, by, bw, kh, 10, true);
      else
        c.fillRoundRect(bx, by, bw, kh, 10, go ? uiWithAlpha(UI_COL_ACCENT, 0x60) : 0x30FFFFFF);
      const char *label;
      if (r < last) label = keyLabel(r, col);
      else if (col == 0) label = app.tr("Espacio", "Space");
      else if (col == 1) label = app.tr("Borrar", "Delete");
      else label = app.tr("Buscar", "Search");
      c.drawTextFit(UI_FONT_BODY, bx, by + 6, bw, label, 0xFFFFFFFF, UI_ALIGN_CENTER);
    }
  }
  const char *hints[] = {
    app.tr("START:Buscar", "START:Search"), app.tr("A:Escribir", "A:Type"),
    app.tr("X:Borrar", "X:Delete"), app.tr("Y:Espacio", "Y:Space"),
    app.tr("BACK:Borrar todo", "BACK:Clear"), app.tr("B:Cancelar", "B:Cancel")
  };
  app.drawHints(c, hints, 6);
}

void UiKeyboardScreen::draw(UiApp &app, UiCanvas &c)
{
  if (m_search) {
    drawSearch(app, c);
    return;
  }
  c.fillRect(0, 0, UI_W, UI_H, 0x90000000);
  int w = 640, h = 470;
  int x = (UI_W - w) / 2, y = 100;
  uiDrawPanel(c, x, y, w, h, 20, 0xFA);
  c.drawTextFit(UI_FONT_BODY, x + 32, y + 22, w - 64, m_title.c_str(), UI_COL_TEXT);

  // Text field with a blinking caret
  c.fillRoundRect(x + 32, y + 66, w - 64, 52, 10, 0x40000000);
  std::string shown = m_secret ? std::string(m_text.size(), '*') : m_text;   // a password isn't shown
  int tw = c.drawTextFit(UI_FONT_BODY, x + 48, y + 78, w - 110, shown.c_str(), UI_COL_TEXT);
  if (m_account) {
    const char *layers[3] = { "abc", "ABC", "#+=" };
    c.drawTextFit(UI_FONT_SMALL, x + w - 140, y + 26, 108, layers[m_layer], UI_COL_ACCENT, UI_ALIGN_RIGHT);
  }
  if ((app.frameCount() / 30) % 2 == 0)
    c.fillRect(x + 50 + tw, y + 78, 2, 28, UI_COL_ACCENT);

  int nc = keyCols(), last = keyRows();
  int gap = m_full ? 6 : 8, kh = m_full ? 50 : 56;
  int kw = m_full ? (w - 64 - gap * (nc - 1)) / nc : 84;   // 10 or 11 keys fill the panel
  int gx = x + (w - (kw * nc + gap * (nc - 1))) / 2, gy = y + 140;
  for (int r = 0; r <= last; r++) {
    int cols = r == last ? 3 : nc;
    int bw = r == last ? (kw * nc + gap * (nc - 1) - gap * 2) / 3 : kw;
    for (int col = 0; col < cols; col++) {
      int bx = gx + col * (bw + gap), by = gy + r * (kh + gap);
      bool sel = r == m_row && col == m_col;
      if (sel)
        uiDrawSelection(c, bx, by, bw, kh, 10, true);
      else
        c.fillRoundRect(bx, by, bw, kh, 10, 0x30FFFFFF);
      const char *label;
      if (r < last) label = keyLabel(r, col);
      else if (col == 0) label = app.tr("Espacio", "Space");
      else if (col == 1) label = app.tr("Borrar", "Delete");
      else label = "OK";
      c.drawTextFit(UI_FONT_BODY, bx, by + 14, bw, label, 0xFFFFFFFF, UI_ALIGN_CENTER);
    }
  }
  const char *hints[] = {
    app.tr("A:Escribir", "A:Type"), app.tr("X:Borrar", "X:Delete"),
    app.tr("Y:Espacio", "Y:Space"), app.tr("START:Aceptar", "START:Done"),
    app.tr("BACK:Borrar todo", "BACK:Clear"), app.tr("B:Cancelar", "B:Cancel"),
    app.tr("LT:abc / ABC / #+=", "LT:abc / ABC / #+=")
  };
  app.drawHints(c, hints, m_account ? 7 : 6);
}

// --- UiCoverScreen -----------------------------------------------------------

void UiCoverScreen::update(UiApp &app, const UiInputState &in)
{
  app.host().coverDownloadStatus(m_status);
  app.keepDrawing();   // the numbers change without any button pressed
  if (m_status.running) {
    if ((in.pressed & PAD_BIT(PAD_B)) && !m_closing) {
      m_closing = true;
      app.host().cancelCoverDownload();
    }
    return;
  }
  if (m_closing || (in.pressed & (PAD_BIT(PAD_A) | PAD_BIT(PAD_B) | PAD_BIT(PAD_START)))) {
    app.pop();
    app.onCoversClosed();
  }
}

void UiCoverScreen::draw(UiApp &app, UiCanvas &c)
{
  c.fillRect(0, 0, UI_W, UI_H, 0xA0000000);
  int w = 760, h = 300;
  int x = (UI_W - w) / 2, y = (UI_H - h) / 2 - 30;
  uiDrawPanel(c, x, y, w, h, 20, 0xFA);
  const UiCoverStatus &s = m_status;
  c.drawTextFit(UI_FONT_BODY, x + 32, y + 24, w - 64,
                s.running ? app.tr("Descargando portadas...", "Downloading box art...")
                          : app.tr("Descarga de portadas", "Box art download"), UI_COL_TEXT);

  // Progress bar
  int bx = x + 32, by = y + 78, bw = w - 64, bh = 18;
  c.fillRoundRect(bx, by, bw, bh, 9, 0x40000000);
  int fill = s.total > 0 ? bw * s.done / s.total : 0;
  if (fill > 0)
    uiDrawSelection(c, bx, by, fill < bh ? bh : fill, bh, 9, true);

  char line[160];
  sprintf(line, app.tr("%d de %d juegos", "%d of %d games"), s.done, s.total);
  c.drawText(UI_FONT_SMALL, bx, by + 30, line, UI_COL_TEXT_DIM);
  sprintf(line, app.tr("Guardadas: %d   Sin portada: %d", "Saved: %d   No cover: %d"), s.found, s.missing);
  c.drawTextFit(UI_FONT_SMALL, bx, by + 30, bw, line, UI_COL_TEXT_DIM, UI_ALIGN_RIGHT);

  // What is happening / how it ended
  std::string msg, msg2;
  UiColor col = UI_COL_TEXT;
  if (s.running) {
    msg = m_closing ? app.tr("Cancelando...", "Cancelling...") : s.current;
    msg2 = app.tr("Puedes cancelar con B; las portadas ya guardadas se quedan.",
                  "B cancels; the covers already saved stay.");
  } else if (s.error == "nolink") {
    msg = app.tr("La consola no est\xe1 conectada a la red.", "The console isn't connected to a network.");
    col = 0xFFFF9A8A;
  } else if (s.error == "noip") {
    msg = app.tr("No se obtuvo direcci\xf3n de red; revisa la conexi\xf3n.", "No network address; check the connection.");
    col = 0xFFFF9A8A;
  } else if (s.error == "dns") {
    msg = app.tr("No se encontr\xf3 el servidor (\xbfhay internet?).", "The server wasn't found (is there internet?).");
    col = 0xFFFF9A8A;
  } else if (s.error == "connect" || s.error == "empty" || s.error == "send") {
    msg = app.tr("No se pudo conectar con el servidor de portadas.", "Couldn't connect to the box art server.");
    col = 0xFFFF9A8A;
  } else if (s.error == "https") {
    msg = app.tr("El servidor s\xf3lo acepta HTTPS, que la consola no puede usar.",
                 "The server only accepts HTTPS, which the console can't use.");
    msg2 = app.tr("Se puede usar otro servidor con cover_server= en vba360.ini.",
                  "Another server can be set with cover_server= in vba360.ini.");
    col = 0xFFFF9A8A;
  } else if (s.error == "cancel") {
    msg = app.tr("Cancelado.", "Cancelled.");
  } else if (!s.error.empty()) {
    msg = std::string(app.tr("Error del servidor: ", "Server error: ")) + s.error;
    col = 0xFFFF9A8A;
  } else if (m_single) {
    msg = s.found ? app.tr("Portada guardada.", "Box art saved.")
                  : app.tr("No se encontr\xf3 portada para este juego.", "No box art found for this game.");
    if (!s.found)
      msg2 = app.tr("Funciona mejor si la ROM tiene su nombre oficial, p. ej. \"Pokemon - Emerald Version (USA, Europe)\".",
                    "It works best with the ROM's official name, e.g. \"Pokemon - Emerald Version (USA, Europe)\".");
  } else {
    sprintf(line, app.tr("Listo: %d portadas nuevas.", "Done: %d new covers."), s.found);
    msg = line;
    if (s.missing)
      msg2 = app.tr("Los juegos sin portada necesitan el nombre oficial de la ROM (No-Intro).",
                    "Games without one need the ROM's official (No-Intro) name.");
  }
  c.drawTextFit(UI_FONT_BODY, x + 32, y + 160, w - 64, msg.c_str(), col);
  if (!msg2.empty())
    c.drawTextFit(UI_FONT_SMALL, x + 32, y + 206, w - 64, msg2.c_str(), UI_COL_TEXT_DIM);

  const char *runHints[] = { app.tr("B:Cancelar", "B:Cancel") };
  const char *endHints[] = { app.tr("A:Cerrar", "A:Close") };
  app.drawHints(c, s.running ? runHints : endHints, 1);
}

// --- UiCaptureScreen ---------------------------------------------------------

UiCaptureScreen::UiCaptureScreen(int action) : m_action(action), m_released(false), m_frames(0) {}

void UiCaptureScreen::update(UiApp &app, const UiInputState &in)
{
  m_frames++;
  if (!m_released) {
    // Wait until the A press that opened this screen is released.
    if (in.held == 0)
      m_released = true;
    return;
  }
  for (int b = 0; b < PAD_BUTTON_COUNT; b++) {
    if (in.pressed & PAD_BIT(b)) {
      int action = m_action;
      app.pop();
      app.onCaptured(action, b);
      return;
    }
  }
  if (m_frames > 60 * 6) {   // give up after ~6 seconds
    app.pop();
    app.toast(app.tr("Sin cambios", "Unchanged"));
  }
}

void UiCaptureScreen::draw(UiApp &app, UiCanvas &c)
{
  c.fillRect(0, 0, UI_W, UI_H, 0xA0000000);
  int w = 640, h = 220;
  int x = (UI_W - w) / 2, y = (UI_H - h) / 2 - 20;
  uiDrawPanel(c, x, y, w, h, 20, 0xFA);
  c.drawTextFit(UI_FONT_TITLE, x, y + 36, w, app.tr("Presiona un bot\xf3n", "Press a button"),
                UI_COL_TEXT, UI_ALIGN_CENTER);
  char line[160];
  sprintf(line, "%s: %s", app.tr("Para", "For"), uiActionName(app, m_action));
  c.drawTextFit(UI_FONT_BODY, x, y + 96, w, line, UI_COL_ACCENT, UI_ALIGN_CENTER);
  int left = 6 - m_frames / 60;
  if (left < 0) left = 0;
  sprintf(line, app.tr("Se cancela en %d s", "Cancels in %d s"), left);
  c.drawTextFit(UI_FONT_SMALL, x, y + 150, w, line, UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
}

// --- UiConfirmScreen -----------------------------------------------------------

UiConfirmScreen::UiConfirmScreen(const std::string &message, int purpose, const std::string &arg)
  : m_message(message), m_purpose(purpose), m_arg(arg)
{
}

// --- Option picker ------------------------------------------------------------

UiPickerScreen::UiPickerScreen(UiMenuScreen *menu, int itemId, const std::string &title,
                               const std::vector<std::string> &values, int style, int selected)
  : m_menu(menu), m_itemId(itemId), m_title(title), m_values(values), m_style(style), m_orig(selected)
{
  m_list.sel = selected;   // the current value
  m_list.scroll = selected > 4 ? selected - 4 : 0;
}

void UiPickerScreen::update(UiApp &app, const UiInputState &in)
{
  int n = (int)m_values.size();
  int delta = 0;
  if (in.repeat & (PAD_BIT(PAD_UP) | PAD_BIT(PAD_LEFT))) delta = -1;
  if (in.repeat & (PAD_BIT(PAD_DOWN) | PAD_BIT(PAD_RIGHT))) delta = 1;
  if (delta && n > 1) {
    m_list.move(delta, n, true);
    app.onMenuChange(*m_menu, m_itemId, delta);   // applied right away
  }
  if (in.pressed & PAD_BIT(PAD_B)) {
    // Cancel: step back to the value it had when the list opened.
    int back = m_list.sel - m_orig;
    for (int i = 0; i < back; i++) app.onMenuChange(*m_menu, m_itemId, -1);
    for (int i = 0; i > back; i--) app.onMenuChange(*m_menu, m_itemId, 1);
    app.pop();
  } else if (in.pressed & (PAD_BIT(PAD_A) | PAD_BIT(PAD_START))) {
    app.pop();
  }
}

void UiPickerScreen::draw(UiApp &app, UiCanvas &c)
{
  const int rh = 46;
  int x, y, w, h;
  if (m_style == OPT_STYLE_LIST) {
    // Pop-up list in the middle of the screen.
    c.fillRect(0, 0, UI_W, UI_H, 0x80000000);
    int rows = (int)m_values.size() < 9 ? (int)m_values.size() : 9;
    w = 560;
    h = 96 + rows * rh + 20;
    x = (UI_W - w) / 2;
    y = (UI_H - h) / 2 - 20;
    uiDrawPanel(c, x, y, w, h, 20, 0xFA);
  } else {
    // Side panel on the right, full height.
    c.fillRect(0, 0, UI_W, UI_H, 0x60000000);
    w = 460;
    h = UI_H;
    x = UI_W - w;
    y = 0;
    uiDrawPanel(c, x, y, w, h, 0, 0xF8);
    c.fillRect(x, y, 3, h, UI_COL_ACCENT);
    y += 40;
    h -= 40 + 80;
  }
  c.drawTextFit(UI_FONT_BODY, x + 28, y + 22, w - 56, m_title.c_str(), UI_COL_TEXT);
  c.fillRect(x + 28, y + 70, w - 56, 1, COL_DIVIDER);
  int listY = y + 82;
  int listH = (m_style == OPT_STYLE_LIST) ? h - 96 - 10 : h - 82;
  int visible = listH / rh;
  if (visible < 1) visible = 1;
  m_list.ensureVisible(visible);
  c.setClip(x, listY, w, visible * rh);
  for (int i = 0; i < visible; i++) {
    int idx = m_list.scroll + i;
    if (idx >= (int)m_values.size())
      break;
    int ry = listY + i * rh;
    bool sel = idx == m_list.sel;
    if (sel)
      uiDrawRowSelection(c, x + 16, ry + 2, w - 32, rh - 4);
    c.drawTextFit(UI_FONT_BODY, x + 36, ry + 8, w - 72, m_values[idx].c_str(),
                  sel ? UI_COL_TEXT : 0xFFD0D5E2);
  }
  c.resetClip();
  drawScrollbar(c, x + w - 12, listY, visible * rh, m_list.scroll, visible, (int)m_values.size());
  const char *hints[] = { app.tr("A:Aceptar", "A:OK"), app.tr("B:Cancelar", "B:Cancel") };
  if (m_style == OPT_STYLE_LIST) {
    app.drawHints(c, hints, 2);
  } else {
    // Hints inside the panel.
    int hy = UI_H - 64;
    c.fillRect(x + 28, hy - 8, w - 56, 1, COL_DIVIDER);
    c.drawText(UI_FONT_SMALL, x + 28, hy + 4,
               app.tr("Arriba/abajo: cambiar  A: listo  B: cancelar", "Up/down: change  A: done  B: cancel"),
               UI_COL_TEXT_DIM);
  }
}

void UiConfirmScreen::update(UiApp &app, const UiInputState &in)
{
  if (in.pressed & PAD_BIT(PAD_A)) {
    int purpose = m_purpose;
    std::string arg = m_arg;
    app.pop();
    app.onConfirm(purpose, arg);
  } else if (in.pressed & PAD_BIT(PAD_B)) {
    app.pop();
  }
}

void UiConfirmScreen::draw(UiApp &app, UiCanvas &c)
{
  c.fillRect(0, 0, UI_W, UI_H, 0xA0000000);
  int w = 640, h = 200;
  int x = (UI_W - w) / 2, y = (UI_H - h) / 2 - 20;
  uiDrawPanel(c, x, y, w, h, 20, 0xFA);
  c.drawTextFit(UI_FONT_BODY, x + 24, y + 50, w - 48, m_message.c_str(), UI_COL_TEXT, UI_ALIGN_CENTER);
  const char *hints[] = { app.tr("A:S\xed", "A:Yes"), app.tr("B:No", "B:No") };
  app.drawHints(c, hints, 2);
}
