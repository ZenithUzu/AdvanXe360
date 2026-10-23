// Services the menu UI needs from the platform / emulator. The Xbox 360
// implementation lives in src/Xbox/XboxHost.cpp; tests use a fake host.
#ifndef UI_HOST_H
#define UI_HOST_H

#include <string>
#include <vector>

#include "UiCanvas.h"

struct UiDirEntry
{
  std::string name;   // display / file name ("Hdd1:" style for drives)
  std::string path;   // full path ("hdd:\Roms\GBA\" for folders)
  bool isDir;
  unsigned int size;  // bytes, files only
};

// What a ROM file is (UiHost::romInfo).
enum { UI_ROM_GB = 1, UI_ROM_SGB = 2, UI_ROM_CGB = 4, UI_ROM_CGB_ONLY = 8,
       UI_ROM_SENSOR = 16 };   // tilt / sun / rotation sensor (GBA special carts, GB MBC7)

// Extra hardware a cartridge has (UiHost::cartHardware).
enum { UI_HW_SOLAR = 1, UI_HW_GYRO = 2, UI_HW_RUMBLE = 4, UI_HW_TILT = 8 };

struct UiCheatInfo
{
  std::string desc;
  std::string code;
  int type;          // UiCheatType
  bool enabled;
};

// Box art download (Game options -> Cover).
enum { UI_COVER_GBA = 0, UI_COVER_GB = 1, UI_COVER_GBC = 2, UI_COVER_SYSTEMS = 3 };

struct UiCoverItem
{
  std::string romName;    // ROM file name without extension (what is matched)
  std::string coverPath;  // where the PNG is written (covers\<name>.png)
  int systems[UI_COVER_SYSTEMS];   // UI_COVER_* to look in, in order; -1 ends
};

struct UiCoverStatus
{
  bool running;           // the job is still working
  int total, done;        // games in the job / already looked up
  int found, missing;     // covers saved / games with no matching cover
  std::string current;    // game being looked up
  std::string error;      // why the job stopped ("" if it didn't)
  UiCoverStatus() : running(false), total(0), done(0), found(0), missing(0) {}
};

class UiHost
{
public:
  virtual ~UiHost() {}

  // --- Files -----------------------------------------------------------
  virtual void listDrives(std::vector<UiDirEntry> &out) = 0;
  // Folders first then files, both sorted; no "." / ".." entries.
  virtual bool listDir(const std::string &path, std::vector<UiDirEntry> &out) = 0;
  virtual bool fileExists(const std::string &path) = 0;
  // Folder the emulator runs from ("GAME:\").
  virtual std::string emulatorDir() = 0;
  // Troubleshooting (Settings): delete every save state of every game
  // (the states folder, and next to these ROMs when states are kept there);
  // delete every game's own options. Return how many files went.
  virtual int deleteAllStates(const std::vector<std::string> &knownRoms) { return 0; }
  virtual int deleteAllGameConfigs() { return 0; }

  // --- Current game (only meaningful while a game is loaded) ------------
  virtual std::string currentRom() = 0;          // full path, "" if none
  virtual bool saveState(int slot) = 0;
  virtual bool loadState(int slot) = 0;
  virtual bool stateExists(int slot) = 0;
  // Thumbnail written alongside a state (may be missing).
  virtual std::string stateThumbPath(int slot) = 0;
  virtual bool takeScreenshot(std::string &savedTo) = 0;
  virtual void resetGame() = 0;
  // Last emulated frame, 32-bit 0x00RRGGBB; returns 0 if none.
  virtual const UiColor *gameFrame(int &w, int &h, int &pitch) = 0;
  // Both sticks, raw -32768..32767 (y up = +); used to map the sensors.
  virtual void sticks(int &lx, int &ly, int &rx, int &ry) { lx = ly = rx = ry = 0; }
  // Extra cartridge hardware of the running game (UI_HW_* bits).
  virtual int cartHardware() { return 0; }
  // UI_ROM_* bits from the ROM header (0 for GBA games or unknown).
  virtual int romInfo(const std::string &rom) { return 0; }
  // The ROM inside a .zip / .7z (its name in the archive), "" if none.
  virtual std::string archiveRomName(const std::string &archive) { return ""; }
  // True when a .bps/.ups/.ips patch was applied to the running game.
  virtual bool romPatched() { return false; }
  // Writes the patched ROM next to the original, e.g. "Game (patched).gba".
  virtual bool savePatchedRom(std::string &savedTo) { return false; }

  // --- Cheats ------------------------------------------------------------
  // Each game has its own list (cheats\<game>.vch), kept whether the game
  // runs or not; changes to the running game's list apply right away.
  virtual void listCheats(const std::string &rom, std::vector<UiCheatInfo> &out) = 0;
  virtual void setCheatEnabled(const std::string &rom, int index, bool enabled) = 0;
  // type: UiCheatType (UiCheatParse.h). Returns false if the code isn't valid
  // for that type and that console.
  virtual bool addCheat(const std::string &rom, const std::string &code, int type,
                        const std::string &desc) = 0;
  virtual void removeCheat(const std::string &rom, int index) = 0;
  // Imports "<rom>.cht" / "<rom>.txt" style text lists; returns cheats added.
  virtual int importCheatFile(const std::string &rom, const std::string &path) = 0;
  // Looks the game up in libretro's online cheat database and imports what it
  // finds: codes added, or -1 with err set ("nomatch", "net"...).
  virtual int downloadCheats(const std::string &rom, std::string &err) { err = "net"; return -1; }

  // Decodes an image file (PNG, JPG, BMP...) scaled to w x h, 0xAARRGGBB.
  virtual bool loadImage(const std::string &path, int w, int h, std::vector<UiColor> &out) { return false; }
  // The same keeping the image's proportions, at most maxW x maxH; w / h get
  // the size used.
  virtual bool loadImageFit(const std::string &path, int maxW, int maxH,
                            std::vector<UiColor> &out, int &w, int &h) { return false; }

  // --- Box art download (runs in the background) ----------------------------
  // server: base URL, e.g. "http://thumbnails.libretro.com". False if a
  // download is already running or this host can't download.
  virtual bool startCoverDownload(const std::vector<UiCoverItem> &items, const std::string &server) { return false; }
  virtual void coverDownloadStatus(UiCoverStatus &out) { out = UiCoverStatus(); }
  virtual void cancelCoverDownload() {}

  // --- System --------------------------------------------------------------
  virtual void quitToDashboard() = 0;
  // Called after settings change so video/audio/input pick them up.
  virtual void applySettings() = 0;
};

#endif // UI_HOST_H
