// Screens of the menu UI (see UiApp.h).
#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <string>
#include <vector>

#include "UiApp.h"

class UiMenuScreen;

class UiScreen
{
public:
  virtual ~UiScreen() {}
  virtual void update(UiApp &app, const UiInputState &in) = 0;
  virtual void draw(UiApp &app, UiCanvas &c) = 0;
  // Overlays are drawn on top of the screen below them.
  virtual bool isOverlay() const { return false; }
  // Called when the screen becomes the top one again.
  virtual void onResume(UiApp &) {}
  // Avoids dynamic_cast (the Xbox build may have RTTI disabled).
  virtual UiMenuScreen *asMenu() { return 0; }
};

// Scrollable list state shared by the screens.
struct UiListState
{
  int sel;
  int scroll;
  UiListState() : sel(0), scroll(0) {}
  // Moves the selection; returns true if it changed.
  bool move(int delta, int count, bool wrap);
  void clamp(int count);
  void ensureVisible(int visibleRows);
};

// --- Home: games / recent / favorites / settings tabs ------------------------
class UiHomeScreen : public UiScreen
{
public:
  enum Tab { TAB_GAMES = 0, TAB_RECENT, TAB_FAVORITES, TAB_SETTINGS, TAB_COUNT };

  UiHomeScreen(UiApp &app, const std::string &selectRom);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual void onResume(UiApp &app);

  void openDir(UiApp &app, const std::string &dir, const std::string &selectName);

private:
  struct Row
  {
    std::string title;
    std::string detail;   // second line / right side
    std::string path;     // rom path or folder path
    bool isDir;
    int menuId;           // settings tab
    unsigned int size;
  };

  void refresh(UiApp &app);
  void buildRows(UiApp &app);
  void activate(UiApp &app);
  void drawList(UiApp &app, UiCanvas &c, int x, int y, int w, int h);
  void drawInfo(UiApp &app, UiCanvas &c, int x, int y, int w, int h);
  int rowHeight() const;
  void jumpLetter(int dir);
  void cycleSort(UiApp &app);

  int m_tab;
  std::string m_dir;                 // "" = drive list
  std::vector<UiDirEntry> m_entries;
  std::vector<Row> m_rows;
  UiListState m_list[TAB_COUNT];
  bool m_dirError;
  std::string m_filter;              // list filter the rows were built with
};

// --- Library: the start screen. The consoles (each as its cartridge), then
// one console's games as box art in a carousel or on shelves. Only ROMs:
// folders and settings live in the browser (BACK). Games are sorted into
// consoles by file type, from the library folder and its subfolders.
class UiLibraryScreen : public UiScreen
{
public:
  UiLibraryScreen(UiApp &app, const std::string &selectRom);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual void onResume(UiApp &app);

private:
  enum { SYSTEMS = 4 };                 // GBA, GBC, GB, zip / 7z
  void scan(UiApp &app);
  void scanDir(UiApp &app, const std::string &dir, int depth, int &budget);
  std::vector<std::string> m_games[SYSTEMS];
  UiListState m_list[SYSTEMS];
  std::vector<int> m_systems;           // consoles with games, in order
  int m_sys;                            // index into m_systems
  bool m_inSystem;                      // showing that console's games
  bool m_rescan;                        // back from the browser: look again
  std::string m_root;                   // folder the games were read from
};

// --- Generic list menu (settings pages, quick menu, cheats, controls) --------
enum UiItemKind
{
  ITEM_ACTION = 0,  // A runs it
  ITEM_OPTION,      // left/right (or A) cycles a value
  ITEM_SUBMENU,     // A opens another menu
  ITEM_INFO,        // plain text, not selectable
  ITEM_HEADER       // section title, not selectable
};

struct UiMenuItem
{
  int id;
  int kind;
  std::string label;
  std::string value;    // shown on the right
  std::string help;     // shown at the bottom when selected
  std::string thumb;    // optional preview image for the side panel
  bool enabled;
  int choice;           // options: position of the current value (-1 = unknown),
                        // so the picker lists the values in their own order
  UiMenuItem() : id(0), kind(ITEM_ACTION), enabled(true), choice(-1) {}
};

class UiMenuScreen : public UiScreen
{
public:
  UiMenuScreen(int menuId, bool overlay);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return m_overlay; }
  virtual void onResume(UiApp &app);
  virtual UiMenuScreen *asMenu() { return this; }

  // Helpers for UiApp when (re)building the item list.
  void clear();
  UiMenuItem &add(int id, int kind, const std::string &label,
                  const std::string &value = "", const std::string &help = "");
  void addHeader(const std::string &label);
  void addInfo(const std::string &label);

  int menuId;
  std::string title;
  std::string subtitle;
  std::string arg;                   // e.g. ROM path for game options
  std::vector<std::string> hints;    // extra "BUTTON:Label" hints
  bool showPreview;                  // side panel with item thumbnails
  std::vector<UiMenuItem> items;
  UiListState list;

  int selectedId() const;
  const UiMenuItem *findItem(int id) const;   // 0 if not in the menu
  void selectId(int id);

