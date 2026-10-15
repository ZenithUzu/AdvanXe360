// Xbox 360 implementation of the menu UI host, controller input, per-game
// files and the in-game menu loop.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include "XboxHost.h"

#include "../ui/UiApp.h"
#include "../ui/UiCanvas.h"
#include "../ui/UiCheatParse.h"
#include "../ui/UiHost.h"
#include "../ui/UiThumb.h"

#include "../System.h"
#include "../gba/GBA.h"
#include "../gba/Cheats.h"
#include "../gba/Flash.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gb/gb.h"
#include "../gb/gbCheats.h"
#include "../sdl/filters.h"

// --- Things owned by other modules -------------------------------------------
extern u8 *pix;
extern int srcPitch, srcWidth, srcHeight;
extern int cartridgeType;              // 0 = GBA, 1 = GB
extern struct EmulatedSystem emulator;
extern int cpuSaveType;
extern bool useBios;
extern char biosFileName[2048];
extern int autoFrameSkip;
extern int systemFrameSkip;
extern int frameSkip;
extern int gbFrameSkip;
extern int showSpeed;
extern Filter filter;
extern FilterFunc filterFunction;
extern int systemColorDepth;
extern IDirect3DDevice9 *pDevice;
extern gbCheat gbCheatList[100];
extern int gbCheatNumber;
extern bool skipBios;                  // gba/Globals.cpp
extern char gbBiosFileName[2048];      // sdl/SDL.cpp
extern const char *gbPendingPatch;     // gb/GB.cpp
extern bool soundMuted;                // common/SoundSDL.cpp
extern bool soundNoThrottle;
FilterFunc initFilter(const Filter f, const int colorDepth, const int srcWidth);
extern bool wasPaused;                 // sdl/SDL.cpp: skip one frame-skip measurement
extern u32 autoFrameSkipLastTime;
extern u32 throttleLastTime;
extern u8 *rom;
extern u16 systemGbPalette[24];         // sdl/SDL.cpp
extern u16 gbPalette[128];              // gb/gbGlobals.cpp
extern int gbPaletteOption;
extern int gbCgbMode;
extern int gbSgbMode;
extern int CPUGetRomSize();             // gba/GBA.cpp
extern u8 *gbRom;                      // gb/gbGlobals.cpp
extern int gbRomSize;
u32 systemGetClock();
void systemScreenMessage(const char *msg);

UiConfig g_cfg;
UiApp *g_ui = 0;
bool xboxGameLoadFailed = false;
bool xboxRomPatched = false;
char xboxLoadMessage[256] = "";

namespace {

std::string g_rom;            // full path of the running game
UiGameConfig g_gameCfg;
bool g_isGba = true;
int g_cartHw = 0;              // UI_HW_* of the running GBA cartridge
bool g_rumbleOn = false;
unsigned int g_prevPads = 0;
bool g_waitRelease = false;   // ignore the pad until everything is released
bool g_ffHeld = false;
int g_ffCounter = 0;
short g_rightX = 0, g_rightY = 0;
short g_leftX = 0, g_leftY = 0;
Filter g_videoFilter = kStretch2x;
int g_videoStretch = -1;
UiCanvas *g_menuCanvas = 0;

bool pathExists(const std::string &path)
{
  return GetFileAttributes(path.c_str()) != (DWORD)-1;
}

void ensureDir(const std::string &path)
{
  std::string p = path;
  while (!p.empty() && (p[p.size() - 1] == '\\' || p[p.size() - 1] == '/'))
    p.erase(p.size() - 1);
  if (!p.empty() && !pathExists(p))
    CreateDirectory(p.c_str(), NULL);
}

std::string romDirOf(const std::string &rom)
{
  return uiParentDir(rom);
}

// --- Cheats ------------------------------------------------------------------
// The core keeps one entry per code line; the UI shows one item per cheat,
// so consecutive entries with the same description are grouped.
struct CheatGroup
{
  int first, count;
};

int coreCheatCount()
{
  return cartridgeType == 1 ? gbCheatNumber : cheatsNumber;
}

std::string coreCheatDesc(int i)
{
  return cartridgeType == 1 ? gbCheatList[i].cheatDesc : cheatsList[i].desc;
}

bool coreCheatEnabled(int i)
{
  return cartridgeType == 1 ? gbCheatList[i].enabled : cheatsList[i].enabled;
}

void cheatGroups(std::vector<CheatGroup> &out)
{
  out.clear();
  int n = coreCheatCount();
  for (int i = 0; i < n; i++) {
    if (!out.empty() && coreCheatDesc(i) == coreCheatDesc(out.back().first)) {
      out.back().count++;
    } else {
      CheatGroup g;
      g.first = i;
      g.count = 1;
      out.push_back(g);
    }
  }
}

// The core copies descriptions into 32-byte buffers without a length check.
std::string safeDesc(const std::string &d)
{
  return d.size() > 31 ? d.substr(0, 31) : d;
}

bool addParsedCode(const UiParsedCode &c, const std::string &desc)
{
  int before = coreCheatCount();
  std::string d = safeDesc(desc);
  switch (c.kind) {
  case CODE_GBA_RAW: cheatsAddCheatCode(c.code.c_str(), d.c_str()); break;
  case CODE_GBA_GSA: cheatsAddGSACode(c.code.c_str(), d.c_str(), g_cfg.gsaV3 != 0); break;
  case CODE_GBA_CBA: cheatsAddCBACode(c.code.c_str(), d.c_str()); break;
  case CODE_GB_GS: gbAddGsCheat(c.code.c_str(), d.c_str()); break;
  case CODE_GB_GG: gbAddGgCheat(c.code.c_str(), d.c_str()); break;
  }
  return coreCheatCount() > before;
}

void setCoreCheat(int i, bool on)
{
  if (cartridgeType == 1) {
    if (on) gbCheatEnable(i); else gbCheatDisable(i);
  } else {
    if (on) cheatsEnable(i); else cheatsDisable(i);
  }
}

void saveCheats()
{
  if (g_rom.empty())
    return;
  std::string path = xboxCheatListPath();
  if (coreCheatCount() == 0) {
    DeleteFile(path.c_str());
    return;
  }
  if (cartridgeType == 1)
    gbCheatsSaveCheatList(path.c_str());
  else
    cheatsSaveCheatList(path.c_str());
}

// --- Video -----------------------------------------------------------------------

Filter coreFilterFor(int uiFilter)
{
  switch (uiFilter) {
  case FILTER_SCALE2X: return kAdMame2x;
  case FILTER_LQ2X: return klq2x;
  default: return kStretch2x;
  }
}

void applySampler()
{
  D3DTEXTUREFILTERTYPE f = g_cfg.filter == FILTER_SHARP ? D3DTEXF_POINT : D3DTEXF_LINEAR;
  pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, f);
  pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, f);
  // Clamp: with the default wrap mode the smooth filter blends the
  // opposite edge of the picture in (stray lines / letters at the sides).
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

