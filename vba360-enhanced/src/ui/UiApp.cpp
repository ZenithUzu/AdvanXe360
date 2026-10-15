#include "UiApp.h"
#include "UiScreens.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// Item ids (unique within a menu)
enum
{
  Q_RESUME = 1, Q_SAVE, Q_LOAD, Q_SLOT, Q_FF, Q_CHEATS, Q_GAME_OPTIONS,
  Q_SETTINGS, Q_SCREENSHOT, Q_RESET, Q_EXIT,

  V_FILTER = 1, V_STRETCH, V_EFFECT, V_GB_COLORS, V_BORDER, V_BORDER_FIT, V_FPS, V_FRAMESKIP,
  A_SOUND = 1, A_MUTE_FF,
  E_FF_SPEED = 1, E_FF_MODE, E_BIOS, E_GSA,
  S_SAVE_DIR = 1, S_STATE_DIR, S_AUTO_SAVE, S_AUTO_LOAD,
  U_LANGUAGE = 1, U_HIDE, U_HOME_DIR,
  C_RESET = 1, C_ACTION_BASE = 100,
  G_SAVE_TYPE = 1, G_RTC, G_PATCH, G_PLAY, G_SAVE_PATCHED,
  CH_ADD = 1, CH_IMPORT_BASE = 10, CH_CHEAT_BASE = 1000
};

// Confirm / keyboard purposes
enum
{
  CONFIRM_QUIT = 1, CONFIRM_DELETE_CHEAT, CONFIRM_RESET_CONTROLS, CONFIRM_RESET_GAME,
  KEYBOARD_CHEAT = 1
};

const int kStateSlots = 9;   // slots 1..9 (0 is the automatic one)

std::string intStr(int n)
{
  char b[16];
  sprintf(b, "%d", n);
  return b;
}

int wrapIndex(int v, int delta, int count)
{
  v += delta;
  while (v < 0) v += count;
  while (v >= count) v -= count;
  return v;
}

std::string lowerStr(std::string s)
{
  for (size_t i = 0; i < s.size(); i++)
    s[i] = (char)tolower((unsigned char)s[i]);
  return s;
}

// Image files in the emulator's borders\ folder (background choices).
std::vector<std::string> borderImages(UiHost &host)
{
  std::vector<UiDirEntry> entries;
  std::vector<std::string> names;
  host.listDir(uiJoinPath(host.emulatorDir(), "borders"), entries);
  for (size_t i = 0; i < entries.size(); i++)
    if (!entries[i].isDir && uiIsImageFile(entries[i].name))
      names.push_back(entries[i].name);
  return names;
}

// 0 = black, 1..n = borders\ image n-1 (matching the saved settings).
int borderChoice(const UiConfig &cfg, const std::vector<std::string> &images)
{
  if (!cfg.border)
    return 0;
  for (size_t i = 0; i < images.size(); i++)
    if (lowerStr(images[i]) == lowerStr(cfg.borderImage))
      return (int)i + 1;
  // Older settings: border.png / .jpg / ...
  for (size_t i = 0; i < images.size(); i++)
    if (lowerStr(uiStripExtension(images[i])) == "border")
      return (int)i + 1;
  return images.empty() ? 0 : 1;
}

} // namespace

UiApp::UiApp(UiHost &host, UiConfig &cfg)
  : fastForward(false), m_host(host), m_cfg(cfg), m_recents(30), m_favorites(500),
    m_inGame(false), m_prevHeld(0), m_repeatTimer(0), m_frame(0),
    m_request(UI_REQ_NONE), m_toastFrames(0), m_hintsAllowed(true)
{
}

UiApp::~UiApp()
{
  clearStack();
  for (size_t i = 0; i < m_trash.size(); i++)
    delete m_trash[i];
}

const char *UiApp::tr(const char *es, const char *en) const
{
  return m_cfg.language == 1 ? en : es;
}

std::string UiApp::configPath() const
{
  return uiJoinPath(m_host.emulatorDir(), "vba360.ini");
}

void UiApp::loadLists()
{
  m_recents.load(uiJoinPath(m_host.emulatorDir(), "recent.txt"));
  m_favorites.load(uiJoinPath(m_host.emulatorDir(), "favorites.txt"));
}

void UiApp::saveAll()
{
  m_cfg.save(configPath());
  m_recents.save(uiJoinPath(m_host.emulatorDir(), "recent.txt"));
  m_favorites.save(uiJoinPath(m_host.emulatorDir(), "favorites.txt"));
}

std::string UiApp::gameThumbPath(const std::string &rom) const
{
  return uiGameFile(m_host.emulatorDir(), "thumbs", rom, "", ".tmb");
}

std::string UiApp::gameConfigPath(const std::string &rom) const
{
  return uiGameFile(m_host.emulatorDir(), "config", rom, "", ".ini");
}

// --- Screen stack ------------------------------------------------------------

void UiApp::clearStack()
{
  for (size_t i = 0; i < m_stack.size(); i++)
    m_trash.push_back(m_stack[i]);
  m_stack.clear();
}

void UiApp::push(UiScreen *s)
{
  m_stack.push_back(s);
}

void UiApp::pop()
{
  if (m_stack.empty())
    return;
  m_trash.push_back(m_stack.back());
  m_stack.pop_back();
  if (!m_stack.empty())
    m_stack.back()->onResume(*this);
}

void UiApp::openHome(const std::string &selectRom)
{
  clearStack();
  m_inGame = false;
  fastForward = false;
  m_thumbKeys.clear();   // a new preview may have been written while playing
  m_thumbs.clear();
  push(new UiHomeScreen(*this, selectRom));
}

void UiApp::openInGameMenu()
{
  clearStack();
  m_inGame = true;
  m_thumbKeys.clear();
  m_thumbs.clear();
  openMenu(MENU_QUICK, m_host.currentRom());
}

void UiApp::request(UiRequest r, const std::string &rom)
{
  m_request = r;
  m_requestRom = rom;
}

UiRequest UiApp::takeRequest(std::string &romPath)
{
  UiRequest r = m_request;
  romPath = m_requestRom;
  m_request = UI_REQ_NONE;
  m_requestRom.clear();
  return r;
}

