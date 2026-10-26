// Xbox 360 glue between the menu UI (src/ui), the SDL frontend (sdl/SDL.cpp)
// and the emulator cores.
#ifndef XBOX_HOST_H
#define XBOX_HOST_H

#include <string>

#include "../System.h"
#include "../ui/UiConfig.h"

class UiApp;
class UiCanvas;
typedef unsigned int UiColor;   // as in ui/UiCanvas.h

// Settings shared by the whole program (loaded from GAME:\vba360.ini).
extern UiConfig g_cfg;
extern UiApp *g_ui;

// --- Paths -------------------------------------------------------------------
std::string xboxEmulatorDir();                  // "game:\"
std::string xboxBatteryPath();                  // .sav of the running game
std::string xboxStatePath(int slot);            // slot 0 = automatic state
std::string xboxStateThumbPath(int slot);
std::string xboxCheatListPath();
void xboxCreateFolders();

// --- Game lifecycle (called from gba_main in sdl/SDL.cpp) ---------------------
// romPath is the full path of the ROM being started.
void xboxOnGameLoading(const char *romPath);
// After the ROM is loaded and patched: applies per-game save/RTC options.
void xboxApplyGameOverrides(bool isGba);
// Patch auto-apply allowed for this game?
bool xboxPatchAllowed();
// After CPUInit/CPUReset: cheats, automatic state.
void xboxOnGameStarted();
// Before the core is cleaned up: cheats, thumbnail, automatic state.
void xboxOnGameExiting();
// True if the last gba_main call could not open the ROM.
extern bool xboxGameLoadFailed;
// A soft patch was applied to the running game.
extern bool xboxRomPatched;
// First core message (systemMessage) since the game started loading; tells
// the user why a ROM could not be opened.
extern char xboxLoadMessage[256];

// --- Per frame -------------------------------------------------------------------
// Reads controllers, handles hotkeys and opens the in-game menu when asked.
void xboxPollEvents();
// GBA joypad bits for the core (with remapping and turbo).
u32 xboxReadJoypad();
// Fast forward: true when this frame should not be presented.
bool xboxSkipFrame();
bool xboxFastForwardActive();
// Re-applies video settings (filter, scaling, FPS, frame skip, sound).
void xboxApplySettings();

// --- Controllers -----------------------------------------------------------------
// UiPadButton bit mask of every connected controller (sticks act as D-pad
// only when includeSticks is set).
unsigned int xboxReadPads(bool includeSticks);

// Right stick (raw XInput values), used for the tilt sensor in a few games.
void xboxGetRightStick(int &x, int &y);
// Boktai solar sensor darkness for the sunlight setting (0xE8 dark .. 0x50 sun).
u8 xboxSunDarkness();
// Tilt (centred on 2047) and rotation targets from the right stick.
void xboxMotionTargets(int &tiltX, int &tiltY, int &gyro);
// Cartridge rumble -> controller vibration.
void xboxCartridgeRumble(bool on);

// --- UI ----------------------------------------------------------------------------
class UiHost;
UiHost &xboxHost();
// Canvas the in-game menu draws into (owned by main()).
void xboxSetMenuCanvas(UiCanvas *canvas);
// Core filter matching the "Image filter" setting.
#include "../sdl/filters.h"
Filter xboxSelectedFilter();
// Image filter in effect (FILTER_*): Settings, or the game's own options.
int xboxVideoFilter();
// Game Boy Printer: saves the printed picture to screenshots\ as a .bmp.
void xboxGbPrint(const unsigned char *data, int palette);
bool xboxPointSampler();
// A picture over the game (achievement pop-ups): its pixels (0 = none), then
// where it goes each frame and how many of its columns show (it unfolds).
class UiCanvas;
void xboxOverlaySet(const UiCanvas *canvas);
void xboxOverlayPlace(int x, int y, int shownWidth);
// A short sound in the menus or the game: a 16-bit .wav, or a little chime
// when path is "" or the file can't be read.
void xboxPlaySound(const std::string &path);
// Size of the GBA ROM that was loaded (after patches), for RetroAchievements.
extern int xboxGbaRomSize;   // the game picture needs point sampling (sharp, Scale2x/3x)
// Show FPS (per-game value when the game has a custom picture).
int xboxShowFps();

// --- UI rendering ------------------------------------------------------------------
bool xboxUiRendererInit();
// Background drawn around the game picture: black, plus the image at path
// (NULL / "" = black only), fitted with fit (UiBorderFit).
bool xboxSetBorderImage(const char *path, int fit);
void xboxSetBorderFit(int fit);
// Where the game picture goes on screen (picture size setting) and the
// screen effect; call before SDL_SetVideoMode with the console resolution.
void xboxSetupPicture(int srcWidth, int srcHeight);
// Sets the texture filter the game picture uses (sharp / smooth).
void xboxApplyGameSampler();
// changed = false: show the last uploaded picture again (nothing new drawn).
void xboxUiPresent(const UiCanvas &canvas, bool changed = true);
// Decodes an image (PNG, JPG, BMP, TGA, DDS) scaled to w x h for the menu.
// smooth: box filter (better when shrinking a lot, slower); else linear.
bool xboxLoadImagePixels(const char *path, int w, int h, std::vector<UiColor> &out, bool smooth = false);
// The same keeping the proportions, at most maxW x maxH (covers).
bool xboxLoadImageFit(const char *path, int maxW, int maxH, std::vector<UiColor> &out, int &w, int &h);
// Mixes the new frame in pix with the last one (Settings > Video).
void xboxBlendFrame();
// Copies the game's .sav to saves\backups before the session writes it.
void xboxBackupSave(const char *savPath);

// Implemented in sdl/SDL.cpp
void sdlWriteState(int num);
void sdlReadState(int num);
void sdlInitVideo();
void sdlWriteBattery();
extern char filename[2048];
extern int emulating;

// Implemented in libSDLx360 (SDL_xboxvideo.c)
extern "C" void XBOX_RestoreVideoState(void);

#endif // XBOX_HOST_H
