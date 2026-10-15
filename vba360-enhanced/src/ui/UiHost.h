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

// Extra hardware a cartridge has (UiHost::cartHardware).
enum { UI_HW_SOLAR = 1, UI_HW_GYRO = 2, UI_HW_RUMBLE = 4, UI_HW_TILT = 8 };

struct UiCheatInfo
{
  std::string desc;
  std::string code;
  bool enabled;
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
  // True when a .bps/.ups/.ips patch was applied to the running game.
  virtual bool romPatched() { return false; }
  // Writes the patched ROM next to the original, e.g. "Game (patched).gba".
  virtual bool savePatchedRom(std::string &savedTo) { return false; }

  // --- Cheats ------------------------------------------------------------
  virtual void listCheats(std::vector<UiCheatInfo> &out) = 0;
  virtual void setCheatEnabled(int index, bool enabled) = 0;
  // Accepts GameShark / Action Replay / CodeBreaker / raw VBA codes for GBA
  // and GameShark / Game Genie for GB. Returns false if not recognised.
  virtual bool addCheat(const std::string &code, const std::string &desc) = 0;
  virtual void removeCheat(int index) = 0;
  // Imports "<rom>.cht" / "<rom>.txt" style text lists; returns codes added.
  virtual int importCheatFile(const std::string &path) = 0;

  // --- System --------------------------------------------------------------
  virtual void quitToDashboard() = 0;
  // Called after settings change so video/audio/input pick them up.
  virtual void applySettings() = 0;
};

#endif // UI_HOST_H