void UiApp::toast(const std::string &msg, int frames)
{
  m_toast = msg;
  m_toastFrames = frames;
}

void UiApp::launch(const std::string &rom)
{
  m_recents.pushFront(rom);
  m_cfg.lastDir = uiParentDir(rom);
  saveAll();
  request(UI_REQ_LAUNCH, rom);
}

void UiApp::toggleFavorite(const std::string &rom)
{
  if (m_favorites.contains(rom)) {
    m_favorites.remove(rom);
    toast(tr("Quitado de favoritos", "Removed from favorites"));
  } else {
    m_favorites.pushFront(rom);
    toast(tr("A\xf1""adido a favoritos", "Added to favorites"));
  }
  saveAll();
}

// --- Input / frame -------------------------------------------------------------

void UiApp::update(unsigned int held)
{
  UiInputState in;
  in.held = held;
  in.pressed = held & ~m_prevHeld;
  in.repeat = in.pressed;
  const unsigned int dirs = PAD_BIT(PAD_UP) | PAD_BIT(PAD_DOWN) | PAD_BIT(PAD_LEFT) |
                            PAD_BIT(PAD_RIGHT) | PAD_BIT(PAD_LT) | PAD_BIT(PAD_RT) |
                            PAD_BIT(PAD_X);
  if ((held & dirs) && (held & dirs) == (m_prevHeld & dirs)) {
    // Auto-repeat: first after 18 frames, then every 4 frames.
    m_repeatTimer++;
    if (m_repeatTimer >= 18 && (m_repeatTimer - 18) % 4 == 0)
      in.repeat |= held & dirs;
  } else {
    m_repeatTimer = 0;
  }
  m_prevHeld = held;

  if (!m_stack.empty())
    m_stack.back()->update(*this, in);

  for (size_t i = 0; i < m_trash.size(); i++)
    delete m_trash[i];
  m_trash.clear();

  if (m_toastFrames > 0)
    m_toastFrames--;
  m_frame++;
}

void UiApp::draw(UiCanvas &c)
{
  if (m_stack.empty()) {
    drawBackground(c);
    return;
  }
  int base = (int)m_stack.size() - 1;
  while (base > 0 && m_stack[base]->isOverlay())
    base--;
  if (m_stack[base]->isOverlay()) {
    // In-game menus float over the game: show only the newest menu panel
    // (plus any dialog above it), never several panels on top of each other.
    drawGameBackdrop(c);
    for (int i = (int)m_stack.size() - 1; i > base; i--) {
      if (m_stack[i]->asMenu()) {
        base = i;
        break;
      }
    }
  }
  for (size_t i = base; i < m_stack.size(); i++) {
    m_hintsAllowed = i + 1 == m_stack.size();
    m_stack[i]->draw(*this, c);
  }
  m_hintsAllowed = true;

  if (m_toastFrames > 0 && !m_toast.empty()) {
    int a = m_toastFrames < 20 ? m_toastFrames * 12 : 240;
    int w = c.textWidth(UI_FONT_BODY, m_toast.c_str()) + 48;
    int x = (UI_W - w) / 2, y = UI_FOOTER_Y - 124;
    if (m_inGame)
      y = 4;   // above the floating panel
    c.fillRoundRect(x, y, w, 48, 24, uiWithAlpha(0xFF2A3150, a));
    c.drawText(UI_FONT_BODY, x + 24, y + 10, m_toast.c_str(), uiWithAlpha(0xFFFFFFFF, a));
  }
}

// --- Thumbnails -------------------------------------------------------------

const UiThumb *UiApp::thumb(const std::string &path)
{
  for (size_t i = 0; i < m_thumbKeys.size(); i++) {
    if (m_thumbKeys[i] == path)
      return m_thumbs[i].valid() ? &m_thumbs[i] : 0;
  }
  if (m_thumbKeys.size() >= 24) {      // small cache, drop the oldest
    m_thumbKeys.erase(m_thumbKeys.begin());
    m_thumbs.erase(m_thumbs.begin());
  }
  m_thumbKeys.push_back(path);
  m_thumbs.push_back(UiThumb());
  uiLoadThumb(path, m_thumbs.back());   // a miss is cached too
  return m_thumbs.back().valid() ? &m_thumbs.back() : 0;
}

void UiApp::invalidateThumb(const std::string &path)
{
  for (size_t i = 0; i < m_thumbKeys.size(); i++) {
    if (m_thumbKeys[i] == path) {
      m_thumbKeys.erase(m_thumbKeys.begin() + i);
      m_thumbs.erase(m_thumbs.begin() + i);
      return;
    }
  }
}

// --- Drawing helpers ---------------------------------------------------------

void UiApp::drawBackground(UiCanvas &c)
{
  c.fillGradient(0, 0, UI_W, UI_H, 0xFF0E1220, 0xFF181E31);
}

void UiApp::drawHeader(UiCanvas &c, const char *title, const char *subtitle)
{
  c.drawTextFit(UI_FONT_TITLE, UI_MARGIN_X, 30, UI_W - UI_MARGIN_X * 2, title, UI_COL_TEXT);
  if (subtitle && *subtitle)
    c.drawTextFit(UI_FONT_SMALL, UI_MARGIN_X, 86, UI_W - UI_MARGIN_X * 2, subtitle, UI_COL_TEXT_DIM);
  c.fillRect(UI_MARGIN_X, UI_HEADER_H, UI_W - UI_MARGIN_X * 2, 1, 0x30FFFFFF);
}

void UiApp::drawGameBackdrop(UiCanvas &c)
{
  c.clear(0xFF000000);
  int w, h, pitch;
  const UiColor *frame = m_host.gameFrame(w, h, pitch);
  if (!frame)
    return;
  int dh = UI_H, dw = UI_H * w / h;
  if (dw > UI_W) { dw = UI_W; dh = UI_W * h / w; }
  c.blitScaled(frame, w, h, pitch, (UI_W - dw) / 2, (UI_H - dh) / 2, dw, dh, 80);
}

