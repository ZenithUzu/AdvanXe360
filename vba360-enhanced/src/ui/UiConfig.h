// Emulator settings, recent/favorite lists and per-game options.
// Stored as plain text files next to the emulator (GAME:\), so they survive
// without a signed-in profile (BadUpdate setups usually have none).
#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <string>
#include <vector>

// Controller buttons, as a bitmask (also used for UI input).
enum UiPadButton
{
  PAD_UP = 0, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
  PAD_A, PAD_B, PAD_X, PAD_Y,
  PAD_LB, PAD_RB, PAD_LT, PAD_RT,
  PAD_START, PAD_BACK, PAD_LS, PAD_RS,
  PAD_BUTTON_COUNT,
  PAD_NONE = PAD_BUTTON_COUNT
};
#define PAD_BIT(b) (1u << (b))

// Remappable actions. The first ten are GBA buttons.
enum UiAction
{
  ACT_GBA_A = 0, ACT_GBA_B, ACT_GBA_L, ACT_GBA_R, ACT_GBA_START, ACT_GBA_SELECT,
  ACT_GBA_UP, ACT_GBA_DOWN, ACT_GBA_LEFT, ACT_GBA_RIGHT,
  ACT_TURBO_A, ACT_TURBO_B,
  ACT_FAST_FORWARD, ACT_MENU, ACT_QUICK_SAVE, ACT_QUICK_LOAD,
  ACT_COUNT
};

// Scale2x, sharp bilinear and Scale3x run on the GPU; LQ2x on the CPU.
enum UiFilter { FILTER_SHARP = 0, FILTER_SMOOTH, FILTER_SCALE2X, FILTER_LQ2X, FILTER_SHARP_SMOOTH, FILTER_SCALE3X, FILTER_COUNT };
// Picture size: console proportions, fill the screen, or whole-number zoom.
enum UiPictureSize { SIZE_ORIGINAL = 0, SIZE_FULL, SIZE_INTEGER, SIZE_WIDE, SIZE_COUNT };
// How the background image is fitted: whole image centered, cover, stretch.
enum UiBorderFit { FIT_CENTER = 0, FIT_FILL, FIT_STRETCH, FIT_COUNT };
// Screen effect drawn over the game picture.
enum UiScreenEffect { EFFECT_NONE = 0, EFFECT_SCANLINES, EFFECT_GRID, EFFECT_DOTS, EFFECT_COUNT };
// Where a cartridge sensor reads from: right stick axis, optionally inverted.
// A stick direction a cartridge sensor movement is mapped to.
enum UiStickDir { SDIR_NONE = 0, SDIR_RS_RIGHT, SDIR_RS_LEFT, SDIR_RS_UP, SDIR_RS_DOWN,
                  SDIR_LS_RIGHT, SDIR_LS_LEFT, SDIR_LS_UP, SDIR_LS_DOWN, SDIR_COUNT };
// Sensor movements that can be mapped (Settings > Controls). Left/right
// tilt also turns WarioWare: Twisted!'s rotation sensor.
enum UiSensorAction { SENS_TILT_RIGHT = 0, SENS_TILT_LEFT, SENS_TILT_UP, SENS_TILT_DOWN,
                      SENS_SUN_UP, SENS_SUN_DOWN, SENS_COUNT };
