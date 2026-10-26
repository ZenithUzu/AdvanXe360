// Xbox 360 implementation of the menu UI host, controller input, per-game
// files and the in-game menu loop.
#include <xtl.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include "XboxHost.h"
#include "XboxCovers.h"
#include "XboxCheevos.h"
#include "XboxRewind.h"
#include "fex/fex.h"
#include <zlib.h>   // crc32: Game Boy cheats found by the ROM's CRC
#include <map>
#include "../gb/gbGambatteBridge.h"
namespace gambatte { extern long rtcTimeOffset; }   // gambatte/src/mem/rtc.cpp: the custom game time

// Kinect as the Game Boy Camera. nuiapi.lib needs st.lib (skeleton tracking,
// head detector) even if only the color camera is used. Define
// XBOX_NO_KINECT to build without them.
#ifndef XBOX_NO_KINECT
#include <nuiapi.h>
#pragma comment(lib, "nuiapi.lib")
#pragma comment(lib, "st.lib")
#endif
static void kinectCapture(unsigned char *gray);   // Game Boy Camera picture (below)
static void kinectStop();
static bool kinectHand(int &sx, int &sy);
#define XBOX_LOG_NO_REDIRECT
#include "XboxLog.h"

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
#include "../gb/gbGambatte.h"
#include "../gb/gbPrinter.h"
#include "../Util.h"
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
extern int soundVolumePercent;
extern int soundBufferMs;
FilterFunc initFilter(const Filter f, const int colorDepth, const int srcWidth);
extern bool wasPaused;                 // sdl/SDL.cpp: skip one frame-skip measurement
extern u32 autoFrameSkipLastTime;
extern u32 throttleLastTime;
extern u8 *rom;
int xboxGbaRomSize = 0;
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

// Extra hardware of a GBA cartridge from its game code (defined below,
// outside this namespace).
static int specialCartHw(const char *code3);

namespace {

std::string g_rom;            // full path of the running game
UiGameConfig g_gameCfg;

// Picture settings in effect: Settings > Video, or the running game's own
// ("Custom" in its game options).
static bool gameVideoCustom() { return !g_rom.empty() && g_gameCfg.custom != 0; }
static int videoStretch() { return gameVideoCustom() ? g_gameCfg.stretch : g_cfg.stretch; }
static int videoEffect() { return gameVideoCustom() ? g_gameCfg.screenEffect : g_cfg.screenEffect; }
static int videoGbPalette() { return gameVideoCustom() ? g_gameCfg.gbPalette : g_cfg.gbPalette; }
static int videoGbaColor() { return gameVideoCustom() ? g_gameCfg.gbaColor : g_cfg.gbaColor; }
static int videoFrameBlend() { return gameVideoCustom() ? g_gameCfg.frameBlend : g_cfg.frameBlend; }
static int videoShowFps() { return gameVideoCustom() ? g_gameCfg.showFps : g_cfg.showFps; }
static int videoFrameSkip() { return gameVideoCustom() ? g_gameCfg.frameSkip : g_cfg.frameSkip; }

// GBA color table in use (-1 = rebuild) and the last frame for blending.
static int g_colorMapMode = -1;
static std::vector<u32> g_prevFrame;
static bool g_prevValid = false;

// Buttons in effect: Settings > Controls, or the running game's own map.
static const int *ctlMap()
{
  return (!g_rom.empty() && g_gameCfg.customControls) ? g_gameCfg.map : g_cfg.map;
}
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
  case CODE_GBA_GSA12: cheatsAddGSACode(c.code.c_str(), d.c_str(), false); break;
  case CODE_GBA_GSA3: cheatsAddGSACode(c.code.c_str(), d.c_str(), true); break;
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

// Each game's cheats live in a text file of their own, by console:
// cheats\gba\<game>.vch or cheats\gb\<game>.vch (UiCheatParse.h), so a list
// is never fed to the wrong core. The running game's core list is rebuilt
// from that file whenever it changes.
// The ROM inside a .zip / .7z, by name ("" if none); each archive is read once.
std::string archiveInnerRom(const std::string &archive)
{
  static std::map<std::string, std::string> cache;
  std::map<std::string, std::string>::iterator it = cache.find(archive);
  if (it != cache.end())
    return it->second;
  std::string found;
  fex_t *fe = 0;
  if (!fex_open(&fe, archive.c_str()) && fe) {
    while (!fex_done(fe) && found.empty()) {
      std::string n = fex_name(fe);
      std::string e = uiExtension(n);
      if (e == ".gba" || e == ".agb" || e == ".gb" || e == ".gbc" || e == ".cgb" || e == ".dmg" || e == ".sgb")
        found = n;
      else if (fex_next(fe))
        break;
    }
    fex_close(fe);
  }
  cache[archive] = found;
  return found;
}

// CRC-32 of a Game Boy / Color ROM (the file, or the ROM inside a .zip / .7z),
// as libretro's game lists give it: finds the game's cheats whatever the file
// is called.
bool romCrc32(const std::string &rom, unsigned long &crc)
{
  std::string data;
  std::string inner = archiveInnerRom(rom);
  std::string ext = uiExtension(rom);
  if (ext == ".zip" || ext == ".7z" || ext == ".rar") {
    fex_t *fe = 0;
    if (inner.empty() || fex_open(&fe, rom.c_str()) || !fe)
      return false;
    while (!fex_done(fe)) {
      if (inner == fex_name(fe)) {
        const void *p = 0;
        if (!fex_data(fe, &p) && p)
          data.assign((const char *)p, fex_size(fe));
        break;
      }
      if (fex_next(fe))
        break;
    }
    fex_close(fe);
  } else {
    FILE *f = fopen(rom.c_str(), "rb");
    if (!f)
      return false;
    char buf[64 * 1024];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0 && data.size() < 16 * 1024 * 1024)
      data.append(buf, got);
    fclose(f);
  }
  if (data.empty())
    return false;
  crc = crc32(0L, (const Bytef *)data.data(), (uInt)data.size());
  return true;
}

// A cheat code compared without spaces, '+' or case ("8200055C+0000" = "8200055c 0000").
std::string cheatKey(const std::string &code)
{
  std::string k;
  for (size_t i = 0; i < code.size(); i++)
    if (isalnum((unsigned char)code[i]))
      k += (char)toupper((unsigned char)code[i]);
  return k;
}

// A code's letters and digits in upper case, every blank ('?', 'X') as '?'.
std::string blankKey(const std::string &code)
{
  std::string k;
  for (size_t i = 0; i < code.size(); i++) {
    char c = (char)toupper((unsigned char)code[i]);
    if (c == '?' || c == 'X')
      k += '?';
    else if (isalnum((unsigned char)c))
      k += c;
  }
  return k;
}

// A code with blanks ("33002708+00??") and a stored one, its blanks filled
// or not: the same cheat?
bool sameBlankCode(const std::string &blank, const std::string &code)
{
  std::string a = blankKey(blank), b = blankKey(code);
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++)
    if (a[i] != '?' && a[i] != b[i])
      return false;
  return true;
}

bool romIsGb(const std::string &rom)
{
  std::string ext = uiExtension(rom);
  if (ext == ".gb" || ext == ".gbc" || ext == ".cgb" || ext == ".dmg" || ext == ".sgb")
    return true;
  if (ext == ".gba" || ext == ".agb" || ext == ".bin" || ext == ".elf" || ext == ".mb")
    return false;
  // Compressed: the running game knows; else the ROM inside; else GBA.
  if (!g_rom.empty() && rom == g_rom)
    return cartridgeType == 1;
  std::string inner = archiveInnerRom(rom);
  return !inner.empty() && romIsGb(inner);
}