void UiApp::drawHints(UiCanvas &c, const char *const *hints, int count)
{
  if (!m_hintsAllowed)
    return;
  int x = UI_MARGIN_X, y = UI_FOOTER_Y + 8;
  c.fillRect(UI_MARGIN_X, UI_FOOTER_Y - 4, UI_W - UI_MARGIN_X * 2, 1, 0x30FFFFFF);
  for (int i = 0; i < count; i++) {
    const char *h = hints[i];
    const char *colon = strchr(h, ':');
    if (!colon)
      continue;
    std::string btn(h, colon - h);
    const char *label = colon + 1;
    UiColor col = 0;
    if (btn == "A") col = 0xFF5DBB63;
    else if (btn == "B") col = 0xFFE5534B;
    else if (btn == "X") col = 0xFF3D8BFD;
    else if (btn == "Y") col = 0xFFF2B33D;
    if (col) {
      c.fillCircle(x + 15, y + 15, 15, col);
      c.drawTextFit(UI_FONT_SMALL, x, y + 3, 30, btn.c_str(), 0xFFFFFFFF, UI_ALIGN_CENTER);
      x += 38;
    } else {
      int bw = c.textWidth(UI_FONT_SMALL, btn.c_str()) + 18;
      c.fillRoundRect(x, y + 1, bw, 28, 8, 0x40FFFFFF);
      c.drawText(UI_FONT_SMALL, x + 9, y + 4, btn.c_str(), 0xFFFFFFFF);
      x += bw + 8;
    }
    x += c.drawText(UI_FONT_SMALL, x, y + 4, label, UI_COL_TEXT_DIM) + 28;
  }
}

// --- Menus ---------------------------------------------------------------------

void UiApp::openMenu(int menuId, const std::string &rom)
{
  UiMenuScreen *m = new UiMenuScreen(menuId, m_inGame);
  m->arg = rom;
  if (menuId == MENU_GAME_OPTIONS) {
    m_gameCfg = UiGameConfig();
    m_gameCfg.load(gameConfigPath(rom));
    m_gameCfgRom = rom;
  }
  buildMenu(*m);
  m->selectId(m->items.empty() ? 0 : m->items[0].id);
  push(m);
}

void UiApp::rebuildMenus()
{
  for (size_t i = 0; i < m_stack.size(); i++) {
    UiMenuScreen *m = m_stack[i]->asMenu();
    if (m) {
      int id = m->selectedId();
      buildMenu(*m);
      m->selectId(id);
    }
  }
}

