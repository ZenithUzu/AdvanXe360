#include "UiScreens.h"
#include "UiGlyphs.h"

#include <stdio.h>
#include <string.h>
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

void uiApplyTheme(int index)
{
  if (index < 0 || index >= UI_THEME_COUNT)
    index = 0;
  const UiTheme &t = uiThemes[index];
  UI_COL_ACCENT = t.accent;
  UI_COL_ROW_SEL = uiWithAlpha(t.accent, 0x40);
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
  if (UI_THEME_STYLE != THEME_AERO) {
    c.fillRoundRect(x, y, w, h, radius, uiWithAlpha(UI_COL_PANEL, alpha));
    return;
  }
  // Glass: tinted, lighter at the top, a reflection over the upper part and
  // a bright edge.
  int a = alpha >= 0xE0 ? alpha : alpha * 3 / 4;   // dialogs stay solid enough to read
  c.fillRoundRect(x + 4, y + 10, w, h, radius, 0x30001432);                      // shadow
  c.fillRoundRect(x, y, w, h, radius, uiWithAlpha(UI_COL_PANEL, a));
  c.fillGradient(x + radius / 2, y + 1, w - radius, h - 2, 0x20FFFFFF, 0x04FFFFFF);
  c.fillGradient(x + radius / 2, y + 1, w - radius, h * 45 / 100, 0x30FFFFFF, 0x00FFFFFF);
  c.fillRect(x + radius, y, w - 2 * radius, 1, 0xCCFFFFFF);
  c.fillRect(x, y + radius, 1, h - 2 * radius, 0x70FFFFFF);
  c.fillRect(x + w - 1, y + radius, 1, h - 2 * radius, 0x70FFFFFF);
  c.fillRect(x + radius, y + h - 1, w - 2 * radius, 1, 0x40FFFFFF);
}

