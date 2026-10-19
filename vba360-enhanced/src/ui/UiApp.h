// The emulator's menu interface: game browser (folders, drives, recents,
// favorites), settings, in-game quick menu, cheats and control remapping.
//
// UiApp is platform independent. Each frame the host calls update() with the
// buttons held, draw() into a 1280x720 canvas, presents it, and checks
// takeRequest() for things only the host can do (launch / resume / exit).
#ifndef UI_APP_H
#define UI_APP_H

#include <string>
#include <map>
#include <vector>

#include "UiCanvas.h"
#include "UiConfig.h"
#include "UiHost.h"
#include "UiThumb.h"

class UiScreen;
class UiMenuScreen;

struct UiInputState
{
  unsigned int held;     // PAD_BIT mask
  unsigned int pressed;  // went down this frame
  unsigned int repeat;   // pressed, plus auto-repeat for held directions
};

enum UiRequest
{
  UI_REQ_NONE = 0,
  UI_REQ_LAUNCH,     // romPath = game to start
  UI_REQ_RESUME,     // close the in-game menu
  UI_REQ_EXIT_GAME   // stop emulation, back to the browser
};

// Menu identifiers used by UiApp::onMenu*.
enum UiMenuId
{
  MENU_QUICK = 1, MENU_SETTINGS_VIDEO, MENU_SETTINGS_AUDIO, MENU_SETTINGS_EMU,
  MENU_SETTINGS_SAVES, MENU_SETTINGS_UI, MENU_CONTROLS, MENU_GAME_OPTIONS,
  MENU_CHEATS, MENU_ABOUT, MENU_SETTINGS_ROOT, MENU_CLOCK
};

class UiApp
{
public:
  UiApp(UiHost &host, UiConfig &cfg);
  ~UiApp();

  // Paths of the files the UI persists (inside host.emulatorDir()).
  std::string configPath() const;
  void loadLists();
  void saveAll();

  // selectRom: highlight this game when the browser opens (e.g. after playing).
  void openHome(const std::string &selectRom = "");
  void openInGameMenu();
  bool inGame() const { return m_inGame; }

  void update(unsigned int heldButtons);
  void draw(UiCanvas &canvas);
  // False when nothing on screen can have changed since the last draw (no
  // buttons, no message, nothing blinking): the host then shows the last
  // picture again instead of drawing it all over.
  bool needsRedraw() const { return m_dirtyFrames > 0; }
  // A screen whose content changes on its own (download progress).
  void keepDrawing() { if ((m_frame % 6) == 0 && m_dirtyFrames < 2) m_dirtyFrames = 2; }
  // Draw the next frame too (covers still loading on screen).
  void drawAgain() { if (m_dirtyFrames < 2) m_dirtyFrames = 2; }
  // Box art download (Game options -> Cover): one game or its whole folder.
  void startCoverDownload(const std::string &rom, bool wholeFolder);
  void onCoversClosed();

  UiRequest takeRequest(std::string &romPath);

  // Fast-forward state, shared with the host's hotkey handling.
  bool fastForward;

  void toast(const std::string &msg, int frames = 150);
  const char *tr(const char *es, const char *en) const;

  // --- Used by the screens -------------------------------------------------
  UiHost &host() { return m_host; }
  UiConfig &cfg() { return m_cfg; }
  UiPathList &recents() { return m_recents; }
  UiPathList &favorites() { return m_favorites; }

  void push(UiScreen *s);
  void pop();
  int screenCount() const { return (int)m_stack.size(); }
  void request(UiRequest r, const std::string &rom = "");
  void openMenu(int menuId, const std::string &rom = "");
  void rebuildMenus();
  void openKeyboard(const std::string &title, const std::string &initial, int purpose);
  void onKeyboardDone(int purpose, const std::string &text, bool accepted);
  void openConfirm(const std::string &message, int purpose, const std::string &arg);
  void onConfirm(int purpose, const std::string &arg);
  void openCapture(int action);
  void onCaptured(int action, int button);
  void onMenuActivate(UiMenuScreen &menu, int itemId);
  void onMenuChange(UiMenuScreen &menu, int itemId, int delta);
  // A on an option: its values in a side panel / list (or the next value in
  // the classic style).
  void openPicker(UiMenuScreen &menu, int itemId);
  // Image files in an emulator folder (borders, backgrounds), cached while
  // a menu is open; ROM header info (UI_ROM_*), cached.
  const std::vector<std::string> &cachedImages(const char *folder);
  int romInfo(const std::string &rom);
  // Button map being edited: Settings, or the game's (controls opened from
  // its game options).
  int *ctlMap();
  void onMenuSecondary(UiMenuScreen &menu, int itemId, int button);
  void onMenuClosed(UiMenuScreen &menu);
  void toggleFavorite(const std::string &rom);
  void launch(const std::string &rom);