std::string cheatStorePath(const std::string &rom)
{
  return uiGameFile(xboxEmulatorDir(), romIsGb(rom) ? "cheats\\gb" : "cheats\\gba", rom, "", ".vch");
}

void loadCheatStore(const std::string &rom, std::vector<UiStoredCheat> &list)
{
  int system = 0;
  uiLoadCheatStore(cheatStorePath(rom), list, system);
  // Wrong console in the file (renamed ROM...): ignore it.
  if (system != 0 && system != (romIsGb(rom) ? 2 : 1))
    list.clear();
}

void saveCheatStore(const std::string &rom, const std::vector<UiStoredCheat> &list)
{
  std::string path = cheatStorePath(rom);
  if (list.empty()) {
    DeleteFile(path.c_str());
    return;
  }
  uiSaveCheatStore(path, list, romIsGb(rom) ? 2 : 1);
}

// The running game's core gets exactly the enabled and disabled cheats of
// its list.
void syncCoreCheats()
{
  if (g_rom.empty())
    return;
  if (cartridgeType == 1)
    gbCheatRemoveAll();
  else
    cheatsDeleteAll(true);
  std::vector<UiStoredCheat> list;
  loadCheatStore(g_rom, list);
  bool gba = cartridgeType != 1;
  for (size_t i = 0; i < list.size(); i++) {
    std::vector<UiParsedCode> codes;
    if (!uiParseCheatCodesAs(list[i].code, gba, list[i].type, codes))
      continue;
    int first = coreCheatCount();
    for (size_t k = 0; k < codes.size(); k++)
      addParsedCode(codes[k], list[i].desc);
    for (int c = first; c < coreCheatCount(); c++)
      setCoreCheat(c, list[i].enabled);
  }
}

// Lists saved by older versions (cheats\<game>.clt, the core's own format)
// are turned into the new file once.
void migrateOldCheats()
{
  std::string old = xboxCheatListPath();
  if (!pathExists(old) || pathExists(cheatStorePath(g_rom)))
    return;
  if (cartridgeType == 1)
    gbCheatsLoadCheatList(old.c_str());
  else
    cheatsLoadCheatList(old.c_str());
  std::vector<CheatGroup> groups;
  cheatGroups(groups);
  std::vector<UiStoredCheat> list;
  for (size_t i = 0; i < groups.size(); i++) {
    UiStoredCheat c;
    int f = groups[i].first;
    c.desc = coreCheatDesc(f);
    c.enabled = coreCheatEnabled(f);
    for (int k = 0; k < groups[i].count; k++) {
      if (k) c.code += " ";
      c.code += cartridgeType == 1 ? gbCheatList[f + k].cheatCode : cheatsList[f + k].codestring;
    }
    list.push_back(c);
  }
  saveCheatStore(g_rom, list);
  DeleteFile(old.c_str());
}

// --- Video -----------------------------------------------------------------------

Filter coreFilterFor(int uiFilter)
{
  // Only LQ2x still runs on the CPU; the others are done by the GPU from
  // the plain 2x picture (see shaderFilterFor).
  return uiFilter == FILTER_LQ2X ? klq2x : kStretch2x;
}

// XBOX_ShaderFilter for a menu filter (SDL_xboxvideo.c).
static int shaderFilterFor(int uiFilter)
{
  switch (uiFilter) {
  case FILTER_SHARP_SMOOTH: return 1;
  case FILTER_SCALE2X: return 2;
  case FILTER_SCALE3X: return 3;
  default: return 0;
  }
}

// Filters that read single console pixels need point sampling.
bool pointSampler()
{
  int f = xboxVideoFilter();
  return f == FILTER_SHARP || f == FILTER_SCALE2X || f == FILTER_SCALE3X;
}

void applySampler()
{
  D3DTEXTUREFILTERTYPE f = pointSampler() ? D3DTEXF_POINT : D3DTEXF_LINEAR;
  pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, f);
  pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, f);
  // Clamp: with the default wrap mode the smooth filter blends the
  // opposite edge of the picture in (stray lines / letters at the sides).
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

void resetFrameTiming();
static bool g_rwHeld = false;   // the rewind button is held (xboxPollEvents)
static int g_shakeFrames = 0;   // frames left of a console flick (ACT_SHAKE)

