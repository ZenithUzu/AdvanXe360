// Xbox 360 implementation of the menu UI host, controller input, per-game
// files and the in-game menu loop.
#include <xtl.h>
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
extern const char *gbPendingPatch;     // gb/GB.cpp
extern bool soundMuted;                // common/SoundSDL.cpp
extern bool soundNoThrottle;
FilterFunc initFilter(const Filter f, const int colorDepth, const int srcWidth);
void systemScreenMessage(const char *msg);

UiConfig g_cfg;
UiApp *g_ui = 0;
bool xboxGameLoadFailed = false;

namespace {

std::string g_rom;            // full path of the running game
UiGameConfig g_gameCfg;
bool g_isGba = true;
unsigned int g_prevPads = 0;
bool g_waitRelease = false;   // ignore the pad until everything is released
bool g_ffHeld = false;
int g_ffCounter = 0;
short g_rightX = 0, g_rightY = 0;
Filter g_videoFilter = kStretch2x;
int g_videoStretch = -1;
UiCanvas *g_menuCanvas = 0;

bool fileExists(const std::string &path)
{
  return GetFileAttributes(path.c_str()) != (DWORD)-1;
}

void ensureDir(const std::string &path)
{
  std::string p = path;
  while (!p.empty() && (p[p.size() - 1] == '\\' || p[p.size() - 1] == '/'))
    p.erase(p.size() - 1);
  if (!p.empty() && !fileExists(p))
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

  virtual bool fileExists(const std::string &path) { return ::fileExists(path); }
  virtual std::string emulatorDir() { return xboxEmulatorDir(); }
  virtual std::string currentRom() { return g_rom; }

  virtual bool saveState(int slot)
  {
    std::string path = xboxStatePath(slot);
    DeleteFile(path.c_str());
    sdlWriteState(slot);
    if (!::fileExists(path))
      return false;
    saveFrameThumb(xboxStateThumbPath(slot));
    return true;
  }

  virtual bool loadState(int slot)
  {
    if (!::fileExists(xboxStatePath(slot)))
      return false;
    sdlReadState(slot);
    return true;
  }

  virtual bool stateExists(int slot) { return ::fileExists(xboxStatePath(slot)); }
  virtual std::string stateThumbPath(int slot) { return xboxStateThumbPath(slot); }

  virtual bool takeScreenshot(std::string &savedTo)
  {
    for (int n = 1; n < 1000; n++) {
      char suffix[16];
      sprintf(suffix, "-%03d", n);
      std::string path = uiGameFile(xboxEmulatorDir(), "screenshots", g_rom, suffix, ".png");
      if (::fileExists(path))
        continue;
      emulator.emuWritePNG(path.c_str());
      savedTo = path;
      return ::fileExists(path);
    }
    return false;
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

// Opens the in-game menu and runs it until the player resumes or quits.
void runInGameMenu()
{
  if (!g_ui || !g_menuCanvas)
    return;
  soundPause();
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
  if (emulating)
    soundResume();
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
  static const char *dirs[] = { "saves", "states", "cheats", "screenshots", "thumbs", "config", "bios" };
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

  // Optional real BIOS (needed by a few games, nicer boot logo).
  std::string bios = uiJoinPath(uiJoinPath(xboxEmulatorDir(), "bios"), "gba_bios.bin");
  useBios = g_cfg.useBios && fileExists(bios);
  strcpy(biosFileName, useBios ? bios.c_str() : "");

  // Game Boy patches are applied inside gbLoadRom().
  gbPendingPatch = 0;
  static std::string gbPatch;
  if (g_gameCfg.patch) {
    static const char *exts[] = { ".bps", ".ups", ".ips" };
    for (int i = 0; i < 3; i++) {
      std::string p = uiStripExtension(g_rom) + exts[i];
      if (fileExists(p)) {
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

void xboxApplyGameOverrides(bool isGba)
{
  g_isGba = isGba;
  if (!isGba)
    return;
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
  if (fileExists(cheats)) {
    if (cartridgeType == 1)
      gbCheatsLoadCheatList(cheats.c_str());
    else
      cheatsLoadCheatList(cheats.c_str());
  }
  if (g_cfg.autoLoadState && fileExists(xboxStatePath(0)))
    sdlReadState(0);
  g_waitRelease = true;
  g_prevPads = 0;
  g_videoStretch = -1;   // force the first video setup
  xboxApplySettings();
  systemScreenMessage(g_cfg.language ? "Menu: right stick click" : "Menu: clic stick derecho");
}

void xboxOnGameExiting()
{
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
  unsigned int pads = xboxReadPads(true);
  if (g_waitRelease) {
    if (pads == 0)
      g_waitRelease = false;
    g_prevPads = pads;
    return;
  }
  unsigned int pressed = pads & ~g_prevPads;
  g_prevPads = pads;

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
  }
  if (g_cfg.map[ACT_QUICK_LOAD] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(g_cfg.map[ACT_QUICK_LOAD]))) {
    bool ok = g_host.loadState(g_cfg.stateSlot);
    systemScreenMessage(ok ? (g_cfg.language ? "State loaded" : "Estado cargado")
                           : (g_cfg.language ? "Empty slot" : "Ranura vacia"));
  }

  // Menu: the configured button, or BACK + START as a fallback that always works.
  bool combo = (pads & PAD_BIT(PAD_BACK)) && (pads & PAD_BIT(PAD_START)) &&
               (pressed & (PAD_BIT(PAD_BACK) | PAD_BIT(PAD_START)));
  int menuButton = g_cfg.map[ACT_MENU];
  if (combo || (menuButton < PAD_BUTTON_COUNT && (pressed & PAD_BIT(menuButton))))
    runInGameMenu();
}

u32 xboxReadJoypad()
{
  if (g_waitRelease)
    return 0;
  unsigned int pads = xboxReadPads(true);
  // While BACK + START is held for the menu, don't press them in the game.
  if ((pads & PAD_BIT(PAD_BACK)) && (pads & PAD_BIT(PAD_START)))
    return 0;

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

void xboxApplySettings()
{
  applySoundFlags();
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
  }
  return mask;
}