private:
  bool m_overlay;
  void skipUnselectable(int dir);
};

// --- On-screen keyboard for cheat codes ------------------------------------
class UiKeyboardScreen : public UiScreen
{
public:
  // full: letters and digits (names, searches); otherwise the code keys
  // (0-9, A-F, ':' and '-').
  // search: the game list filters as you type (the list stays visible above
  // the keyboard), START / the Search key keep it and B puts the old one back.
  UiKeyboardScreen(const std::string &title, const std::string &text, int purpose, bool full = false,
                   bool search = false);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  std::string m_title;
  std::string m_text;
  int m_purpose;
  int m_row, m_col;
  bool m_full;
  bool m_search;
  std::string m_orig;     // search: the filter before the keyboard opened
  int keyRows() const { return m_full ? 4 : 3; }
  void drawSearch(UiApp &app, UiCanvas &c);
  int keyCols() const { return m_full ? 10 : 6; }
  const char *keyLabel(int r, int c) const;
};

// --- Waits for a button press to remap a control -----------------------------
class UiCaptureScreen : public UiScreen
{
public:
  explicit UiCaptureScreen(int action);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  int m_action;
  bool m_released;
  int m_frames;
};

// --- Yes / no question -------------------------------------------------------
class UiConfirmScreen : public UiScreen
{
public:
  UiConfirmScreen(const std::string &message, int purpose, const std::string &arg);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  std::string m_message;
  int m_purpose;
  std::string m_arg;
};

// --- Box art download progress (Game options -> Cover) ------------------------
// Follows the host's background download; B cancels it, A closes it at the end.
class UiCoverScreen : public UiScreen
{
public:
  explicit UiCoverScreen(bool single) : m_single(single), m_closing(false) {}
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  bool m_single;          // one game (different wording at the end)
  bool m_closing;         // B pressed: waiting for the download to stop
  UiCoverStatus m_status;
};

// --- Option picker: the values of one option in a side panel or a pop-up
// list. Moving the selection changes the option right away (so filters and
// backgrounds can be previewed); A or B closes it.
class UiPickerScreen : public UiScreen
{
public:
  UiPickerScreen(UiMenuScreen *menu, int itemId, const std::string &title,
                 const std::vector<std::string> &values, int style, int selected);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  UiMenuScreen *m_menu;
  int m_itemId;
  std::string m_title;
  std::vector<std::string> m_values;
  int m_style;
  int m_orig;            // value selected when the list opened (B goes back to it)
  UiListState m_list;
};

// Names shown for controller buttons / remappable actions.
const char *uiButtonName(UiApp &app, int button);
const char *uiActionName(UiApp &app, int action);

// Theme colors (the text colors are the same in every theme).
extern const UiColor UI_COL_TEXT;
extern const UiColor UI_COL_TEXT_DIM;
extern UiColor UI_COL_ACCENT;
extern UiColor UI_COL_PANEL;       // opaque; drawn with the alpha each panel needs
extern UiColor UI_COL_ROW_SEL;
extern UiColor UI_COL_BG_TOP;
extern UiColor UI_COL_BG_BOTTOM;

struct UiTheme
{
  const char *name[2];   // Spanish, English
  UiColor accent, bgTop, bgBottom, panel;
  int style;             // UiThemeStyle: how selections, panels and the background are drawn
};
enum { UI_THEME_COUNT = 7 };
extern const UiTheme uiThemes[UI_THEME_COUNT];
void uiApplyTheme(int index);

// Flat: plain tinted shapes. Aero (Frutiger Aero): glossy, glassy pills and
// panels over a sky background with bubbles.
enum UiThemeStyle { THEME_FLAT = 0, THEME_AERO = 1 };
extern int UI_THEME_STYLE;   // set by uiApplyTheme()
// Selected row of a list (with the accent bar in flat themes).
void uiDrawRowSelection(UiCanvas &c, int x, int y, int w, int h);
// Selected button / tab (strong) or a highlighted area.
void uiDrawSelection(UiCanvas &c, int x, int y, int w, int h, int radius, bool strong);
// Panel / dialog box in the theme's panel color.
void uiDrawPanel(UiCanvas &c, int x, int y, int w, int h, int radius, int alpha);
// Theme decorations over the background gradient (bubbles, glow...).
// Moving effects over the menu background (Interface -> Background effect),
// UI_EFFECT_* bits; frame drives the motion. Frutiger Aero theme only.
void uiDrawEffects(UiCanvas &c, int effects, int frame);

// Layout (1280x720, inside the TV safe area).
enum
{
  UI_W = 1280, UI_H = 720,
  UI_MARGIN_X = 64,
  UI_HEADER_H = 112,
  UI_FOOTER_Y = 652
};

#endif // UI_SCREENS_H