void applySoundFlags()
{
  bool ff = xboxFastForwardActive();
  // Back to normal speed: the first throttled frames look slow to the auto
  // frame skip, which then skipped frames for a second or two. Measure afresh.
  if (soundNoThrottle && !ff)
    resetFrameTiming();
  soundMuted = !g_cfg.sound || (ff && g_cfg.muteFastForward) || xboxRewinding;   // backwards: no sound
  soundNoThrottle = ff;
  soundVolumePercent = uiVolumeLevels[g_cfg.volume];
  soundBufferMs = uiAudioLatencyMs[g_cfg.audioLatency];
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

  // Deletes the files in a folder of the emulator with one of two
  // extensions (lower case, with the dot).
  int deleteInFolder(const char *folder, const char *ext1, const char *ext2)
  {
    std::vector<UiDirEntry> files;
    if (!listDir(uiJoinPath(xboxEmulatorDir(), folder), files))
      return 0;
    int n = 0;
    for (size_t i = 0; i < files.size(); i++) {
      if (files[i].isDir)
        continue;
      std::string ext = uiExtension(files[i].name);
      if ((ext == ext1 || (ext2 && ext == ext2)) && DeleteFile(files[i].path.c_str()))
        n++;
    }
    return n;
  }

  virtual int deleteAllStates(const std::vector<std::string> &knownRoms)
  {
    int n = deleteInFolder("states", ".sgm", ".tmb");
    // States kept next to the ROMs: the games the menu knows about.
    for (size_t r = 0; r < knownRoms.size(); r++) {
      for (int slot = 0; slot <= 9; slot++) {
        char suffix[16];
        if (slot == 0)
          strcpy(suffix, "-auto");
        else
          sprintf(suffix, "-%d", slot);
        std::string base = uiStripExtension(uiFileName(knownRoms[r])) + suffix;
        std::string sgm = uiJoinPath(romDirOf(knownRoms[r]), uiSafeFileName(base, ".sgm"));
        if (DeleteFile(sgm.c_str()))
          n++;
        DeleteFile((uiStripExtension(sgm) + ".tmb").c_str());
      }
    }
    fprintf(stdout, "Deleted %d save states\n", n);
    return n;
  }

  virtual int deleteAllGameConfigs()
  {
    int n = deleteInFolder("config", ".ini", 0);
    fprintf(stdout, "Deleted %d game option files\n", n);
    if (!g_rom.empty())
      g_gameCfg = UiGameConfig();
    return n;
  }
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
    cheevosReset();
    xboxRewindStart();   // the steps saved led to another moment of the game
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
    return g_rom.empty() ? 0 : g_cartHw;
  }

  virtual bool romPatched()
  {
    return xboxRomPatched && !g_rom.empty();
  }

  virtual std::string archiveRomName(const std::string &archive) { return archiveInnerRom(archive); }
  virtual bool latestRelease(std::string &tag, std::string &err)
  {
    std::string body;
    if (!xboxNetStart(err) ||
        !httpRequest("https://api.github.com/repos/ZenithUzu/AdvanXe360/releases/latest", "", body, err, ""))
      return false;
    std::vector<std::string> t = jsonStrings(body, "tag_name");
    if (t.empty()) {
      err = "reply";
      return false;
    }
    tag = t[0];
    xboxLog("Update check: latest release %s, this is %s", tag.c_str(), UI_APP_VERSION);
    return true;
  }

  virtual bool netTest(std::string &report)
  {
    std::string err;
    if (!xboxNetStart(err)) {
      report = g_cfg.language ? "No network (" + err + ")" : "Sin red (" + err + ")";
      return false;
    }
    static const char *const names[2] = { "RetroAchievements", "GitHub" };
    static const char *const urls[2] = {
      "https://retroachievements.org/",
      "https://raw.githubusercontent.com/libretro/libretro-database/master/README.md" };
    bool all = true;
    report.clear();
    for (int i = 0; i < 2; i++) {
      std::string body;
      err.clear();
      DWORD t0 = GetTickCount();
      bool ok = httpRequest(urls[i], "", body, err, "");
      char line[160];
      sprintf(line, "%s: %s (%.1f s, %u bytes)", names[i], ok ? "OK" : err.c_str(), (GetTickCount() - t0) / 1000.0,
              (unsigned)body.size());
      xboxLog("HTTPS test: %s", line);
      report += (i ? " | " : "") + std::string(line);
      all = all && ok;
    }
    return all;
  }

  virtual bool handCursor(int &x, int &y) { return kinectHand(x, y); }
  virtual void handStop() { kinectStop(); }

  virtual int romInfo(const std::string &rom)
  {
    std::string ext = uiExtension(rom);
    if (ext == ".gba" || ext == ".agb" || ext == ".bin" || ext == ".elf" || ext == ".mb") {
      // Sensor cartridges, from the game code in the header.
      unsigned char code[3];
      FILE *f = fopen(rom.c_str(), "rb");
      if (!f)
        return 0;
      bool ok = fseek(f, 0xAC, SEEK_SET) == 0 && fread(code, 1, 3, f) == 3;
      fclose(f);
      char c3[4] = { (char)code[0], (char)code[1], (char)code[2], 0 };
      return ok && (specialCartHw(c3) & (UI_HW_SOLAR | UI_HW_GYRO | UI_HW_TILT)) ? UI_ROM_SENSOR : 0;
    }
    if (ext != ".gb" && ext != ".gbc" && ext != ".cgb" && ext != ".dmg" && ext != ".sgb")
      return romIsGb(rom) ? UI_ROM_GB | UI_ROM_SGB : 0;   // zip / 7z: only known when running
    unsigned char h[0x150];
    FILE *f = fopen(rom.c_str(), "rb");
    if (!f)
      return UI_ROM_GB;
    size_t got = fread(h, 1, sizeof(h), f);
    fclose(f);
    if (got < sizeof(h))
      return UI_ROM_GB;
    int info = UI_ROM_GB;
    if (h[0x146] == 0x03 && h[0x14B] == 0x33)
      info |= UI_ROM_SGB;
    if (h[0x143] == 0xC0)
      info |= UI_ROM_CGB | UI_ROM_CGB_ONLY;
    else if (h[0x143] & 0x80)
      info |= UI_ROM_CGB;
    // Color-only games can't run on the Super Game Boy.
    if (info & UI_ROM_CGB_ONLY)
      info &= ~UI_ROM_SGB;
    if (h[0x147] == 0x22)
      info |= UI_ROM_SENSOR;   // MBC7: Kirby Tilt 'n' Tumble
    return info;
  }

  virtual bool loadImage(const std::string &path, int w, int h, std::vector<UiColor> &out)
  {
    return xboxLoadImagePixels(path.c_str(), w, h, out, true);
  }

  virtual bool loadImageFit(const std::string &path, int maxW, int maxH,
                            std::vector<UiColor> &out, int &w, int &h)
  {
    return xboxLoadImageFit(path.c_str(), maxW, maxH, out, w, h);
  }

  virtual bool startCoverDownload(const std::vector<UiCoverItem> &items, const std::string &server)
  {
    return coversStart(items, server);
  }
  virtual void coverDownloadStatus(UiCoverStatus &out) { coversStatus(out); }
  virtual void cancelCoverDownload() { coversCancel(); }

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
    cheevosReset();
    xboxRewindStart();
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

  virtual void listCheats(const std::string &rom, std::vector<UiCheatInfo> &out)
  {
    out.clear();
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);
    for (size_t i = 0; i < list.size(); i++) {
      UiCheatInfo c;
      c.desc = list[i].desc;
      c.code = list[i].code;
      c.type = list[i].type;
      c.enabled = list[i].enabled;
      out.push_back(c);
    }
  }

  virtual void setCheatEnabled(const std::string &rom, int index, bool enabled)
  {
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);
    if (index < 0 || index >= (int)list.size())
      return;
    list[index].enabled = enabled;
    saveCheatStore(rom, list);
    if (rom == g_rom)
      syncCoreCheats();
  }

  virtual bool addCheat(const std::string &rom, const std::string &code, int type,
                        const std::string &desc)
  {
    std::vector<UiParsedCode> codes;
    if (!uiParseCheatCodesAs(code, !romIsGb(rom), type, codes))
      return false;
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);
    UiStoredCheat c;
    c.enabled = true;
    c.type = type;
    c.code = code;
    c.desc = safeDesc(desc);
    list.push_back(c);
    saveCheatStore(rom, list);
    if (rom == g_rom)
      syncCoreCheats();
    return true;
  }

  virtual bool editCheat(const std::string &rom, int index, const std::string &code)
  {
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);
    if (index < 0 || index >= (int)list.size())
      return false;
    bool blank = uiCheatHasBlank(code);
    std::vector<UiParsedCode> codes;
    if (!uiParseCheatCodesAs(blank ? uiCheatFillBlank(code, "0") : code, !romIsGb(rom), list[index].type, codes))
      return false;
    list[index].code = code;
    list[index].enabled = !blank;
    saveCheatStore(rom, list);
    if (rom == g_rom)
      syncCoreCheats();
    return true;
  }

  virtual void removeCheat(const std::string &rom, int index)
  {
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);
    if (index < 0 || index >= (int)list.size())
      return;
    list.erase(list.begin() + index);
    saveCheatStore(rom, list);
    if (rom == g_rom)
      syncCoreCheats();
  }

  virtual int downloadCheats(const std::string &rom, std::string &err)
  {
    const char *sys = "Nintendo - Game Boy Advance";
    if (romIsGb(rom))
      sys = (romInfo(rom) & UI_ROM_CGB) || uiExtension(archiveInnerRom(rom)) == ".gbc" ? "Nintendo - Game Boy Color"
                                                                                       : "Nintendo - Game Boy";
    std::string dir = uiJoinPath(xboxEmulatorDir(), "cheats");
    std::string got = uiJoinPath(dir, "_libretro.cht");
    // GBA game code (header 0xAC), so any language of the game finds its cheats.
    std::string code;
    std::string ext = uiExtension(rom);   // a .zip has no header to read
    FILE *f = ext == ".gba" || ext == ".agb" || ext == ".bin" ? fopen(rom.c_str(), "rb") : 0;
    if (f) {
      char c[4];
      if (fseek(f, 0xAC, SEEK_SET) == 0 && fread(c, 1, 4, f) == 4 && isalnum((unsigned char)c[0]) &&
          isalnum((unsigned char)c[1]) && isalnum((unsigned char)c[2]) && isalnum((unsigned char)c[3]))
        code.assign(c, 4);
      fclose(f);
    }
    std::string name = uiStripExtension(uiFileName(rom));
    std::string pack = uiJoinPath(dir, "libretro-cheats.zip"), packErr = "nopack";
    // Game Boy / Color: the ROM's CRC in the pack's index gives the game's own
    // cheat file (its region and language), whatever the ROM is called.
    std::string folder = sys;
    bool otherVersion = false;
    unsigned long crc = 0;
    if (romIsGb(rom) && pathExists(pack) && romCrc32(rom, crc)) {
      char key[16];
      sprintf(key, "CRC:%08lX", crc);
      std::string file = cheatsIndexLookup(pack, key);
      size_t slash = file.find('/');
      if (slash != std::string::npos && file.size() > slash + 5) {
        folder = file.substr(0, slash);
        name = file.substr(slash + 1, file.size() - slash - 5);   // without ".cht"
      } else {
        // No cheats of its own (another language, a fan translation): the
        // game RetroAchievements recognised in the ROM, if it's running.
        // Its cheats are for another version: they come in turned off.
        std::string title = rom == g_rom ? cheevosGameTitle() : std::string();
        size_t bar = title.find(" | ");   // "Pokemon Red Version | Pokemon Blue Version"
        if (bar != std::string::npos)
          title = title.substr(0, bar);
        if (!title.empty()) {
          name = title;
          otherVersion = true;
        } else if (file == "-") {   // a known game whose version has no cheats: don't guess by name
          xboxLog("Cheats for %s: %s, a known game with no cheats", name.c_str(), key);
          err = "nomatch";
          return -1;
        }
      }
      xboxLog("Cheats for %s: %s -> %s%s", uiFileName(rom).c_str(), key,
              file.empty() && !otherVersion ? "(not in the index)" : name.c_str(),
              otherVersion ? " (RetroAchievements: another version's cheats)" : "");
    }
    sys = folder.c_str();
    // 1. GitHub (HTTPS): the newest codes of libretro's database and SuperFW's.
    std::vector<std::string> texts;
    std::string netErr;
    bool online = cheatsFetchOnline(name, code, sys, texts, netErr);
    int n = 0;
    for (size_t i = 0; i < texts.size(); i++) {
      FILE *t = fopen(got.c_str(), "wb");
      if (!t)
        continue;
      fwrite(texts[i].data(), 1, texts[i].size(), t);
      fclose(t);
      n += importCheats(rom, got, otherVersion);   // codes the game already has are skipped
      DeleteFile(got.c_str());
    }
    // 2. The pack that comes with the emulator (works offline; finds GBA games
    //    in any language by their code).
    bool inPack = pathExists(pack) && cheatsFetch(name, code, sys, pack, got, packErr);
    if (inPack) {
      n += importCheats(rom, got, otherVersion);
      DeleteFile(got.c_str());
    }
    xboxLog("Cheats for %s: online %s (%d files), pack %s, %d new codes", name.c_str(), online ? "found" : netErr.c_str(),
            (int)texts.size(), inPack ? "found" : packErr.c_str(), n);
    if (!online && !inPack) {
      err = netErr == "nomatch" || packErr == "nomatch" ? "nomatch" : "net";
      return -1;
    }
    if (otherVersion)
      err = "other:" + name;   // the menu warns: another version's codes, all off
    return n;
  }

  virtual int importCheatFile(const std::string &rom, const std::string &path)
  {
    return importCheats(rom, path, false);
  }

  // allOff: every cheat starts off (codes made for another version of the game).
  int importCheats(const std::string &rom, const std::string &path, bool allOff)
  {
    std::vector<UiCheatEntry> entries;
    if (!uiParseCheatFile(path, !romIsGb(rom), entries))
      return 0;
    std::vector<UiStoredCheat> list;
    loadCheatStore(rom, list);   // the player's own cheats stay as they are
    int added = 0;
    for (size_t i = 0; i < entries.size(); i++) {
      if (entries[i].codes.empty())
        continue;
      // Already there (imported before, or typed by the player): skip it.
      // One with a value to fill matches the same code with any value chosen.
      const std::string &blank = entries[i].blank;
      std::string key;
      for (size_t k = 0; k < entries[i].codes.size(); k++)
        key += cheatKey(entries[i].codes[k].code);
      bool have = false;
      for (size_t k = 0; k < list.size() && !have; k++)
        have = blank.empty() ? cheatKey(list[k].code) == key : sameBlankCode(blank, list[k].code);
      if (have)
        continue;
      UiStoredCheat c;
      c.desc = entries[i].desc;
      if (c.desc.empty()) {
        char b[32];
        sprintf(b, g_cfg.language ? "Cheat %d" : "Truco %d", (int)list.size() + 1);
        c.desc = b;
      }
      c.desc = safeDesc(c.desc);
      // Imported cheats start off unless the file says otherwise: turning
      // dozens of codes on at once tends to crash games.
      c.enabled = entries[i].enabled && blank.empty() && !allOff;
      c.type = CT_AUTO;
      for (size_t k = 0; k < entries[i].codes.size(); k++) {
        if (k) c.code += " ";
        c.code += entries[i].codes[k].code;
      }
      if (!blank.empty())
        c.code = blank;   // as written: the player picks the value when turning it on
      list.push_back(c);
      added++;
    }
    saveCheatStore(rom, list);
    if (rom == g_rom)
      syncCoreCheats();
    return added;
  }

  virtual void quitToDashboard()
  {
    if (!g_rom.empty())
      sdlWriteBattery();   // leaving from the in-game menu: keep the save
    XLaunchNewImage("", 0);
  }

  virtual void applySettings()
  {
    applySoundFlags();
    cheevosStart();   // RetroAchievements turned on / off
  }

  virtual void raPump() { cheevosIdle(); }   // menus: network replies, and the paused game
  virtual void raInfo(UiRaInfo &out) { cheevosInfo(out); }
  virtual void raLogin(const std::string &user, const std::string &password) { cheevosLogin(user, password); }
  virtual void raLogout() { cheevosLogout(); }
  virtual void raAchievements(std::vector<UiRaAchievement> &out) { cheevosList(out); }
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
  DWORD t0 = GetTickCount();
  soundPause();
  xboxCartridgeRumble(false);
  // The save isn't written here any more (it took time on every opening):
  // the game writes it soon after it changes, on exit, and quitToDashboard().
  DWORD t1 = GetTickCount();
  g_ui->openInGameMenu();
  bool firstFrame = true;
  for (;;) {
    g_ui->update(xboxReadPads(true));
    bool redraw = g_ui->needsRedraw();
    if (redraw)
      g_ui->draw(*g_menuCanvas);
    xboxUiPresent(*g_menuCanvas, redraw);
    if (firstFrame) {   // where the time goes when the menu is slow to show
      firstFrame = false;
      xboxLog("In-game menu: sound pause %u ms, menu shown after %u ms", (unsigned)(t1 - t0),
              (unsigned)(GetTickCount() - t0));
    }
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
  if (videoFrameSkip() == 0)
    systemFrameSkip = 0;
}