void UiApp::buildMenu(UiMenuScreen &m)
{
  m.clear();
  m.showPreview = false;
  const char *yesNo[2] = { tr("No", "No"), tr("S\xed", "Yes") };
  const char *onOff[2] = { tr("Desactivado", "Off"), tr("Activado", "On") };

  switch (m.menuId) {
  case MENU_QUICK:
    buildQuickMenu(m);
    break;

  case MENU_SETTINGS_ROOT:
    m.title = tr("Ajustes", "Settings");
    m.subtitle = tr("Los cambios se guardan autom\xe1ticamente", "Changes are saved automatically");
    m.add(MENU_SETTINGS_VIDEO, ITEM_SUBMENU, tr("Video", "Video"));
    m.add(MENU_SETTINGS_AUDIO, ITEM_SUBMENU, tr("Audio", "Audio"));
    m.add(MENU_SETTINGS_EMU, ITEM_SUBMENU, tr("Emulaci\xf3n", "Emulation"));
    m.add(MENU_SETTINGS_SAVES, ITEM_SUBMENU, tr("Guardado", "Saving"));
    m.add(MENU_CONTROLS, ITEM_SUBMENU, tr("Controles", "Controls"));
    m.add(MENU_SETTINGS_UI, ITEM_SUBMENU, tr("Interfaz e idioma", "Interface & language"));
    break;

  case MENU_SETTINGS_VIDEO: {
    m.title = tr("Video", "Video");
    m.subtitle = tr("C\xf3mo se ve el juego", "How the game looks");
    const char *filters[FILTER_COUNT] = {
      tr("N\xedtido", "Sharp"), tr("Suave", "Smooth"), "Scale2x", "LQ2x" };
    m.add(V_FILTER, ITEM_OPTION, tr("Filtro de imagen", "Image filter"), filters[m_cfg.filter],
          tr("N\xedtido: p\xedxeles definidos. Suave: bilineal. Scale2x/LQ2x: suavizan bordes.",
             "Sharp: crisp pixels. Smooth: bilinear. Scale2x/LQ2x: smooth the edges."));
    const char *stretch[SIZE_COUNT] = {
      tr("Original", "Original"), tr("Pantalla completa", "Full screen"), tr("Entera (p\xedxel exacto)", "Integer (pixel perfect)"),
      tr("16:9 (toda la pantalla)", "16:9 (whole screen)") };
    m.add(V_STRETCH, ITEM_OPTION, tr("Tama\xf1o de imagen", "Picture size"), stretch[m_cfg.stretch],
          tr("Original: proporci\xf3n de la consola. Entera: zoom exacto. 16:9: llena la TV (estirada).",
             "Original: console proportions. Integer: exact zoom. 16:9: fills the TV (stretched)."));
    const char *effects[EFFECT_COUNT] = {
      tr("Ninguno", "None"), tr("CRT (TV antigua)", "CRT (old TV)"), tr("LCD (pantalla port\xe1til)", "LCD (handheld screen)") };
    m.add(V_EFFECT, ITEM_OPTION, tr("Efecto de pantalla", "Screen effect"), effects[m_cfg.screenEffect],
          tr("CRT: l\xedneas y m\xe1scara de TV (zfast_crt). LCD: rejilla de port\xe1til (lcd3x). Mejor con Entera.",
             "CRT: TV scanlines and mask (zfast_crt). LCD: handheld grid (lcd3x). Best with Integer size."));
    m.add(V_GB_COLORS, ITEM_OPTION, tr("Colores (Game Boy)", "Colors (Game Boy)"),
          uiGbPaletteNames[m_cfg.gbPalette][m_cfg.language ? 1 : 0],
          tr("Paleta para juegos de Game Boy, al estilo de Super Game Boy.",
             "Palette for Game Boy games, in the style of the Super Game Boy."));
    {
      // Background: black, or an image from borders\ next to default.xex
      std::vector<std::string> images = borderImages(m_host);
      int choice = borderChoice(m_cfg, images);
      std::string value = choice == 0 ? std::string(tr("Negro", "Black")) : images[choice - 1];
      m.add(V_BORDER, ITEM_OPTION, tr("Fondo de la pantalla", "Screen background"), value,
            tr("Negro o una imagen de la carpeta borders\\ (PNG, JPG, BMP, TGA, DDS).",
               "Black or an image from the borders\\ folder (PNG, JPG, BMP, TGA, DDS)."));
      if (choice != 0) {
        const char *fits[FIT_COUNT] = {
          tr("Centrada", "Centered"), tr("Rellenar", "Fill"), tr("Estirar", "Stretch") };
        m.add(V_BORDER_FIT, ITEM_OPTION, tr("Ajuste del fondo", "Background fit"), fits[m_cfg.borderFit],
              tr("Centrada: imagen completa al centro. Rellenar: cubre la pantalla. Estirar: deforma.",
                 "Centered: whole image in the middle. Fill: covers the screen. Stretch: distorts."));
      }
    }
    m.add(V_FPS, ITEM_OPTION, tr("Mostrar FPS", "Show FPS"), yesNo[m_cfg.showFps]);
    const char *skip[5] = { tr("Autom\xe1tico", "Automatic"), "0", "1", "2", "3" };
    m.add(V_FRAMESKIP, ITEM_OPTION, tr("Salto de cuadros", "Frame skip"), skip[m_cfg.frameSkip],
          tr("Autom\xe1tico salta cuadros s\xf3lo si el juego va lento.",
             "Automatic only skips frames when the game runs slow."));
    break;
  }

  case MENU_SETTINGS_AUDIO:
    m.title = tr("Audio", "Audio");
    m.add(A_SOUND, ITEM_OPTION, tr("Sonido", "Sound"), onOff[m_cfg.sound]);
    m.add(A_MUTE_FF, ITEM_OPTION, tr("Silenciar en avance r\xe1pido", "Mute while fast forwarding"),
          yesNo[m_cfg.muteFastForward]);
    break;

  case MENU_SETTINGS_EMU: {
    m.title = tr("Emulaci\xf3n", "Emulation");
    std::string speed = uiFastForwardSpeeds[m_cfg.ffSpeed]
                        ? intStr(uiFastForwardSpeeds[m_cfg.ffSpeed]) + "x"
                        : std::string(tr("M\xe1xima", "Maximum"));
    m.add(E_FF_SPEED, ITEM_OPTION, tr("Velocidad de avance r\xe1pido", "Fast-forward speed"), speed);
    const char *modes[2] = { tr("Mantener presionado", "Hold"), tr("Pulsar para alternar", "Toggle") };
    std::string ffHelp = std::string(tr("Bot\xf3n de avance r\xe1pido: ", "Fast-forward button: ")) +
                         uiButtonName(*this, m_cfg.map[ACT_FAST_FORWARD]);
    m.add(E_FF_MODE, ITEM_OPTION, tr("Modo de avance r\xe1pido", "Fast-forward mode"), modes[m_cfg.ffToggle], ffHelp);
    m.add(E_BIOS, ITEM_OPTION, tr("Usar BIOS original", "Use original BIOS"), yesNo[m_cfg.useBios],
          tr("Requiere bios\\gba_bios.bin. Se salta la animaci\xf3n de inicio.",
             "Needs bios\\gba_bios.bin. The boot animation is skipped."));
    const char *gsa[2] = { "GameShark v1/v2", "GameShark v3 / Action Replay" };
    m.add(E_GSA, ITEM_OPTION, tr("C\xf3""digos de 16 d\xedgitos", "16-digit codes"), gsa[m_cfg.gsaV3],
          tr("Formato de los trucos XXXXXXXX YYYYYYYY que agregues.",
             "Format of the XXXXXXXX YYYYYYYY cheats you add."));
    break;
  }

  case MENU_SETTINGS_SAVES: {
    m.title = tr("Guardado", "Saving");
    const char *dirs[2] = { tr("Junto a la ROM", "Next to the ROM"),
                            tr("Carpeta del emulador", "Emulator folder") };
    m.add(S_SAVE_DIR, ITEM_OPTION, tr("Partidas (.sav)", "Game saves (.sav)"), dirs[m_cfg.saveDirMode],
          tr("Carpeta del emulador = saves\\ junto a default.xex.",
             "Emulator folder = saves\\ next to default.xex."));
    m.add(S_STATE_DIR, ITEM_OPTION, tr("Estados guardados", "Save states"), dirs[m_cfg.stateDirMode],
          tr("Carpeta del emulador = states\\ junto a default.xex.",
             "Emulator folder = states\\ next to default.xex."));
    m.add(S_AUTO_SAVE, ITEM_OPTION, tr("Guardar estado al salir", "Save state on exit"), yesNo[m_cfg.autoSaveState],
          tr("Al salir del juego se crea un estado autom\xe1tico.", "Leaving a game writes an automatic state."));
    m.add(S_AUTO_LOAD, ITEM_OPTION, tr("Continuar donde lo dej\xe9", "Resume where I left off"), yesNo[m_cfg.autoLoadState],
          tr("Al abrir un juego se carga el estado autom\xe1tico.", "Opening a game loads the automatic state."));
    break;
  }

  case MENU_SETTINGS_UI: {
    m.title = tr("Interfaz e idioma", "Interface & language");
    m.add(U_LANGUAGE, ITEM_OPTION, tr("Idioma", "Language"), m_cfg.language ? "English" : "Espa\xf1ol");
    m.add(U_HIDE, ITEM_OPTION, tr("Mostrar s\xf3lo juegos", "Show only games"), yesNo[m_cfg.hideNonGames],
          tr("Oculta los archivos que no son ROMs en el explorador.", "Hides non-ROM files in the browser."));
    m.add(U_HOME_DIR, ITEM_ACTION, tr("Volver a la lista de unidades", "Go back to the drive list"), "",
          tr("El explorador abrir\xe1 la lista de unidades.", "The browser will open the drive list."));
    break;
  }

  case MENU_CONTROLS:
    buildControlsMenu(m);
    break;

  case MENU_GAME_OPTIONS:
    buildGameOptionsMenu(m);
    break;

  case MENU_CHEATS:
    buildCheatsMenu(m);
    break;

  case MENU_ABOUT:
    m.title = tr("Acerca de", "About");
    m.subtitle = "VBA360 enhanced-ai";
    m.addInfo(tr("Emulador de Game Boy Advance y Game Boy.",
                 "Game Boy Advance and Game Boy emulator."));
    m.addInfo(tr("No emula Game Boy Color ni juegos de GBA que usan aditamentos o hardware especial.",
                 "No Game Boy Color, nor GBA games that need add-ons or special hardware."));
    m.addInfo(tr("Proyecto por pasatiempo, hecho con ayuda de IA. Licencia GPL.",
                 "Hobby project, made with the help of AI. GPL license."));
    m.addInfo(tr("Men\xfa en juego: clic del stick derecho (configurable en Controles).",
                 "In-game menu: right stick click (configurable in Controls)."));
    m.addHeader(tr("Cr\xe9""ditos", "Credits"));
    {
      // role (es, en), who, details (es, en)
      static const char *const credits[][5] = {
        { "Este fork", "This fork", "Axel (axelconde7w7-gif)",
          "Idea, pruebas en consola, compilaci\xf3n y capturas.", "Idea, console testing, builds and screenshots." },
        { "Programaci\xf3n con IA", "AI programming", "Claude (Anthropic)",
          "Cambios de c\xf3""digo y documentaci\xf3n de este fork.", "Code changes and documentation of this fork." },
        { "Fork vba360-enhanced", "vba360-enhanced fork", "frankischilling",
          "Arregl\xf3 la compilaci\xf3n del port; base de este proyecto.", "Fixed the port's build; base of this project." },
        { "Port a Xbox 360", "Xbox 360 port", "Autores de VBA360",
          "VBA360 y la librer\xed""a libSDLx360.", "VBA360 and the libSDLx360 library." },
        { "Emulador original", "Original emulator", "Forgotten, equipo VBA",
          "VisualBoyAdvance (1999-2004).", "VisualBoyAdvance (1999-2004)." },
        { "N\xfa""cleo", "Core", "Equipo de VBA-M",
          "VisualBoyAdvance-M; tambi\xe9n c\xf3""digo de VBA-GX.", "VisualBoyAdvance-M; also code from VBA-GX." },
        { "Sonido de Game Boy", "Game Boy sound", "Shay Green (blargg)",
          "Gb_Apu, Blip_Buffer y File_Extractor.", "Gb_Apu, Blip_Buffer and File_Extractor." },
        { "Efecto CRT", "CRT effect", "Greg Hogan (SoltanGris42)",
          "Adaptado de zfast_crt (libretro, GPL v2+).", "Adapted from zfast_crt (libretro, GPL v2+)." },
        { "Efecto LCD", "LCD effect", "Gigaherz",
          "Adaptado de lcd3x (libretro, dominio p\xfa""blico).", "Adapted from lcd3x (libretro, public domain)." },
        { "Paletas de Game Boy", "Game Boy palettes", "Gambatte (Sindre Aam\xe5s)",
          "Paletas DMG, Pocket, Light y Super Game Boy del core de libretro.",
          "DMG, Pocket, Light and Super Game Boy palettes from the libretro core." },
        { "Fuente", "Typeface", "Inter (Rasmus Andersson)",
          "SIL Open Font License.", "SIL Open Font License." },
        { "Librer\xed""as", "Libraries", "zlib, libpng, SDL",
          "Jean-loup Gailly y Mark Adler; Glenn Randers-Pehrson; Sam Lantinga.",
          "Jean-loup Gailly and Mark Adler; Glenn Randers-Pehrson; Sam Lantinga." },
        { "Comunidad", "Community", "Team Avalaunch, Team XeLove",
          "Herramientas y apoyo de la escena de Xbox 360.", "Tools and support from the Xbox 360 scene." },
      };
      int lang = m_cfg.language ? 1 : 0;
      for (size_t i = 0; i < sizeof(credits) / sizeof(credits[0]); i++)
        m.add((int)i + 1, ITEM_ACTION, credits[i][lang], credits[i][2], credits[i][3 + lang]);
    }
    break;
  }
}

