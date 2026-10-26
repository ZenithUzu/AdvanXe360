#include "UiApp.h"
#include <algorithm>
#include "UiGlyphs.h"
#include "UiScreens.h"
#include "UiCheatParse.h"

#include <ctype.h>
#include <map>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum { HAND_DWELL_FRAMES = 75 };   // Kinect hand: ~1.25 s still on the same thing = A
#include <string.h>

namespace {

// Item ids (unique within a menu)
enum
{
  Q_RESUME = 1, Q_SAVE, Q_LOAD, Q_SLOT, Q_FF, Q_CHEATS, Q_GAME_OPTIONS,
  Q_SETTINGS, Q_SCREENSHOT, Q_RESET, Q_EXIT, Q_RA,
  AB_UPDATE = 900,
  RA_ENABLE = 1, RA_LOGIN, RA_LOGOUT, RA_SOUND, RA_STATUS, RA_GAME, RA_ITEM_BASE = 100,

  V_FILTER = 1, V_STRETCH, V_EFFECT, V_GB_COLORS, V_SGB, V_PRINTER, V_BORDER, V_BORDER_FIT, V_FPS, V_FRAMESKIP,
  V_GBA_COLOR, V_BLEND,
  A_SOUND = 1, A_MUTE_FF, A_VOLUME, A_LATENCY,
  E_FF_SPEED = 1, E_FF_MODE, E_BIOS, E_GSA, E_REWIND,
  SN_MAP_BASE = 50, SN_SENS = 60,   // in Controls: SN_MAP_BASE + UiSensorAction
  K_MODE = 1, K_YEAR, K_MONTH, K_DAY, K_HOUR, K_MINUTE, K_NOW,
  S_SAVE_DIR = 1, S_STATE_DIR, S_AUTO_SAVE, S_AUTO_LOAD, S_BACKUPS, S_DELETE_STATES,
  U_LANGUAGE = 1, U_HIDE, U_HOME_DIR, U_THEME, U_BACKGROUND, U_OPTION_STYLE, U_RESET_ALL, U_PREVIEW, U_START, U_RA_TEST, U_KINECT, U_KINECT_HANDS, U_KINECT_CAMERA, U_EFFECT,
  U_BG_VIS, U_PANELS, U_ACCENT, U_COVERS, U_BUBBLES, U_WATER, U_MUSIC, U_MUSIC_VOL, U_MUSIC_SONG,
  C_RESET = 1, C_ACTION_BASE = 100,
  G_SAVE_TYPE = 1, G_RTC, G_PATCH, G_PLAY, G_SAVE_PATCHED,
  G_CUSTOM, G_FILTER, G_STRETCH, G_EFFECT, G_GB_COLORS, G_SGB_MODE,
  G_GBA_COLOR, G_BLEND, G_FPS, G_FRAMESKIP,
  CH_ADD = 1, CH_TYPE, CH_ONLINE, CH_IMPORT_BASE = 10, CH_CHEAT_BASE = 1000,
  G_CHEATS = 40, G_CONTROLS_MODE, G_CONTROLS
};

// Confirm / keyboard purposes
enum
{
  CONFIRM_QUIT = 1, CONFIRM_DELETE_CHEAT, CONFIRM_RESET_CONTROLS, CONFIRM_RESET_GAME,
  CONFIRM_DELETE_STATES, CONFIRM_RESET_ALL,
  KEYBOARD_CHEAT = 1, KEYBOARD_CHEAT_DESC, KEYBOARD_SEARCH, KEYBOARD_RA_USER, KEYBOARD_RA_PASS,
  KEYBOARD_CHEAT_VALUE, KEYBOARD_CHEAT_EDIT
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

// Names of the picture settings (shared by Settings > Video and the
// per-game options).
const char *filterName(const UiApp &app, int i)
{
  const char *n[FILTER_COUNT] = { app.tr("N\xedtido", "Sharp"), app.tr("Suave", "Smooth"), "Scale2x", "LQ2x",
                                  app.tr("N\xedtido suave", "Sharp smooth"), "Scale3x" };
  return n[i < 0 || i >= FILTER_COUNT ? 0 : i];
}

const char *sizeName(const UiApp &app, int i)
{
  const char *n[SIZE_COUNT] = {
    app.tr("Original", "Original"), app.tr("Pantalla completa", "Full screen"),
    app.tr("Entera (p\xedxel exacto)", "Integer (pixel perfect)"),
    app.tr("16:9 (toda la pantalla)", "16:9 (whole screen)") };
  return n[i < 0 || i >= SIZE_COUNT ? 0 : i];
}

const char *effectName(const UiApp &app, int i)
{
  const char *n[EFFECT_COUNT] = {
    app.tr("Ninguno", "None"), app.tr("CRT (TV antigua)", "CRT (old TV)"),
    app.tr("LCD (pantalla port\xe1til)", "LCD (handheld screen)"),
    app.tr("Matriz de puntos (Game Boy)", "Dot matrix (Game Boy)") };
  return n[i < 0 || i >= EFFECT_COUNT ? 0 : i];
}

std::string lowerStr(std::string s)
{
  for (size_t i = 0; i < s.size(); i++)
    s[i] = (char)tolower((unsigned char)s[i]);
  return s;
}

// 0 = none, 1..n = backgrounds\ image n-1.
int menuImageChoice(const UiConfig &cfg, const std::vector<std::string> &images)
{
  if (cfg.menuBackground.empty())
    return 0;
  for (size_t i = 0; i < images.size(); i++)
    if (lowerStr(images[i]) == lowerStr(cfg.menuBackground))
      return (int)i + 1;
  return 0;
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

// "Right stick ->" style label for a sensor mapping.
std::string uiStickDirName(UiApp &app, int dir)
{
  static const char *es[SDIR_COUNT] = { "Sin asignar",
    "Stick der. \xbb derecha", "Stick der. \xab izquierda", "Stick der. arriba", "Stick der. abajo",
    "Stick izq. \xbb derecha", "Stick izq. \xab izquierda", "Stick izq. arriba", "Stick izq. abajo" };
  static const char *en[SDIR_COUNT] = { "Unassigned",
    "Right stick \xbb right", "Right stick \xab left", "Right stick up", "Right stick down",
    "Left stick \xbb right", "Left stick \xab left", "Left stick up", "Left stick down" };
  if (dir < 0 || dir >= SDIR_COUNT)
    dir = SDIR_NONE;
  return app.tr(es[dir], en[dir]);
}

} // namespace

UiApp::UiApp(UiHost &host, UiConfig &cfg)
  : fastForward(false), m_host(host), m_cfg(cfg), m_recents(30), m_favorites(500),
    m_inGame(false), m_prevHeld(0), m_repeatTimer(0), m_frame(0),
    m_request(UI_REQ_NONE), m_toastFrames(0), m_handOn(false), m_handUsed(false), m_handWasOn(false), m_handX(UI_W / 2), m_handY(UI_H / 2),
    m_handTarget(-1), m_handFrames(0), m_handStartIn(-1), m_netTest(0), m_updateCheck(0), m_raWaiting(false), m_raLastState(-1), m_handSwipe(0), m_swipeCooldown(0), m_handHist(0), m_cheatDownload(0), m_stickCapture(0), m_stickCaptureFrames(0),
    m_hintsAllowed(true), m_quietChange(false), m_ctlForGame(false), m_cheatTypeIdx(0), m_dirtyFrames(2), m_cheatEditIndex(-1), m_bgCacheTheme(-1), m_bgCacheVeil(-1), m_listMatches(0), m_coverLoadsLeft(1)
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
  m_dirtyFrames = 3;
  m_stack.push_back(s);
}

void UiApp::pop()
{
  m_dirtyFrames = 3;
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
  if (m_cfg.startScreen == 0)
    push(new UiLibraryScreen(*this, selectRom));
  else
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
  m_coverLoadsLeft = 1;

  // Kinect hand: only in the menus (in a game it would slow the game down).
  int hx, hy;
  bool handsWanted = m_cfg.kinect && m_cfg.kinectHands && !m_inGame;
  if (handsWanted && !m_handWasOn && m_handStartIn < 0) {
    // Starting the Kinect takes a moment and blocks: say so first.
    toast(tr("Iniciando Kinect... levanta la mano en unos segundos", "Starting the Kinect... raise your hand in a few seconds"), 240);
    m_handStartIn = 5;
  }
  if (m_handStartIn > 0)
    m_handStartIn--;
  m_handOn = handsWanted && m_handStartIn == 0 && m_host.handCursor(hx, hy);
  if (handsWanted && m_handStartIn == 0)
    m_handWasOn = true;   // started (a hand may not be up yet)
  m_handSwipe = SWIPE_NONE;
  if (m_swipeCooldown > 0)
    m_swipeCooldown--;
  if (m_handOn) {
    // Swipe: the raw hand moved a third of the screen in the last 6 updates.
    for (int i = 7; i > 0; i--) {
      m_rawX[i] = m_rawX[i - 1];
      m_rawY[i] = m_rawY[i - 1];
    }
    m_rawX[0] = hx;
    m_rawY[0] = hy;
    if (m_handHist < 8)
      m_handHist++;
    if (m_handHist > 6 && m_swipeCooldown == 0) {
      int dx = hx - m_rawX[6], dy = hy - m_rawY[6];
      int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
      if (ay > 240 && ay > ax * 2)
        m_handSwipe = dy < 0 ? SWIPE_UP : SWIPE_DOWN;
      else if (ax > 400 && ax > ay * 2)
        m_handSwipe = dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
      if (m_handSwipe) {
        m_swipeCooldown = 20;
        m_handFrames = 0;   // a swipe is never a "hold still"
      }
    }
    m_handX += (hx - m_handX) / 3;   // smooth out the shake
    m_handY += (hy - m_handY) / 3;
    m_dirtyFrames = 3;
  } else if (!(m_cfg.kinect && m_cfg.kinectHands) && m_handWasOn) {
    m_host.handStop();
    m_handWasOn = false;
    m_handStartIn = -1;
  }
  if (!m_handOn) {
    m_handTarget = -1;
    m_handHist = 0;
  }
  m_handUsed = false;

  if (!m_stack.empty())
    m_stack.back()->update(*this, in);

  for (size_t i = 0; i < m_trash.size(); i++)
    delete m_trash[i];
  m_trash.clear();

  if (m_toastFrames > 0)
    m_toastFrames--;
  // RetroAchievements: network replies; the menus follow the account's state.
  m_host.raPump();
  {
    UiRaInfo ra;
    m_host.raInfo(ra);
    if (ra.state != m_raLastState) {
      if (m_raWaiting && ra.state == UI_RA_LOGGED_IN) {
        toast(std::string(tr("Sesi\xf3n iniciada: ", "Logged in: ")) + ra.user);
        m_raWaiting = false;
        saveAll();   // the access the site gave (not the password)
      } else if (m_raWaiting && ra.state == UI_RA_FAILED) {
        toast(std::string(tr("No se pudo iniciar sesi\xf3n: ", "Login failed: ")) + ra.message, 400);
        m_raWaiting = false;
      }
      m_raLastState = ra.state;
      rebuildMenus();
    }
  }
  if (m_updateCheck > 0 && --m_updateCheck == 0) {
    std::string tag, err;
    if (!m_host.latestRelease(tag, err))
      toast(std::string(tr("No se pudo consultar GitHub (", "Couldn't ask GitHub (")) + err + ")");
    else if (uiVersionNewer(tag, UI_APP_VERSION))
      toast(std::string(tr("\xa1Hay una versi\xf3n nueva: ", "A new version is out: ")) + tag +
            tr("! Est\xe1 en github.com/ZenithUzu/AdvanXe360/releases", "! It's at github.com/ZenithUzu/AdvanXe360/releases"), 600);
    else
      toast(std::string(tr("Tienes la \xfaltima versi\xf3n (", "You have the latest version (")) + UI_APP_VERSION + ")");
  }
  if (m_netTest > 0 && --m_netTest == 0) {
    std::string report;
    m_host.netTest(report);
    toast(report, 600);
  }
  if (m_cheatDownload > 0 && --m_cheatDownload == 0) {
    std::string err;
    int n = m_host.downloadCheats(m_cheatRom, err);
    char buf[256] = "";
    if (n > 0 && err.compare(0, 6, "other:") == 0)
      // Codes of another version of the game (RetroAchievements recognised it):
      // they came in off; some may not work in this one.
      sprintf(buf, tr("%d c\xf3""digos de %.120s (otra versi\xf3n): apagados, pueden no funcionar",
                      "%d codes from %.120s (another version): off, they may not work"),
              n, uiUtf8ToLatin1(err.substr(6)).c_str());
    else if (n > 0)
      sprintf(buf, tr("%d c\xf3""digos agregados de internet", "%d codes added from the internet"), n);
    else if (n == 0)
      strcpy(buf, tr("Ya ten\xed""as todos los trucos de la base para este juego", "You already have every cheat from the database for this game"));
    else
      strcpy(buf, err == "nomatch" ? tr("No hay trucos para este juego en la base", "No cheats for this game in the database")
                                   : tr("Sin internet y sin la base local (cheats\\libretro-cheats.zip)",
                                        "No internet and no local database (cheats\\libretro-cheats.zip)"));
    toast(buf);
    rebuildMenus();
  }
  updateStickCapture();
  m_frame++;

  // Redraw while buttons are held or just released, while a message, a
  // capture or a blinking cursor is on screen, and twice a second anyway.
  if (m_dirtyFrames > 0)
    m_dirtyFrames--;
  if (held || in.pressed || m_toastFrames > 0 || m_stickCapture || m_request != UI_REQ_NONE)
    m_dirtyFrames = 3;
  if ((m_frame % 15) == 0)
    m_dirtyFrames = m_dirtyFrames > 1 ? m_dirtyFrames : 1;
  // Moving background: 30 frames a second is enough (the menu is drawn in
  // software). In game the menus float over the game, without it.
  // Only when something on screen does move (water needs a background image).
  bool moving = (m_cfg.bgEffect & UI_EFFECT_BUBBLES) || ((m_cfg.bgEffect & UI_EFFECT_WATER) && !m_cfg.menuBackground.empty());
  if (moving && !m_inGame && (m_frame & 1) == 0 &&
      m_cfg.menuTheme >= 0 && m_cfg.menuTheme < UI_THEME_COUNT && uiThemes[m_cfg.menuTheme].style == THEME_AERO)
    m_dirtyFrames = m_dirtyFrames > 1 ? m_dirtyFrames : 1;
}

void UiApp::startStickCapture(int id)
{
  m_stickCapture = id;
  m_stickCaptureFrames = 0;
  toast(tr("Mueve un stick hacia donde quieras... (5 s)", "Push a stick the way you want... (5 s)"), 300);
}

// Maps the sensor movement being captured to the stick direction pushed
// furthest (either stick).
void UiApp::updateStickCapture()
{
  if (!m_stickCapture)
    return;
  int lx = 0, ly = 0, rx = 0, ry = 0;
  m_host.sticks(lx, ly, rx, ry);
  int best = SDIR_NONE, bestAmount = 20000;
  for (int d = SDIR_RS_RIGHT; d < SDIR_COUNT; d++) {
    int v = uiStickAmount(d, lx, ly, rx, ry);
    if (v > bestAmount) {
      bestAmount = v;
      best = d;
    }
  }
  if (best == SDIR_NONE) {
    if (++m_stickCaptureFrames > 300) {
      m_stickCapture = 0;
      toast(tr("No se movi\xf3 ning\xfan stick; sin cambios", "No stick moved; nothing changed"));
    }
    return;
  }
  m_cfg.sensorMap[m_stickCapture - SN_MAP_BASE] = best;
  m_stickCapture = 0;
  saveAll();
  rebuildMenus();
  m_host.applySettings();
  toast(tr("Sensor asignado", "Sensor mapped"));
}

void UiApp::draw(UiCanvas &c)
{
  uiApplyTheme(m_cfg.menuTheme, uiAccentTints[m_cfg.accentTint], uiPanelOpacityPercent[m_cfg.panelOpacity]);
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

  if (m_handOn && m_handUsed) {   // the hand cursor, its ring filling while it waits
    c.fillCircle(m_handX, m_handY, 22, 0x50000000);
    c.fillCircle(m_handX, m_handY, 14, 0xE0FFFFFF);
    int dots = m_handFrames > 0 ? 24 * m_handFrames / HAND_DWELL_FRAMES : 0;
    for (int i = 0; i < dots; i++) {
      float a = (float)i / 24 * 6.2832f - 1.5708f;
      c.fillCircle(m_handX + (int)(cosf(a) * 22), m_handY + (int)(sinf(a) * 22), 4, UI_COL_ACCENT);
    }
  }

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
  uiApplyTheme(m_cfg.menuTheme, uiAccentTints[m_cfg.accentTint], uiPanelOpacityPercent[m_cfg.panelOpacity]);
  bool fresh = m_bgCache.size() == (size_t)(UI_W * UI_H) && m_bgCacheVeil == m_cfg.bgVisibility &&
               m_bgCacheTheme == m_cfg.menuTheme && m_bgCacheName == m_cfg.menuBackground;
  // Frutiger Aero only, and only over an image: moving a plain gradient
  // changes nothing on screen but still costs the whole screen every frame.
  const bool water = UI_THEME_STYLE == THEME_AERO && (m_cfg.bgEffect & UI_EFFECT_WATER) && !m_cfg.menuBackground.empty();
  if (fresh && c.width() == UI_W && c.height() == UI_H) {
    if (water)
      uiDrawWater(c, &m_bgCache[0], m_frame, m_cfg.waterStrength);
    else
      c.copyFrom(&m_bgCache[0]);
    uiDrawEffects(c, m_cfg, m_frame);
    return;
  }
  // Optional image from backgrounds\, under a veil of the theme's colors so
  // the text stays readable whatever the picture is.
  if (m_cfg.menuBackground != m_bgName) {
    m_bgName = m_cfg.menuBackground;
    m_bgPixels.clear();
    if (!m_bgName.empty() &&
        !m_host.loadImage(uiJoinPath(uiJoinPath(m_host.emulatorDir(), "backgrounds"), m_bgName),
                          UI_W, UI_H, m_bgPixels))
      m_bgPixels.clear();
  }
  if (m_bgPixels.size() == (size_t)(UI_W * UI_H)) {
    c.blitScaled(&m_bgPixels[0], UI_W, UI_H, UI_W, 0, 0, UI_W, UI_H);
    int veil = uiBgVeilAlpha[m_cfg.bgVisibility];
    c.fillGradient(0, 0, UI_W, UI_H, uiWithAlpha(UI_COL_BG_TOP, veil), uiWithAlpha(UI_COL_BG_BOTTOM, veil * 9 / 10));
  } else {
    c.fillGradient(0, 0, UI_W, UI_H, UI_COL_BG_TOP, UI_COL_BG_BOTTOM);
  }
  if (c.width() == UI_W && c.height() == UI_H) {
    m_bgCache.assign(c.pixels(), c.pixels() + UI_W * UI_H);
    m_bgCacheTheme = m_cfg.menuTheme;
    m_bgCacheName = m_cfg.menuBackground;
    m_bgCacheVeil = m_cfg.bgVisibility;
    // The decoded image stays (3.6 MB): changing the theme or the veil then
    // redraws it at once instead of reading and resizing the file again.
    if (water)
      uiDrawWater(c, &m_bgCache[0], m_frame, m_cfg.waterStrength);
  }
  uiDrawEffects(c, m_cfg, m_frame);
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
    // The controller's own button pictures ("LB/RB" draws both).
    std::string first = btn, second;
    size_t slash = btn.find('/');
    if (slash != std::string::npos) {
      first = btn.substr(0, slash);
      second = btn.substr(slash + 1);
    }
    const UiGlyph *g1 = uiGlyph(first.c_str());
    const UiGlyph *g2 = second.empty() ? 0 : uiGlyph(second.c_str());
    if (g1 && (second.empty() || g2)) {
      c.blitAlpha(g1->px, g1->w, g1->h, x, y + 15 - g1->h / 2);
      x += g1->w + 4;
      if (g2) {
        c.blitAlpha(g2->px, g2->w, g2->h, x, y + 15 - g2->h / 2);
        x += g2->w + 4;
      }
      x += 4;
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
  m_imageCache.clear();   // pick up images copied since the last time
  if (menuId == MENU_CONTROLS)
    m_ctlForGame = !rom.empty();   // opened from a game's options
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
    m.add(MENU_CLOCK, ITEM_SUBMENU, tr("Reloj de los juegos", "Game clock"));
    m.add(MENU_SETTINGS_UI, ITEM_SUBMENU, tr("Interfaz e idioma", "Interface & language"));
    m.add(MENU_SETTINGS_LOOK, ITEM_SUBMENU, tr("Apariencia", "Appearance"));
    m.add(MENU_SETTINGS_KINECT, ITEM_SUBMENU, "Kinect");
    m.add(MENU_SETTINGS_RA, ITEM_SUBMENU, "RetroAchievements");
    break;

  case MENU_SETTINGS_RA: {
    UiRaInfo ra;
    m_host.raInfo(ra);
    m.title = "RetroAchievements";
    m.subtitle = tr("Logros en modo normal (softcore): puedes usar estados y trucos",
                    "Achievements in normal mode (softcore): states and cheats are allowed");
    (m.add(RA_ENABLE, ITEM_OPTION, tr("Logros", "Achievements"), onOff[m_cfg.raEnabled],
           tr("Con una cuenta de retroachievements.org: al jugar se desbloquean logros y aparecen arriba de la pantalla.",
              "With a retroachievements.org account: achievements unlock as you play and show at the top of the screen."))).choice = m_cfg.raEnabled;
    if (m_cfg.raEnabled) {
      std::string st;
      if (ra.state == UI_RA_LOGGED_IN) st = std::string(tr("Conectado como ", "Logged in as ")) + ra.user;
      else if (ra.state == UI_RA_LOGGING_IN) st = tr("Iniciando sesi\xf3n...", "Logging in...");
      else if (ra.state == UI_RA_FAILED) st = std::string(tr("No se pudo: ", "Failed: ")) + ra.message;
      else st = tr("Sin sesi\xf3n", "Not logged in");
      m.add(RA_STATUS, ITEM_INFO, std::string(tr("Cuenta: ", "Account: ")) + st);
      if (ra.state == UI_RA_LOGGED_IN)
        m.add(RA_LOGOUT, ITEM_ACTION, tr("Cerrar sesi\xf3n", "Log out"), "",
              tr("Borra el permiso guardado; para volver hay que escribir la contrase\xf1""a.",
                 "Forgets the saved access; logging in again needs the password."));
      else
        m.add(RA_LOGIN, ITEM_ACTION, tr("Iniciar sesi\xf3n", "Log in"), "",
              tr("Usuario y contrase\xf1""a de retroachievements.org. La contrase\xf1""a no se guarda: solo el permiso que da el sitio.",
                 "Your retroachievements.org user and password. The password isn't kept: only the access the site gives."));
      (m.add(RA_SOUND, ITEM_OPTION, tr("Sonido al desbloquear", "Unlock sound"), onOff[m_cfg.raSound],
             tr("Suena sounds\\unlock.wav (el de RetroArch, si lo pones ah\xed) o un tono propio.",
                "Plays sounds\\unlock.wav (RetroArch's, if you put it there) or a chime of its own."))).choice = m_cfg.raSound;
      if (!ra.game.empty())
        m.add(RA_GAME, ITEM_INFO, ra.game + ":  " + intStr(ra.unlocked) + " / " + intStr(ra.total) + tr(" logros", " achievements"));
    }
    m.add(U_RA_TEST, ITEM_ACTION, tr("Probar conexi\xf3n segura", "Test secure connection"), "",
          tr("Se conecta por HTTPS a RetroAchievements y a GitHub y dice si funcion\xf3 (tambi\xe9n queda en vba360.log).",
             "Connects over HTTPS to RetroAchievements and GitHub and tells whether it worked (also in vba360.log)."));
    break;
  }

  case MENU_RA_LIST: {
    UiRaInfo ra;
    m_host.raInfo(ra);
    std::vector<UiRaAchievement> list;
    m_host.raAchievements(list);
    m.title = ra.game.empty() ? std::string(tr("Logros", "Achievements")) : ra.game;
    m.subtitle = intStr(ra.unlocked) + tr(" de ", " of ") + intStr(ra.total) + tr(" logros  \xb7  ", " achievements  \xb7  ") +
                 intStr(ra.points) + " / " + intStr(ra.pointsTotal) + tr(" puntos", " points");
    for (size_t i = 0; i < list.size(); i++) {
      const UiRaAchievement &a = list[i];
      std::string value = a.unlocked ? std::string(tr("Desbloqueado", "Unlocked"))
                        : !a.progress.empty() ? a.progress
                        : intStr(a.points) + tr(" pts", " pts");
      m.add(RA_ITEM_BASE + (int)i, ITEM_ACTION, a.title, value, a.description + "  (" + intStr(a.points) + tr(" puntos)", " points)"));
    }
    if (list.empty())
      m.add(RA_STATUS, ITEM_INFO, tr("Sin logros para este juego", "No achievements for this game"));
    break;
  }

  case MENU_SETTINGS_KINECT: {
    // Nothing of the Kinect is used (or shown) until it is turned on here.
    m.title = "Kinect";
    m.subtitle = tr("Control con la mano y c\xe1mara para la Game Boy Camera", "Hand control and camera for the Game Boy Camera");
    const char *onOff[2] = { tr("Apagado", "Off"), tr("Encendido", "On") };
    (m.add(U_KINECT, ITEM_OPTION, "Kinect", m_cfg.kinect ? tr("Conectado", "Connected") : tr("No usar", "Don't use"),
           tr("Act\xedvalo si tienes un Kinect conectado: aparecen sus funciones. Necesita los archivos Database del XDK junto al default.xex.",
              "Turn it on if you have a Kinect plugged in: its features appear. Needs the XDK's Database files next to default.xex."))).choice = m_cfg.kinect;
    if (m_cfg.kinect) {
      (m.add(U_KINECT_HANDS, ITEM_OPTION, tr("Control con la mano (Biblioteca)", "Hand control (Library)"), onOff[m_cfg.kinectHands],
             tr("Levanta la mano para mover el cursor; d\xe9jala quieta sobre un juego para abrirlo. Solo en los men\xfas; el Kinect tarda unos segundos en arrancar.",
                "Raise your hand to move the cursor; hold it still on a game to open it. Menus only; the Kinect takes a few seconds to start."))).choice = m_cfg.kinectHands;
      (m.add(U_KINECT_CAMERA, ITEM_OPTION, "Game Boy Camera", m_cfg.kinectCamera ? tr("C\xe1mara del Kinect", "Kinect's camera") : tr("Sin c\xe1mara", "No camera"),
             tr("Lo que ve el Kinect entra en la Game Boy Camera. Se aplica al abrir el juego.",
                "What the Kinect sees goes into the Game Boy Camera. Applies when the game starts."))).choice = m_cfg.kinectCamera;
    }
    break;
  }

  case MENU_CLOCK: {
    m.title = tr("Reloj de los juegos", "Game clock");
    m.subtitle = tr("Juegos con reloj: Pok\xe9mon Rub\xed/Zafiro/Esmeralda, Oro/Plata/Cristal, Boktai...",
                    "Games with a clock: Pokemon Ruby/Sapphire/Emerald, Gold/Silver/Crystal, Boktai...");
    const char *modes[2] = { tr("Hora de la consola", "Console time"), tr("Personalizada", "Custom") };
    m.add(K_MODE, ITEM_OPTION, tr("Hora", "Time"), modes[m_cfg.clockMode],
          tr("Personalizada: elige fecha y hora; despu\xe9s el reloj sigue avanzando.",
             "Custom: pick a date and time; the clock keeps running from there."));
    time_t now = time(NULL) + (m_cfg.clockMode ? m_cfg.clockOffset : 0);
    struct tm t = *localtime(&now);
    char buf[48];
    if (m_cfg.clockMode) {
      sprintf(buf, "%04d", t.tm_year + 1900);
      m.add(K_YEAR, ITEM_OPTION, tr("A\xf1o", "Year"), buf);
      sprintf(buf, "%02d", t.tm_mon + 1);
      m.add(K_MONTH, ITEM_OPTION, tr("Mes", "Month"), buf);
      sprintf(buf, "%02d", t.tm_mday);
      m.add(K_DAY, ITEM_OPTION, tr("D\xed""a", "Day"), buf);
      sprintf(buf, "%02d", t.tm_hour);
      m.add(K_HOUR, ITEM_OPTION, tr("Hora", "Hour"), buf);
      sprintf(buf, "%02d", t.tm_min);
      m.add(K_MINUTE, ITEM_OPTION, tr("Minuto", "Minute"), buf);
      m.add(K_NOW, ITEM_ACTION, tr("Volver a la hora de la consola", "Back to the console time"));
    }
    sprintf(buf, "%04d-%02d-%02d %02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    m.addInfo(std::string(tr("Fecha y hora para los juegos: ", "Date and time for games: ")) + buf);
    m.addInfo(tr("Se aplica al reiniciar el juego o cuando el juego vuelve a leer el reloj.",
                 "Takes effect when the game restarts or reads the clock again."));
    break;
  }

  case MENU_SETTINGS_VIDEO: {
    m.title = tr("Video", "Video");
    m.subtitle = tr("C\xf3mo se ve el juego", "How the game looks");
    const char *filters[FILTER_COUNT] = {
      tr("N\xedtido", "Sharp"), tr("Suave", "Smooth"), "Scale2x", "LQ2x", tr("N\xedtido suave", "Sharp smooth"), "Scale3x" };
    (m.add(V_FILTER, ITEM_OPTION, tr("Filtro de imagen", "Image filter"), filters[m_cfg.filter],
          tr("N\xedtido: p\xedxeles definidos. Suave: bilineal. N\xedtido suave: definido sin p\xedxeles desiguales. Scale2x/3x y LQ2x: redondean los bordes.",
             "Sharp: crisp pixels. Smooth: bilinear. Sharp smooth: crisp, no uneven pixels. Scale2x/3x and LQ2x: round off edges."))).choice = m_cfg.filter;
    const char *stretch[SIZE_COUNT] = {
      tr("Original", "Original"), tr("Pantalla completa", "Full screen"), tr("Entera (p\xedxel exacto)", "Integer (pixel perfect)"),
      tr("16:9 (toda la pantalla)", "16:9 (whole screen)") };
    (m.add(V_STRETCH, ITEM_OPTION, tr("Tama\xf1o de imagen", "Picture size"), stretch[m_cfg.stretch],
          tr("Original: proporci\xf3n de la consola. Entera: zoom exacto. 16:9: llena la TV (estirada).",
             "Original: console proportions. Integer: exact zoom. 16:9: fills the TV (stretched)."))).choice = m_cfg.stretch;
    const char *effects[EFFECT_COUNT] = {
      tr("Ninguno", "None"), tr("CRT (TV antigua)", "CRT (old TV)"), tr("LCD (pantalla port\xe1til)", "LCD (handheld screen)"),
      tr("Matriz de puntos (Game Boy)", "Dot matrix (Game Boy)") };
    (m.add(V_EFFECT, ITEM_OPTION, tr("Efecto de pantalla", "Screen effect"), effects[m_cfg.screenEffect],
          tr("CRT: l\xedneas y m\xe1scara de TV (zfast_crt). LCD: rejilla de port\xe1til (lcd3x). Matriz: puntos del Game Boy. Mejor con Entera.",
             "CRT: TV scanlines and mask (zfast_crt). LCD: handheld grid (lcd3x). Dot matrix: Game Boy dots. Best with Integer size."))).choice = m_cfg.screenEffect;
    (m.add(V_GB_COLORS, ITEM_OPTION, tr("Colores (Game Boy)", "Colors (Game Boy)"),
          uiGbPaletteNames[m_cfg.gbPalette][m_cfg.language ? 1 : 0],
          tr("Para juegos de Game Boy (los de Color usan los suyos). Autom\xe1tica: los que les da el GBC.",
             "For Game Boy games (Color games keep theirs). Automatic: the ones the GBC gives them."))).choice = m_cfg.gbPalette;
    const char *sgb[3] = { tr("Apagado", "Off"), tr("Juegos de Game Boy", "Game Boy games"),
                           tr("Tambi\xe9n compatibles con Color", "Also Color-compatible ones") };
    (m.add(V_SGB, ITEM_OPTION, tr("Super Game Boy (marcos)", "Super Game Boy (borders)"), sgb[m_cfg.sgbBorders],
          tr("Marco y colores del juego en el Super Game Boy. Se aplica al abrir el juego.",
             "The game's Super Game Boy border and colors. Applies when the game starts."))).choice = m_cfg.sgbBorders;
    const char *plugged[2] = { tr("Desconectada", "Unplugged"), tr("Conectada", "Plugged in") };
    (m.add(V_PRINTER, ITEM_OPTION, "Game Boy Printer", plugged[m_cfg.gbPrinter],
          tr("Lo que el juego imprima se guarda como imagen en screenshots. Se aplica al abrir el juego.",
             "What the game prints is saved as a picture in screenshots. Applies when the game starts."))).choice = m_cfg.gbPrinter;
    {
      // Background: black, or an image from borders\ next to default.xex
      std::vector<std::string> images = cachedImages("borders");
      int choice = borderChoice(m_cfg, images);
      std::string value = choice == 0 ? std::string(tr("Negro", "Black")) : images[choice - 1];
      (m.add(V_BORDER, ITEM_OPTION, tr("Fondo de la pantalla", "Screen background"), value,
            tr("Negro o una imagen de la carpeta borders\\ (PNG, JPG, BMP, TGA, DDS).",
               "Black or an image from the borders\\ folder (PNG, JPG, BMP, TGA, DDS)."))).choice = choice;
      if (choice != 0) {
        const char *fits[FIT_COUNT] = {
          tr("Centrada", "Centered"), tr("Rellenar", "Fill"), tr("Estirar", "Stretch") };
        (m.add(V_BORDER_FIT, ITEM_OPTION, tr("Ajuste del fondo", "Background fit"), fits[m_cfg.borderFit],
              tr("Centrada: imagen completa al centro. Rellenar: cubre la pantalla. Estirar: deforma.",
                 "Centered: whole image in the middle. Fill: covers the screen. Stretch: distorts."))).choice = m_cfg.borderFit;
      }
    }
    {
      const char *cc[3] = { tr("Apagada", "Off"), tr("Suave", "Soft"), tr("Como la pantalla real", "Like the real screen") };
      (m.add(V_GBA_COLOR, ITEM_OPTION, tr("Correcci\xf3n de color (GBA)", "Color correction (GBA)"), cc[m_cfg.gbaColor],
             tr("Colores menos chillones, como en la pantalla del GBA (gba-color de Pokefan531).",
                "Less garish colors, like on the GBA screen (Pokefan531's gba-color)."))).choice = m_cfg.gbaColor;
      const char *bl[3] = { tr("Apagada", "Off"), tr("Mezclar 2 cuadros", "Mix 2 frames"), tr("Estela de LCD", "LCD ghosting") };
      (m.add(V_BLEND, ITEM_OPTION, tr("Mezcla de cuadros", "Frame blending"), bl[m_cfg.frameBlend],
             tr("Quita el parpadeo de sprites (Golden Sun, F-Zero...), como el LCD de la consola.",
                "Removes sprite flicker (Golden Sun, F-Zero...), like the console's LCD."))).choice = m_cfg.frameBlend;
    }
    (m.add(V_FPS, ITEM_OPTION, tr("Mostrar FPS", "Show FPS"), yesNo[m_cfg.showFps])).choice = m_cfg.showFps;
    const char *skip[5] = { tr("Autom\xe1tico", "Automatic"), "0", "1", "2", "3" };
    (m.add(V_FRAMESKIP, ITEM_OPTION, tr("Salto de cuadros", "Frame skip"), skip[m_cfg.frameSkip],
          tr("Autom\xe1tico salta cuadros s\xf3lo si el juego va lento.",
             "Automatic only skips frames when the game runs slow."))).choice = m_cfg.frameSkip;
    break;
  }

  case MENU_SETTINGS_AUDIO:
    m.title = tr("Audio", "Audio");
    m.add(A_SOUND, ITEM_OPTION, tr("Sonido", "Sound"), onOff[m_cfg.sound]);
    m.add(A_MUTE_FF, ITEM_OPTION, tr("Silenciar en avance r\xe1pido", "Mute while fast forwarding"),
          yesNo[m_cfg.muteFastForward]);
    (m.add(A_VOLUME, ITEM_OPTION, tr("Volumen", "Volume"), intStr(uiVolumeLevels[m_cfg.volume]) + "%",
           tr("M\xe1s del 100% sube el volumen de los juegos bajos (puede saturar).",
              "Over 100% boosts quiet games (may clip)."))).choice = m_cfg.volume;
    {
      const char *lat[3] = { tr("Normal (100 ms)", "Normal (100 ms)"), tr("Bajo (60 ms)", "Low (60 ms)"),
                             tr("M\xednimo (40 ms)", "Lowest (40 ms)") };
      (m.add(A_LATENCY, ITEM_OPTION, tr("Retraso del sonido", "Sound delay"), lat[m_cfg.audioLatency],
             tr("Menos retraso: el sonido va m\xe1s pegado a la imagen. Si oyes cortes, vuelve a Normal.",
                "Less delay keeps the sound closer to the picture. If it crackles, go back to Normal."))).choice = m_cfg.audioLatency;
    }
    m.addHeader(tr("M\xfasica del men\xfa", "Menu music"));
    {
      const char *mm[MUSIC_MODE_COUNT] = { tr("Apagada", "Off"), tr("En orden", "In order"), tr("Aleatoria", "Shuffle") };
      (m.add(U_MUSIC, ITEM_OPTION, tr("M\xfasica del men\xfa", "Menu music"), mm[m_cfg.menuMusic],
             tr("Suena s\xf3lo en el men\xfa principal. Pon archivos .wav en la carpeta music\\ del emulador.",
                "Plays only in the main menu. Put .wav files in the emulator's music\\ folder."))).choice = m_cfg.menuMusic;
      if (m_cfg.menuMusic != MUSIC_OFF) {
        (m.add(U_MUSIC_VOL, ITEM_OPTION, tr("Volumen de la m\xfasica", "Music volume"),
               intStr(uiMusicVolumePercent[m_cfg.musicVolume]) + "%")).choice = m_cfg.musicVolume;
        // One song chosen by hand (it repeats), or all of them.
        const std::vector<std::string> &songs = cachedImages("music", ".wav");
        int cur = 0;
        for (size_t i = 0; i < songs.size(); i++)
          if (songs[i] == m_cfg.musicSong)
            cur = (int)i + 1;
        (m.add(U_MUSIC_SONG, ITEM_OPTION, tr("Canci\xf3n", "Song"),
               cur ? uiStripExtension(songs[cur - 1]) : std::string(tr("Todas", "All of them")),
               tr("Todas: en orden o aleatorias. Elige una para que se repita.",
                  "All of them: in order or shuffled. Pick one to keep it playing."))).choice = cur;
      }
    }
    break;

  case MENU_SETTINGS_EMU: {
    m.title = tr("Emulaci\xf3n", "Emulation");
    std::string speed = uiFastForwardSpeeds[m_cfg.ffSpeed]
                        ? intStr(uiFastForwardSpeeds[m_cfg.ffSpeed]) + "x"
                        : std::string(tr("M\xe1xima", "Maximum"));
    (m.add(E_FF_SPEED, ITEM_OPTION, tr("Velocidad de avance r\xe1pido", "Fast-forward speed"), speed)).choice = m_cfg.ffSpeed;
    const char *modes[2] = { tr("Mantener presionado", "Hold"), tr("Pulsar para alternar", "Toggle") };
    std::string ffHelp = std::string(tr("Bot\xf3n de avance r\xe1pido: ", "Fast-forward button: ")) +
                         uiButtonName(*this, m_cfg.map[ACT_FAST_FORWARD]);
    (m.add(E_FF_MODE, ITEM_OPTION, tr("Modo de avance r\xe1pido", "Fast-forward mode"), modes[m_cfg.ffToggle], ffHelp)).choice = m_cfg.ffToggle;
    m.add(E_BIOS, ITEM_OPTION, tr("Usar BIOS original", "Use original BIOS"), yesNo[m_cfg.useBios],
          tr("Requiere bios\\gba_bios.bin. Se salta la animaci\xf3n de inicio.",
             "Needs bios\\gba_bios.bin. The boot animation is skipped."));
    const char *gsa[2] = { "GameShark v1/v2", "GameShark v3 / Action Replay" };
    m.add(E_GSA, ITEM_OPTION, tr("C\xf3""digos de 16 d\xedgitos", "16-digit codes"), gsa[m_cfg.gsaV3],
          tr("Formato de los trucos XXXXXXXX YYYYYYYY que agregues.",
             "Format of the XXXXXXXX YYYYYYYY cheats you add."));
    m.add(E_REWIND, ITEM_OPTION, tr("Rebobinar", "Rewind"), onOff[m_cfg.rewind],
          std::string(tr("Guarda los \xfaltimos segundos para volver atr\xe1s manteniendo ",
                         "Keeps the last seconds to go back to by holding ")) +
              uiButtonName(*this, m_cfg.map[ACT_REWIND]) + tr(". Usa CPU: si un juego se pone lento, desact\xedvalo.",
                                                     ". Uses CPU: if a game slows down, turn it off."))
        .choice = m_cfg.rewind;
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
    m.add(S_BACKUPS, ITEM_OPTION, tr("Copias de seguridad del .sav", "Save (.sav) backups"),
          m_cfg.saveBackups ? tr("3 copias", "3 copies") : tr("No", "No"),
          tr("Al abrir un juego copia su partida a saves\\backups (guarda las 3 \xfaltimas).",
             "Opening a game copies its save to saves\\backups (keeps the last 3)."));
    m.addHeader(tr("Soluci\xf3n de problemas", "Troubleshooting"));
    m.add(S_DELETE_STATES, ITEM_ACTION, tr("Borrar todos los estados guardados", "Delete all save states"), "",
          tr("Ranuras y estado autom\xe1tico de todos los juegos (por si uno viejo falla). Las partidas .sav no se tocan.",
             "Slots and automatic state of every game (in case an old one fails). .sav saves are kept."));
    break;
  }

  case MENU_SETTINGS_UI: {
    m.title = tr("Interfaz e idioma", "Interface & language");
    (m.add(U_LANGUAGE, ITEM_OPTION, tr("Idioma", "Language"), m_cfg.language ? "English" : "Espa\xf1ol")).choice = m_cfg.language;
    m.add(U_HIDE, ITEM_OPTION, tr("Mostrar s\xf3lo juegos", "Show only games"), yesNo[m_cfg.hideNonGames],
          tr("Oculta los archivos que no son ROMs en el explorador.", "Hides non-ROM files in the browser."));
    const char *styles[OPT_STYLE_COUNT] = {
      tr("Panel lateral", "Side panel"), tr("Lista emergente", "Pop-up list"),
      tr("Cl\xe1sico (izquierda / derecha)", "Classic (left / right)") };
    (m.add(U_OPTION_STYLE, ITEM_OPTION, tr("Al cambiar una opci\xf3n", "Changing an option"), styles[m_cfg.optionStyle],
          tr("A abre los valores en un panel o una lista; izquierda/derecha siempre funcionan.",
             "A opens the values in a panel or a list; left/right always work."))).choice = m_cfg.optionStyle;
    {
      const char *st[2] = { tr("Biblioteca", "Library"), tr("Explorador", "File browser") };
      std::string lib = m_cfg.libraryDir.empty() ? std::string(tr("la carpeta Roms de cada unidad", "the Roms folder of every drive")) : m_cfg.libraryDir;
      (m.add(U_START, ITEM_OPTION, tr("Pantalla de inicio", "Start screen"), st[m_cfg.startScreen],
             std::string(tr("Biblioteca: consolas y portadas, sin carpetas (BACK abre el explorador). Juegos de: ",
                            "Library: consoles and box art, no folders (BACK opens the browser). Games from: ")) + lib +
             tr(" (Y sobre una carpeta en el explorador la cambia).", " (Y on a folder in the browser changes it)."))).choice = m_cfg.startScreen;
    }
    {
      const char *pv[PREVIEW_COUNT] = { tr("Portada o \xfaltima partida", "Box art or last played"),
                                        tr("S\xf3lo portada", "Box art only"),
                                        tr("S\xf3lo \xfaltima partida", "Last played only") };
      (m.add(U_PREVIEW, ITEM_OPTION, tr("Vista previa de la lista", "Game list preview"), pv[m_cfg.previewMode],
             tr("Qu\xe9 se ve junto a la lista: la portada o la imagen de la \xfaltima vez que jugaste.",
                "What shows next to the list: the box art or a picture of the last time you played."))).choice = m_cfg.previewMode;
    }
    m.add(U_HOME_DIR, ITEM_ACTION, tr("Volver a la lista de unidades", "Go back to the drive list"), "",
          tr("El explorador abrir\xe1 la lista de unidades.", "The browser will open the drive list."));
    m.addHeader(tr("Soluci\xf3n de problemas", "Troubleshooting"));
    m.add(U_RESET_ALL, ITEM_ACTION, tr("Restablecer toda la configuraci\xf3n", "Reset all settings"), "",
          tr("Ajustes y opciones de cada juego vuelven a lo de f\xe1""brica. Partidas, estados, trucos y recientes se quedan.",
             "Settings and every game's options go back to defaults. Saves, states, cheats and recents stay."));
    break;
  }
  case MENU_SETTINGS_LOOK: {
    m.title = tr("Apariencia", "Appearance");
    m.subtitle = tr("Tema, fondo y efectos del men\xfa", "Menu theme, background and effects");
    (m.add(U_THEME, ITEM_OPTION, tr("Tema de colores", "Color theme"),
          uiThemes[m_cfg.menuTheme].name[m_cfg.language ? 1 : 0],
          tr("Colores del men\xfa, inspirados en las consolas.", "Menu colors, inspired by the consoles."))).choice = m_cfg.menuTheme;
    {
      std::vector<std::string> images = cachedImages("backgrounds");
      int choice = menuImageChoice(m_cfg, images);
      (m.add(U_BACKGROUND, ITEM_OPTION, tr("Imagen de fondo", "Background image"),
            choice ? images[choice - 1] : std::string(tr("Ninguna", "None")),
            tr("Una imagen de la carpeta backgrounds\\ (ideal 1280x720), oscurecida.",
               "An image from the backgrounds\\ folder (1280x720 is best), darkened."))).choice = choice;
      if (choice) {   // only matters with an image
        const char *vis[BG_VIS_COUNT] = { tr("Tenue", "Faint"), tr("Normal", "Normal"),
                                          tr("Visible", "Visible"), tr("Muy visible", "Very visible") };
        (m.add(U_BG_VIS, ITEM_OPTION, tr("Visibilidad de la imagen", "Image visibility"), vis[m_cfg.bgVisibility],
               tr("Cu\xe1nto se ve tu imagen bajo los colores del tema.",
                  "How much of your image shows under the theme's colors."))).choice = m_cfg.bgVisibility;
      }
    }
    {
      const char *op[PANEL_OPACITY_COUNT] = { tr("Opacos", "Solid"), tr("Normal", "Normal"),
                                              tr("Transparentes", "See-through"), tr("Muy transparentes", "Very see-through") };
      (m.add(U_PANELS, ITEM_OPTION, tr("Transparencia de los paneles", "Panel transparency"), op[m_cfg.panelOpacity],
             tr("Cu\xe1nto fondo se ve a trav\xe9s de los paneles y men\xfas.",
                "How much background shows through panels and menus."))).choice = m_cfg.panelOpacity;
      const char *tint[ACCENT_TINT_COUNT] = { tr("La del tema", "The theme's"), tr("Azul cielo", "Sky blue"),
                                              tr("Turquesa", "Turquoise"), tr("Verde", "Green"), tr("Dorado", "Gold"),
                                              tr("Naranja", "Orange"), tr("Rojo", "Red"), tr("Rosa", "Pink"),
                                              tr("Morado", "Purple") };
      (m.add(U_ACCENT, ITEM_OPTION, tr("Tono de resaltado", "Highlight color"), tint[m_cfg.accentTint],
             tr("El color de la selecci\xf3n y los detalles, sobre cualquier tema.",
                "The color of the selection and details, over any theme."))).choice = m_cfg.accentTint;
    }
    if (uiThemes[m_cfg.menuTheme].style == THEME_AERO) {   // Frutiger Aero only
      m.addHeader(tr("Efectos de Frutiger Aero", "Frutiger Aero effects"));
      const char *fx[UI_EFFECT_COUNT] = { tr("Ninguno", "None"), tr("Burbujas", "Bubbles"),
                                          tr("Agua", "Water"), tr("Burbujas y agua", "Bubbles and water") };
      (m.add(U_EFFECT, ITEM_OPTION, tr("Efecto animado del fondo", "Moving background effect"), fx[m_cfg.bgEffect],
             tr("Burbujas que suben y agua en movimiento sobre el fondo (o tu imagen). S\xf3lo con Frutiger Aero.",
                "Rising bubbles and moving water over the background (or your image). Frutiger Aero only."))).choice = m_cfg.bgEffect;
      const char *lv[EFFECT_LEVEL_COUNT] = { tr("Pocas", "Few"), tr("Normal", "Normal"), tr("Muchas", "Many") };
      const char *wv[EFFECT_LEVEL_COUNT] = { tr("Suaves", "Gentle"), tr("Normales", "Normal"), tr("Fuertes", "Strong") };
      if (m_cfg.bgEffect & UI_EFFECT_BUBBLES)
        (m.add(U_BUBBLES, ITEM_OPTION, tr("Cantidad de burbujas", "Number of bubbles"), lv[m_cfg.bubbleAmount])).choice = m_cfg.bubbleAmount;
      if (m_cfg.bgEffect & UI_EFFECT_WATER)
        (m.add(U_WATER, ITEM_OPTION, tr("Ondas del agua", "Water ripples"), wv[m_cfg.waterStrength],
               tr("Cu\xe1nto ondula el fondo. Se nota sobre una imagen de fondo (backgrounds\\).",
                  "How much the background ripples. It shows over a background image (backgrounds\\)."))).choice = m_cfg.waterStrength;
    }
    m.addHeader(tr("Portadas", "Box art"));
    m.add(U_COVERS, ITEM_ACTION, tr("Descargar las portadas que faltan", "Download missing box art"), "",
          tr("Para todos los juegos de la Biblioteca sin portada (internet: thumbnails.libretro.com).",
             "For every Library game without box art (online: thumbnails.libretro.com)."));
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
    m.subtitle = tr(UI_APP_NAME " " UI_APP_VERSION " (antes VBA360 enhanced-ai)", UI_APP_NAME " " UI_APP_VERSION " (formerly VBA360 enhanced-ai)");
    m.add(AB_UPDATE, ITEM_ACTION, tr("Buscar actualizaci\xf3n", "Check for updates"), "v" UI_APP_VERSION,
          tr("Pregunta a GitHub por la \xfaltima versi\xf3n publicada (necesita internet).",
             "Asks GitHub for the latest published version (needs internet)."));
    m.addInfo(tr("Emulador de Game Boy Advance, Game Boy y Game Boy Color para Xbox 360.",
                 "Game Boy Advance, Game Boy and Game Boy Color emulator for the Xbox 360."));
    m.addInfo(tr("Proyecto por pasatiempo, hecho con ayuda de IA. Licencia GPL.",
                 "Hobby project, made with the help of AI. GPL license."));
    m.addInfo(tr("Men\xfa en juego: clic del stick derecho (configurable en Controles).",
                 "In-game menu: right stick click (configurable in Controls)."));
    {
      // Selectable rows, so the list can scroll through them: what (es, en),
      // short answer (es, en), details (es, en).
      static const char *const yes[][6] = {
        { "Game Boy Advance", "Game Boy Advance", "VBA-M + VBA-Next", "VBA-M + VBA-Next",
          "Guardado EEPROM, SRAM y Flash, reloj (Pok\xe9mon), parches .ips/.ups/.bps.",
          "EEPROM, SRAM and Flash saves, clock (Pokemon), .ips/.ups/.bps patches." },
        { "Classic NES Series / Famicom Mini", "Classic NES Series / Famicom Mini", "S\xed, ya arrancan", "Yes, they start now",
          "Los juegos de NES para GBA (Super Mario Bros., Zelda, Metroid...).",
          "The NES games for GBA (Super Mario Bros., Zelda, Metroid...)." },
        { "Sensores de GBA", "GBA sensors", "Con el stick derecho", "On the right stick",
          "Inclinaci\xf3n (Yoshi, Koro Koro), giroscopio (WarioWare: Twisted!) y sol (Boktai).",
          "Tilt (Yoshi, Koro Koro), gyroscope (WarioWare: Twisted!) and sun (Boktai)." },
        { "Vibraci\xf3n", "Rumble", "En el control", "On the controller",
          "Drill Dozer, WarioWare: Twisted! y los cartuchos de GB/GBC con vibraci\xf3n.",
          "Drill Dozer, WarioWare: Twisted! and GB/GBC carts with rumble." },
        { "Game Boy y Game Boy Color", "Game Boy and Game Boy Color", "Gambatte", "Gambatte",
          "Tambi\xe9n los juegos s\xf3lo de Color, con reloj (Pok\xe9mon Oro/Plata/Cristal).",
          "Color-only games too, with clock (Pokemon Gold/Silver/Crystal)." },
        { "Super Game Boy", "Super Game Boy", "Marcos y colores", "Borders and colors",
          "El marco y las paletas que tra\xed""a cada juego.", "The border and palettes each game came with." },
        { "Kirby Tilt 'n' Tumble (GBC)", "Kirby Tilt 'n' Tumble (GBC)", "S\xed, con el stick", "Yes, on the stick",
          "Cartucho MBC7: sensor de inclinaci\xf3n y memoria EEPROM.",
          "MBC7 cartridge: tilt sensor and EEPROM memory." },
        { "Extras", "Extras", "", "",
          "Estados, trucos, efectos CRT y LCD, mezcla de cuadros, correcci\xf3n de color y temas.",
          "States, cheats, CRT and LCD effects, frame blending, color correction and themes." },
      };
      static const char *const no[][6] = {
        { "Cable link y adaptador inal\xe1mbrico", "Link cable and wireless adapter", "No", "No",
          "Multijugador e intercambios, en GBA y en GB/GBC.", "Multiplayer and trading, on GBA and on GB/GBC." },
        { "e-Reader", "e-Reader", "No", "No", "Ni el lector ni sus tarjetas.", "Neither the reader nor its cards." },
        { "Game Boy Camera y Game Boy Printer", "Game Boy Camera and Game Boy Printer", "No", "No",
          "Los juegos de la c\xe1mara no arrancan; la impresora no se emula.",
          "Camera games don't start; the printer isn't emulated." },
        { "Infrarrojo del Game Boy Color", "Game Boy Color infrared", "No", "No",
          "Los intercambios por infrarrojo (Pok\xe9mon Cristal, Mystery Gift...).",
          "Infrared trading (Pokemon Crystal, Mystery Gift...)." },
        { "Bandai TAMA5 (Tamagotchi)", "Bandai TAMA5 (Tamagotchi)", "No", "No",
          "Cartucho especial de Game Boy.", "Special Game Boy cartridge." },
        { "Super Game Boy completo", "Full Super Game Boy", "Parcial", "Partial",
          "Sin marco gen\xe9rico, animaci\xf3n ni sonidos de arranque, ni sonido extra.",
          "No generic border, boot animation or sounds, or extra sound." },
      };
      int lang = m_cfg.language ? 1 : 0;
      m.addHeader(tr("Qu\xe9 emula", "What it emulates"));
      for (size_t i = 0; i < sizeof(yes) / sizeof(yes[0]); i++)
        m.add(100 + (int)i, ITEM_ACTION, yes[i][lang], yes[i][2 + lang], yes[i][4 + lang]);
      m.addHeader(tr("Qu\xe9 no emula (todav\xed""a)", "What it doesn't emulate (yet)"));
      for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++)
        m.add(200 + (int)i, ITEM_ACTION, no[i][lang], no[i][2 + lang], no[i][4 + lang]);
    }
    m.addHeader(tr("Cr\xe9""ditos", "Credits"));
    {
      // role (es, en), who, details (es, en)
      static const char *const credits[][5] = {
        { "Este fork", "This fork", "ZenithUzu (ZenithARC)",
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
        { "N\xfa""cleo de Game Boy y Color", "Game Boy and Color core", "Gambatte (Sindre Aam\xe5s)",
          "gambatte-libretro (equipo de libretro), GPL v2.", "gambatte-libretro (libretro team), GPL v2." },
        { "Super Game Boy y MBC7", "Super Game Boy and MBC7", "SameBoy (Lior Halphon)",
          "Marcos, colores y el cartucho de Kirby Tilt 'n' Tumble (licencia MIT).",
          "Borders, colors and the Kirby Tilt 'n' Tumble cartridge (MIT license)." },
        { "Mejoras del n\xfa""cleo de GBA", "GBA core improvements", "VBA-Next (libretro, Squarepusher)",
          "Dibujo por bloques, fondos rotados y arreglos que hacen arrancar Classic NES Series (GPL v2).",
          "Tiled drawing, rotation backgrounds and the fixes that make Classic NES Series start (GPL v2)." },
        { "Correcci\xf3n de color de GBA", "GBA color correction", "Pokefan531",
          "F\xf3rmulas del shader gba-color (libretro).", "Math from the gba-color shader (libretro)." },
        { "Pruebas del n\xfa""cleo", "Core tests", "jsmolka, Blargg, Matt Currie",
          "gba-tests, cpu_instrs y dmg/cgb-acid2 para comprobar los cambios.",
          "gba-tests, cpu_instrs and dmg/cgb-acid2 used to check the changes." },
        { "Sensores del cartucho", "Cartridge sensors", "Equipo de VBA-M",
          "Sensor solar, giroscopio y vibraci\xf3n, de la VBA-M actual.",
          "Solar sensor, gyroscope and rumble, from current VBA-M." },
        { "Sonido de Game Boy", "Game Boy sound", "Shay Green (blargg)",
          "Gb_Apu, Blip_Buffer y File_Extractor.", "Gb_Apu, Blip_Buffer and File_Extractor." },
        { "Efecto CRT", "CRT effect", "Greg Hogan (SoltanGris42)",
          "Adaptado de zfast_crt (libretro, GPL v2+).", "Adapted from zfast_crt (libretro, GPL v2+)." },
        { "Efecto LCD", "LCD effect", "Gigaherz",
          "Adaptado de lcd3x (libretro, dominio p\xfa""blico).", "Adapted from lcd3x (libretro, public domain)." },
        { "Paletas de Game Boy", "Game Boy palettes", "Gambatte (Sindre Aam\xe5s)",
          "Paletas DMG, Pocket, Light, Super Game Boy y las autom\xe1ticas del GBC.",
          "DMG, Pocket, Light, Super Game Boy and the GBC's automatic palettes." },
        { "Botones del control", "Controller buttons", "Microsoft (XNA Game Studio)",
          "Im\xe1genes de los botones de Xbox 360 (LB/RB reflejados), licencia Ms-PL.", "Xbox 360 button images (LB/RB mirrored), Ms-PL license." },
        { "Portadas", "Box art", "libretro-thumbnails (libretro)",
          "Las descarga de thumbnails.libretro.com; hechas por la comunidad de libretro.",
          "Downloaded from thumbnails.libretro.com; made by the libretro community." },
        { "Logros", "Achievements", "RetroAchievements (rcheevos)",
          "rcheevos, licencia MIT; los logros y el servidor son de retroachievements.org y su comunidad.",
          "rcheevos, MIT license; the achievements and the server belong to retroachievements.org and its community." },
        { "Aviso de logros", "Achievement pop-up", "RetroArch (libretro)",
          "Hecho como el de RetroArch. Su sonido (unlock, de ViRiX Dreamcore, CC BY-NC 3.0) no viene incluido.",
          "Made like RetroArch's. Its sound (unlock, by ViRiX Dreamcore, CC BY-NC 3.0) isn't included." },
        { "Conexi\xf3n segura (HTTPS)", "Secure connection (HTTPS)", "BearSSL (Thomas Pornin)",
          "Cifrado TLS para hablar con RetroAchievements y GitHub, licencia MIT. Ra\xed""ces de Mozilla.",
          "TLS to talk to RetroAchievements and GitHub, MIT license. Mozilla's root certificates." },
        { "Base de trucos", "Cheat database", "libretro-database (libretro), superfw-cheats (davidgfnet)",
          "Trucos de GB, GBC y GBA (CC BY-SA 4.0) y de GBA de SuperFW, en cheats\\libretro-cheats.zip.",
          "GB, GBC and GBA cheats (CC BY-SA 4.0) and SuperFW's GBA cheats, in cheats\\libretro-cheats.zip." },
        { "Cartuchos de la Biblioteca", "Library cartridges", "AdvanXe360",
          "Gen\xe9ricos, sin logos. Pon los tuyos en carts\\ (gba.png, gbc.png, gb.png, zip.png).",
          "Generic, no logos. Put your own in carts\\ (gba.png, gbc.png, gb.png, zip.png)." },
        { "Filtros Scale2x / Scale3x", "Scale2x / Scale3x filters", "Andrea Mazzoleni (AdvanceMAME)",
          "Algoritmo reescrito para la tarjeta gr\xe1""fica; N\xedtido suave: sharp-bilinear de libretro.",
          "Algorithm rewritten for the GPU; Sharp smooth: libretro's sharp-bilinear." },
        { "Efecto de agua", "Water effect", "snes360-enhanced (frankischilling)",
          "La idea del agua que ondula el fondo, adaptada a este men\xfa.",
          "The idea of water rippling the background, adapted to this menu." },
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

  UiRaInfo ra;
  m_host.raInfo(ra);
  if (!ra.game.empty())
    m.add(Q_RA, ITEM_SUBMENU, tr("Logros", "Achievements"), intStr(ra.unlocked) + " / " + intStr(ra.total),
          std::string("RetroAchievements: ") + ra.game);
  m.add(Q_GAME_OPTIONS, ITEM_SUBMENU, tr("Opciones de este juego", "Options for this game"), "",
        tr("Imagen de este juego, trucos y c\xf3""digos, parche...",
           "This game's picture, cheats and codes, patch..."));
  m.add(Q_SETTINGS, ITEM_SUBMENU, tr("Ajustes", "Settings"));
  m.add(Q_SCREENSHOT, ITEM_ACTION, tr("Captura de pantalla", "Take screenshot"));
  m.add(Q_RESET, ITEM_ACTION, tr("Reiniciar juego", "Restart game"));
  m.add(Q_EXIT, ITEM_ACTION, tr("Salir del juego", "Quit game"), "",
        tr("La partida (.sav) se guarda sola al salir.", "Your save (.sav) is written when you quit."));
}

void UiApp::buildControlsMenu(UiMenuScreen &m)
{
  m.title = tr("Controles", "Controls");
  if (m_ctlForGame)
    m.title = std::string(tr("Controles de ", "Controls for ")) + uiStripExtension(uiFileName(m.arg));
  m.subtitle = tr("A: presiona el bot\xf3n nuevo  \xb7  \xab\xbb: elige de la lista",
                  "A: press the new button  \xb7  \xab\xbb: pick from the list");
  m.hints.push_back(tr("X:Sin asignar", "X:Unassign"));
  m.hints.push_back(tr("START:Restablecer", "START:Defaults"));
  // Which console: the game these controls are for, or the one running.
  std::string rom = m_ctlForGame ? m.arg : (m_inGame ? m_host.currentRom() : std::string());
  int info = rom.empty() ? 0 : romInfo(rom);
  bool gb = (info & UI_ROM_GB) != 0;
  if (rom.empty())
    m.addHeader(tr("BOTONES DEL JUEGO (GBA, GB Y GBC)", "GAME BUTTONS (GBA, GB AND GBC)"));
  else if (gb)
    m.addHeader((info & UI_ROM_CGB) ? tr("GAME BOY COLOR", "GAME BOY COLOR") : tr("GAME BOY", "GAME BOY"));
  else
    m.addHeader(tr("GAME BOY ADVANCE", "GAME BOY ADVANCE"));
  for (int a = ACT_GBA_A; a <= ACT_GBA_RIGHT; a++) {
    bool shoulder = a == ACT_GBA_L || a == ACT_GBA_R;
    if (gb && shoulder)
      continue;   // the Game Boy has no L / R
    UiMenuItem &it = m.add(C_ACTION_BASE + a, ITEM_OPTION, uiActionName(*this, a), uiButtonName(*this, ctlMap()[a]));
    if (shoulder && rom.empty())
      it.help = tr("S\xf3lo en juegos de GBA (el Game Boy no tiene L ni R).",
                   "GBA games only (the Game Boy has no L or R).");
  }
  m.addHeader(tr("FUNCIONES", "SHORTCUTS"));
  for (int a = ACT_TURBO_A; a < ACT_COUNT; a++) {
    UiMenuItem &it = m.add(C_ACTION_BASE + a, ITEM_OPTION, uiActionName(*this, a),
                           uiButtonName(*this, ctlMap()[a]));
    if (a == ACT_MENU)
      it.help = tr("Sin asignar: clic del stick derecho (si est\xe1 libre). Los dos sticks a la vez siempre lo abren.",
                   "Unassigned: right stick click (if free). Both stick clicks at once always open it.");
    else if (a == ACT_REWIND)
      it.help = tr("Mantenlo y el juego va hacia atr\xe1s. Se activa en Emulaci\xf3n.",
                   "Hold it and the game goes backwards. Turned on under Emulation.");
    else if (a == ACT_SHAKE)
      it.help = tr("Kirby Tilt 'n' Tumble: como sacudir la consola hacia arriba (Kirby salta).",
                   "Kirby Tilt 'n' Tumble: like flicking the console up (Kirby jumps).");
    else if (a == ACT_FAST_FORWARD)
      it.help = tr("El modo (mantener o alternar) est\xe1 en Emulaci\xf3n.",
                   "The mode (hold or toggle) is under Emulation.");
  }
  m.add(C_RESET, ITEM_ACTION, tr("Restablecer controles", "Reset controls"));

  // Cartridge sensors (only active in the games that have them). For one
  // game, only shown when it has a sensor; they are shared by all games.
  if (m_ctlForGame && !(info & UI_ROM_SENSOR))
    return;
  m.addHeader(tr("SENSORES DE MOVIMIENTO", "MOTION SENSORS"));
  if (m_ctlForGame)
    m.addInfo(tr("Los sensores son los mismos para todos los juegos.",
                 "Sensors are the same for every game."));
  const char *names[SENS_COUNT][2] = {
    { "Inclinar a la derecha", "Tilt right" },
    { "Inclinar a la izquierda", "Tilt left" },
    { "Inclinar hacia arriba", "Tilt up" },
    { "Inclinar hacia abajo", "Tilt down" },
    { "M\xe1s luz solar", "More sunlight" },
    { "Menos luz solar", "Less sunlight" } };
  const char *helps[SENS_COUNT][2] = {
    { "Yoshi, Koro Koro y Kirby Tilt 'n' Tumble. Tambi\xe9n gira en WarioWare: Twisted!", "Yoshi, Koro Koro and Kirby Tilt 'n' Tumble. Also turns in WarioWare: Twisted!" },
    { "Yoshi, Koro Koro y Kirby Tilt 'n' Tumble. Tambi\xe9n gira en WarioWare: Twisted!", "Yoshi, Koro Koro and Kirby Tilt 'n' Tumble. Also turns in WarioWare: Twisted!" },
    { "Yoshi Topsy-Turvy, Koro Koro Puzzle, Kirby Tilt 'n' Tumble.", "Yoshi Topsy-Turvy, Koro Koro Puzzle, Kirby Tilt 'n' Tumble." },
    { "Yoshi Topsy-Turvy, Koro Koro Puzzle, Kirby Tilt 'n' Tumble.", "Yoshi Topsy-Turvy, Koro Koro Puzzle, Kirby Tilt 'n' Tumble." },
    { "Boktai: sube el sol (se muestra en pantalla).", "Boktai: raises the sun (shown on screen)." },
    { "Boktai: baja el sol (se muestra en pantalla).", "Boktai: lowers the sun (shown on screen)." } };
  int lang = m_cfg.language ? 1 : 0;
  for (int i = 0; i < SENS_COUNT; i++) {
    if (m_ctlForGame && gb && i >= SENS_SUN_UP)
      continue;   // Kirby Tilt 'n' Tumble: tilt only
    m.add(SN_MAP_BASE + i, ITEM_OPTION, names[i][lang], uiStickDirName(*this, m_cfg.sensorMap[i]),
          std::string(helps[i][lang]) + tr(" A: mueve el stick.", " A: push the stick."));
  }
  char pct[16];
  sprintf(pct, "%d%%", uiSensorSensPercent[m_cfg.sensorSens]);
  m.add(SN_SENS, ITEM_OPTION, tr("Sensibilidad de inclinaci\xf3n", "Tilt sensitivity"), pct,
        tr("Cu\xe1nto inclina o gira con el stick al tope.", "How far it tilts or turns with the stick all the way."));
  m.addInfo(tr("S\xf3lo funcionan en los juegos que tienen esos sensores.",
               "They only work in the games that have those sensors."));
}

void UiApp::buildGameOptionsMenu(UiMenuScreen &m)
{
  m.title = tr("Opciones del juego", "Game options");
  m.subtitle = uiStripExtension(uiFileName(m.arg));
  int rom = romInfo(m.arg);
  bool gb = (rom & UI_ROM_GB) != 0;

  if (!gb) {
    // Game Boy Advance: save chip and clock.
    const char *types[SAVE_TYPE_COUNT] = {
      tr("Autom\xe1tico", "Automatic"), "EEPROM", "SRAM", "Flash 64 KB", "Flash 128 KB",
      tr("Ninguno", "None") };
    (m.add(G_SAVE_TYPE, ITEM_OPTION, tr("Tipo de guardado", "Save type"), types[m_gameCfg.saveType],
          tr("Autom\xe1tico detecta el tipo leyendo la ROM. C\xe1mbialo s\xf3lo si no guarda.",
             "Automatic reads it from the ROM. Change it only if the game won't save."))).choice = m_gameCfg.saveType;
    const char *tri[3] = { tr("Autom\xe1tico", "Automatic"), tr("Activado", "On"), tr("Desactivado", "Off") };
    (m.add(G_RTC, ITEM_OPTION, tr("Reloj (RTC)", "Clock (RTC)"), tri[m_gameCfg.rtc],
          tr("Pok\xe9mon Rub\xed/Zafiro/Esmeralda y sus hacks usan el reloj.",
             "Pokemon Ruby/Sapphire/Emerald and their hacks use the clock."))).choice = m_gameCfg.rtc;
  }
  const char *patch[2] = { tr("No aplicar", "Don't apply"), tr("Aplicar autom\xe1ticamente", "Apply automatically") };
  (m.add(G_PATCH, ITEM_OPTION, tr("Parche .bps/.ups/.ips", "Patch .bps/.ups/.ips"), patch[m_gameCfg.patch],
        tr("Usa el parche con el mismo nombre que la ROM.", "Uses the patch with the same name as the ROM."))).choice = (1 - m_gameCfg.patch);
  if (m_inGame && m_host.romPatched())
    m.add(G_SAVE_PATCHED, ITEM_ACTION, tr("Guardar ROM ya parchada", "Save patched ROM"), "",
          tr("Crea una copia con el parche aplicado junto a la ROM; carga m\xe1s r\xe1pido.",
             "Writes a copy with the patch applied next to the ROM; it loads faster."));

  {
    std::vector<UiCheatInfo> cheats;
    m_host.listCheats(m.arg, cheats);
    int on = 0;
    for (size_t i = 0; i < cheats.size(); i++)
      if (cheats[i].enabled) on++;
    char buf[64];
    sprintf(buf, tr("%d de %d activos", "%d of %d on"), on, (int)cheats.size());
    m.add(G_CHEATS, ITEM_SUBMENU, tr("Trucos y c\xf3""digos", "Cheats and codes"), cheats.empty() ? "" : buf,
          gb ? tr("GameShark y Game Genie de Game Boy / Color. Se guardan para este juego.",
                  "Game Boy / Color GameShark and Game Genie. Kept for this game.")
             : tr("GameShark, Action Replay, CodeBreaker. Se guardan para este juego.",
                  "GameShark, Action Replay, CodeBreaker. Kept for this game."));
  }

  // Picture: the global settings, or this game's own.
  m.addHeader(tr("Imagen de este juego", "This game's picture"));
  const char *custom[2] = { tr("Como en Ajustes", "As in Settings"), tr("Personalizada", "Custom") };
  (m.add(G_CUSTOM, ITEM_OPTION, tr("Configuraci\xf3n", "Settings"), custom[m_gameCfg.custom],
        tr("Personalizada: imagen, colores, mezcla, FPS y modo s\xf3lo para este juego.",
           "Custom: picture, colors, blending, FPS and mode for this game only."))).choice = m_gameCfg.custom;
  if (m_gameCfg.custom) {
    if (rom & UI_ROM_SGB) {
      const char *modes[2] = { tr("Normal", "Normal"), "Super Game Boy" };
      (m.add(G_SGB_MODE, ITEM_OPTION, tr("Modo de juego", "Game mode"), modes[m_gameCfg.sgbMode],
            tr("Super Game Boy: marco y colores del juego. Se aplica al iniciar el juego.",
               "Super Game Boy: the game's border and colors. Applies when the game starts."))).choice = m_gameCfg.sgbMode;
    }
    (m.add(G_FILTER, ITEM_OPTION, tr("Filtro de imagen", "Image filter"), filterName(*this, m_gameCfg.filter))).choice = m_gameCfg.filter;
    (m.add(G_STRETCH, ITEM_OPTION, tr("Tama\xf1o de imagen", "Picture size"), sizeName(*this, m_gameCfg.stretch))).choice = m_gameCfg.stretch;
    (m.add(G_EFFECT, ITEM_OPTION, tr("Efecto de pantalla", "Screen effect"), effectName(*this, m_gameCfg.screenEffect))).choice = m_gameCfg.screenEffect;
    bool sgbOn = (rom & UI_ROM_SGB) && m_gameCfg.sgbMode;
    if (gb && !(rom & UI_ROM_CGB_ONLY) && !sgbOn)
      (m.add(G_GB_COLORS, ITEM_OPTION, tr("Colores (Game Boy)", "Colors (Game Boy)"),
            uiGbPaletteNames[m_gameCfg.gbPalette][m_cfg.language ? 1 : 0])).choice = m_gameCfg.gbPalette;
    if (!gb) {
      const char *cc[3] = { tr("Apagada", "Off"), tr("Suave", "Soft"), tr("Como la pantalla real", "Like the real screen") };
      (m.add(G_GBA_COLOR, ITEM_OPTION, tr("Correcci\xf3n de color (GBA)", "Color correction (GBA)"), cc[m_gameCfg.gbaColor],
             tr("Colores menos chillones, como en la pantalla del GBA.",
                "Less garish colors, like on the GBA screen."))).choice = m_gameCfg.gbaColor;
    }
    const char *bl[3] = { tr("Apagada", "Off"), tr("Mezclar 2 cuadros", "Mix 2 frames"), tr("Estela de LCD", "LCD ghosting") };
    (m.add(G_BLEND, ITEM_OPTION, tr("Mezcla de cuadros", "Frame blending"), bl[m_gameCfg.frameBlend],
           tr("Quita el parpadeo de sprites, como el LCD de la consola.",
              "Removes sprite flicker, like the console's LCD."))).choice = m_gameCfg.frameBlend;
    const char *yn[2] = { tr("No", "No"), tr("S\xed", "Yes") };
    (m.add(G_FPS, ITEM_OPTION, tr("Mostrar FPS", "Show FPS"), yn[m_gameCfg.showFps])).choice = m_gameCfg.showFps;
    const char *skip[5] = { tr("Autom\xe1tico", "Automatic"), "0", "1", "2", "3" };
    (m.add(G_FRAMESKIP, ITEM_OPTION, tr("Salto de cuadros", "Frame skip"), skip[m_gameCfg.frameSkip],
           tr("Autom\xe1tico salta cuadros s\xf3lo si el juego va lento.",
              "Automatic only skips frames when the game runs slow."))).choice = m_gameCfg.frameSkip;
  }

  // Buttons: the ones in Settings, or this game's own.
  m.addHeader(tr("Controles de este juego", "This game's controls"));
  const char *ctl[2] = { tr("Como en Ajustes", "As in Settings"), tr("Personalizados", "Custom") };
  (m.add(G_CONTROLS_MODE, ITEM_OPTION, tr("Botones", "Buttons"), ctl[m_gameCfg.customControls],
         tr("Personalizados: botones s\xf3lo para este juego (empiezan como los de Ajustes).",
            "Custom: buttons for this game only (they start as the ones in Settings)."))).choice = m_gameCfg.customControls;
  if (m_gameCfg.customControls)
    m.add(G_CONTROLS, ITEM_SUBMENU, tr("Configurar botones", "Set up buttons"));


  if (m_inGame)
    m.addInfo(tr("La imagen cambia al momento; el resto al reiniciar el juego.",
                 "The picture changes right away; the rest when the game restarts."));
  else
    m.add(G_PLAY, ITEM_ACTION, tr("Jugar", "Play"));
}

// Code types offered per console.
const int kGbaCheatTypes[] = { CT_AUTO, CT_GBA_GSA12, CT_GBA_GSA3, CT_GBA_CBA, CT_GBA_RAW };
const int kGbCheatTypes[] = { CT_AUTO, CT_GB_GS, CT_GB_GG };

const char *cheatTypeName(const UiApp &app, int type)
{
  switch (type) {
  case CT_GBA_GSA12: return "GameShark / Action Replay v1, v2";
  case CT_GBA_GSA3: return "GameShark v3 / Action Replay v3";
  case CT_GBA_CBA: return "CodeBreaker";
  case CT_GBA_RAW: return app.tr("VBA (direcci\xf3n:valor)", "VBA (address:value)");
  case CT_GB_GS: return "GameShark";
  case CT_GB_GG: return "Game Genie";
  default: return app.tr("Autom\xe1tico", "Automatic");
  }
}

void UiApp::buildCheatsMenu(UiMenuScreen &m)
{
  bool gb = (romInfo(m.arg) & UI_ROM_GB) != 0;
  m.title = tr("Trucos y c\xf3""digos", "Cheats and codes");
  m.subtitle = uiStripExtension(uiFileName(m.arg)) +
               (gb ? "  \xb7  Game Boy / Game Boy Color" : "  \xb7  Game Boy Advance");
  const int *types = gb ? kGbCheatTypes : kGbaCheatTypes;
  int typeCount = gb ? 3 : 5;
  if (m_cheatTypeIdx < 0 || m_cheatTypeIdx >= typeCount)
    m_cheatTypeIdx = 0;
  int type = types[m_cheatTypeIdx];
  std::string example = uiCheatTypeExample(type);
  (m.add(CH_TYPE, ITEM_OPTION, tr("Tipo de c\xf3""digo", "Code type"), cheatTypeName(*this, type),
         gb ? tr("Autom\xe1tico reconoce GameShark (01XXXXXX) y Game Genie (XXX-XXX-XXX).",
                 "Automatic recognises GameShark (01XXXXXX) and Game Genie (XXX-XXX-XXX).")
            : tr("Autom\xe1tico reconoce el c\xf3""digo por c\xf3mo est\xe1 escrito.",
                 "Automatic recognises the code by how it is written."))).choice = m_cheatTypeIdx;
  m.add(CH_ADD, ITEM_ACTION, tr("Agregar c\xf3""digo", "Add code"), "",
        example.empty() ? std::string(tr("Escr\xed""belo con el teclado; luego le pones nombre.",
                                         "Type it with the keyboard, then give it a name."))
                        : std::string(tr("Formato: ", "Format: ")) + example);

  m.add(CH_ONLINE, ITEM_ACTION, tr("Buscar trucos en internet", "Find cheats online"), "",
        tr("Lo m\xe1s nuevo de GitHub (libretro y SuperFW) y la base que viene con el emulador; sin internet, solo esa.",
           "The newest from GitHub (libretro and SuperFW) and the database that comes with the emulator; offline, just that one."));

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
  m_host.listCheats(m.arg, cheats);
  if (cheats.empty()) {
    m.addInfo(tr("No hay trucos para este juego.", "No cheats for this game."));
  } else {
    m.addHeader(tr("TRUCOS DEL JUEGO", "GAME CHEATS"));
    m.hints.push_back(tr("X:Editar", "X:Edit"));
    m.hints.push_back(tr("Y:Borrar", "Y:Delete"));
    for (size_t i = 0; i < cheats.size(); i++) {
      // A code with a value left to the player (?? / XX): asked when turned on.
      const char *state = uiCheatHasBlank(cheats[i].code) ? tr("Elegir valor", "Pick value")
                          : cheats[i].enabled           ? tr("Activado", "On")
                                                        : tr("Desactivado", "Off");
      UiMenuItem &it = m.add(CH_CHEAT_BASE + (int)i, ITEM_OPTION, cheats[i].desc, state);
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
    case Q_RA:
      openMenu(MENU_RA_LIST);
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

  case MENU_RA_LIST:
    return;   // just to read: the description shows below

  case MENU_ABOUT:
    if (id == AB_UPDATE) {
      toast(tr("Preguntando a GitHub...", "Asking GitHub..."), 600);
      m_updateCheck = 4;   // shown first, then the (blocking) request
    }
    return;

  case MENU_SETTINGS_RA:
    if (id == RA_LOGIN) {
      openKeyboard(tr("Usuario de RetroAchievements", "RetroAchievements user"), m_cfg.raUser, KEYBOARD_RA_USER);
    } else if (id == RA_LOGOUT) {
      m_host.raLogout();
      saveAll();
      rebuildMenus();
      toast(tr("Sesi\xf3n cerrada", "Logged out"));
    } else if (id == U_RA_TEST) {
      toast(tr("Conectando por HTTPS...", "Connecting over HTTPS..."), 600);
      m_netTest = 4;   // shown first, then the (blocking) test
    }
    return;

  case MENU_SETTINGS_UI:
    if (id == U_HOME_DIR) {
      m_cfg.lastDir.clear();
      saveAll();
      toast(tr("Listo", "Done"));
    } else if (id == U_RESET_ALL) {
      openConfirm(tr("\xbfRestablecer los ajustes y las opciones de todos los juegos?",
                     "Reset the settings and every game's options?"), CONFIRM_RESET_ALL, "");
    }
    return;

  case MENU_SETTINGS_LOOK:
    if (id == U_COVERS)
      startCoverDownload();
    return;

  case MENU_SETTINGS_SAVES:
    if (id == S_DELETE_STATES)
      openConfirm(tr("\xbf""Borrar los estados guardados de todos los juegos?",
                     "Delete the save states of every game?"), CONFIRM_DELETE_STATES, "");
    return;

  case MENU_CLOCK:
    if (id == K_NOW) {
      m_cfg.clockMode = 0;
      m_cfg.clockOffset = 0;
      saveAll();
      rebuildMenus();
      m_host.applySettings();
    }
    return;

  case MENU_CONTROLS:
    if (id >= SN_MAP_BASE && id < SN_MAP_BASE + SENS_COUNT) {
      startStickCapture(id);
    } else if (id == SN_SENS) {
      onMenuChange(m, id, 1);
    } else if (id == C_RESET) {
      openConfirm(tr("\xbfRestablecer todos los controles?", "Reset all controls?"), CONFIRM_RESET_CONTROLS, "");
    } else if (id >= C_ACTION_BASE) {
      openCapture(id - C_ACTION_BASE);
    }
    return;

  case MENU_GAME_OPTIONS:
    if (id == G_CONTROLS) {
      m_gameCfg.save(gameConfigPath(m_gameCfgRom));
      openMenu(MENU_CONTROLS, m.arg);
    } else if (id == G_CHEATS) {
      m_gameCfg.save(gameConfigPath(m_gameCfgRom));
      m_cheatTypeIdx = 0;
      openMenu(MENU_CHEATS, m.arg);
    } else if (id == G_PLAY) {
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
    m_cheatRom = m.arg;
    if (id == CH_ADD) {
      bool gb = (romInfo(m.arg) & UI_ROM_GB) != 0;
      int type = (gb ? kGbCheatTypes : kGbaCheatTypes)[m_cheatTypeIdx];
      std::string title = tr("Escribe el c\xf3""digo del truco", "Type the cheat code");
      if (*uiCheatTypeExample(type))
        title += std::string(" (") + uiCheatTypeExample(type) + ")";
      openKeyboard(title, "", KEYBOARD_CHEAT);
    } else if (id == CH_TYPE) {
      onMenuChange(m, id, 1);
    } else if (id == CH_ONLINE) {
      // Shown first, then done a few frames later (the download blocks).
      toast(tr("Buscando trucos en internet...", "Looking for cheats online..."), 600);
      m_cheatDownload = 4;
    } else if (id >= CH_IMPORT_BASE && id < CH_IMPORT_BASE + 4) {
      const std::string &path = m.items[m.list.sel].help;
      int n = m_host.importCheatFile(m.arg, path);
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
    else if (id == V_SGB) m_cfg.sgbBorders = wrapIndex(m_cfg.sgbBorders, delta, 3);
    else if (id == V_PRINTER) m_cfg.gbPrinter = wrapIndex(m_cfg.gbPrinter, delta, 2);
    else if (id == V_BORDER_FIT) m_cfg.borderFit = wrapIndex(m_cfg.borderFit, delta, FIT_COUNT);
    else if (id == V_BORDER) {
      std::vector<std::string> images = cachedImages("borders");
      int choice = wrapIndex(borderChoice(m_cfg, images), delta, (int)images.size() + 1);
      m_cfg.border = choice != 0;
      m_cfg.borderImage = choice != 0 ? images[choice - 1] : std::string();
    }
    else if (id == V_FPS) m_cfg.showFps ^= 1;
    else if (id == V_GBA_COLOR) m_cfg.gbaColor = wrapIndex(m_cfg.gbaColor, delta, 3);
    else if (id == V_BLEND) m_cfg.frameBlend = wrapIndex(m_cfg.frameBlend, delta, 3);
    else if (id == V_FRAMESKIP) m_cfg.frameSkip = wrapIndex(m_cfg.frameSkip, delta, 5);
    break;
  case MENU_CLOCK:
    if (id == K_MODE) {
      m_cfg.clockMode ^= 1;
    } else if (id >= K_YEAR && id <= K_MINUTE) {
      time_t base = time(NULL);
      time_t now = base + m_cfg.clockOffset;
      struct tm t = *localtime(&now);
      if (id == K_YEAR) t.tm_year += delta;
      else if (id == K_MONTH) t.tm_mon += delta;
      else if (id == K_DAY) t.tm_mday += delta;
      else if (id == K_HOUR) t.tm_hour += delta;
      else t.tm_min += delta;
      t.tm_isdst = -1;
      time_t nt = mktime(&t);
      struct tm check = *localtime(&nt);
      // The GBA clock stores two-digit years: keep it within 2000-2099.
      if (nt != (time_t)-1 && check.tm_year >= 100 && check.tm_year <= 199)
        m_cfg.clockOffset = (int)(nt - base);
    }
    break;
  case MENU_SETTINGS_AUDIO:
    if (id == A_SOUND) m_cfg.sound ^= 1;
    else if (id == A_MUTE_FF) m_cfg.muteFastForward ^= 1;
    else if (id == A_VOLUME) m_cfg.volume = wrapIndex(m_cfg.volume, delta, uiVolumeLevelCount);
    else if (id == A_LATENCY) m_cfg.audioLatency = wrapIndex(m_cfg.audioLatency, delta, uiAudioLatencyCount);
    else if (id == U_MUSIC) m_cfg.menuMusic = wrapIndex(m_cfg.menuMusic, delta, MUSIC_MODE_COUNT);
    else if (id == U_MUSIC_VOL) m_cfg.musicVolume = wrapIndex(m_cfg.musicVolume, delta, MUSIC_VOLUME_COUNT);
    else if (id == U_MUSIC_SONG) {
      const std::vector<std::string> &songs = cachedImages("music", ".wav");
      int cur = 0;
      for (size_t i = 0; i < songs.size(); i++)
        if (songs[i] == m_cfg.musicSong)
          cur = (int)i + 1;
      cur = wrapIndex(cur, delta, (int)songs.size() + 1);
      m_cfg.musicSong = cur ? songs[cur - 1] : std::string();
    }
    break;
  case MENU_SETTINGS_EMU:
    if (id == E_FF_SPEED) m_cfg.ffSpeed = wrapIndex(m_cfg.ffSpeed, delta, uiFastForwardSpeedCount);
    else if (id == E_FF_MODE) m_cfg.ffToggle ^= 1;
    else if (id == E_BIOS) m_cfg.useBios ^= 1;
    else if (id == E_GSA) m_cfg.gsaV3 ^= 1;
    else if (id == E_REWIND) m_cfg.rewind ^= 1;

    break;
  case MENU_SETTINGS_SAVES:
    if (id == S_SAVE_DIR) m_cfg.saveDirMode ^= 1;
    else if (id == S_STATE_DIR) m_cfg.stateDirMode ^= 1;
    else if (id == S_AUTO_SAVE) m_cfg.autoSaveState ^= 1;
    else if (id == S_AUTO_LOAD) m_cfg.autoLoadState ^= 1;
    else if (id == S_BACKUPS) m_cfg.saveBackups ^= 1;
    break;
  case MENU_SETTINGS_RA:
    if (id == RA_ENABLE) m_cfg.raEnabled ^= 1;   // applySettings() logs in / out
    else if (id == RA_SOUND) m_cfg.raSound ^= 1;
    break;
  case MENU_SETTINGS_UI:
  case MENU_SETTINGS_LOOK:
  case MENU_SETTINGS_KINECT:
    if (id == U_LANGUAGE) m_cfg.language ^= 1;
    else if (id == U_HIDE) m_cfg.hideNonGames ^= 1;
    else if (id == U_THEME) m_cfg.menuTheme = wrapIndex(m_cfg.menuTheme, delta, UI_THEME_COUNT);
    else if (id == U_OPTION_STYLE) m_cfg.optionStyle = wrapIndex(m_cfg.optionStyle, delta, OPT_STYLE_COUNT);
    else if (id == U_PREVIEW) m_cfg.previewMode = wrapIndex(m_cfg.previewMode, delta, PREVIEW_COUNT);
    else if (id == U_START) m_cfg.startScreen = wrapIndex(m_cfg.startScreen, delta, 2);
    else if (id == U_KINECT) m_cfg.kinect = wrapIndex(m_cfg.kinect, delta, 2);   // the rebuild below shows / hides its options
    else if (id == U_KINECT_HANDS) m_cfg.kinectHands = wrapIndex(m_cfg.kinectHands, delta, 2);
    else if (id == U_KINECT_CAMERA) m_cfg.kinectCamera = wrapIndex(m_cfg.kinectCamera, delta, 2);
    else if (id == U_EFFECT) m_cfg.bgEffect = wrapIndex(m_cfg.bgEffect, delta, UI_EFFECT_COUNT);
    else if (id == U_BG_VIS) m_cfg.bgVisibility = wrapIndex(m_cfg.bgVisibility, delta, BG_VIS_COUNT);
    else if (id == U_PANELS) m_cfg.panelOpacity = wrapIndex(m_cfg.panelOpacity, delta, PANEL_OPACITY_COUNT);
    else if (id == U_ACCENT) m_cfg.accentTint = wrapIndex(m_cfg.accentTint, delta, ACCENT_TINT_COUNT);
    else if (id == U_BUBBLES) m_cfg.bubbleAmount = wrapIndex(m_cfg.bubbleAmount, delta, EFFECT_LEVEL_COUNT);
    else if (id == U_WATER) m_cfg.waterStrength = wrapIndex(m_cfg.waterStrength, delta, EFFECT_LEVEL_COUNT);
    else if (id == U_BACKGROUND) {
      std::vector<std::string> images = cachedImages("backgrounds");
      int choice = wrapIndex(menuImageChoice(m_cfg, images), delta, (int)images.size() + 1);
      m_cfg.menuBackground = choice ? images[choice - 1] : std::string();
    }
    break;
  case MENU_CONTROLS:
    if (id >= SN_MAP_BASE && id < SN_MAP_BASE + SENS_COUNT) {
      int &d = m_cfg.sensorMap[id - SN_MAP_BASE];
      d = wrapIndex(d, delta, SDIR_COUNT);
    } else if (id == SN_SENS) {
      m_cfg.sensorSens = wrapIndex(m_cfg.sensorSens, delta, SENSOR_SENS_COUNT);
    } else if (id >= C_ACTION_BASE) {
      int a = id - C_ACTION_BASE;
      int *map = ctlMap();
      // cycle through every button plus "unassigned" (any action, the menu too)
      map[a] = wrapIndex(map[a], delta, PAD_BUTTON_COUNT + 1);
      if (m_ctlForGame && !m_quietChange)
        m_gameCfg.save(gameConfigPath(m_gameCfgRom));
    }
    break;
  case MENU_GAME_OPTIONS:
    if (id == G_SAVE_TYPE) m_gameCfg.saveType = wrapIndex(m_gameCfg.saveType, delta, SAVE_TYPE_COUNT);
    else if (id == G_RTC) m_gameCfg.rtc = wrapIndex(m_gameCfg.rtc, delta, 3);
    else if (id == G_PATCH) m_gameCfg.patch ^= 1;
    else if (id == G_CUSTOM) {
      m_gameCfg.custom ^= 1;
      if (m_gameCfg.custom && !m_quietChange)
        m_gameCfg.copyVideoFrom(m_cfg);   // start from what the game looks like now
    }
    else if (id == G_SGB_MODE) m_gameCfg.sgbMode ^= 1;
    else if (id == G_CONTROLS_MODE) {
      m_gameCfg.customControls ^= 1;
      if (m_gameCfg.customControls && !m_quietChange)
        m_gameCfg.copyControlsFrom(m_cfg);
    }
    else if (id == G_FILTER) m_gameCfg.filter = wrapIndex(m_gameCfg.filter, delta, FILTER_COUNT);
    else if (id == G_STRETCH) m_gameCfg.stretch = wrapIndex(m_gameCfg.stretch, delta, SIZE_COUNT);
    else if (id == G_EFFECT) m_gameCfg.screenEffect = wrapIndex(m_gameCfg.screenEffect, delta, EFFECT_COUNT);
    else if (id == G_GB_COLORS) m_gameCfg.gbPalette = wrapIndex(m_gameCfg.gbPalette, delta, GB_PALETTE_COUNT);
    else if (id == G_GBA_COLOR) m_gameCfg.gbaColor = wrapIndex(m_gameCfg.gbaColor, delta, 3);
    else if (id == G_BLEND) m_gameCfg.frameBlend = wrapIndex(m_gameCfg.frameBlend, delta, 3);
    else if (id == G_FPS) m_gameCfg.showFps ^= 1;
    else if (id == G_FRAMESKIP) m_gameCfg.frameSkip = wrapIndex(m_gameCfg.frameSkip, delta, 5);
    // Saved right away so the running game (if it is this one) can use it.
    if (!m_quietChange)
      m_gameCfg.save(gameConfigPath(m_gameCfgRom));
    settings = m_inGame;
    break;
  case MENU_CHEATS:
    settings = false;
    if (id == CH_TYPE) {
      int count = (romInfo(m.arg) & UI_ROM_GB) ? 3 : 5;
      m_cheatTypeIdx = wrapIndex(m_cheatTypeIdx, delta, count);
    } else if (id >= CH_CHEAT_BASE) {
      std::vector<UiCheatInfo> cheats;
      m_host.listCheats(m.arg, cheats);
      int i = id - CH_CHEAT_BASE;
      if (i < (int)cheats.size() && uiCheatHasBlank(cheats[i].code)) {
        m_cheatRom = m.arg;
        m_cheatEditIndex = i;
        openKeyboard(cheats[i].desc + tr(": valor en hexadecimal (ej. E4)", ": value in hexadecimal (e.g. E4)"), "",
                     KEYBOARD_CHEAT_VALUE);
        return;
      }
      if (i < (int)cheats.size())
        m_host.setCheatEnabled(m.arg, i, !cheats[i].enabled);
    }
    break;
  }
  if (m_quietChange) {
    // Only listing the values: rebuild just this menu.
    int sel = m.selectedId();
    buildMenu(m);
    m.selectId(sel);
    return;
  }
  rebuildMenus();
  if (settings)
    m_host.applySettings();
}

const std::vector<std::string> &UiApp::cachedImages(const char *folder, const char *ext)
{
  // Read once per menu opened: rebuilding a menu after every change listed
  // the folder again (slow on the Xbox hard drive).
  std::map<std::string, std::vector<std::string> >::iterator it = m_imageCache.find(folder);
  if (it != m_imageCache.end())
    return it->second;
  std::vector<std::string> &names = m_imageCache[folder];
  std::vector<UiDirEntry> entries;
  m_host.listDir(uiJoinPath(m_host.emulatorDir(), folder), entries);
  for (size_t i = 0; i < entries.size(); i++)
    if (!entries[i].isDir && (ext ? uiExtension(entries[i].name) == ext : uiIsImageFile(entries[i].name)))
      names.push_back(entries[i].name);
  return names;
}

int UiApp::romInfo(const std::string &rom)
{
  std::map<std::string, int>::iterator it = m_romInfoCache.find(rom);
  if (it != m_romInfoCache.end())
    return it->second;
  int info = m_host.romInfo(rom);
  m_romInfoCache[rom] = info;
  return info;
}

int *UiApp::ctlMap()
{
  return m_ctlForGame ? m_gameCfg.map : m_cfg.map;
}

void UiApp::openPicker(UiMenuScreen &m, int id)
{
  // In Controls, A captures the new button / stick direction (only the
  // tilt sensitivity is a plain list of values).
  if (m.menuId == MENU_CONTROLS && id != SN_SENS) {
    onMenuActivate(m, id);
    return;
  }
  // Cheats switch on / off directly (and listing their values would write
  // the cheat file twice); the date and time fields are not a fixed list.
  if (m_cfg.optionStyle == OPT_STYLE_CLASSIC || (m.menuId == MENU_CLOCK && id != K_MODE) ||
      (m.menuId == MENU_CHEATS && id >= CH_CHEAT_BASE)) {
    onMenuChange(m, id, 1);
    return;
  }
  const UiMenuItem *it = m.findItem(id);
  if (!it)
    return;
  std::string title = it->label;
  std::string start = it->value;
  int choice = it->choice;

  // List the values by stepping through them once, then back to the start.
  std::vector<std::string> values;
  values.push_back(start);
  m_quietChange = true;
  int steps = 0;
  for (; steps < 64; steps++) {
    onMenuChange(m, id, 1);
    const UiMenuItem *now = m.findItem(id);
    if (!now)
      break;
    if (now->value == start) {
      steps = -1;   // back where it started
      break;
    }
    values.push_back(now->value);
  }
  if (steps >= 0)   // didn't come back around: undo the steps taken
    for (int i = 0; i <= steps && i < 64; i++)
      onMenuChange(m, id, -1);
  m_quietChange = false;
  rebuildMenus();
  m.selectId(id);
  if (values.size() < 2)
    return;
  if (steps >= 0) {
    // Too many values to list (or they don't come back around): A steps it.
    onMenuChange(m, id, 1);
    return;
  }
  // values[] starts at the current one; when the option knows its position,
  // show them in their own order with the current one selected.
  int n = (int)values.size();
  int sel = 0;
  if (choice > 0 && choice < n) {
    std::vector<std::string> ordered(n);
    for (int k = 0; k < n; k++)
      ordered[k] = values[(k - choice + n) % n];
    values.swap(ordered);
    sel = choice;
  }
  push(new UiPickerScreen(&m, id, title, values, m_cfg.optionStyle, sel));
}

void UiApp::onMenuSecondary(UiMenuScreen &m, int id, int button)
{
  if (m.menuId == MENU_CONTROLS) {
    if (button == PAD_X && id >= C_ACTION_BASE) {   // any: the menu keeps the right stick click
      ctlMap()[id - C_ACTION_BASE] = PAD_NONE;
      if (m_ctlForGame)
        m_gameCfg.save(gameConfigPath(m_gameCfgRom));
      rebuildMenus();
      m_host.applySettings();
    } else if (button == PAD_X && id >= SN_MAP_BASE && id < SN_MAP_BASE + SENS_COUNT) {
      m_cfg.sensorMap[id - SN_MAP_BASE] = SDIR_NONE;
      rebuildMenus();
      m_host.applySettings();
    } else if (button == PAD_START) {
      openConfirm(tr("\xbfRestablecer todos los controles?", "Reset all controls?"), CONFIRM_RESET_CONTROLS, "");
    }
  } else if (m.menuId == MENU_CHEATS && button == PAD_X && id >= CH_CHEAT_BASE) {
    // Change the code: another value (an item, a score...) or a typo.
    std::vector<UiCheatInfo> cheats;
    m_host.listCheats(m.arg, cheats);
    int i = id - CH_CHEAT_BASE;
    if (i < (int)cheats.size()) {
      m_cheatRom = m.arg;
      m_cheatEditIndex = i;
      openKeyboard(cheats[i].desc + tr(": editar c\xf3""digo", ": edit code"), cheats[i].code, KEYBOARD_CHEAT_EDIT);
    }
  } else if (m.menuId == MENU_CHEATS && button == PAD_Y && id >= CH_CHEAT_BASE) {
    m_cheatRom = m.arg;
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
  if (m.menuId == MENU_CONTROLS && m_ctlForGame) {
    m_gameCfg.save(gameConfigPath(m_gameCfgRom));
    saveAll();   // the sensors shown there are the global ones
  } else if (m.menuId == MENU_GAME_OPTIONS)
    m_gameCfg.save(gameConfigPath(m_gameCfgRom));
  else if (m.menuId != MENU_CHEATS && m.menuId != MENU_ABOUT)
    saveAll();
  if (m.menuId == MENU_CONTROLS)
    m_ctlForGame = false;
  pop();
}

void UiApp::openKeyboard(const std::string &title, const std::string &initial, int purpose)
{
  bool account = purpose == KEYBOARD_RA_USER || purpose == KEYBOARD_RA_PASS;
  bool hex = purpose == KEYBOARD_CHEAT || purpose == KEYBOARD_CHEAT_VALUE || purpose == KEYBOARD_CHEAT_EDIT;
  push(new UiKeyboardScreen(title, initial, purpose, !hex, purpose == KEYBOARD_SEARCH,
                            account, purpose == KEYBOARD_RA_PASS));
}

void UiApp::openSearch()
{
  openKeyboard(tr("Buscar en esta carpeta", "Search this folder"), m_listFilter, KEYBOARD_SEARCH);
}

void UiApp::startCoverDownload()
{
  std::vector<std::string> roms;   // every game of the Library, those without one
  uiLibraryRoms(*this, roms);
  std::string covers = uiJoinPath(m_host.emulatorDir(), "covers");
  std::vector<UiCoverItem> items;
  for (size_t i = 0; i < roms.size(); i++) {
    std::string name = uiStripExtension(uiFileName(roms[i]));
    std::string base = uiStripExtension(roms[i]);
    if ((m_host.fileExists(uiJoinPath(covers, name + ".png")) || m_host.fileExists(uiJoinPath(covers, name + ".jpg")) ||
         m_host.fileExists(base + ".png") || m_host.fileExists(base + ".jpg")))
      continue;   // already has one
    UiCoverItem it;
    it.romName = name;
    it.coverPath = uiJoinPath(covers, name + ".png");
    int info = romInfo(roms[i]);
    std::string ext = uiExtension(roms[i]);
    if ((info & UI_ROM_GB) && (info & UI_ROM_CGB)) {
      it.systems[0] = UI_COVER_GBC; it.systems[1] = UI_COVER_GB; it.systems[2] = -1;
    } else if (info & UI_ROM_GB) {
      it.systems[0] = UI_COVER_GB; it.systems[1] = UI_COVER_GBC; it.systems[2] = -1;
    } else if (ext == ".zip" || ext == ".7z") {
      it.systems[0] = UI_COVER_GBA; it.systems[1] = UI_COVER_GBC; it.systems[2] = UI_COVER_GB;
    } else {
      it.systems[0] = UI_COVER_GBA; it.systems[1] = -1; it.systems[2] = -1;
    }
    items.push_back(it);
  }
  if (items.empty()) {
    toast(tr("Todos los juegos de la Biblioteca ya tienen portada", "Every Library game already has box art"));
    return;
  }
  if (!m_host.startCoverDownload(items, m_cfg.coverServer)) {
    toast(tr("Ya hay una descarga en curso", "A download is already running"));
    return;
  }
  push(new UiCoverScreen(false));
}

bool UiApp::handDwell(int target)
{
  m_handUsed = true;
  if (!m_handOn || target < 0 || target != m_handTarget) {
    m_handTarget = m_handOn ? target : -1;
    m_handFrames = 0;
    return false;
  }
  if (++m_handFrames < HAND_DWELL_FRAMES)
    return false;
  m_handFrames = -HAND_DWELL_FRAMES / 2;   // a pause before it can fire again
  return true;
}

void UiApp::onCoversClosed()
{
  m_covers.clear();
  m_coverOrder.clear();   // show the new covers (and forget "no cover" answers)
}

const UiApp::Cover *UiApp::cover(const std::string &rom, bool allowLoad)
{
  std::map<std::string, Cover>::iterator it = m_covers.find(rom);
  if (it != m_covers.end()) {
    if (it->second.px.empty())
      return 0;
    // Used again: last to be dropped, so the games on screen stay loaded
    // when you go to another console and back.
    std::vector<std::string>::iterator o = std::find(m_coverOrder.begin(), m_coverOrder.end(), rom);
    if (o != m_coverOrder.end() && o + 1 != m_coverOrder.end()) {
      m_coverOrder.erase(o);
      m_coverOrder.push_back(rom);
    }
    return &it->second;
  }
  // One file read per frame, so a shelf full of covers fills in without
  // freezing the menu.
  if (!allowLoad || m_coverLoadsLeft <= 0)
    return 0;
  m_coverLoadsLeft--;
  // Keep the last kCoverCache images (up to ~450 KB each) and the "no cover"
  // answers; the oldest image goes first.
  const size_t kCoverCache = 40;   // ~18 MB: two full grids and the browser
  if (m_covers.size() > 400) {
    m_covers.clear();
    m_coverOrder.clear();
  }
  while (m_coverOrder.size() >= kCoverCache) {
    m_covers.erase(m_coverOrder.front());
    m_coverOrder.erase(m_coverOrder.begin());
  }
  Cover &c = m_covers[rom];
  c.w = c.h = 0;
  std::string name = uiStripExtension(uiFileName(rom));
  std::string base = uiStripExtension(rom);
  std::string covers = uiJoinPath(m_host.emulatorDir(), "covers");
  const std::string candidates[4] = {
    uiJoinPath(covers, name + ".png"), uiJoinPath(covers, name + ".jpg"),
    base + ".png", base + ".jpg" };
  for (int i = 0; i < 4; i++) {
    if (!m_host.fileExists(candidates[i]))
      continue;
    if (m_host.loadImageFit(candidates[i], COVER_MAX_W, COVER_MAX_H, c.px, c.w, c.h) &&
        c.w > 0 && c.h > 0 && c.px.size() == (size_t)(c.w * c.h)) {
      m_coverOrder.push_back(rom);
      return &c;
    }
    c.px.clear();
  }
  return 0;
}

void UiApp::onKeyboardDone(int purpose, const std::string &text, bool accepted)
{
  if (!accepted)
    return;
  if (purpose == KEYBOARD_SEARCH) {
    std::string f = text;
    while (!f.empty() && f[f.size() - 1] == ' ')
      f.erase(f.size() - 1);
    m_listFilter = f;
    return;   // the browser rebuilds its list when it is back on top
  }
  if (purpose == KEYBOARD_RA_USER) {   // then the password
    m_raUserTyped = text;
    while (!m_raUserTyped.empty() && m_raUserTyped[m_raUserTyped.size() - 1] == ' ')
      m_raUserTyped.erase(m_raUserTyped.size() - 1);
    if (!m_raUserTyped.empty())
      openKeyboard(tr("Contrase\xf1""a de RetroAchievements", "RetroAchievements password"), "", KEYBOARD_RA_PASS);
    return;
  }
  if (purpose == KEYBOARD_RA_PASS) {
    if (text.empty())
      return;
    m_host.raLogin(m_raUserTyped, text);   // the password goes to the site and is forgotten here
    m_raWaiting = true;
    toast(tr("Iniciando sesi\xf3n en RetroAchievements...", "Logging in to RetroAchievements..."), 600);
    rebuildMenus();
    return;
  }
  if (purpose == KEYBOARD_CHEAT_VALUE || purpose == KEYBOARD_CHEAT_EDIT) {
    std::vector<UiCheatInfo> cheats;
    m_host.listCheats(m_cheatRom, cheats);
    if (m_cheatEditIndex < 0 || m_cheatEditIndex >= (int)cheats.size())
      return;
    std::string code;
    if (purpose == KEYBOARD_CHEAT_VALUE) {
      std::string v;
      for (size_t i = 0; i < text.size(); i++)
        if (text[i] != ' ')
          v += text[i];
      bool hexOnly = !v.empty() && v.size() <= 8;
      for (size_t i = 0; i < v.size() && hexOnly; i++)
        hexOnly = isxdigit((unsigned char)v[i]) != 0;
      if (!hexOnly) {
        toast(tr("Escribe un valor hexadecimal (0-9, A-F)", "Type a hexadecimal value (0-9, A-F)"));
        return;
      }
      code = uiCheatFillBlank(cheats[m_cheatEditIndex].code, v);
    } else {
      for (size_t i = 0; i < text.size(); i++)   // single spaces, none at the ends
        if (text[i] != ' ' || (!code.empty() && code[code.size() - 1] != ' '))
          code += text[i];
      while (!code.empty() && code[code.size() - 1] == ' ')
        code.erase(code.size() - 1);
    }
    if (!m_host.editCheat(m_cheatRom, m_cheatEditIndex, code))
      toast(tr("C\xf3""digo no v\xe1lido", "Invalid code"));
    else if (uiCheatHasBlank(code))
      toast(tr("Guardado; falta elegir su valor", "Saved; its value is still to pick"));
    else
      toast(tr("Truco actualizado y activado", "Cheat updated and enabled"));
    rebuildMenus();
    return;
  }
  if (purpose == KEYBOARD_CHEAT_DESC) {
    // Second step: the name. The code was checked in the first step.
    std::string desc = text;
    while (!desc.empty() && desc[desc.size() - 1] == ' ')
      desc.erase(desc.size() - 1);
    if (desc.empty())
      desc = m_pendingCheatDesc;
    bool gb = (romInfo(m_cheatRom) & UI_ROM_GB) != 0;
    int type = (gb ? kGbCheatTypes : kGbaCheatTypes)[m_cheatTypeIdx];
    if (m_host.addCheat(m_cheatRom, m_pendingCheatCode, type, desc))
      toast(tr("Truco agregado y activado", "Cheat added and enabled"));
    else
      toast(tr("C\xf3""digo no v\xe1lido", "Invalid code"));
    rebuildMenus();
    return;
  }
  if (purpose != KEYBOARD_CHEAT)
    return;
  std::string code;
  for (size_t i = 0; i < text.size(); i++)
    if (text[i] != ' ' || (!code.empty() && code[code.size() - 1] != ' '))
      code += text[i];
  while (!code.empty() && code[code.size() - 1] == ' ')
    code.erase(code.size() - 1);
  if (code.empty())
    return;
  // Check it against the chosen type and the game's console first.
  bool gb = (romInfo(m_cheatRom) & UI_ROM_GB) != 0;
  int type = (gb ? kGbCheatTypes : kGbaCheatTypes)[m_cheatTypeIdx];
  std::vector<UiParsedCode> parsed;
  if (!uiParseCheatCodesAs(code, !gb, type, parsed)) {
    toast(gb ? tr("C\xf3""digo no v\xe1lido para Game Boy / Color", "Not a valid Game Boy / Color code")
             : tr("C\xf3""digo no v\xe1lido para Game Boy Advance", "Not a valid Game Boy Advance code"));
    return;
  }
  std::vector<UiCheatInfo> cheats;
  m_host.listCheats(m_cheatRom, cheats);
  m_pendingCheatCode = code;
  m_pendingCheatDesc = std::string(tr("C\xf3""digo ", "Code ")) + intStr((int)cheats.size() + 1);
  openKeyboard(tr("Nombre del truco (opcional)", "Cheat name (optional)"), m_pendingCheatDesc,
               KEYBOARD_CHEAT_DESC);
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
    m_host.removeCheat(m_cheatRom, atoi(arg.c_str()));
    rebuildMenus();
    break;
  case CONFIRM_RESET_CONTROLS:
    if (m_ctlForGame) {
      m_gameCfg.copyControlsFrom(m_cfg);   // back to the ones in Settings
      m_gameCfg.save(gameConfigPath(m_gameCfgRom));
    } else {
      m_cfg.resetControls();
    }
    saveAll();
    rebuildMenus();
    m_host.applySettings();
    toast(tr("Controles restablecidos", "Controls reset"));
    break;
  case CONFIRM_RESET_GAME:
    m_host.resetGame();
    request(UI_REQ_RESUME);
    break;
  case CONFIRM_DELETE_STATES: {
    std::vector<std::string> roms = m_recents.items();
    roms.insert(roms.end(), m_favorites.items().begin(), m_favorites.items().end());
    int n = m_host.deleteAllStates(roms);
    m_thumbs.clear();
    m_thumbKeys.clear();
    rebuildMenus();
    toast(std::string(tr("Estados borrados: ", "States deleted: ")) + intStr(n));
    break;
  }
  case CONFIRM_RESET_ALL: {
    int lang = m_cfg.language;
    m_cfg = UiConfig();
    m_cfg.language = lang;   // keep the language the menu is in
    m_gameCfg = UiGameConfig();
    m_host.deleteAllGameConfigs();
    m_covers.clear();
    m_coverOrder.clear();
    saveAll();
    m_host.applySettings();
    rebuildMenus();
    toast(tr("Configuraci\xf3n restablecida", "Settings reset"));
    break;
  }
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
  int *map = ctlMap();
  if (action <= ACT_GBA_RIGHT) {
    for (int a = ACT_GBA_A; a <= ACT_GBA_RIGHT; a++)
      if (a != action && map[a] == button)
        map[a] = map[action];
  }
  map[action] = button;
  if (m_ctlForGame)
    m_gameCfg.save(gameConfigPath(m_gameCfgRom));
  rebuildMenus();
  m_host.applySettings();
  toast(std::string(uiActionName(*this, action)) + ": " + uiButtonName(*this, button));
}