bool padDown(unsigned int pads, int action)
{
  int b = ctlMap()[action];
  return b >= 0 && b < PAD_BUTTON_COUNT && (pads & PAD_BIT(b)) != 0;
}

} // namespace

// For the achievement pop-ups (XboxCheevos.cpp), which make one frame slower.
void xboxResetFrameTiming() { resetFrameTiming(); }

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
  static const char *dirs[] = { "saves", "states", "cheats", "screenshots", "thumbs", "config", "bios", "borders", "backgrounds",
                                "cheats\\gba", "cheats\\gb",
                                "saves\\backups", "covers", "music", "carts", "sounds", "cheevos", "cheevos\\badges" };
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
  // Game Boy Color boot ROM (Game Boy Color games).
  extern char gambatteCgbBiosFileName[2048];
  std::string gbcBios = uiJoinPath(uiJoinPath(xboxEmulatorDir(), "bios"), "gbc_bios.bin");
  strcpy(gambatteCgbBiosFileName, g_cfg.useBios && pathExists(gbcBios) ? gbcBios.c_str() : "");
  // Super Game Boy borders and colors: Settings > Video, or this game's
  // own choice (its game options, "Custom").
  extern u8 (*gbSerialFunction)(u8);
  gbSerialFunction = g_cfg.gbPrinter ? gbPrinterSend : 0;
  kinectStop();
  gmbBridge::setCamera(g_cfg.kinect && g_cfg.kinectCamera ? kinectCapture : 0);   // no Kinect: a gray picture
  gambatteSgbMode = g_gameCfg.custom ? (g_gameCfg.sgbMode ? 2 : 0) : g_cfg.sgbBorders;

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

