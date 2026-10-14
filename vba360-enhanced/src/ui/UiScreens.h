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

  int m_tab;
  std::string m_dir;                 // "" = drive list
  std::vector<UiDirEntry> m_entries;
  std::vector<Row> m_rows;
  UiListState m_list[TAB_COUNT];
  bool m_dirError;
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
  UiMenuItem() : id(0), kind(ITEM_ACTION), enabled(true) {}
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
  void selectId(int id);

private:
  bool m_overlay;
  void skipUnselectable(int dir);
};

// --- On-screen keyboard for cheat codes ------------------------------------
class UiKeyboardScreen : public UiScreen
{
public:
  UiKeyboardScreen(const std::string &title, const std::string &text, int purpose);
  virtual void update(UiApp &app, const UiInputState &in);
  virtual void draw(UiApp &app, UiCanvas &c);
  virtual bool isOverlay() const { return true; }

private:
  std::string m_title;
  std::string m_text;
  int m_purpose;
  int m_row, m_col;
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

// Names shown for controller buttons / remappable actions.
const char *uiButtonName(UiApp &app, int button);
const char *uiActionName(UiApp &app, int action);

// Theme colors.
extern const UiColor UI_COL_TEXT;
extern const UiColor UI_COL_TEXT_DIM;
extern const UiColor UI_COL_ACCENT;
extern const UiColor UI_COL_PANEL;
extern const UiColor UI_COL_ROW_SEL;

// Layout (1280x720, inside the TV safe area).
enum
{
  UI_W = 1280, UI_H = 720,
  UI_MARGIN_X = 64,
  UI_HEADER_H = 112,
  UI_FOOTER_Y = 652
};

#endif // UI_SCREENS_H