void UiApp::buildQuickMenu(UiMenuScreen &m)
{
  std::string rom = m.arg;
  m.title = uiStripExtension(uiFileName(rom));
  m.subtitle = tr("Juego en pausa", "Game paused");
  m.showPreview = true;
  int slot = m_cfg.stateSlot;
  if (slot < 1 || slot > kStateSlots) slot = 1;
  std::string slotThumb = m_host.stateThumbPath(slot);
  std::string slotName = std::string(tr("Ranura ", "Slot ")) + intStr(slot);
  bool exists = m_host.stateExists(slot);

  m.add(Q_RESUME, ITEM_ACTION, tr("Continuar", "Resume"));
  UiMenuItem &save = m.add(Q_SAVE, ITEM_ACTION, tr("Guardar estado", "Save state"), slotName);
  save.thumb = slotThumb;
  UiMenuItem &load = m.add(Q_LOAD, ITEM_ACTION, tr("Cargar estado", "Load state"),
                           exists ? slotName : std::string(tr("Vac\xed""a", "Empty")));
  load.thumb = slotThumb;
  load.enabled = exists;
  UiMenuItem &sl = m.add(Q_SLOT, ITEM_OPTION, tr("Ranura de estado", "State slot"), intStr(slot),
                         exists ? "" : tr("Esta ranura est\xe1 vac\xed""a.", "This slot is empty."));
  sl.thumb = slotThumb;
  m.add(Q_FF, ITEM_OPTION, tr("Avance r\xe1pido", "Fast forward"),
        fastForward ? tr("Activado", "On") : tr("Desactivado", "Off"),
        std::string(tr("Tambi\xe9n con ", "Also with ")) + uiButtonName(*this, m_cfg.map[ACT_FAST_FORWARD]));

  std::vector<UiCheatInfo> cheats;
  m_host.listCheats(cheats);
  int on = 0;
  for (size_t i = 0; i < cheats.size(); i++)
    if (cheats[i].enabled) on++;
  char buf[64];
  sprintf(buf, tr("%d activos", "%d on"), on);
  m.add(Q_CHEATS, ITEM_SUBMENU, tr("Trucos", "Cheats"), cheats.empty() ? "" : buf);
  m.add(Q_GAME_OPTIONS, ITEM_SUBMENU, tr("Opciones de este juego", "Options for this game"));
  m.add(Q_SETTINGS, ITEM_SUBMENU, tr("Ajustes", "Settings"));
  m.add(Q_SCREENSHOT, ITEM_ACTION, tr("Captura de pantalla", "Take screenshot"));
  m.add(Q_RESET, ITEM_ACTION, tr("Reiniciar juego", "Restart game"));
  m.add(Q_EXIT, ITEM_ACTION, tr("Salir del juego", "Quit game"), "",
        tr("La partida (.sav) se guarda sola al salir.", "Your save (.sav) is written when you quit."));
}