static int specialCartHw(const char *code3)
{
  for (size_t i = 0; i < sizeof(kSpecialCarts) / sizeof(kSpecialCarts[0]); i++)
    if (strncmp(code3, kSpecialCarts[i].code3, 3) == 0)
      return kSpecialCarts[i].hw;
  return 0;
}

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
  if (!g_cartHw)
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
  const bool tilt = (g_cartHw & UI_HW_TILT) != 0;   // GBA tilt carts and GB MBC7
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
  if (!isGba) {
    // Game Boy: only MBC7 cartridges have a sensor (tilt).
    g_cartHw = gambatteHasTilt() ? UI_HW_TILT : 0;
    return;
  }
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
  migrateOldCheats();
  syncCoreCheats();
  if (g_cfg.autoLoadState && pathExists(xboxStatePath(0)))
    sdlReadState(0);
  g_waitRelease = true;
  g_prevPads = 0;
  g_videoStretch = -1;   // force the first video setup
  g_colorMapMode = -1;   // the core rebuilt the color table at load
  g_prevValid = false;
  xboxApplySettings();
  resetFrameTiming();
  g_settleFrames = 30;
  systemScreenMessage(g_cfg.language ? "Menu: right stick click" : "Menu: clic stick derecho");
  // RetroAchievements: find the game (by the ROM in memory, after patches).
  extern u8 *gbRom;
  extern int gbRomSize;
  if (cartridgeType == 0)
    cheevosGameStarted(CHEEVOS_GBA, rom, (unsigned)xboxGbaRomSize);
  else
    cheevosGameStarted(gambatteIsCgb() ? CHEEVOS_GBC : CHEEVOS_GB, gbRom, (unsigned)gbRomSize);
  xboxRewindStart();
}

// --- Rewind (XboxRewind.h) ------------------------------------------------------
// A step is saved every REWIND_EVERY frames into REWIND_BUDGET bytes (seconds
// to minutes: only what changed is kept). Held, one step goes back every
// REWIND_SHOW frames: about 3x the game's speed, shown at 30 frames a second.
// The game's thread only copies the state out (plain bytes, no zlib); working
// out what changed (45 ms a step on the console, it took the game to 40 fps)
// happens on a thread of its own, on another core. Busy: that step is skipped.
enum { REWIND_STATE = 1 << 20, REWIND_BUDGET = 32 << 20, REWIND_EVERY = 6, REWIND_SHOW = 2 };
static std::vector<unsigned char> g_rwBuf;   // the state copied out / read back
static int g_rwFrame = 0;
static bool g_rwOk = false;   // on, and this core's state fits in REWIND_STATE
static int g_rwSetting = -1;  // Settings > Emulation > Rewind when it started
static HANDLE g_rwThread = 0, g_rwWake = 0;
static volatile LONG g_rwBusy = 0;           // the thread is saving g_rwBuf
static DWORD g_rwCopyMs = 0, g_rwSaveMs = 0;  // for the log: the game's thread / the other core
static int g_rwSaved = 0, g_rwSkipped = 0;
bool xboxRewinding = false;

static DWORD WINAPI rewindThread(void *)
{
  for (;;) {
    WaitForSingleObject(g_rwWake, INFINITE);
    DWORD t0 = GetTickCount();
    rewindPush(&g_rwBuf[0]);
    g_rwSaveMs += GetTickCount() - t0;
    MemoryBarrier();   // the Xbox 360's Interlocked calls aren't barriers: the steps first, then "done"
    InterlockedExchange(&g_rwBusy, 0);
  }
  return 0;
}

// Until the thread is done with g_rwBuf and the steps (before reading or
// resetting them on the game's thread).
static void rewindWaitIdle()
{
  while (InterlockedCompareExchange(&g_rwBusy, 0, 0))
    Sleep(1);
  MemoryBarrier();   // see what the thread wrote
}

void xboxRewindStart()
{
  rewindWaitIdle();
  g_rwFrame = 0;
  g_rwCopyMs = g_rwSaveMs = 0;
  g_rwSaved = g_rwSkipped = 0;
  g_rwSetting = g_cfg.rewind;
  g_rwOk = g_cfg.rewind && emulator.emuWriteMemState && emulator.emuReadMemState;
  if (g_rwOk) {
    g_rwBuf.assign(REWIND_STATE, 0);
    rewindReset(REWIND_STATE, REWIND_BUDGET);
    if (!g_rwThread) {
      g_rwWake = CreateEvent(NULL, FALSE, FALSE, NULL);
      g_rwThread = CreateThread(NULL, 64 * 1024, rewindThread, NULL, CREATE_SUSPENDED, NULL);
      XSetThreadProcessor(g_rwThread, 4);   // away from the game (0), the sound (2) and the network (5)
      ResumeThread(g_rwThread);
    }
  } else {
    rewindReset(0, 0);
    std::vector<unsigned char>().swap(g_rwBuf);   // off: its memory back
  }
}

bool xboxRewindStep()
{
  if (g_rwSetting != g_cfg.rewind)
    xboxRewindStart();   // turned on / off in the menu
  if (!g_rwOk || !g_rwHeld || !rewindHasState()) {   // (nothing saved yet: nothing to go back to)
    if (xboxRewinding) {   // let go: the game goes on from here
      xboxRewinding = false;
      applySoundFlags();
      resetFrameTiming();
      cheevosReset();   // its memory jumped: achievements wait for fresh conditions
    }
    if (g_rwOk && ++g_rwFrame >= REWIND_EVERY) {
      if (InterlockedCompareExchange(&g_rwBusy, 0, 0)) {
        g_rwSkipped++;   // still saving the last one: this one waits a frame
        return false;
      }
      g_rwFrame = 0;
      MemoryBarrier();   // the thread is done reading g_rwBuf
      DWORD t0 = GetTickCount();
      if (emulator.emuWriteMemState((char *)&g_rwBuf[0], REWIND_STATE)) {
        g_rwCopyMs += GetTickCount() - t0;
        MemoryBarrier();   // the state written before the thread is woken
        InterlockedExchange(&g_rwBusy, 1);
        SetEvent(g_rwWake);
        if (++g_rwSaved == 100)   // once a game: what rewind costs on this console
          xboxLog("Rewind: copy %u ms (game), save %u ms (other core) per step; %d frames waited; %d kept",
                  (unsigned)(g_rwCopyMs / 100), (unsigned)(g_rwSaveMs / 99), g_rwSkipped, rewindSteps());
      } else {
        g_rwOk = false;
        xboxLog("Rewind off: this game's state needs more than %d bytes", (int)REWIND_STATE);
      }
    }
    return false;
  }
  if (!xboxRewinding) {
    rewindWaitIdle();   // the last step saved first
    xboxRewinding = true;
    applySoundFlags();
    g_rwFrame = 0;
  }
  // One step back every REWIND_SHOW frames; in between the same moment again
  // (its picture), so it never runs forward.
  if (g_rwFrame++ % REWIND_SHOW == 0)
    rewindPop(&g_rwBuf[0]);   // at the oldest step it stays there
  emulator.emuReadMemState((char *)&g_rwBuf[0], REWIND_STATE);
  emulator.emuMain(emulator.emuCount);
  return true;
}