void applySoundFlags()
{
  bool ff = xboxFastForwardActive();
  soundMuted = !g_cfg.sound || (ff && g_cfg.muteFastForward);
  soundNoThrottle = ff;
}

// Writes the current frame as a thumbnail (game preview / save state).
void saveFrameThumb(const std::string &path)
{
  if (!pix || srcWidth <= 0 || systemColorDepth != 32)
    return;
  uiSaveThumb(path, (const UiColor *)(pix + srcPitch), srcWidth, srcHeight, srcPitch / 4);
}

// --- The UiHost implementation ------------------------------------------------

class XboxHost : public UiHost
{
public:
  virtual void listDrives(std::vector<UiDirEntry> &out)
  {
    struct Drive { const char *root, *es, *en; };
    static const Drive drives[] = {
      { "game:\\", "Carpeta del emulador", "Emulator folder" },
      { "hdd:\\", "Disco duro", "Hard drive" },
      { "usb0:\\", "USB 1", "USB 1" },
      { "usb1:\\", "USB 2", "USB 2" },
      { "usb2:\\", "USB 3", "USB 3" },
      { "memunit0:\\", "Unidad de memoria 1", "Memory unit 1" },
      { "memunit1:\\", "Unidad de memoria 2", "Memory unit 2" },
      { "dvd:\\", "DVD", "DVD" }
    };
    for (size_t i = 0; i < sizeof(drives) / sizeof(drives[0]); i++) {
      std::string pattern = std::string(drives[i].root) + "*";
      WIN32_FIND_DATA fd;
      HANDLE h = FindFirstFile(pattern.c_str(), &fd);
      bool present = h != INVALID_HANDLE_VALUE || GetLastError() == ERROR_FILE_NOT_FOUND;
      if (h != INVALID_HANDLE_VALUE)
        FindClose(h);
      if (!present)
        continue;
      UiDirEntry e;
      std::string root = drives[i].root;
      e.name = std::string(g_cfg.language ? drives[i].en : drives[i].es) + "  (" +
               root.substr(0, root.size() - 1) + ")";
      e.path = root;
      e.isDir = true;
      e.size = 0;
      out.push_back(e);
    }
  }

  static bool byName(const UiDirEntry &a, const UiDirEntry &b)
  {
    return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
  }