void UiApp::buildControlsMenu(UiMenuScreen &m)
{
  m.title = tr("Controles", "Controls");
  m.subtitle = tr("A: presiona el bot\xf3n nuevo  \xb7  \xab\xbb: elige de la lista",
                  "A: press the new button  \xb7  \xab\xbb: pick from the list");
  m.hints.push_back(tr("X:Sin asignar", "X:Unassign"));
  m.hints.push_back(tr("START:Restablecer", "START:Defaults"));
  m.addHeader(tr("GAME BOY ADVANCE", "GAME BOY ADVANCE"));
  for (int a = ACT_GBA_A; a <= ACT_GBA_RIGHT; a++)
    m.add(C_ACTION_BASE + a, ITEM_OPTION, uiActionName(*this, a), uiButtonName(*this, m_cfg.map[a]));
  m.addHeader(tr("FUNCIONES", "SHORTCUTS"));
  for (int a = ACT_TURBO_A; a < ACT_COUNT; a++) {
    UiMenuItem &it = m.add(C_ACTION_BASE + a, ITEM_OPTION, uiActionName(*this, a),
                           uiButtonName(*this, m_cfg.map[a]));
    if (a == ACT_MENU)
      it.help = tr("Este bot\xf3n no se puede dejar sin asignar.", "This button can't be left unassigned.");
    else if (a == ACT_FAST_FORWARD)
      it.help = tr("El modo (mantener o alternar) est\xe1 en Emulaci\xf3n.",
                   "The mode (hold or toggle) is under Emulation.");
  }
  m.add(C_RESET, ITEM_ACTION, tr("Restablecer controles", "Reset controls"));
}

void UiApp::buildGameOptionsMenu(UiMenuScreen &m)
{
  m.title = tr("Opciones del juego", "Game options");
  m.subtitle = uiStripExtension(uiFileName(m.arg));
  const char *types[SAVE_TYPE_COUNT] = {
    tr("Autom\xe1tico", "Automatic"), "EEPROM", "SRAM", "Flash 64 KB", "Flash 128 KB",
    tr("Ninguno", "None") };
  m.add(G_SAVE_TYPE, ITEM_OPTION, tr("Tipo de guardado", "Save type"), types[m_gameCfg.saveType],
        tr("Autom\xe1tico detecta el tipo leyendo la ROM. C\xe1mbialo s\xf3lo si no guarda.",
           "Automatic reads it from the ROM. Change it only if the game won't save."));
  const char *tri[3] = { tr("Autom\xe1tico", "Automatic"), tr("Activado", "On"), tr("Desactivado", "Off") };
  m.add(G_RTC, ITEM_OPTION, tr("Reloj (RTC)", "Clock (RTC)"), tri[m_gameCfg.rtc],
        tr("Pok\xe9mon Rub\xed/Zafiro/Esmeralda y sus hacks usan el reloj.",
           "Pokemon Ruby/Sapphire/Emerald and their hacks use the clock."));
  const char *patch[2] = { tr("No aplicar", "Don't apply"), tr("Aplicar autom\xe1ticamente", "Apply automatically") };
  m.add(G_PATCH, ITEM_OPTION, tr("Parche .bps/.ups/.ips", "Patch .bps/.ups/.ips"), patch[m_gameCfg.patch],
        tr("Usa el parche con el mismo nombre que la ROM.", "Uses the patch with the same name as the ROM."));
  if (m_inGame && m_host.romPatched())
    m.add(G_SAVE_PATCHED, ITEM_ACTION, tr("Guardar ROM ya parchada", "Save patched ROM"), "",
          tr("Crea una copia con el parche aplicado junto a la ROM; carga m\xe1s r\xe1pido.",
             "Writes a copy with the patch applied next to the ROM; it loads faster."));
  if (m_inGame)
    m.addInfo(tr("Los cambios se aplican al reiniciar el juego.", "Changes apply when the game restarts."));
  else
    m.add(G_PLAY, ITEM_ACTION, tr("Jugar", "Play"));
}

void UiApp::buildCheatsMenu(UiMenuScreen &m)
{
  m.title = tr("Trucos", "Cheats");
  m.subtitle = uiStripExtension(uiFileName(m.arg));
  m.add(CH_ADD, ITEM_ACTION, tr("Agregar c\xf3""digo", "Add code"), "",
        tr("GameShark, Action Replay, CodeBreaker o Game Genie.",
           "GameShark, Action Replay, CodeBreaker or Game Genie."));

  // Text lists the player dropped next to the ROM or in cheats\.
  std::string base = uiStripExtension(m.arg);
  const std::string candidates[4] = {
    base + ".cht", base + ".txt",
    uiGameFile(m_host.emulatorDir(), "cheats", m.arg, "", ".cht"),
    uiGameFile(m_host.emulatorDir(), "cheats", m.arg, "", ".txt") };
  for (int i = 0; i < 4; i++) {
    if (m_host.fileExists(candidates[i])) {
      UiMenuItem &it = m.add(CH_IMPORT_BASE + i, ITEM_ACTION,
                             std::string(tr("Importar ", "Import ")) + uiFileName(candidates[i]));
      it.help = candidates[i];
    }
  }

  std::vector<UiCheatInfo> cheats;
  m_host.listCheats(cheats);
  if (cheats.empty()) {
    m.addInfo(tr("No hay trucos para este juego.", "No cheats for this game."));
  } else {
    m.addHeader(tr("TRUCOS DEL JUEGO", "GAME CHEATS"));
    m.hints.push_back(tr("Y:Borrar", "Y:Delete"));
    for (size_t i = 0; i < cheats.size(); i++) {
      UiMenuItem &it = m.add(CH_CHEAT_BASE + (int)i, ITEM_OPTION, cheats[i].desc,
                             cheats[i].enabled ? tr("Activado", "On") : tr("Desactivado", "Off"));
      it.help = cheats[i].code;
    }
  }
}

