// The emulator's menu interface: game browser (folders, drives, recents,
// favorites), settings, in-game quick menu, cheats and control remapping.
//
// UiApp is platform independent. Each frame the host calls update() with the
// buttons held, draw() into a 1280x720 canvas, presents it, and checks
// takeRequest() for things only the host can do (launch / resume / exit).
#ifndef UI_APP_H
#define UI_APP_H

#include <string>
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
  MENU_CHEATS, MENU_ABOUT, MENU_SETTINGS_ROOT
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

  UiRequest takeRequest(std::string &romPath);

  // Fast-forward state, shared with the host's hotkey handling.
  bool fastForward;

  void toast(const std::string &msg);
  const char *tr(const char *es, const char *en) const;

  // --- Used by the screens -------------------------------------------------
  UiHost &host() { return m_host; }
  UiConfig &cfg() { return m_cfg; }
  UiPathList &recents() { return m_recents; }
  UiPathList &favorites() { return m_favorites; }

  void push(UiScreen *s);
  void pop();
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
  void onMenuSecondary(UiMenuScreen &menu, int itemId, int button);
  void onMenuClosed(UiMenuScreen &menu);
  void toggleFavorite(const std::string &rom);
  void launch(const std::string &rom);

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
  bool m_hintsAllowed;               // only the top screen draws hints
  UiGameConfig m_gameCfg;            // options of the game being edited
  std::string m_gameCfgRom;
  std::vector<std::string> m_thumbKeys;
  std::vector<UiThumb> m_thumbs;

  UiApp(const UiApp &);
  UiApp &operator=(const UiApp &);
};

#endif // UI_APP_H