  virtual bool listDir(const std::string &path, std::vector<UiDirEntry> &out)
  {
    std::vector<UiDirEntry> dirs, files;
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(uiJoinPath(path, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
      return GetLastError() == ERROR_FILE_NOT_FOUND;   // empty folder
    do {
      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
        continue;
      UiDirEntry e;
      e.name = fd.cFileName;
      e.isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      e.path = uiJoinPath(path, e.name) + (e.isDir ? "\\" : "");
      e.size = e.isDir ? 0 : fd.nFileSizeLow;
      (e.isDir ? dirs : files).push_back(e);
    } while (FindNextFile(h, &fd));
    FindClose(h);
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    out.insert(out.end(), dirs.begin(), dirs.end());
    out.insert(out.end(), files.begin(), files.end());
    return true;
  }

  virtual bool fileExists(const std::string &path) { return pathExists(path); }
  virtual std::string emulatorDir() { return xboxEmulatorDir(); }
  virtual std::string currentRom() { return g_rom; }

  virtual bool saveState(int slot)
  {
    std::string path = xboxStatePath(slot);
    DeleteFile(path.c_str());
    sdlWriteState(slot);
    if (!pathExists(path))
      return false;
    saveFrameThumb(xboxStateThumbPath(slot));
    return true;
  }

  virtual bool loadState(int slot)
  {
    if (!pathExists(xboxStatePath(slot)))
      return false;
    sdlReadState(slot);
    return true;
  }

  virtual bool stateExists(int slot) { return pathExists(xboxStatePath(slot)); }
  virtual std::string stateThumbPath(int slot) { return xboxStateThumbPath(slot); }

  virtual bool takeScreenshot(std::string &savedTo)
  {
    for (int n = 1; n < 1000; n++) {
      char suffix[16];
      sprintf(suffix, "-%03d", n);
      std::string path = uiGameFile(xboxEmulatorDir(), "screenshots", g_rom, suffix, ".png");
      if (pathExists(path))
        continue;
      emulator.emuWritePNG(path.c_str());
      savedTo = path;
      return pathExists(path);
    }
    return false;
  }

  virtual void sticks(int &lx, int &ly, int &rx, int &ry)
  {
    lx = g_leftX;
    ly = g_leftY;
    rx = g_rightX;
    ry = g_rightY;
  }

  virtual int cartHardware()
  {
    return g_rom.empty() || !g_isGba ? 0 : g_cartHw;
  }

  virtual bool romPatched()
  {
    return xboxRomPatched && !g_rom.empty();
  }

  virtual bool savePatchedRom(std::string &savedTo)
  {
    if (!romPatched())
      return false;
    const u8 *data = cartridgeType == 1 ? gbRom : rom;
    int size = cartridgeType == 1 ? gbRomSize : CPUGetRomSize();
    if (!data || size <= 0)
      return false;
    std::string ext = ".gba";
    if (cartridgeType == 1)
      ext = (gbRom[0x143] & 0x80) ? ".gbc" : ".gb";
    std::string name = uiSafeFileName(uiStripExtension(uiFileName(g_rom)) + " (patched)", ext);
    std::string path = uiJoinPath(uiParentDir(g_rom), name);
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
      return false;
    bool ok = fwrite(data, 1, size, f) == (size_t)size;
    if (fclose(f) != 0)
      ok = false;
    if (!ok)
      DeleteFile(path.c_str());
    savedTo = path;
    return ok;
  }

  virtual void resetGame()
  {
    emulator.emuReset();
  }

  virtual const UiColor *gameFrame(int &w, int &h, int &pitch)
  {
    if (g_rom.empty() || !pix || systemColorDepth != 32 || srcWidth <= 0)
      return 0;
    w = srcWidth;
    h = srcHeight;
    pitch = srcPitch / 4;
    return (const UiColor *)(pix + srcPitch);
  }

  virtual void listCheats(std::vector<UiCheatInfo> &out)
  {
    out.clear();
    if (g_rom.empty())
      return;
    std::vector<CheatGroup> groups;
    cheatGroups(groups);
    for (size_t i = 0; i < groups.size(); i++) {
      UiCheatInfo c;
      int f = groups[i].first;
      c.desc = coreCheatDesc(f);
      c.enabled = coreCheatEnabled(f);
      for (int k = 0; k < groups[i].count; k++) {
        if (k) c.code += "  ";
        c.code += cartridgeType == 1 ? gbCheatList[f + k].cheatCode : cheatsList[f + k].codestring;
      }
      out.push_back(c);
    }
  }

  virtual void setCheatEnabled(int index, bool enabled)
  {
    std::vector<CheatGroup> groups;
    cheatGroups(groups);
    if (index < 0 || index >= (int)groups.size())
      return;
    for (int k = 0; k < groups[index].count; k++)
      setCoreCheat(groups[index].first + k, enabled);
    saveCheats();
  }

  virtual bool addCheat(const std::string &code, const std::string &desc)
  {
    std::vector<UiParsedCode> codes;
    if (!uiParseCheatCodes(code, g_isGba, codes))
      return false;
    bool any = false;
    for (size_t i = 0; i < codes.size(); i++)
      any = addParsedCode(codes[i], desc) || any;
    saveCheats();
    return any;
  }

  virtual void removeCheat(int index)
  {
    std::vector<CheatGroup> groups;
    cheatGroups(groups);
    if (index < 0 || index >= (int)groups.size())
      return;
    // Remove from the end so the indices stay valid.
    for (int k = groups[index].count - 1; k >= 0; k--) {
      int i = groups[index].first + k;
      if (cartridgeType == 1)
        gbCheatRemove(i);
      else
        cheatsDelete(i, true);
    }
    saveCheats();
  }

  virtual int importCheatFile(const std::string &path)
  {
    std::vector<UiCheatEntry> entries;
    if (!uiParseCheatFile(path, g_isGba, entries))
      return 0;
    int added = 0;
    for (size_t i = 0; i < entries.size(); i++) {
      std::string desc = entries[i].desc;
      if (desc.empty()) {
        char b[32];
        sprintf(b, g_cfg.language ? "Cheat %d" : "Truco %d", (int)i + 1);
        desc = b;
      }
      int first = coreCheatCount();
      bool any = false;
      for (size_t k = 0; k < entries[i].codes.size(); k++)
        any = addParsedCode(entries[i].codes[k], desc) || any;
      if (any) {
        added++;
        // Imported cheats start off unless the file says otherwise:
        // turning dozens of codes on at once tends to crash games.
        for (int c = first; c < coreCheatCount(); c++)
          setCoreCheat(c, entries[i].enabled);
      }
    }
    saveCheats();
    return added;
  }

  virtual void quitToDashboard()
  {
    XLaunchNewImage("", 0);
  }

  virtual void applySettings()
  {
    applySoundFlags();
  }
};

XboxHost g_host;

void resetFrameTiming();

// Frames left before the frame-skip timing is measured afresh after a game
// starts: setting up the video (filter textures, shaders) makes the first
// frames slow, and auto frame skip would keep skipping as if the game were.
static int g_settleFrames = 0;

// Opens the in-game menu and runs it until the player resumes or quits.
void runInGameMenu()
{
  if (!g_ui || !g_menuCanvas)
    return;
  soundPause();
  xboxCartridgeRumble(false);
  // Write the save now too, so quitting from the dashboard loses nothing.
  sdlWriteBattery();
  g_ui->openInGameMenu();
  for (;;) {
    g_ui->update(xboxReadPads(true));
    g_ui->draw(*g_menuCanvas);
    xboxUiPresent(*g_menuCanvas);
    std::string rom;
    UiRequest r = g_ui->takeRequest(rom);
    if (r == UI_REQ_RESUME)
      break;
    if (r == UI_REQ_EXIT_GAME) {
      emulating = 0;
      break;
    }
  }
  g_ui->saveAll();
  g_waitRelease = true;
  g_prevPads = 0;
  xboxApplySettings();
  resetFrameTiming();
  if (emulating)
    soundResume();
}

// Time spent outside emulation (menu, file I/O, video rebuild) must not
// count as slow emulation, or auto frame skip jumps to 9 and only slowly
// comes back down (low FPS for a while after closing the menu).
void resetFrameTiming()
{
  wasPaused = true;
  autoFrameSkipLastTime = throttleLastTime = systemGetClock();
  if (g_cfg.frameSkip == 0)
    systemFrameSkip = 0;
}

bool padDown(unsigned int pads, int action)
{
  int b = g_cfg.map[action];
  return b >= 0 && b < PAD_BUTTON_COUNT && (pads & PAD_BIT(b)) != 0;
}

} // namespace

UiHost &xboxHost()
{
  return g_host;
}

void xboxSetMenuCanvas(UiCanvas *canvas)
{
  g_menuCanvas = canvas;
}

// --- Paths -------------------------------------------------------------------------

std::string xboxEmulatorDir()
{
  return "game:\\";
}

std::string xboxBatteryPath()
{
  if (g_rom.empty())
    return "";   // no game running
  if (g_cfg.saveDirMode == DIR_EMULATOR)
    return uiGameFile(xboxEmulatorDir(), "saves", g_rom, "", ".sav");
  // Classic VBA behaviour: "<rom name>.sav" next to the ROM.
  return uiStripExtension(g_rom) + ".sav";
}

std::string xboxStatePath(int slot)
{
  char suffix[16];
  if (slot == 0)
    strcpy(suffix, "-auto");
  else
    sprintf(suffix, "-%d", slot);
  if (g_cfg.stateDirMode == DIR_EMULATOR)
    return uiGameFile(xboxEmulatorDir(), "states", g_rom, suffix, ".sgm");
  std::string base = uiStripExtension(uiFileName(g_rom)) + suffix;
  return uiJoinPath(romDirOf(g_rom), uiSafeFileName(base, ".sgm"));
}

std::string xboxStateThumbPath(int slot)
{
  return uiStripExtension(xboxStatePath(slot)) + ".tmb";
}

std::string xboxCheatListPath()
{
  return uiGameFile(xboxEmulatorDir(), "cheats", g_rom, "", ".clt");
}

void xboxCreateFolders()
{
  static const char *dirs[] = { "saves", "states", "cheats", "screenshots", "thumbs", "config", "bios", "borders" };
  for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    ensureDir(uiJoinPath(xboxEmulatorDir(), dirs[i]));
}

// --- Game lifecycle ------------------------------------------------------------------

void xboxOnGameLoading(const char *romPath)
{
  g_rom = romPath ? romPath : "";
  g_gameCfg = UiGameConfig();
  g_gameCfg.load(uiGameFile(xboxEmulatorDir(), "config", g_rom, "", ".ini"));
  g_ffHeld = false;
  if (g_ui)
    g_ui->fastForward = false;
  xboxGameLoadFailed = false;
  xboxRomPatched = false;
  xboxLoadMessage[0] = 0;

  // Optional real BIOS (needed by a few games, nicer boot logo).
  std::string bios = uiJoinPath(uiJoinPath(xboxEmulatorDir(), "bios"), "gba_bios.bin");
  useBios = g_cfg.useBios && pathExists(bios);
  strcpy(biosFileName, useBios ? bios.c_str() : "");
  // Game Boy boot ROM (optional, only used by GB/SGB games).
  std::string gbBios = uiJoinPath(uiJoinPath(xboxEmulatorDir(), "bios"), "gb_bios.bin");
  strcpy(gbBiosFileName, g_cfg.useBios && pathExists(gbBios) ? gbBios.c_str() : "");

  // Game Boy patches are applied inside gbLoadRom().
  gbPendingPatch = 0;
  static std::string gbPatch;
  if (g_gameCfg.patch) {
    static const char *exts[] = { ".bps", ".ups", ".ips" };
    for (int i = 0; i < 3; i++) {
      std::string p = uiStripExtension(g_rom) + exts[i];
      if (pathExists(p)) {
        gbPatch = p;
        gbPendingPatch = gbPatch.c_str();
        break;
      }
    }
  }
}

bool xboxPatchAllowed()
{
  return g_gameCfg.patch != 0;
}

// Cartridges with extra hardware the ROM scan can't detect (what VBA's
// vba-over.ini used to provide). Matched on the first 3 letters of the game
// code so every region (and hacks that keep the code) is covered.
struct SpecialCart
{
  const char *code3;
  int saveType;   // -1 = keep detected, 4 = EEPROM + tilt sensor
  int rtc;        // -1 = keep detected, 1 = force the clock on
  int hw;         // UI_HW_* extra hardware on the cartridge
};

const SpecialCart kSpecialCarts[] = {
  { "U3I", -1,  1, UI_HW_SOLAR },                // Boktai: The Sun Is in Your Hand
  { "U32", -1,  1, UI_HW_SOLAR },                // Boktai 2: Solar Boy Django
  { "U33", -1,  1, UI_HW_SOLAR },                // Shin Bokura no Taiyou
  { "RZW", -1, -1, UI_HW_GYRO | UI_HW_RUMBLE },  // WarioWare: Twisted!
  { "V49", -1, -1, UI_HW_RUMBLE },               // Drill Dozer
  { "BR4", -1,  1, 0 },                          // Rockman EXE 4.5 Real Operation
  { "BKA", -1,  1, 0 },                          // Sennen Kazoku
  { "KYG",  4, -1, UI_HW_TILT },                 // Yoshi Topsy-Turvy (tilt)
  { "KHP",  4, -1, UI_HW_TILT },                 // Koro Koro Puzzle Happy Panechu! (tilt)
};

static void applySpecialCart()
{
  g_cartHw = 0;
  rtcEnableSolar(false);
  rtcEnableGyro(false);
  rtcEnableRumble(false);
  if (!rom)
    return;
  char code[4] = { (char)rom[0xAC], (char)rom[0xAD], (char)rom[0xAE], 0 };
  for (size_t i = 0; i < sizeof(kSpecialCarts) / sizeof(kSpecialCarts[0]); i++) {
    if (strncmp(code, kSpecialCarts[i].code3, 3) != 0)
      continue;
    if (kSpecialCarts[i].saveType >= 0)
      cpuSaveType = kSpecialCarts[i].saveType;
    if (kSpecialCarts[i].rtc == 1)
      rtcEnable(true);
    g_cartHw = kSpecialCarts[i].hw;
    rtcEnableSolar((g_cartHw & UI_HW_SOLAR) != 0);
    rtcEnableGyro((g_cartHw & UI_HW_GYRO) != 0);
    rtcEnableRumble((g_cartHw & UI_HW_RUMBLE) != 0);
    return;
  }
}

// Boktai solar sensor: light level (0..10) as the darkness value the
// cartridge compares against (lower = more sun). Measured on the console:
// 0x50 shows 6 of Boktai's 8 bars and 0x18 shows 7, so the scale now ends
// at 0x00 (the sensor's maximum) for a full gauge at 10, with 0x50 at 6.
u8 xboxSunDarkness()
{
  static const u8 kDarkness[11] = {
    0xE8, 0xC8, 0xA8, 0x90, 0x78, 0x64, 0x50, 0x38, 0x24, 0x10, 0x00 };
  int level = g_cfg.sunLevel;
  if (level < 0) level = 0;
  if (level > 10) level = 10;
  return kDarkness[level];
}

// How far the stick direction mapped to a sensor movement is pushed
// (0..32767, after the dead zone).
static int sensorAmount(int action)
{
  int v = uiStickAmount(g_cfg.sensorMap[action], g_leftX, g_leftY, g_rightX, g_rightY);
  return v < uiSensorDeadzone[g_cfg.sensorDeadzone] ? 0 : v;
}

// In a sensor game, a left-stick direction mapped to one of its sensors
// must not also move the D-pad.
static bool sensorsUseLeftStick()
{
  if (!g_isGba || !g_cartHw)
    return false;
  for (int i = 0; i < SENS_COUNT; i++) {
    bool used = (i <= SENS_TILT_DOWN) ? (g_cartHw & (UI_HW_TILT | UI_HW_GYRO)) != 0
                                      : (g_cartHw & UI_HW_SOLAR) != 0;
    if (used && uiStickDirIsLeftStick(g_cfg.sensorMap[i]))
      return true;
  }
  return false;
}

// Tilt / rotation targets from the right stick, as set in Settings ->
// Sensors (axis, direction, sensitivity, dead zone). Tilt is centred on
// 2047 like the cartridge's sensor; rotation is about -360..360.
void xboxMotionTargets(int &tiltX, int &tiltY, int &gyro)
{
  const int pct = uiSensorSensPercent[g_cfg.sensorSens];
  // Only for the games that have these sensors, so the sticks never
  // interfere with anything else.
  const bool tilt = g_isGba && (g_cartHw & UI_HW_TILT);
  const bool rot = g_isGba && (g_cartHw & UI_HW_GYRO);
  int lr = 0, ud = 0;
  if (tilt || rot)
    lr = sensorAmount(SENS_TILT_RIGHT) - sensorAmount(SENS_TILT_LEFT);
  if (tilt)
    ud = sensorAmount(SENS_TILT_UP) - sensorAmount(SENS_TILT_DOWN);
  tiltX = 2047 - (tilt ? lr : 0) * 3 * pct / 32767;
  tiltY = 2047 - ud * 3 * pct / 32767;
  // Rotation (WarioWare: Twisted!) follows tilt left / right.
  gyro = (rot ? lr : 0) * 9 * pct / (5 * 32767);
}

// Boktai: "More sunlight" / "Less sunlight" from Settings > Controls (one
// step at once, then every quarter second while held), shown on screen.
static void updateSunFromStick()
{
  static int repeat = 0;
  if (!g_isGba || !(g_cartHw & UI_HW_SOLAR))
    return;
  int dir = 0;
  if (sensorAmount(SENS_SUN_UP) > 16000) dir = 1;
  else if (sensorAmount(SENS_SUN_DOWN) > 16000) dir = -1;
  if (dir == 0) {
    repeat = 0;
    return;
  }
  if (repeat-- > 0)
    return;
  repeat = 15;
  int level = g_cfg.sunLevel + dir;
  if (level < 0) level = 0;
  if (level > 10) level = 10;
  g_cfg.sunLevel = level;
  char msg[32];
  sprintf(msg, g_cfg.language ? "Sunlight: %d/10" : "Luz solar: %d/10", level);
  systemScreenMessage(msg);
}

// Cartridge rumble motor -> controller vibration (every connected pad).
void xboxCartridgeRumble(bool on)
{
  if (on == g_rumbleOn)
    return;
  g_rumbleOn = on;
  XINPUT_VIBRATION v;
  v.wLeftMotorSpeed = on ? 40000 : 0;
  v.wRightMotorSpeed = on ? 40000 : 0;
  for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
    XInputSetState(i, &v);
}

void xboxApplyGameOverrides(bool isGba)
{
  g_isGba = isGba;
  // With the real GBA BIOS, start the game directly instead of playing the
  // boot animation: the BIOS checks the cartridge header there and hangs on
  // many ROM hacks, and the intro showed garbage around the picture. The
  // BIOS is still used for its system calls.
  skipBios = isGba && useBios;
  if (!isGba)
    return;
  applySpecialCart();
  switch (g_gameCfg.saveType) {
  case SAVE_EEPROM: cpuSaveType = 1; break;
  case SAVE_SRAM: cpuSaveType = 2; break;
  case SAVE_FLASH64: cpuSaveType = 3; flashSetSize(0x10000); break;
  case SAVE_FLASH128: cpuSaveType = 3; flashSetSize(0x20000); break;
  case SAVE_NONE: cpuSaveType = 5; break;
  default: break;   // automatic: keep what the ROM scan found
  }
  if (g_gameCfg.rtc == TRI_ON)
    rtcEnable(true);
  else if (g_gameCfg.rtc == TRI_OFF)
    rtcEnable(false);
}

void xboxOnGameStarted()
{
  // Cheats saved for this game
  std::string cheats = xboxCheatListPath();
  if (pathExists(cheats)) {
    if (cartridgeType == 1)
      gbCheatsLoadCheatList(cheats.c_str());
    else
      cheatsLoadCheatList(cheats.c_str());
  }
  if (g_cfg.autoLoadState && pathExists(xboxStatePath(0)))
    sdlReadState(0);
  g_waitRelease = true;
  g_prevPads = 0;
  g_videoStretch = -1;   // force the first video setup
  xboxApplySettings();
  resetFrameTiming();
  g_settleFrames = 30;
  systemScreenMessage(g_cfg.language ? "Menu: right stick click" : "Menu: clic stick derecho");
}

void xboxOnGameExiting()
{
  xboxCartridgeRumble(false);
  if (g_ui)
    g_ui->saveAll();   // keeps the sunlight level changed with the stick
  saveCheats();
  saveFrameThumb(uiGameFile(xboxEmulatorDir(), "thumbs", g_rom, "", ".tmb"));
  if (g_cfg.autoSaveState)
    g_host.saveState(0);
  if (g_ui)
    g_ui->fastForward = false;
  g_ffHeld = false;
  applySoundFlags();
  g_rom.clear();
}

// --- Per frame -----------------------------------------------------------------------

bool xboxFastForwardActive()
{
  bool toggled = g_ui && g_ui->fastForward;
  return toggled || g_ffHeld;
}

bool xboxSkipFrame()
{
  if (!xboxFastForwardActive()) {
    g_ffCounter = 0;
    return false;
  }
  int speed = uiFastForwardSpeeds[g_cfg.ffSpeed];
  if (speed <= 0)
    speed = 12;   // "maximum": show one frame in twelve
  g_ffCounter++;
  return (g_ffCounter % speed) != 0;
}

void xboxPollEvents()
{
  if (g_settleFrames > 0 && --g_settleFrames == 0) {
    // Same as closing the menu: restart the audio stream and the frame
    // timing, which the slow first frames (video setup) threw off.
    soundPause();
    resetFrameTiming();
    soundResume();
  }
  unsigned int pads = xboxReadPads(true);
  if (g_waitRelease) {
    if (pads == 0)
      g_waitRelease = false;
    g_prevPads = pads;
    return;
  }
  unsigned int pressed = pads & ~g_prevPads;
  g_prevPads = pads;
  updateSunFromStick();

  // Fast forward
  int ffButton = g_cfg.map[ACT_FAST_FORWARD];
  if (ffButton < PAD_BUTTON_COUNT) {
    if (g_cfg.ffToggle) {
      if ((pressed & PAD_BIT(ffButton)) && g_ui) {
        g_ui->fastForward = !g_ui->fastForward;
        systemScreenMessage(g_ui->fastForward ? ">> x" : "1x");
      }
      g_ffHeld = false;
    } else {
      g_ffHeld = (pads & PAD_BIT(ffButton)) != 0;
    }
  }
  applySoundFlags();

  // Quick save / load
  if (g_cfg.map[ACT_QUICK_SAVE] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(g_cfg.map[ACT_QUICK_SAVE]))) {
    bool ok = g_host.saveState(g_cfg.stateSlot);
    systemScreenMessage(ok ? (g_cfg.language ? "State saved" : "Estado guardado")
                           : (g_cfg.language ? "Save failed" : "Error al guardar"));
    if (g_ui) g_ui->invalidateThumb(xboxStateThumbPath(g_cfg.stateSlot));
    resetFrameTiming();
  }
  if (g_cfg.map[ACT_QUICK_LOAD] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(g_cfg.map[ACT_QUICK_LOAD]))) {
    bool ok = g_host.loadState(g_cfg.stateSlot);
    systemScreenMessage(ok ? (g_cfg.language ? "State loaded" : "Estado cargado")
                           : (g_cfg.language ? "Empty slot" : "Ranura vacia"));
    resetFrameTiming();
  }

  // In-game menu: only the configured button (right stick click by
  // default). Start + Select belong to the game (soft reset in many titles).
  int menuButton = g_cfg.map[ACT_MENU];
  if (menuButton < PAD_BUTTON_COUNT && (pressed & PAD_BIT(menuButton)))
    runInGameMenu();
}