void UiApp::onMenuActivate(UiMenuScreen &m, int id)
{
  switch (m.menuId) {
  case MENU_QUICK:
    switch (id) {
    case Q_RESUME:
      request(UI_REQ_RESUME);
      break;
    case Q_SAVE: {
      int slot = m_cfg.stateSlot;
      if (m_host.saveState(slot)) {
        invalidateThumb(m_host.stateThumbPath(slot));
        toast(std::string(tr("Estado guardado en la ranura ", "State saved to slot ")) + intStr(slot));
      } else {
        toast(tr("No se pudo guardar el estado", "Could not save the state"));
      }
      rebuildMenus();
      break;
    }
    case Q_LOAD:
      if (m_host.loadState(m_cfg.stateSlot)) {
        toast(std::string(tr("Estado cargado de la ranura ", "State loaded from slot ")) + intStr(m_cfg.stateSlot));
        request(UI_REQ_RESUME);
      } else {
        toast(tr("No se pudo cargar el estado", "Could not load the state"));
      }
      break;
    case Q_CHEATS:
      openMenu(MENU_CHEATS, m.arg);
      break;
    case Q_GAME_OPTIONS:
      openMenu(MENU_GAME_OPTIONS, m.arg);
      break;
    case Q_SETTINGS:
      openMenu(MENU_SETTINGS_ROOT);
      break;
    case Q_SCREENSHOT: {
      std::string where;
      if (m_host.takeScreenshot(where))
        toast(std::string(tr("Captura guardada: ", "Screenshot saved: ")) + uiFileName(where));
      else
        toast(tr("No se pudo guardar la captura", "Could not save the screenshot"));
      break;
    }
    case Q_RESET:
      openConfirm(tr("\xbfReiniciar el juego? Se perder\xe1 lo no guardado.",
                     "Restart the game? Unsaved progress will be lost."), CONFIRM_RESET_GAME, "");
      break;
    case Q_EXIT:
      request(UI_REQ_EXIT_GAME);
      break;
    }
    return;

  case MENU_SETTINGS_ROOT:
    openMenu(id);
    return;

  case MENU_SETTINGS_UI:
    if (id == U_HOME_DIR) {
      m_cfg.lastDir.clear();
      saveAll();
      toast(tr("Listo", "Done"));
    }
    return;

  case MENU_CONTROLS:
    if (id == C_RESET)
      openConfirm(tr("\xbfRestablecer todos los controles?", "Reset all controls?"), CONFIRM_RESET_CONTROLS, "");
    else if (id >= C_ACTION_BASE)
      openCapture(id - C_ACTION_BASE);
    return;

  case MENU_GAME_OPTIONS:
    if (id == G_PLAY) {
      m_gameCfg.save(gameConfigPath(m_gameCfgRom));
      launch(m_gameCfgRom);
    } else if (id == G_SAVE_PATCHED) {
      std::string where;
      if (m_host.savePatchedRom(where))
        toast(std::string(tr("ROM guardada: ", "ROM saved: ")) + uiFileName(where));
      else
        toast(tr("No se pudo guardar la ROM", "Could not save the ROM"));
    }
    return;

  case MENU_CHEATS:
    if (id == CH_ADD) {
      openKeyboard(tr("Escribe el c\xf3""digo del truco", "Type the cheat code"), "", KEYBOARD_CHEAT);
    } else if (id >= CH_IMPORT_BASE && id < CH_IMPORT_BASE + 4) {
      const std::string &path = m.items[m.list.sel].help;
      int n = m_host.importCheatFile(path);
      char buf[96];
      sprintf(buf, tr("%d c\xf3""digos importados", "%d codes imported"), n);
      toast(buf);
      rebuildMenus();
    }
    return;
  }
}

void UiApp::onMenuChange(UiMenuScreen &m, int id, int delta)
{
  bool settings = true;
  switch (m.menuId) {
  case MENU_QUICK:
    settings = false;
    if (id == Q_SLOT)
      m_cfg.stateSlot = wrapIndex(m_cfg.stateSlot - 1, delta, kStateSlots) + 1;
    else if (id == Q_FF)
      fastForward = !fastForward;
    break;
  case MENU_SETTINGS_VIDEO:
    if (id == V_FILTER) m_cfg.filter = wrapIndex(m_cfg.filter, delta, FILTER_COUNT);
    else if (id == V_STRETCH) m_cfg.stretch = wrapIndex(m_cfg.stretch, delta, SIZE_COUNT);
    else if (id == V_EFFECT) m_cfg.screenEffect = wrapIndex(m_cfg.screenEffect, delta, EFFECT_COUNT);
    else if (id == V_GB_COLORS) m_cfg.gbPalette = wrapIndex(m_cfg.gbPalette, delta, GB_PALETTE_COUNT);
    else if (id == V_BORDER_FIT) m_cfg.borderFit = wrapIndex(m_cfg.borderFit, delta, FIT_COUNT);
    else if (id == V_BORDER) {
      std::vector<std::string> images = borderImages(m_host);
      int choice = wrapIndex(borderChoice(m_cfg, images), delta, (int)images.size() + 1);
      m_cfg.border = choice != 0;
      m_cfg.borderImage = choice != 0 ? images[choice - 1] : std::string();
    }
    else if (id == V_FPS) m_cfg.showFps ^= 1;
    else if (id == V_FRAMESKIP) m_cfg.frameSkip = wrapIndex(m_cfg.frameSkip, delta, 5);
    break;
  case MENU_SETTINGS_AUDIO:
    if (id == A_SOUND) m_cfg.sound ^= 1;
    else if (id == A_MUTE_FF) m_cfg.muteFastForward ^= 1;
    break;
  case MENU_SETTINGS_EMU:
    if (id == E_FF_SPEED) m_cfg.ffSpeed = wrapIndex(m_cfg.ffSpeed, delta, uiFastForwardSpeedCount);
    else if (id == E_FF_MODE) m_cfg.ffToggle ^= 1;
    else if (id == E_BIOS) m_cfg.useBios ^= 1;
    else if (id == E_GSA) m_cfg.gsaV3 ^= 1;
    break;
  case MENU_SETTINGS_SAVES:
    if (id == S_SAVE_DIR) m_cfg.saveDirMode ^= 1;
    else if (id == S_STATE_DIR) m_cfg.stateDirMode ^= 1;
    else if (id == S_AUTO_SAVE) m_cfg.autoSaveState ^= 1;
    else if (id == S_AUTO_LOAD) m_cfg.autoLoadState ^= 1;
    break;
  case MENU_SETTINGS_UI:
    if (id == U_LANGUAGE) m_cfg.language ^= 1;
    else if (id == U_HIDE) m_cfg.hideNonGames ^= 1;
    break;
  case MENU_CONTROLS:
    if (id >= C_ACTION_BASE) {
      int a = id - C_ACTION_BASE;
      // cycle through every button plus "unassigned"
      m_cfg.map[a] = wrapIndex(m_cfg.map[a], delta, PAD_BUTTON_COUNT + 1);
      if (a == ACT_MENU && m_cfg.map[a] == PAD_NONE)
        m_cfg.map[a] = wrapIndex(m_cfg.map[a], delta, PAD_BUTTON_COUNT + 1);
    }
    break;
  case MENU_GAME_OPTIONS:
    settings = false;
    if (id == G_SAVE_TYPE) m_gameCfg.saveType = wrapIndex(m_gameCfg.saveType, delta, SAVE_TYPE_COUNT);
    else if (id == G_RTC) m_gameCfg.rtc = wrapIndex(m_gameCfg.rtc, delta, 3);
    else if (id == G_PATCH) m_gameCfg.patch ^= 1;
    break;
  case MENU_CHEATS:
    settings = false;
    if (id >= CH_CHEAT_BASE) {
      std::vector<UiCheatInfo> cheats;
      m_host.listCheats(cheats);
      int i = id - CH_CHEAT_BASE;
      if (i < (int)cheats.size())
        m_host.setCheatEnabled(i, !cheats[i].enabled);
    }
    break;
  }
  rebuildMenus();
  if (settings)
    m_host.applySettings();
}

