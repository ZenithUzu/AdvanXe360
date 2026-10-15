#include "UiScreens.h"

#include <stdio.h>
#include <string.h>

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
};

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
  m_dir = dir;
  if (!m_dirError)
    app.cfg().lastDir = dir;
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
    for (size_t i = 0; i < m_entries.size(); i++) {
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
  if (in.repeat & PAD_BIT(PAD_UP)) ls.move(-1, count, true);
  if (in.repeat & PAD_BIT(PAD_DOWN)) ls.move(1, count, true);
  if (in.repeat & PAD_BIT(PAD_LEFT)) ls.move(-visible, count, false);
  if (in.repeat & PAD_BIT(PAD_RIGHT)) ls.move(visible, count, false);
  if (in.repeat & PAD_BIT(PAD_LT)) ls.move(-count, count, false);
  if (in.repeat & PAD_BIT(PAD_RT)) ls.move(count, count, false);
  ls.ensureVisible(visible);

  if (in.pressed & PAD_BIT(PAD_A)) {
    activate(app);
    return;
  }
  if (in.pressed & PAD_BIT(PAD_B)) {
    if (m_tab == TAB_GAMES && !m_dir.empty()) {
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

void UiHomeScreen::drawList(UiApp &app, UiCanvas &c, int x, int y, int w, int h)
{
  UiListState &ls = m_list[m_tab];
  int rh = rowHeight();
  int visible = h / rh;
  ls.ensureVisible(visible);

  if (m_rows.empty()) {
    const char *msg;
    if (m_tab == TAB_GAMES)
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
    if (sel) {
      c.fillRoundRect(x, ry + 2, w - 16, rh - 4, 10, UI_COL_ROW_SEL);
      c.fillRoundRect(x, ry + 10, 4, rh - 20, 2, UI_COL_ACCENT);
    }
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
  c.fillRoundRect(x, y, w, h, 16, uiWithAlpha(UI_COL_PANEL, 0x60));
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

  const UiThumb *t = app.thumb(app.gameThumbPath(r.path));
  if (t) {
    c.fillRoundRect(x + pad - 2, ty - 2, tw + 4, th + 4, 10, 0x40000000);
    c.blitScaled(&t->px[0], t->w, t->h, t->w, x + pad, ty, tw, th);
  } else {
    c.fillRoundRect(x + pad, ty, tw, th, 10, 0x40000000);
    const char *k = romKind(r.path);
    c.drawTextFit(UI_FONT_TITLE, x + pad, ty + th / 2 - 20, tw, k, 0x50FFFFFF, UI_ALIGN_CENTER);
  }
  int ly = ty + th + 16;
  std::string name = uiStripExtension(uiFileName(r.path));
  c.drawTextFit(UI_FONT_BODY, x + pad, ly, tw, name.c_str(), UI_COL_TEXT);
  ly += 40;

  char line[256];
  if (r.size) {
    sprintf(line, "%s  \xb7  %s", romKind(r.path), formatSize(r.size).c_str());
    c.drawTextFit(UI_FONT_SMALL, x + pad, ly, tw, line, UI_COL_TEXT_DIM);
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

void UiHomeScreen::draw(UiApp &app, UiCanvas &c)
{
  app.drawBackground(c);

  // Tabs in the header
  static const char *es[TAB_COUNT] = { "Juegos", "Recientes", "Favoritos", "Ajustes" };
  static const char *en[TAB_COUNT] = { "Games", "Recent", "Favorites", "Settings" };
  c.drawText(UI_FONT_TITLE, UI_MARGIN_X, 30, "VBA360", UI_COL_TEXT);
  int tx = 330;
  drawBadge(c, tx, 40, "LB", 0x30FFFFFF, UI_COL_TEXT_DIM);
  tx += 52;
  for (int i = 0; i < TAB_COUNT; i++) {
    const char *label = app.tr(es[i], en[i]);
    int w = c.textWidth(UI_FONT_BODY, label) + 36;
    if (i == m_tab)
      c.fillRoundRect(tx, 32, w, 42, 21, UI_COL_ACCENT);
    c.drawText(UI_FONT_BODY, tx + 18, 38, label, i == m_tab ? 0xFFFFFFFF : UI_COL_TEXT_DIM);
    tx += w + 8;
  }
  drawBadge(c, tx + 4, 40, "RB", 0x30FFFFFF, UI_COL_TEXT_DIM);

  // Breadcrumb / tab description
  std::string sub;
  if (m_tab == TAB_GAMES)
    sub = m_dir.empty() ? std::string(app.tr("Elige una unidad", "Choose a drive")) : m_dir;
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
  drawList(app, c, UI_MARGIN_X, top, 760, listH);
  drawInfo(app, c, 848, top, UI_W - UI_MARGIN_X - 848, listH);

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
  if (m_tab == TAB_GAMES && !m_dir.empty())
    hints.push_back(app.tr("B:Subir", "B:Up"));
  if (onRom) {
    hints.push_back(app.favorites().contains(m_rows[ls.sel].path)
                    ? app.tr("X:Quitar favorito", "X:Unfavorite")
                    : app.tr("X:Favorito", "X:Favorite"));
    hints.push_back(app.tr("Y:Opciones del juego", "Y:Game options"));
    if (m_tab == TAB_RECENT)
      hints.push_back(app.tr("BACK:Quitar", "BACK:Remove"));
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
    // In Controls, A captures the new button / stick direction instead.
    if (it.kind == ITEM_OPTION && menuId != MENU_CONTROLS)
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
    c.fillRoundRect(x, y, w, h, 20, uiWithAlpha(UI_COL_PANEL, 0xF0));
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
      c.fillRoundRect(x, ry + 2, w, rh - 4, 10, UI_COL_ROW_SEL);
      c.fillRoundRect(x, ry + 10, 4, rh - 20, 2, UI_COL_ACCENT);
    }
    UiColor tc = !it.enabled ? 0xFF5E6678 : (sel ? UI_COL_TEXT : 0xFFD0D5E2);
    int valueW = 0;
    if (it.kind == ITEM_OPTION) {
      int vw = c.textWidth(UI_FONT_BODY, it.value.c_str());
      int right = x + w - 24;
      UiColor vc = sel ? 0xFFFFFFFF : UI_COL_TEXT_DIM;
      if (sel) drawArrow(c, right - 8, ry + rh / 2, 7, true, UI_COL_ACCENT);
      c.drawText(UI_FONT_BODY, right - 18 - vw, ry + 10, it.value.c_str(), vc);
      if (sel) drawArrow(c, right - 36 - vw, ry + rh / 2, 7, false, UI_COL_ACCENT);
      valueW = vw + 60;
    } else if (it.kind == ITEM_SUBMENU) {
      drawArrow(c, x + w - 30, ry + rh / 2, 7, true, sel ? UI_COL_TEXT : UI_COL_TEXT_DIM);
      if (!it.value.empty()) {
        int vw = c.textWidth(UI_FONT_SMALL, it.value.c_str());
        c.drawText(UI_FONT_SMALL, x + w - 48 - vw, ry + 14, it.value.c_str(), UI_COL_TEXT_DIM);
        valueW = vw + 60;
      } else {
        valueW = 40;
      }
    } else if (!it.value.empty()) {
      int vw = c.textWidth(UI_FONT_SMALL, it.value.c_str());
      c.drawText(UI_FONT_SMALL, x + w - 24 - vw, ry + 14, it.value.c_str(), UI_COL_TEXT_DIM);
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
}

UiKeyboardScreen::UiKeyboardScreen(const std::string &title, const std::string &text, int purpose)
  : m_title(title), m_text(text), m_purpose(purpose), m_row(0), m_col(0)
{
}

void UiKeyboardScreen::update(UiApp &app, const UiInputState &in)
{
  int cols = m_row == 3 ? 3 : 6;
  if (in.repeat & PAD_BIT(PAD_UP)) m_row = (m_row + 3) % 4;
  if (in.repeat & PAD_BIT(PAD_DOWN)) m_row = (m_row + 1) % 4;
  if (in.repeat & PAD_BIT(PAD_LEFT)) m_col = (m_col + cols - 1) % cols;
  if (in.repeat & PAD_BIT(PAD_RIGHT)) m_col = (m_col + 1) % cols;
  cols = m_row == 3 ? 3 : 6;
  if (m_col >= cols) m_col = cols - 1;

  bool del = (in.repeat & PAD_BIT(PAD_X)) != 0;
  bool space = (in.pressed & PAD_BIT(PAD_Y)) != 0;
  bool ok = (in.pressed & PAD_BIT(PAD_START)) != 0;
  if (in.pressed & PAD_BIT(PAD_A)) {
    if (m_row < 3) {
      if (m_text.size() < 40)
        m_text += kKeys[m_row][m_col];
    } else if (m_col == 0) space = true;
    else if (m_col == 1) del = true;
    else ok = true;
  }
  if (space && m_text.size() < 40) m_text += ' ';
  if (del && !m_text.empty()) m_text.erase(m_text.size() - 1);
  if (ok) {
    std::string text = m_text;
    int purpose = m_purpose;
    app.pop();
    app.onKeyboardDone(purpose, text, true);
    return;
  }
  if (in.pressed & PAD_BIT(PAD_B)) {
    int purpose = m_purpose;
    app.pop();
    app.onKeyboardDone(purpose, "", false);
  }
}

void UiKeyboardScreen::draw(UiApp &app, UiCanvas &c)
{
  c.fillRect(0, 0, UI_W, UI_H, 0x90000000);
  int w = 640, h = 470;
  int x = (UI_W - w) / 2, y = 100;
  c.fillRoundRect(x, y, w, h, 20, uiWithAlpha(UI_COL_PANEL, 0xFA));
  c.drawTextFit(UI_FONT_BODY, x + 32, y + 22, w - 64, m_title.c_str(), UI_COL_TEXT);

  // Text field with a blinking caret
  c.fillRoundRect(x + 32, y + 66, w - 64, 52, 10, 0x40000000);
  int tw = c.drawTextFit(UI_FONT_BODY, x + 48, y + 78, w - 110, m_text.c_str(), UI_COL_TEXT);
  if ((app.frameCount() / 30) % 2 == 0)
    c.fillRect(x + 50 + tw, y + 78, 2, 28, UI_COL_ACCENT);

  int kw = 84, kh = 56, gap = 8;
  int gx = x + (w - (kw * 6 + gap * 5)) / 2, gy = y + 140;
  for (int r = 0; r < 4; r++) {
    int cols = r == 3 ? 3 : 6;
    int bw = r == 3 ? (kw * 6 + gap * 5 - gap * 2) / 3 : kw;
    for (int col = 0; col < cols; col++) {
      int bx = gx + col * (bw + gap), by = gy + r * (kh + gap);
      bool sel = r == m_row && col == m_col;
      c.fillRoundRect(bx, by, bw, kh, 10, sel ? UI_COL_ACCENT : 0x30FFFFFF);
      const char *label;
      if (r < 3) label = kKeys[r][col];
      else if (col == 0) label = app.tr("Espacio", "Space");
      else if (col == 1) label = app.tr("Borrar", "Delete");
      else label = "OK";
      c.drawTextFit(UI_FONT_BODY, bx, by + 14, bw, label, 0xFFFFFFFF, UI_ALIGN_CENTER);
    }
  }
  const char *hints[] = {
    app.tr("A:Escribir", "A:Type"), app.tr("X:Borrar", "X:Delete"),
    app.tr("Y:Espacio", "Y:Space"), app.tr("START:Aceptar", "START:Done"),
    app.tr("B:Cancelar", "B:Cancel")
  };
  app.drawHints(c, hints, 5);
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
  c.fillRoundRect(x, y, w, h, 20, uiWithAlpha(UI_COL_PANEL, 0xFA));
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
  : m_menu(menu), m_itemId(itemId), m_title(title), m_values(values), m_style(style)
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
  if (in.pressed & (PAD_BIT(PAD_A) | PAD_BIT(PAD_B)))
    app.pop();
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
    c.fillRoundRect(x, y, w, h, 20, uiWithAlpha(UI_COL_PANEL, 0xFA));
  } else {
    // Side panel on the right, full height.
    c.fillRect(0, 0, UI_W, UI_H, 0x60000000);
    w = 460;
    h = UI_H;
    x = UI_W - w;
    y = 0;
    c.fillRect(x, y, w, h, uiWithAlpha(UI_COL_PANEL, 0xF8));
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
    if (sel) {
      c.fillRoundRect(x + 16, ry + 2, w - 32, rh - 4, 10, UI_COL_ROW_SEL);
      c.fillRoundRect(x + 16, ry + 10, 4, rh - 20, 2, UI_COL_ACCENT);
    }
    c.drawTextFit(UI_FONT_BODY, x + 36, ry + 8, w - 72, m_values[idx].c_str(),
                  sel ? UI_COL_TEXT : 0xFFD0D5E2);
  }
  c.resetClip();
  drawScrollbar(c, x + w - 12, listY, visible * rh, m_list.scroll, visible, (int)m_values.size());
  const char *hints[] = { app.tr("A:Listo", "A:Done"), app.tr("B:Cerrar", "B:Close") };
  if (m_style == OPT_STYLE_LIST) {
    app.drawHints(c, hints, 2);
  } else {
    // Hints inside the panel.
    int hy = UI_H - 64;
    c.fillRect(x + 28, hy - 8, w - 56, 1, COL_DIVIDER);
    c.drawText(UI_FONT_SMALL, x + 28, hy + 4,
               app.tr("Arriba/abajo: cambiar   A / B: cerrar", "Up/down: change   A / B: close"),
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
  c.fillRoundRect(x, y, w, h, 20, uiWithAlpha(UI_COL_PANEL, 0xFA));
  c.drawTextFit(UI_FONT_BODY, x + 24, y + 50, w - 48, m_message.c_str(), UI_COL_TEXT, UI_ALIGN_CENTER);
  const char *hints[] = { app.tr("A:S\xed", "A:Yes"), app.tr("B:No", "B:No") };
  app.drawHints(c, hints, 2);
}