// Kirby Tilt 'n' Tumble's jump: the console flicked up, then back (added to
// the tilt for 8 frames). Read once a frame by the Game Boy core.
double xboxShakeY()
{
  if (g_shakeFrames <= 0)
    return 0;
  return g_shakeFrames-- > 4 ? 1.8 : -1.8;
}

void xboxOnGameExiting()
{
  cheevosGameExited();
  xboxCartridgeRumble(false);
  if (g_ui)
    g_ui->saveAll();   // keeps the sunlight level changed with the stick
  fprintf(stdout, "Exit: thumbnail\n");
  saveFrameThumb(uiGameFile(xboxEmulatorDir(), "thumbs", g_rom, "", ".tmb"));
  if (g_cfg.autoSaveState) {
    fprintf(stdout, "Exit: automatic state\n");
    g_host.saveState(0);
  }
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
    g_rwHeld = false;   // buttons held from the menu don't count
    return;
  }
  unsigned int pressed = pads & ~g_prevPads;
  g_prevPads = pads;
  updateSunFromStick();

  // Fast forward
  int ffButton = ctlMap()[ACT_FAST_FORWARD];
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

  // Rewind (held) and the console flick (Kirby Tilt 'n' Tumble's jump).
  g_rwHeld = padDown(pads, ACT_REWIND);
  if (ctlMap()[ACT_SHAKE] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(ctlMap()[ACT_SHAKE])))
    g_shakeFrames = 8;

  // Quick save / load
  if (ctlMap()[ACT_QUICK_SAVE] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(ctlMap()[ACT_QUICK_SAVE]))) {
    bool ok = g_host.saveState(g_cfg.stateSlot);
    systemScreenMessage(ok ? (g_cfg.language ? "State saved" : "Estado guardado")
                           : (g_cfg.language ? "Save failed" : "Error al guardar"));
    if (g_ui) g_ui->invalidateThumb(xboxStateThumbPath(g_cfg.stateSlot));
    resetFrameTiming();
  }
  if (ctlMap()[ACT_QUICK_LOAD] < PAD_BUTTON_COUNT && (pressed & PAD_BIT(ctlMap()[ACT_QUICK_LOAD]))) {
    bool ok = g_host.loadState(g_cfg.stateSlot);
    systemScreenMessage(ok ? (g_cfg.language ? "State loaded" : "Estado cargado")
                           : (g_cfg.language ? "Empty slot" : "Ranura vacia"));
    resetFrameTiming();
  }

  // In-game menu: only the configured button (right stick click by
  // default). Start + Select belong to the game (soft reset in many titles).
  int menuButton = ctlMap()[ACT_MENU];
  if (menuButton >= PAD_BUTTON_COUNT) {
    // Unassigned: the right stick click still opens it (the menu must always
    // be reachable), unless that button was given to something else.
    menuButton = PAD_RS;
    for (int a = 0; a < ACT_COUNT; a++)
      if (a != ACT_MENU && ctlMap()[a] == PAD_RS)
        menuButton = PAD_NONE;
  }
  // Both stick clicks at once always open it too: a way out whatever the
  // buttons were given to.
  const unsigned sticks = PAD_BIT(PAD_LS) | PAD_BIT(PAD_RS);
  bool bothSticks = (pads & sticks) == sticks && (pressed & sticks);
  if ((menuButton < PAD_BUTTON_COUNT && (pressed & PAD_BIT(menuButton))) || bothSticks)
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
extern "C" int XBOX_ShaderFilter;
extern "C" float XBOX_EffectSource[2];