u32 xboxReadJoypad()
{
  if (g_waitRelease)
    return 0;
  unsigned int pads = xboxReadPads(!sensorsUseLeftStick());
  static const int bits[ACT_GBA_RIGHT + 1] = {
    1, 2, 512, 256, 8, 4,     // A, B, L, R, Start, Select
    64, 128, 32, 16           // Up, Down, Left, Right
  };
  u32 res = 0;
  for (int a = ACT_GBA_A; a <= ACT_GBA_RIGHT; a++)
    if (padDown(pads, a))
      res |= bits[a];

  // Turbo: ~10 presses per second
  bool turboPhase = ((GetTickCount() / 50) & 1) != 0;
  if (padDown(pads, ACT_TURBO_A) && turboPhase) res |= 1;
  if (padDown(pads, ACT_TURBO_B) && turboPhase) res |= 2;

  // No opposite directions at the same time (some games crash on it).
  if ((res & 48) == 48) res &= ~16;
  if ((res & 192) == 192) res &= ~128;
  return res;
}

void xboxGetRightStick(int &x, int &y)
{
  x = g_rightX;
  y = g_rightY;
}

void xboxApplyGameSampler()
{
  applySampler();
}

// Background image: the file chosen in Video settings, inside the borders folder
// next to default.xex (older settings: border.png / .jpg / .bmp).
static std::string g_borderLoaded;
static bool g_borderReady = false;