// How far (0..32767) a stick is pushed in a direction; sticks are raw
// XInput values (y up = +).
int uiStickAmount(int dir, int lx, int ly, int rx, int ry);
inline bool uiStickDirIsLeftStick(int dir) { return dir >= SDIR_LS_RIGHT && dir <= SDIR_LS_DOWN; }
enum { SENSOR_SENS_COUNT = 5, SENSOR_DEADZONE_COUNT = 4 };
extern const int uiSensorSensPercent[SENSOR_SENS_COUNT];     // 50 .. 200
extern const int uiSensorDeadzone[SENSOR_DEADZONE_COUNT];    // raw stick units
// Colors for (original, non-color) Game Boy games.
// The last one, "Automatic", gives each game the colors the Game Boy Color
// picks for it (its table in uiGbPalettes is only the fallback).
enum { GB_PALETTE_AUTO = 40, GB_PALETTE_COUNT = 41 };
extern const char *const uiGbPaletteNames[GB_PALETTE_COUNT][2];   // [i][0] = Spanish, [i][1] = English
// 4 shades (lightest first) per palette, 0xRRGGBB.
extern const unsigned int uiGbPalettes[GB_PALETTE_COUNT][4];
// Background images the emulator can load (D3DX formats).
bool uiIsImageFile(const std::string &name);
enum UiSaveTypeOverride { SAVE_AUTO = 0, SAVE_EEPROM, SAVE_SRAM, SAVE_FLASH64, SAVE_FLASH128, SAVE_NONE, SAVE_TYPE_COUNT };
enum UiTriState { TRI_AUTO = 0, TRI_ON, TRI_OFF };
enum UiDirMode { DIR_NEXT_TO_ROM = 0, DIR_EMULATOR = 1 };
// Changing an option: left / right only, a side panel, or a pop-up list
// (A opens the panel / list; left / right work in all of them).
enum UiListSort { SORT_NAME = 0, SORT_NAME_DESC, SORT_RECENT, SORT_COUNT };
enum UiOptionStyle { OPT_STYLE_PANEL = 0, OPT_STYLE_LIST, OPT_STYLE_CLASSIC, OPT_STYLE_COUNT };
// Game list preview: the box art (or else the last played moment), only the
// box art, or only the last played moment.
enum UiPreviewMode { PREVIEW_COVER_OR_LAST = 0, PREVIEW_COVER, PREVIEW_LAST, PREVIEW_COUNT };
// Game list: box art on a shelf, file names, or a carousel of box art.
// Moving background effect: bits, so "both" is just the two together.
enum UiBgEffect { UI_EFFECT_BUBBLES = 1, UI_EFFECT_WATER = 2, UI_EFFECT_COUNT = 4 };
// Theme look (Interface): how much of the background image shows through,
// how see-through the panels are, and an accent color instead of the theme's.
enum { BG_VIS_COUNT = 4, PANEL_OPACITY_COUNT = 4, ACCENT_TINT_COUNT = 9, EFFECT_LEVEL_COUNT = 3,
       MUSIC_MODE_COUNT = 3, MUSIC_VOLUME_COUNT = 4 };
extern const int uiBgVeilAlpha[BG_VIS_COUNT];          // theme veil over the image
extern const int uiPanelOpacityPercent[PANEL_OPACITY_COUNT];
extern const unsigned int uiAccentTints[ACCENT_TINT_COUNT];   // [0] unused: the theme's own
extern const int uiMusicVolumePercent[MUSIC_VOLUME_COUNT];
enum UiMusicMode { MUSIC_OFF = 0, MUSIC_IN_ORDER, MUSIC_SHUFFLE };
enum UiListView { LIST_VIEW_COVERS = 0, LIST_VIEW_NAMES, LIST_VIEW_CAROUSEL, LIST_VIEW_COUNT };

struct UiConfig
{
  // Interface
  int language;          // 0 = Espanol, 1 = English
  int hideNonGames;      // show only ROMs, archives, patches-free list
  std::string lastDir;   // folder the browser reopens
  int menuTheme;         // index into uiThemes (UiScreens.h)
  int optionStyle;       // UiOptionStyle: how an option's values are picked
  int listSort;          // UiListSort: game list order
  std::string menuBackground;   // image in backgrounds\ ("" = the theme's colors)
  std::string coverServer;      // box art server (plain HTTP), Game options -> Cover
  int previewMode;              // UiPreviewMode: what the game list's preview shows
  int listView;                 // UiListView
  int bgEffect;                 // UiBgEffect bits (0 = none)
  int startScreen;              // 0 = library, 1 = file browser
  int raEnabled;                // RetroAchievements on (softcore)
  std::string raUser, raToken;  // the account, and the token the site gave at login (not the password)
  int raSound;                  // a sound with each achievement
  int kinect;                   // Kinect connected: 0 = never touched, 1 = its features below can be used
  int kinectHands;              // move through the Library with your hand (Kinect)
  int kinectCamera;             // the Game Boy Camera sees through the Kinect
  int bgVisibility;             // index into uiBgVeilAlpha
  int panelOpacity;             // index into uiPanelOpacityPercent
  int accentTint;               // index into uiAccentTints (0 = the theme's)
  int bubbleAmount;             // 0 few, 1 normal, 2 many
  int waterStrength;            // 0 soft, 1 normal, 2 strong
  int menuMusic;                // UiMusicMode: songs from music\ in the menu
  int musicVolume;              // index into uiMusicVolumePercent
  std::string musicSong;        // a .wav in music\ to play on its own ("" = all, by menuMusic)
  std::string libraryDir;       // the library's games folder ("" = lastDir)

  // Video
  int filter;            // UiFilter
  int stretch;           // UiPictureSize
  int border;            // around the picture: 0 = black, 1 = image from the borders folder
  std::string borderImage;  // file name inside borders\ ("" = border.png/.jpg/...)
  int borderFit;         // UiBorderFit
  int screenEffect;      // UiScreenEffect
  int gbPalette;         // index into uiGbPalettes
  int gbPrinter;         // Game Boy Printer on the link port: prints are saved as pictures
  int sgbBorders;        // Super Game Boy: 0 off, 1 Game Boy games, 2 also Color-compatible ones
  int gbaColor;          // GBA color correction: 0 off, 1 soft, 2 like the real screen
  int frameBlend;        // 0 off, 1 mix 2 frames (anti-flicker), 2 LCD ghosting
  int showFps;
  int frameSkip;         // 0 = auto, 1..4 = fixed 0..3