  // Game list search (Games tab): the text typed, "" = no filter.
  const std::string &listFilter() const { return m_listFilter; }
  void setListFilter(const std::string &f) { m_listFilter = f; }
  // How many entries the current search shows (set by the game list).
  int listMatches() const { return m_listMatches; }
  void setListMatches(int n) { m_listMatches = n; }
  void openSearch();

  // Cover image for a ROM (covers\<name>.png/.jpg or <rom>.png/.jpg next to
  // it), cached; 0 when there is none. With allowLoad false only the cache
  // is used (no disk access while the list is scrolling).
  struct Cover { std::vector<UiColor> px; int w, h; };
  enum { COVER_MAX_W = 328, COVER_MAX_H = 340 };   // the browser's preview area
  const Cover *cover(const std::string &rom, bool allowLoad = true);
  // The cover was already looked for (found or not); false while unknown.
  bool coverChecked(const std::string &rom) const { return m_covers.find(rom) != m_covers.end(); }
  // A button is held right now (the list is being scrolled).
  bool inputHeld() const { return m_prevHeld != 0; }

  // Thumbnails (cached by path).
  const UiThumb *thumb(const std::string &path);
  void invalidateThumb(const std::string &path);
  std::string gameThumbPath(const std::string &rom) const;
  std::string gameConfigPath(const std::string &rom) const;

  // Drawing helpers shared by screens.
  void drawBackground(UiCanvas &c);
  void drawHeader(UiCanvas &c, const char *title, const char *subtitle);
  void drawHints(UiCanvas &c, const char *const *hints, int count);
  void drawGameBackdrop(UiCanvas &c);
  int frameCount() const { return m_frame; }

private:
  void buildMenu(UiMenuScreen &m);
  void buildQuickMenu(UiMenuScreen &m);
  void buildCheatsMenu(UiMenuScreen &m);
  void buildControlsMenu(UiMenuScreen &m);
  void buildGameOptionsMenu(UiMenuScreen &m);
  void clearStack();

  UiHost &m_host;
  UiConfig &m_cfg;
  UiPathList m_recents;
  UiPathList m_favorites;
  std::vector<UiScreen *> m_stack;
  std::vector<UiScreen *> m_trash;   // screens popped during update()
  bool m_inGame;
  unsigned int m_prevHeld;
  int m_repeatTimer;
  int m_frame;
  UiRequest m_request;
  std::string m_requestRom;
  std::string m_toast;
  int m_toastFrames;
  // Sensor being mapped by moving the right stick (item id, 0 = none).
  int m_stickCapture;
  int m_stickCaptureFrames;
  void startStickCapture(int id);
  void updateStickCapture();
  bool m_hintsAllowed;               // only the top screen draws hints
  UiGameConfig m_gameCfg;            // options of the game being edited
  std::string m_gameCfgRom;
  // True while openPicker steps through an option's values to list them:
  // nothing is saved or applied then.
  bool m_quietChange;
  bool m_ctlForGame;
  std::string m_listFilter;
  int m_listMatches;
  std::map<std::string, Cover> m_covers;
  std::vector<std::string> m_coverOrder;   // loaded images, oldest first
  int m_coverLoadsLeft;                    // cover files that may still be read this frame

  // Cheats menu: the game, the code type picked, the code waiting for its name.
  std::string m_cheatRom;
  int m_cheatTypeIdx;
  std::string m_pendingCheatCode, m_pendingCheatDesc;
  int m_dirtyFrames;
  std::map<std::string, std::vector<std::string> > m_imageCache;
  std::map<std::string, int> m_romInfoCache;

  // Menu background image (Settings > Interface), loaded once per name.
  std::string m_bgName;
  std::vector<UiColor> m_bgPixels;
  // The finished background (colors or image + veil), drawn once and then
  // copied every frame.
  std::vector<UiColor> m_bgCache;
  int m_bgCacheTheme;
  std::string m_bgCacheName;

  std::vector<std::string> m_thumbKeys;
  std::vector<UiThumb> m_thumbs;

  UiApp(const UiApp &);
  UiApp &operator=(const UiApp &);
};

#endif // UI_APP_H