std::string xboxBorderImagePath()
{
  std::string dir = uiJoinPath(xboxEmulatorDir(), "borders");
  if (!g_cfg.borderImage.empty()) {
    std::string p = uiJoinPath(dir, g_cfg.borderImage);
    return pathExists(p) ? p : std::string();
  }
  static const char *names[] = { "border.png", "border.jpg", "border.bmp", "border.tga", "border.dds" };
  for (int i = 0; i < 5; i++) {
    std::string p = uiJoinPath(dir, names[i]);
    if (pathExists(p))
      return p;
  }
  return "";
}

static void applyBorder()
{
  std::string wanted = g_cfg.border ? xboxBorderImagePath() : std::string();
  if (g_borderReady && wanted == g_borderLoaded) {
    xboxSetBorderFit(g_cfg.borderFit);
    return;
  }
  g_borderReady = true;
  g_borderLoaded = xboxSetBorderImage(wanted.c_str(), g_cfg.borderFit) ? wanted : std::string();
}

// --- Picture position, screen effect, Game Boy colors -----------------------------

extern "C" int XBOX_UseCustomRect;
extern "C" int XBOX_CustomRect[4];
extern "C" int XBOX_ScreenEffect;
extern "C" float XBOX_EffectSource[2];

void xboxSetupPicture(int srcW, int srcH)
{
  const int sw = 1280, sh = 720;
  if (srcW <= 0 || srcH <= 0) {
    XBOX_UseCustomRect = 0;
    return;
  }
  int w, h;
  if (g_cfg.stretch == SIZE_WIDE) {
    // 16:9: the whole 1280x720 screen, no margins (the picture is stretched).
    w = sw;
    h = sh;
  } else if (g_cfg.stretch == SIZE_FULL) {
    // Fill the screen, leaving the TV-safe margins the original port used.
    w = sw - 160;
    h = sh - 80;
  } else if (g_cfg.stretch == SIZE_INTEGER) {
    // Biggest whole-number zoom that fits: every console pixel gets the
    // same size (GBA x4 = 960x640, Game Boy x5 = 800x720).
    int k = sh / srcH;
    if (sw / srcW < k)
      k = sw / srcW;
    if (k < 1)
      k = 1;
    w = srcW * k;
    h = srcH * k;
  } else {
    // Console proportions (GBA 3:2, Game Boy 10:9), as big as fits.
    h = sh;
    w = (srcW * sh + srcH / 2) / srcH;
    if (w > sw) {
      w = sw;
      h = (srcH * sw + srcW / 2) / srcW;
    }
  }
  XBOX_CustomRect[0] = (sw - w) / 2;
  XBOX_CustomRect[1] = (sh - h) / 2;
  XBOX_CustomRect[2] = XBOX_CustomRect[0] + w;
  XBOX_CustomRect[3] = XBOX_CustomRect[1] + h;
  XBOX_UseCustomRect = 1;
  XBOX_EffectSource[0] = (float)srcW;
  XBOX_EffectSource[1] = (float)srcH;
  XBOX_ScreenEffect = g_cfg.screenEffect;
}