  // Audio
  int sound;
  int muteFastForward;
  int volume;            // index into uiVolumeLevels
  int audioLatency;      // index into uiAudioLatencyMs (sound buffer)

  // Emulation
  int ffSpeed;           // index into uiFastForwardSpeeds
  int ffToggle;          // 0 = hold, 1 = toggle
  int useBios;
  int gsaV3;             // treat 16-digit codes as GameShark v3 / Action Replay
  int sunLevel;          // Boktai solar sensor: 0 = dark ... 10 = full sun

  // Sensors (right stick)
  int sensorMap[SENS_COUNT];   // UiStickDir for every sensor movement
  int sensorSens;        // index into uiSensorSensPercent
  int sensorDeadzone;    // index into uiSensorDeadzone

  // Game clock (GBA real-time clock)
  int clockMode;         // 0 = console time, 1 = custom
  int clockOffset;       // custom: seconds added to the console time

  // Saves
  int saveDirMode;       // UiDirMode for battery saves (.sav)
  int stateDirMode;      // UiDirMode for save states
  int autoSaveState;     // write state slot 0 ("auto") when leaving a game
  int autoLoadState;     // load it when starting the game
  int stateSlot;
  int saveBackups;       // keep the last 3 copies of each .sav (saves\backups)

  // Controls: pad button for every UiAction (PAD_NONE = unassigned)
  int map[ACT_COUNT];

  UiConfig();
  void resetControls();   // buttons and sensors
  bool load(const std::string &path);
  bool save(const std::string &path) const;
};

// Per-game overrides, keyed by ROM file name (not by the cartridge ID, so
// ROM hacks that keep the original game code get their own settings).
struct UiGameConfig
{
  int saveType;   // UiSaveTypeOverride
  int rtc;        // UiTriState
  int patch;      // 1 = auto-apply <rom>.bps/.ups/.ips, 0 = never
  // Picture: 0 = as in Settings, 1 = the values below for this game only.
  int custom;
  int filter, stretch, screenEffect, gbPalette;
  int sgbMode;    // Super Game Boy games: 0 = normal Game Boy, 1 = Super Game Boy
  int gbaColor, frameBlend, showFps, frameSkip;   // as in UiConfig
  // Buttons: 0 = as in Settings > Controls, 1 = map below for this game only.
  int customControls;
  int map[ACT_COUNT];

  UiGameConfig();
  // Starts the custom values from the global settings.
  void copyVideoFrom(const UiConfig &cfg);
  void copyControlsFrom(const UiConfig &cfg);
  bool load(const std::string &path);
  bool save(const std::string &path) const;
};

// Volume in percent, and how much sound is buffered ahead (ms).
extern const int uiVolumeLevels[];
extern const int uiVolumeLevelCount;
enum { VOLUME_DEFAULT = 3 };
extern const int uiAudioLatencyMs[];
extern const int uiAudioLatencyCount;

// Fast-forward multipliers; 0 means "as fast as possible".
extern const int uiFastForwardSpeeds[];
extern const int uiFastForwardSpeedCount;

// Ordered list of paths (most recent first), persisted one per line.
class UiPathList
{
public:
  explicit UiPathList(size_t maxItems) : m_max(maxItems) {}
  bool load(const std::string &path);
  bool save(const std::string &path) const;
  void pushFront(const std::string &item);
  void remove(const std::string &item);
  bool contains(const std::string &item) const;
  const std::vector<std::string> &items() const { return m_items; }

private:
  std::vector<std::string> m_items;
  size_t m_max;
};

// --- Path helpers (Xbox paths use '\' and "drive:" prefixes) -------------
std::string uiJoinPath(const std::string &dir, const std::string &name);
std::string uiParentDir(const std::string &path);   // "" at a drive root
std::string uiFileName(const std::string &path);
std::string uiStripExtension(const std::string &name);
std::string uiExtension(const std::string &name);   // lower case, with dot
// A file name safe for FATX (max 42 chars): long names are shortened and
// suffixed with a hash so different ROMs never collide.
std::string uiSafeFileName(const std::string &base, const std::string &ext);
// Per-game file inside the emulator folder, e.g.
// uiGameFile("game:\\", "states", rom, "-1", ".sgm") -> game:\states\<name>-1.sgm
std::string uiGameFile(const std::string &emuDir, const char *folder,
                       const std::string &rom, const std::string &suffix,
                       const std::string &ext);
bool uiIsRomFile(const std::string &name);
bool uiIsPatchFile(const std::string &name);

#endif // UI_CONFIG_H