void UiApp::onMenuSecondary(UiMenuScreen &m, int id, int button)
{
  if (m.menuId == MENU_CONTROLS) {
    if (button == PAD_X && id >= C_ACTION_BASE && id - C_ACTION_BASE != ACT_MENU) {
      m_cfg.map[id - C_ACTION_BASE] = PAD_NONE;
      rebuildMenus();
      m_host.applySettings();
    } else if (button == PAD_START) {
      openConfirm(tr("\xbfRestablecer todos los controles?", "Reset all controls?"), CONFIRM_RESET_CONTROLS, "");
    }
  } else if (m.menuId == MENU_CHEATS && button == PAD_Y && id >= CH_CHEAT_BASE) {
    openConfirm(tr("\xbf""Borrar este truco?", "Delete this cheat?"), CONFIRM_DELETE_CHEAT,
                intStr(id - CH_CHEAT_BASE));
  }
}

void UiApp::onMenuClosed(UiMenuScreen &m)
{
  if (m.menuId == MENU_QUICK) {
    request(UI_REQ_RESUME);
    return;
  }
  if (m.menuId == MENU_GAME_OPTIONS)
    m_gameCfg.save(gameConfigPath(m_gameCfgRom));
  else if (m.menuId != MENU_CHEATS && m.menuId != MENU_ABOUT)
    saveAll();
  pop();
}

void UiApp::openKeyboard(const std::string &title, const std::string &initial, int purpose)
{
  push(new UiKeyboardScreen(title, initial, purpose));
}

void UiApp::onKeyboardDone(int purpose, const std::string &text, bool accepted)
{
  if (!accepted || purpose != KEYBOARD_CHEAT)
    return;
  std::string code;
  for (size_t i = 0; i < text.size(); i++)
    if (text[i] != ' ' || (!code.empty() && code[code.size() - 1] != ' '))
      code += text[i];
  while (!code.empty() && code[code.size() - 1] == ' ')
    code.erase(code.size() - 1);
  if (code.empty())
    return;
  std::vector<UiCheatInfo> cheats;
  m_host.listCheats(cheats);
  std::string desc = std::string(tr("C\xf3""digo ", "Code ")) + intStr((int)cheats.size() + 1);
  if (m_host.addCheat(code, desc))
    toast(tr("Truco agregado y activado", "Cheat added and enabled"));
  else
    toast(tr("C\xf3""digo no v\xe1lido", "Invalid code"));
  rebuildMenus();
}

void UiApp::openConfirm(const std::string &message, int purpose, const std::string &arg)
{
  push(new UiConfirmScreen(message, purpose, arg));
}

void UiApp::onConfirm(int purpose, const std::string &arg)
{
  switch (purpose) {
  case CONFIRM_QUIT:
    saveAll();
    m_host.quitToDashboard();
    break;
  case CONFIRM_DELETE_CHEAT:
    m_host.removeCheat(atoi(arg.c_str()));
    rebuildMenus();
    break;
  case CONFIRM_RESET_CONTROLS:
    m_cfg.resetControls();
    saveAll();
    rebuildMenus();
    m_host.applySettings();
    toast(tr("Controles restablecidos", "Controls reset"));
    break;
  case CONFIRM_RESET_GAME:
    m_host.resetGame();
    request(UI_REQ_RESUME);
    break;
  }
}

void UiApp::openCapture(int action)
{
  push(new UiCaptureScreen(action));
}

void UiApp::onCaptured(int action, int button)
{
  if (action < 0 || action >= ACT_COUNT)
    return;
  // A button can only do one GBA job: swap with whoever had it.
  if (action <= ACT_GBA_RIGHT) {
    for (int a = ACT_GBA_A; a <= ACT_GBA_RIGHT; a++)
      if (a != action && m_cfg.map[a] == button)
        m_cfg.map[a] = m_cfg.map[action];
  }
  m_cfg.map[action] = button;
  rebuildMenus();
  m_host.applySettings();
  toast(std::string(uiActionName(*this, action)) + ": " + uiButtonName(*this, button));
}