namespace {

UiColor mixColor(UiColor a, UiColor b, int t, int n)   // a..b, t of n
{
  UiColor r = 0xFF000000;
  for (int s = 0; s < 24; s += 8)
    r |= (UiColor)((((a >> s) & 0xFF) * (n - t) + ((b >> s) & 0xFF) * t) / n) << s;
  return r;
}

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

void uiDrawBackgroundDecor(UiCanvas &c, bool scenery)
{
  if (UI_THEME_STYLE != THEME_AERO)
    return;
  const int W = c.width(), H = c.height();
  if (scenery) {
    // A meadow under a clear sky: deep blue up top (where the text is), a
    // bright horizon, green hills below.
    int hz = H * 80 / 100;
    c.fillGradient(0, 0, W, hz, 0xFF0A4A86, 0xFF1E6FB5);
    c.fillGradient(0, hz, W, H - hz, 0xFF1E6FB5, 0xFF5AB8E8);
    for (int i = 0; i < 9; i++)                                   // sun, up right
      c.fillCircle(W * 78 / 100, H * 12 / 100, 380 - i * 40, 0x0AFFFFFF);
    const int bands = 14;
    for (int i = 0; i < bands; i++) {                             // far hill, right
      UiColor col = mixColor(0xFF2E8A26, 0xFF083806, i, bands);
      c.fillCircle(W * 85 / 100, hz - 60 + 1300 + i * 8, 1300, col);
    }
    for (int i = 0; i < bands; i++) {                             // near hill, left
      UiColor col = mixColor(0xFF3A9A1C, 0xFF0A4005, i, bands);
      c.fillCircle(W * 28 / 100, hz - 30 + 1500 + i * 9, 1500, col);
    }
  }
  // Bubbles (fixed places, so the cached background never changes).
  static const short bub[][3] = {
    { 1180, 170, 44 }, { 1095, 255, 20 }, { 1225, 335, 13 }, { 965, 145, 11 },
    { 115, 590, 34 }, { 215, 515, 15 }, { 62, 470, 9 }, { 705, 655, 22 },
    { 1010, 600, 30 }, { 1150, 525, 16 }, { 1245, 655, 21 }, { 905, 300, 7 } };
  for (size_t i = 0; i < sizeof(bub) / sizeof(bub[0]); i++)
    drawBubble(c, bub[i][0] * W / 1280, bub[i][1] * H / 720, bub[i][2]);
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
  if (e == ".gb" || e == ".gbc" || e == ".cgb" || e == ".dmg" || e == ".sgb") return "GB";
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

static std::string lowerAscii(const std::string &s)
{
  std::string r(s);
  for (size_t i = 0; i < r.size(); i++)
    if (r[i] >= 'A' && r[i] <= 'Z')
      r[i] = (char)(r[i] - 'A' + 'a');
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
      { MENU_SETTINGS_AUDIO, "Audio", "Audio", "Sonido y avance r\xe1pido", "Sound and fast forward" },
      { MENU_SETTINGS_EMU, "Emulaci\xf3n", "Emulation", "Velocidad, BIOS y trucos", "Speed, BIOS and cheats" },
      { MENU_SETTINGS_SAVES, "Guardado", "Saving", "Carpetas y guardado autom\xe1tico", "Folders and auto save" },
      { MENU_CONTROLS, "Controles", "Controls", "Botones y sensores (stick derecho)", "Buttons and sensors (right stick)" },
      { MENU_CLOCK, "Reloj de los juegos", "Game clock", "Hora de la consola o personalizada", "Console time or a custom one" },
      { MENU_SETTINGS_UI, "Interfaz e idioma", "Interface & language", "Idioma y lista de juegos", "Language and game list" },
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
  bool shelf = shelfMode(app);
  if (carouselMode(app)) {
    // Carousel: left / right one by one, up / down five at a time.
    if ((in.repeat & PAD_BIT(PAD_LEFT)) && ls.sel > 0) ls.sel--;
    if ((in.repeat & PAD_BIT(PAD_RIGHT)) && ls.sel < count - 1) ls.sel++;
    if (in.repeat & PAD_BIT(PAD_UP)) ls.move(-5, count, false);
    if (in.repeat & PAD_BIT(PAD_DOWN)) ls.move(5, count, false);
  } else if (shelf) {
    // Shelf: left / right along a shelf, up / down to the shelf above / below.
    if ((in.repeat & PAD_BIT(PAD_LEFT)) && ls.sel > 0) ls.sel--;
    if ((in.repeat & PAD_BIT(PAD_RIGHT)) && ls.sel < count - 1) ls.sel++;
    if ((in.repeat & PAD_BIT(PAD_UP)) && ls.sel >= SHELF_COLS) ls.sel -= SHELF_COLS;
    if ((in.repeat & PAD_BIT(PAD_DOWN)) && ls.sel / SHELF_COLS < (count - 1) / SHELF_COLS)
      ls.sel = std::min(ls.sel + SHELF_COLS, count - 1);
  } else {
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
  if (shelf) {
    int row = ls.sel / SHELF_COLS;   // scroll counts shelves here
    if (row < ls.scroll) ls.scroll = row;
    if (row >= ls.scroll + SHELF_ROWS) ls.scroll = row - SHELF_ROWS + 1;
  } else {
    ls.ensureVisible(visible);
  }

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
    }
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
    c.drawTextFit(UI_FONT_TITLE, x + pad, ty, tw, "VBA360", UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x + pad, ty + 46, tw, "enhanced-ai", UI_COL_ACCENT);
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

bool UiHomeScreen::shelfMode(UiApp &app) const
{
  return app.cfg().listView == LIST_VIEW_COVERS && m_tab != TAB_SETTINGS &&
         !(m_tab == TAB_GAMES && m_dir.empty());   // drives stay a list
}

bool UiHomeScreen::carouselMode(UiApp &app) const
{
  return app.cfg().listView == LIST_VIEW_CAROUSEL && m_tab != TAB_SETTINGS &&
         !(m_tab == TAB_GAMES && m_dir.empty());
}

// Box art standing on shelves: boxes of any shape share a baseline like on a
// real shelf; the chosen one lifts off it. The name and details of the
// chosen game run along the bottom.
void UiHomeScreen::drawShelf(UiApp &app, UiCanvas &c, int x, int y, int w, int h)
{
  UiListState &ls = m_list[m_tab];
  const int count = (int)m_rows.size();
  const int gap = 24;
  const int tileW = (w - gap * (SHELF_COLS - 1)) / SHELF_COLS;
  const int stripH = 64;                                    // name of the chosen game
  const int shelfH = (h - stripH) / SHELF_ROWS;
  const int artH = shelfH - 34;                             // tallest box on a shelf
  const int lift = 10;

  for (int r = 0; r < SHELF_ROWS; r++) {
    int base = y + (r + 1) * shelfH - 18;                   // where the boxes stand
    // A wooden plank: lit top, front edge, shadow under it.
    c.fillGradient(x - 16, base - 3, w + 32, 6, 0xFFB98552, 0xFF8E5C33);
    c.fillGradient(x - 16, base + 3, w + 32, 10, 0xFF6E4426, 0xFF4A2C17);
    c.fillGradient(x - 16, base + 13, w + 32, 14, 0x60000000, 0x00000000);
    for (int col = 0; col < SHELF_COLS; col++) {
      int idx = (ls.scroll + r) * SHELF_COLS + col;
      if (idx >= count)
        break;
      const Row &row = m_rows[idx];
      bool sel = idx == ls.sel;
      int tx = x + col * (tileW + gap);
      int maxW = tileW, maxH = artH - lift;
      int aw, ah;
      const UiThumb *t = 0;
      const UiApp::Cover *cv = row.isDir ? 0 : gameArt(app, row.path, t);
      if (cv) {
        aw = maxW; ah = cv->h * maxW / cv->w;
        if (ah > maxH) { ah = maxH; aw = cv->w * maxH / cv->h; }
      } else if (t) {
        aw = maxW; ah = maxW * t->h / t->w;
      } else {
        aw = maxW * 4 / 5; ah = aw * 6 / 5;                 // a blank cartridge label
      }
      if (aw < 1) aw = 1;
      if (ah < 1) ah = 1;
      int ax = tx + (tileW - aw) / 2;
      int ay = base - ah - (sel ? lift : 0);
      if (sel)
        uiDrawSelection(c, ax - 7, ay - 7, aw + 14, ah + 14, 12, false);
      else
        c.fillRect(ax + 4, base - 4, aw - 8, 4, 0x50000000);   // contact shadow
      int bright = sel ? 256 : 190;
      if (cv) {
        c.blitScaled(&cv->px[0], cv->w, cv->h, cv->w, ax, ay, aw, ah, bright);
      } else if (t) {
        c.blitScaled(&t->px[0], t->w, t->h, t->w, ax, ay, aw, ah, bright);
      } else {
        c.fillRoundRect(ax, ay, aw, ah, 10, uiWithAlpha(UI_COL_PANEL, 0xE0));
        c.fillRoundRect(ax + 8, ay + 8, aw - 16, ah * 3 / 5, 6, 0x30FFFFFF);
        if (row.isDir) {
          drawFolderIcon(c, ax + aw / 2 - 13, ay + ah * 3 / 10 - 4, COL_DIR);
        } else {
          c.drawTextFit(UI_FONT_TITLE, ax + 8, ay + ah * 3 / 10 - 14, aw - 16, romKind(row.path),
                        sel ? UI_COL_TEXT : 0x80FFFFFF, UI_ALIGN_CENTER);
        }
        std::string name = row.isDir ? row.title : uiStripExtension(uiFileName(row.path));
        c.drawTextFit(UI_FONT_SMALL, ax + 8, ay + ah * 3 / 5 + 18, aw - 16, name.c_str(),
                      sel ? UI_COL_TEXT : UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
      }
      if (!row.isDir && app.favorites().contains(row.path))
        c.fillCircle(ax + aw - 10, ay + 10, 6, COL_FAV);
    }
  }
  int shelves = (count + SHELF_COLS - 1) / SHELF_COLS;
  drawScrollbar(c, x + w + 14, y, SHELF_ROWS * shelfH - 18, ls.scroll, SHELF_ROWS, shelves);

  // The chosen game's name and details.
  if (ls.sel < 0 || ls.sel >= count)
    return;
  const Row &cur = m_rows[ls.sel];
  int sy = y + SHELF_ROWS * shelfH + 6;
  std::string name = cur.isDir ? cur.title : uiStripExtension(uiFileName(cur.path));
  std::string detail;
  if (cur.isDir) {
    detail = app.tr("Carpeta", "Folder");
  } else {
    const char *k = romKind(cur.path);
    int bw = c.textWidth(UI_FONT_SMALL, k) + 14;
    drawBadge(c, x, sy + 8, k, 0x30FFFFFF, UI_COL_TEXT);
    if (cur.size)
      detail = formatSize(cur.size);
    std::string patch = findPatch(app, cur.path);
    if (!patch.empty())
      detail += std::string(detail.empty() ? "" : "   ") + app.tr("con parche", "patched");
    c.drawTextFit(UI_FONT_BODY, x + bw + 12, sy + 2, w - bw - 200, name.c_str(), UI_COL_TEXT);
    c.drawTextFit(UI_FONT_SMALL, x + w - 180, sy + 8, 180, detail.c_str(), UI_COL_TEXT_DIM, UI_ALIGN_RIGHT);
    return;
  }
  c.drawTextFit(UI_FONT_BODY, x, sy + 2, w - 200, name.c_str(), UI_COL_TEXT);
  c.drawTextFit(UI_FONT_SMALL, x + w - 180, sy + 8, 180, detail.c_str(), UI_COL_TEXT_DIM, UI_ALIGN_RIGHT);
}

// --- Carousel view -------------------------------------------------------------

namespace {

// A box drawn at most maxW x maxH with its own proportions (a plain
// label when there is no art). Returns the size used.
void boxSize(const UiApp::Cover *cv, int maxW, int maxH, int &w, int &h)
{
  if (!cv) { h = maxH; w = maxH * 9 / 10; return; }
  w = maxW; h = cv->h * maxW / cv->w;
  if (h > maxH) { h = maxH; w = cv->w * maxH / cv->h; }
  if (w < 1) w = 1;
  if (h < 1) h = 1;
}

void drawBox(UiCanvas &c, const UiApp::Cover *cv, int x, int y, int w, int h, int bright,
             const std::string &name, const char *kind)
{
  c.fillRoundRect(x + 6, y + 10, w, h, 6, 0x60000000);      // shadow
  if (cv) {
    c.blitScaled(&cv->px[0], cv->w, cv->h, cv->w, x, y, w, h, bright);
    return;
  }
  c.fillRoundRect(x, y, w, h, 6, uiWithAlpha(UI_COL_PANEL, 0xF0));
  c.fillRoundRect(x + 10, y + 10, w - 20, h * 3 / 5, 4, 0x24FFFFFF);
  c.drawTextFit(UI_FONT_TITLE, x + 10, y + h * 3 / 10 - 20, w - 20, kind, 0x70FFFFFF, UI_ALIGN_CENTER);
  c.drawTextFit(UI_FONT_SMALL, x + 10, y + h * 3 / 5 + 24, w - 20, name.c_str(), UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
}

// The box art, read when the list stops (and drawn again until it is).
const UiApp::Cover *boxArt(UiApp &app, const std::string &rom)
{
  const UiApp::Cover *cv = app.cover(rom, !app.inputHeld());
  if (!cv && !app.coverChecked(rom))
    app.drawAgain();
  return cv;
}

// A box seen edge-on: the left strip of its art is the spine (GBA boxes
// carry "GAME BOY ADVANCE" there).
void drawSpine(UiCanvas &c, const UiApp::Cover *cv, int x, int y, int w, int h, int bright)
{
  if (cv) {
    int sw = cv->w / 9 > 0 ? cv->w / 9 : 1;
    c.blitScaled(&cv->px[0], sw, cv->h, cv->w, x, y, w, h, bright);
  } else {
    c.fillRect(x, y, w, h, uiWithAlpha(UI_COL_PANEL, 0xF0));
  }
  c.fillRect(x, y, 1, h, 0x50FFFFFF);                        // lit edge
  c.fillRect(x, y, w, h, uiWithAlpha(0, (256 - bright) / 2));
}

} // namespace

// The chosen box faces you; its neighbours stand smaller beside it and the
// rest show only their spines, like boxes lined up on a shelf.
void UiHomeScreen::drawCarousel(UiApp &app, UiCanvas &c, int x, int y, int w, int h)
{
  UiListState &ls = m_list[m_tab];
  const int count = (int)m_rows.size();
  if (ls.sel < 0 || ls.sel >= count)
    return;
  const int cx = x + w / 2;
  const int floor = y + h * 70 / 100;               // where the boxes stand

  // Far boxes first (behind), edge-on; no new file reads for them.
  for (int side = -1; side <= 1; side += 2) {
    for (int d = 8; d >= 2; d--) {
      int idx = ls.sel + side * d;
      if (idx < 0 || idx >= count)
        continue;
      const Row &r = m_rows[idx];
      const UiApp::Cover *cv = r.isDir ? 0 : app.cover(r.path, false);
      int sh = 230 - d * 10, sw = 20;
      int sx = cx + side * (385 + (d - 2) * 26) - sw / 2;
      drawSpine(c, cv, sx, floor - sh, sw, sh, 220 - d * 15);
    }
  }
  // The two neighbours.
  for (int side = -1; side <= 1; side += 2) {
    int idx = ls.sel + side;
    if (idx < 0 || idx >= count)
      continue;
    const Row &r = m_rows[idx];
    const UiApp::Cover *cv = r.isDir ? 0 : boxArt(app, r.path);
    int bw, bh;
    boxSize(cv, 190, 230, bw, bh);
    std::string name = r.isDir ? r.title : uiStripExtension(uiFileName(r.path));
    drawBox(c, cv, cx + side * 265 - bw / 2, floor - bh, bw, bh, 170, name, r.isDir ? "" : romKind(r.path));
  }
  // The chosen one, lifted off the floor.
  const Row &cur = m_rows[ls.sel];
  std::string name = cur.isDir ? cur.title : uiStripExtension(uiFileName(cur.path));
  const UiApp::Cover *cv = cur.isDir ? 0 : boxArt(app, cur.path);
  int bw, bh;
  boxSize(cv, 300, 320, bw, bh);
  int bx = cx - bw / 2, by = floor - bh - 14;
  uiDrawSelection(c, bx - 8, by - 8, bw + 16, bh + 16, 12, false);
  drawBox(c, cv, bx, by, bw, bh, 256, name, cur.isDir ? "" : romKind(cur.path));
  if (cur.isDir)
    drawFolderIcon(c, cx - 13, by + bh / 2 - 12, COL_DIR);
  if (!cur.isDir && app.favorites().contains(cur.path))
    c.fillCircle(bx + bw - 12, by + 12, 7, COL_FAV);

  // Name, then what it is.
  int ty = floor + 20;
  c.drawTextFit(UI_FONT_TITLE, x + 40, ty, w - 80, name.c_str(), UI_COL_TEXT, UI_ALIGN_CENTER);
  std::string detail;
  if (cur.isDir) {
    detail = app.tr("Carpeta", "Folder");
  } else {
    int info = app.romInfo(cur.path);
    detail = (info & UI_ROM_GB) ? ((info & UI_ROM_CGB) ? "Game Boy Color" : "Game Boy") : "Game Boy Advance";
    if (cur.size)
      detail += "   " + formatSize(cur.size);
    if (!findPatch(app, cur.path).empty())
      detail += std::string("   ") + app.tr("con parche", "patched");
  }
  c.drawTextFit(UI_FONT_SMALL, x + 40, ty + 48, w - 80, detail.c_str(), UI_COL_TEXT_DIM, UI_ALIGN_CENTER);
  char pos[32];
  sprintf(pos, "%d / %d", ls.sel + 1, count);
  c.drawTextFit(UI_FONT_SMALL, x + w - 160, y - 4, 160, pos, UI_COL_TEXT_DIM, UI_ALIGN_RIGHT);
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
  c.drawText(UI_FONT_TITLE, UI_MARGIN_X, 30, "VBA360", UI_COL_TEXT);
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
  if (carouselMode(app) && !m_rows.empty()) {
    drawCarousel(app, c, UI_MARGIN_X, top, UI_W - UI_MARGIN_X * 2, listH);
  } else if (shelfMode(app) && !m_rows.empty()) {
    drawShelf(app, c, UI_MARGIN_X, top, UI_W - UI_MARGIN_X * 2, listH);
  } else {
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
const char *const kFullKeys[4] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-'.&", "1234567890" };
}

UiKeyboardScreen::UiKeyboardScreen(const std::string &title, const std::string &text, int purpose, bool full,
                                   bool search)
  : m_title(title), m_text(text), m_purpose(purpose), m_row(0), m_col(0), m_full(full),
    m_search(search), m_orig(text)
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
  buf[0] = kFullKeys[r][c];
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
  c.fillRect(0, y - 16, UI_W, UI_H - y + 16, 0x60000000);
  uiDrawPanel(c, x, y, w, h, 20, 0xF4);
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
  int tw = c.drawTextFit(UI_FONT_BODY, x + 48, y + 78, w - 110, m_text.c_str(), UI_COL_TEXT);
  if ((app.frameCount() / 30) % 2 == 0)
    c.fillRect(x + 50 + tw, y + 78, 2, 28, UI_COL_ACCENT);

  int nc = keyCols(), last = keyRows();
  int kw = m_full ? 52 : 84, kh = m_full ? 50 : 56, gap = m_full ? 6 : 8;
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
    app.tr("BACK:Borrar todo", "BACK:Clear"), app.tr("B:Cancelar", "B:Cancel")
  };
  app.drawHints(c, hints, 6);
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