void xboxSetupPicture(int srcW, int srcH)
{
  const int sw = 1280, sh = 720;
  if (srcW <= 0 || srcH <= 0) {
    XBOX_UseCustomRect = 0;
    return;
  }
  int w, h;
  if (videoStretch() == SIZE_WIDE) {
    // 16:9: the whole 1280x720 screen, no margins (the picture is stretched).
    w = sw;
    h = sh;
  } else if (videoStretch() == SIZE_FULL) {
    // Fill the screen, leaving the TV-safe margins the original port used.
    w = sw - 160;
    h = sh - 80;
  } else if (videoStretch() == SIZE_INTEGER) {
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
  XBOX_ScreenEffect = videoEffect();
  XBOX_ShaderFilter = shaderFilterFor(xboxVideoFilter());
}

static void applyGbPalette()
{
  // 15-bit BGR colors, lightest first, the same 4 shades for the
  // background and the sprites.
  const unsigned int *p = uiGbPalettes[videoGbPalette()];
  for (int i = 0; i < 8; i++) {
    unsigned int c = p[i & 3];
    systemGbPalette[i] = (u16)(((c >> 19) & 0x1F) | (((c >> 11) & 0x1F) << 5) | (((c >> 3) & 0x1F) << 10));
  }
  gbPaletteOption = 0;
  // Gambatte (Game Boy Color games keep their own colors).
  if (gambatteActive()) {
    if (videoGbPalette() != GB_PALETTE_AUTO || !gambatteSetAutoPalette())
      gambatteSetDmgPalette(p);
    return;
  }
  // A Game Boy game is running: recolor it right away.
  if (emulating && cartridgeType == 1 && !gbCgbMode && !gbSgbMode)
    for (int i = 0; i < 8; i++)
      gbPalette[i] = systemGbPalette[i];
}

// --- GBA color correction ------------------------------------------------------
// The GBA screen was dark and washed out, so games used strong colors that
// look garish on a TV. Same math as Pokefan531's gba-color shader (libretro,
// public domain), baked into the 15-bit -> screen color table the core
// already uses for every pixel: no cost while playing.
static void applyColorMap()
{
  int mode = (emulating && cartridgeType == 0) ? videoGbaColor() : 0;
  if (mode == g_colorMapMode)
    return;
  g_colorMapMode = mode;
  if (!mode) {
    utilUpdateSystemColorMaps();
    return;
  }
  // 1 = soft (gamma 2.2 in), 2 = like the real screen (the shader's
  // default "darken screen" 1.0: gamma 3.2 in).
  const float inGamma = mode == 2 ? 3.2f : 2.2f;
  const float lum = 0.94f;
  float lin[32];
  for (int v = 0; v < 32; v++)
    lin[v] = (float)pow(v / 31.0, (double)inGamma) * lum;
  unsigned char out[1024];   // linear 0..1 -> 8-bit with gamma 1/2.2
  for (int i = 0; i < 1024; i++)
    out[i] = (unsigned char)(pow(i / 1023.0, 1.0 / 2.2) * 255.0 + 0.5);
  int rs = systemRedShift - 3, gs = systemGreenShift - 3, bs = systemBlueShift - 3;
  if (rs < 0) rs = 0;
  if (gs < 0) gs = 0;
  if (bs < 0) bs = 0;
  for (int i = 0; i < 0x8000; i++) {
    float r = lin[i & 31], g = lin[(i >> 5) & 31], b = lin[(i >> 10) & 31];
    float R = 0.82f * r + 0.24f * g - 0.06f * b;
    float G = 0.125f * r + 0.665f * g + 0.21f * b;
    float B = 0.195f * r + 0.075f * g + 0.73f * b;
    int ri = (int)(R * 1023.0f + 0.5f), gi = (int)(G * 1023.0f + 0.5f), bi = (int)(B * 1023.0f + 0.5f);
    ri = ri < 0 ? 0 : ri > 1023 ? 1023 : ri;
    gi = gi < 0 ? 0 : gi > 1023 ? 1023 : gi;
    bi = bi < 0 ? 0 : bi > 1023 ? 1023 : bi;
    u32 c = ((u32)out[ri] << rs) | ((u32)out[gi] << gs) | ((u32)out[bi] << bs);
    systemColorMap32[i] = c;
    systemColorMap32[i | 0x8000] = c;
  }
}

// --- Frame blending (anti-flicker) ----------------------------------------------
// Games that blink sprites every other frame to fake transparency (Golden
// Sun, F-Zero...) relied on the slow LCD; mixing each frame with the last
// one looks like the real screen. 1 = mix 2 frames, 2 = LCD ghosting (a
// longer trail).
void xboxBlendFrame()
{
  int mode = videoFrameBlend();
  if (!mode || !pix || srcWidth <= 0 || srcHeight <= 0 || systemColorDepth != 32) {
    g_prevValid = false;
    return;
  }
  int pitch = srcPitch / 4;
  u32 *row = (u32 *)(pix + srcPitch);
  size_t n = (size_t)srcWidth * srcHeight;
  if (!g_prevValid || g_prevFrame.size() != n) {
    g_prevFrame.resize(n);
    for (int y = 0; y < srcHeight; y++)
      memcpy(&g_prevFrame[(size_t)y * srcWidth], row + y * pitch, srcWidth * 4);
    g_prevValid = true;
    return;
  }
  u32 *prev = &g_prevFrame[0];
  for (int y = 0; y < srcHeight; y++, row += pitch) {
    for (int x = 0; x < srcWidth; x++, prev++) {
      u32 c = row[x], p = *prev;
      u32 m = ((c & 0xFEFEFEFE) >> 1) + ((p & 0xFEFEFEFE) >> 1);
      row[x] = m;
      *prev = mode == 1 ? c : m;
    }
  }
}

// --- .sav backups -----------------------------------------------------------------
// When a game starts, its save is copied to saves\backups\<game>-1.sav, the
// older copies moving to -2 and -3, so a bad hack or cheat can't wipe it.
static bool readWholeFile(const std::string &path, std::vector<unsigned char> &data)
{
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  bool ok = size >= 0 && size <= 4 * 1024 * 1024;
  if (ok) {
    data.resize((size_t)size);
    ok = size == 0 || fread(&data[0], 1, (size_t)size, f) == (size_t)size;
  }
  fclose(f);
  return ok;
}

void xboxBackupSave(const char *savPath)
{
  if (!g_cfg.saveBackups || g_rom.empty() || !savPath || !pathExists(savPath))
    return;
  std::vector<unsigned char> cur, last;
  if (!readWholeFile(savPath, cur) || cur.empty())
    return;
  std::string b[3];
  for (int i = 0; i < 3; i++) {
    char suffix[8];
    sprintf(suffix, "-%d", i + 1);
    b[i] = uiGameFile(xboxEmulatorDir(), "saves\\backups", g_rom, suffix, ".sav");
  }
  if (readWholeFile(b[0], last) && last == cur)
    return;   // already the newest copy
  DeleteFile(b[2].c_str());
  rename(b[1].c_str(), b[2].c_str());
  rename(b[0].c_str(), b[1].c_str());
  FILE *f = fopen(b[0].c_str(), "wb");
  if (f) {
    bool ok = fwrite(&cur[0], 1, cur.size(), f) == cur.size();
    ok = fclose(f) == 0 && ok;
    fprintf(stdout, "Save backup %s: %s\n", b[0].c_str(), ok ? "ok" : "FAILED");
  }
}

void xboxApplySettings()
{
  // The game options may have just changed in the in-game menu.
  if (!g_rom.empty()) {
    // No file (never set, or deleted by "Reset all settings") = defaults.
    UiGameConfig fresh;
    fresh.load(uiGameFile(xboxEmulatorDir(), "config", g_rom, "", ".ini"));
    g_gameCfg = fresh;
  }
  applySoundFlags();
  applyBorder();
  applyGbPalette();
  rtcSetTimeOffset(g_cfg.clockMode ? g_cfg.clockOffset : 0);   // GBA
  gambatte::rtcTimeOffset = g_cfg.clockMode ? g_cfg.clockOffset : 0;   // GB / GBC (Pokemon Gold, Silver, Crystal...)
  XBOX_ScreenEffect = videoEffect();
  XBOX_ShaderFilter = shaderFilterFor(xboxVideoFilter());
  showSpeed = videoShowFps() ? 2 : 0;
  if (videoFrameSkip() == 0) {
    autoFrameSkip = 1;
  } else {
    autoFrameSkip = 0;
    frameSkip = gbFrameSkip = systemFrameSkip = videoFrameSkip() - 1;
  }

  if (!emulating)
    return;
  Filter wanted = coreFilterFor(xboxVideoFilter());
  if (wanted != g_videoFilter || videoStretch() != g_videoStretch) {
    // Texture size / scaling changed: rebuild the video surface.
    fprintf(stdout, "Video settings changed: rebuilding the picture\n");
    g_prevValid = false;
    g_videoFilter = wanted;
    g_videoStretch = videoStretch();
    sdlInitVideo();
    filterFunction = initFilter(filter, systemColorDepth, srcWidth);
  } else {
    XBOX_RestoreVideoState();
  }
  applySampler();
  applyColorMap();
}

Filter xboxSelectedFilter()
{
  return coreFilterFor(xboxVideoFilter());
}

// Game Boy Camera picture: the Kinect's color camera, mirrored like a selfie,
// cropped to the camera's 8:7 shape. Static noise without a Kinect.
#ifndef XBOX_NO_KINECT
static int g_kinect = 0;   // 0 off, 1 camera (color) on, 2 hand tracking on, -1 failed
static HANDLE g_kinectStream = 0;
#endif

// Turns the Kinect off: once on, its processing keeps running and slows down
// every other game. Called when a game loads; the camera turns it on again.
static void kinectStop()
{
#ifndef XBOX_NO_KINECT
  if (g_kinect > 0)
    NuiShutdown();
  g_kinect = 0;
  g_kinectStream = 0;
#endif
}

// Kinect hand cursor for the menus: the raised hand (the higher one) moves
// over the screen relative to its shoulder, like the Xbox's own Kinect menus.
static bool kinectHand(int &sx, int &sy)
{
#ifndef XBOX_NO_KINECT
  if (g_kinect == 1)
    kinectStop();   // the camera was on (Game Boy Camera): switch to tracking
  if (g_kinect == 0) {
    g_kinect = -1;
    HRESULT hr = NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                               NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD);
    HRESULT hrTrack = SUCCEEDED(hr) ? NuiSkeletonTrackingEnable(NULL, 0) : 0;
    if (SUCCEEDED(hr) && SUCCEEDED(hrTrack))
      g_kinect = 2;
    else if (SUCCEEDED(hr))
      NuiShutdown();
    xboxLog("Kinect hands: %s (NuiInitialize 0x%08X, tracking 0x%08X)", g_kinect == 2 ? "on" : "failed",
            (unsigned)hr, (unsigned)hrTrack);
  }
  if (g_kinect != 2)
    return false;
  static NUI_SKELETON_FRAME frame;
  static bool have = false;
  if (SUCCEEDED(NuiSkeletonGetNextFrame(0, &frame))) {
    NuiTransformSmooth(&frame, NULL);
    have = true;
  }
  if (!have)
    return false;
  for (int i = 0; i < NUI_SKELETON_COUNT; i++) {
    const NUI_SKELETON_DATA &sk = frame.SkeletonData[i];
    if (sk.eTrackingState != NUI_SKELETON_TRACKED)
      continue;
    const XMVECTOR *p = sk.SkeletonPositions;
    bool right = sk.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT] == NUI_SKELETON_POSITION_TRACKED;
    bool left = sk.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT] == NUI_SKELETON_POSITION_TRACKED;
    if (right && left)
      right = p[NUI_SKELETON_POSITION_HAND_RIGHT].y >= p[NUI_SKELETON_POSITION_HAND_LEFT].y;
    else if (!right && !left)
      return false;
    const XMVECTOR &hand = p[right ? NUI_SKELETON_POSITION_HAND_RIGHT : NUI_SKELETON_POSITION_HAND_LEFT];
    const XMVECTOR &shoulder = p[right ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT];
    if (hand.y < p[NUI_SKELETON_POSITION_HIP_CENTER].y)
      return false;   // hand down: no cursor
    // A box in front of the shoulder (meters) covers the screen: 60 cm wide,
    // from 25 cm above the shoulder to 35 cm below it.
    float fx = (hand.x - shoulder.x + (right ? 0.15f : 0.45f)) / 0.60f;
    float fy = (shoulder.y + 0.25f - hand.y) / 0.60f;
    fx = fx < 0 ? 0 : fx > 1 ? 1 : fx;
    fy = fy < 0 ? 0 : fy > 1 ? 1 : fy;
    sx = (int)(fx * 1279);
    sy = (int)(fy * 719);
    return true;
  }