static void applyGbPalette()
{
  // 15-bit BGR colors, lightest first, the same 4 shades for the
  // background and the sprites.
  const unsigned int *p = uiGbPalettes[g_cfg.gbPalette];
  for (int i = 0; i < 8; i++) {
    unsigned int c = p[i & 3];
    systemGbPalette[i] = (u16)(((c >> 19) & 0x1F) | (((c >> 11) & 0x1F) << 5) | (((c >> 3) & 0x1F) << 10));
  }
  gbPaletteOption = 0;
  // A Game Boy game is running: recolor it right away.
  if (emulating && cartridgeType == 1 && !gbCgbMode && !gbSgbMode)
    for (int i = 0; i < 8; i++)
      gbPalette[i] = systemGbPalette[i];
}

void xboxApplySettings()
{
  applySoundFlags();
  applyBorder();
  applyGbPalette();
  rtcSetTimeOffset(g_cfg.clockMode ? g_cfg.clockOffset : 0);
  XBOX_ScreenEffect = g_cfg.screenEffect;
  showSpeed = g_cfg.showFps ? 2 : 0;
  if (g_cfg.frameSkip == 0) {
    autoFrameSkip = 1;
  } else {
    autoFrameSkip = 0;
    frameSkip = gbFrameSkip = systemFrameSkip = g_cfg.frameSkip - 1;
  }

  if (!emulating)
    return;
  Filter wanted = coreFilterFor(g_cfg.filter);
  if (wanted != g_videoFilter || g_cfg.stretch != g_videoStretch) {
    // Texture size / scaling changed: rebuild the video surface.
    g_videoFilter = wanted;
    g_videoStretch = g_cfg.stretch;
    sdlInitVideo();
    filterFunction = initFilter(filter, systemColorDepth, srcWidth);
  } else {
    XBOX_RestoreVideoState();
  }
  applySampler();
}