#endif
  return false;
}

static void kinectCapture(unsigned char *gray)
{
#ifndef XBOX_NO_KINECT
  int &state = g_kinect;
  HANDLE &stream = g_kinectStream;
  static unsigned char last[128 * 112];
  static bool haveLast = false;
  if (state == 2)
    kinectStop();   // hand tracking from the menu: the camera needs color
  if (state == 0) {
    state = -1;
    HRESULT hrInit = NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR, NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD);
    HRESULT hrColor = hrInit;
    if (FAILED(hrInit))   // as the XDK's own samples do: all three streams
      hrInit = NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                             NUI_INITIALIZE_FLAG_USES_COLOR, NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD);
    HRESULT hrOpen = FAILED(hrInit) ? 0 : NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 2, NULL, &stream);
    if (SUCCEEDED(hrInit) && SUCCEEDED(hrOpen))
      state = 1;
    xboxLog("Game Boy Camera: Kinect %s (color only 0x%08X, with skeleton 0x%08X, stream 0x%08X)",
            state > 0 ? "on" : "not found", (unsigned)hrColor, (unsigned)hrInit, (unsigned)hrOpen);
  }
  const NUI_IMAGE_FRAME *frame = 0;
  if (state > 0 && SUCCEEDED(NuiImageStreamGetNextFrame(stream, 0, &frame)) && frame) {
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    if (frame->pFrameTexture && SUCCEEDED(frame->pFrameTexture->GetLevelDesc(0, &desc)) &&
        SUCCEEDED(frame->pFrameTexture->LockRect(0, &lr, NULL, D3DLOCK_READONLY))) {
      bool uyvy = desc.Format == D3DFMT_UYVY || desc.Format == D3DFMT_LIN_UYVY;
      bool yuy2 = desc.Format == D3DFMT_YUY2 || desc.Format == D3DFMT_LIN_YUY2;
      int cw = desc.Height * 8 / 7, x0 = ((int)desc.Width - cw) / 2;
      for (int y = 0; y < 112; y++) {
        const unsigned char *row = (const unsigned char *)lr.pBits + (y * desc.Height / 112) * lr.Pitch;
        for (int x = 0; x < 128; x++) {
          int sx = x0 + (127 - x) * cw / 128;
          if (uyvy || yuy2) {
            last[y * 128 + x] = row[sx * 2 + (uyvy ? 1 : 0)];
          } else {
            DWORD c = ((const DWORD *)row)[sx];   // X8R8G8B8
            last[y * 128 + x] = (unsigned char)((((c >> 16) & 0xFF) * 77 + ((c >> 8) & 0xFF) * 150 + (c & 0xFF) * 29) >> 8);
          }
        }
      }
      frame->pFrameTexture->UnlockRect(0);
      haveLast = true;
    }
    NuiImageStreamReleaseFrame(stream, frame);
  }
  if (haveLast) {   // no new frame yet: the last one
    memcpy(gray, last, sizeof(last));
    return;
  }
#endif
  for (int i = 0; i < 128 * 112; i++)
    gray[i] = (unsigned char)(rand() & 0xFF);
}

void xboxGbPrint(const unsigned char *data, int palette)
{
  extern int gbPrinterDataCount;
  int w = 160, h = gbPrinterDataCount / 320 * 8;   // 20 tiles of 16 bytes per 8-pixel row
  if (h <= 0)
    return;
  if (!palette)
    palette = 0xE4;   // games may send 0: the normal one
  std::string path;
  for (int n = 1; n < 1000 && path.empty(); n++) {
    char suffix[24];
    sprintf(suffix, "-impresion-%03d", n);
    std::string p = uiGameFile(xboxEmulatorDir(), "screenshots", g_rom, suffix, ".bmp");
    if (!pathExists(p))
      path = p;
  }
  FILE *f = path.empty() ? 0 : fopen(path.c_str(), "wb");
  if (!f)
    return;
  int row = w * 3, size = 54 + row * h;
  unsigned char hd[54] = { 'B', 'M' };
  int fields[] = { 2, size, 10, 54, 14, 40, 18, w, 22, -h, 34, row * h };   // offset, value (top-down)
  for (int i = 0; i < 12; i += 2)
    for (int b = 0; b < 4; b++)
      hd[fields[i] + b] = (unsigned char)(fields[i + 1] >> (b * 8));
  hd[26] = 1;
  hd[28] = 24;
  fwrite(hd, 1, 54, f);
  static const unsigned char shade[4] = { 255, 168, 96, 0 };
  std::vector<unsigned char> line(row);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const unsigned char *t = data + ((y / 8) * 20 + x / 8) * 16 + (y % 8) * 2;
      int bit = 7 - (x % 8);
      int c = ((t[0] >> bit) & 1) | (((t[1] >> bit) & 1) << 1);
      line[x * 3] = line[x * 3 + 1] = line[x * 3 + 2] = shade[(palette >> (c * 2)) & 3];
    }
    fwrite(&line[0], 1, row, f);
  }
  fclose(f);
  systemScreenMessage(g_cfg.language ? "Printed: saved to screenshots" : "Impreso: guardado en screenshots");
}

bool xboxPointSampler()
{
  return pointSampler();
}

int xboxVideoFilter()
{
  return gameVideoCustom() ? g_gameCfg.filter : g_cfg.filter;
}

int xboxShowFps()
{
  return videoShowFps();
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