Filter xboxSelectedFilter()
{
  return coreFilterFor(g_cfg.filter);
}

// --- Controllers ---------------------------------------------------------------------

unsigned int xboxReadPads(bool includeSticks)
{
  unsigned int mask = 0;
  for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
    XINPUT_STATE st;
    ZeroMemory(&st, sizeof(st));
    if (XInputGetState(i, &st) != ERROR_SUCCESS)
      continue;
    const XINPUT_GAMEPAD &g = st.Gamepad;
    WORD b = g.wButtons;
    if (b & XINPUT_GAMEPAD_DPAD_UP) mask |= PAD_BIT(PAD_UP);
    if (b & XINPUT_GAMEPAD_DPAD_DOWN) mask |= PAD_BIT(PAD_DOWN);
    if (b & XINPUT_GAMEPAD_DPAD_LEFT) mask |= PAD_BIT(PAD_LEFT);
    if (b & XINPUT_GAMEPAD_DPAD_RIGHT) mask |= PAD_BIT(PAD_RIGHT);
    if (b & XINPUT_GAMEPAD_A) mask |= PAD_BIT(PAD_A);
    if (b & XINPUT_GAMEPAD_B) mask |= PAD_BIT(PAD_B);
    if (b & XINPUT_GAMEPAD_X) mask |= PAD_BIT(PAD_X);
    if (b & XINPUT_GAMEPAD_Y) mask |= PAD_BIT(PAD_Y);
    if (b & XINPUT_GAMEPAD_LEFT_SHOULDER) mask |= PAD_BIT(PAD_LB);
    if (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) mask |= PAD_BIT(PAD_RB);
    if (b & XINPUT_GAMEPAD_START) mask |= PAD_BIT(PAD_START);
    if (b & XINPUT_GAMEPAD_BACK) mask |= PAD_BIT(PAD_BACK);
    if (b & XINPUT_GAMEPAD_LEFT_THUMB) mask |= PAD_BIT(PAD_LS);
    if (b & XINPUT_GAMEPAD_RIGHT_THUMB) mask |= PAD_BIT(PAD_RS);
    if (g.bLeftTrigger > 64) mask |= PAD_BIT(PAD_LT);
    if (g.bRightTrigger > 64) mask |= PAD_BIT(PAD_RT);
    if (includeSticks) {
      const int dz = 16000;
      if (g.sThumbLY > dz) mask |= PAD_BIT(PAD_UP);
      if (g.sThumbLY < -dz) mask |= PAD_BIT(PAD_DOWN);
      if (g.sThumbLX < -dz) mask |= PAD_BIT(PAD_LEFT);
      if (g.sThumbLX > dz) mask |= PAD_BIT(PAD_RIGHT);
    }
    if (i == 0 || g_rightX == 0) {
      g_rightX = g.sThumbRX;
      g_rightY = g.sThumbRY;
    }
    if (i == 0 || g_leftX == 0) {
      g_leftX = g.sThumbLX;
      g_leftY = g.sThumbLY;
    }
  }
  return mask;
}
