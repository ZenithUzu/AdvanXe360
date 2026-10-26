// Game Boy / Game Boy Color through the Gambatte core.
//
// Gambatte: Copyright (C) 2007-2010 Sindre Aamås and the gambatte-libretro
// contributors, GPL v2 (see src/gambatte/COPYING). This file connects it to
// the VBA-M frontend: video goes into pix, sound into the same sound driver
// the other cores use, the joypad comes from systemReadJoypad().

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#include "../System.h"
#include "../Util.h"
#include "../common/Port.h"
#include "../gba/Sound.h"
#include "../apu/Multi_Buffer.h"
#include "gb.h"

double xboxShakeY();   // Xbox/XboxHost.cpp: Kirby Tilt 'n' Tumble's jump, as extra tilt
#include "gbGlobals.h"
#include "gbCheats.h"
#include "gbGambatte.h"
#include "gbGambatteBridge.h"
#include "gbGambatteSgb.h"
#include "gbPrinter.h"
#include "../Xbox/XboxLog.h"   // last: fprintf(stdout) goes to vba360.log

extern u8 *pix;
extern bool speedup;
extern bool cheatsEnabled;
extern int systemFrameSkip;
void flush_samples(Multi_Buffer *buffer);

// Game Boy Color boot ROM (bios\gbc_bios.bin), set by the frontend.
char gambatteCgbBiosFileName[2048];
// Super Game Boy mode for the next game (see gbGambatte.h).
int gambatteSgbMode = 0;
extern char gbBiosFileName[2048];

namespace {

enum {
  SAMPLES_PER_FRAME = 35112,       // stereo samples per frame, at 2 MHz
  SAMPLES_PER_RUN   = 2064,
  SOUND_BUF_SIZE    = SAMPLES_PER_RUN + 2064,
  NATIVE_RATE       = 2097152      // the Game Boy's sound clock
};

bool gmb = false;   // a game runs on Gambatte
bool gmbCgb = false;
bool gmbSgbOn = false;   // Super Game Boy mode (colors per area, border)

unsigned short gmbVideo[160 * 144];   // RGB565
unsigned gmbSound[SOUND_BUF_SIZE + 64];

// Sound: Gambatte gives one stereo sample per 2 MHz tick; Blip_Buffer turns
// the changes into band-limited output at the sound driver's rate.
Stereo_Buffer *gmbStereo = 0;
typedef Blip_Synth<blip_good_quality, 65536> GmbSynth;
GmbSynth *gmbSynthL = 0, *gmbSynthR = 0;
int gmbLastL = 0, gmbLastR = 0;
float gmbVolume = -1.0f;
long gmbSampleRate = 0;

unsigned gmbJoy = 0;
int gmbFrameCount = 0;
int gmbSkipCount = 0;
u32 gmbLastTime = 0;
bool gmbCapturePrev = false;
int gmbCaptureNumber = 0;

// Battery: Gambatte doesn't say when the game writes its save, so the save
// memory is compared every half second, like the old core's "save updated".
u32 gmbSaveHash = 0;
int gmbSaveCheck = 0;

// Cheats: the frontend keeps them in gbCheatList (GameShark / Game Genie).
std::string gmbCheatKey;
int gmbCheatCheck = 0;

unsigned gmbButtons()
{
  // In Super Game Boy multiplayer, only player 1 has a controller.
  if (gmbSgbOn && gmbSgb::currentPlayer() != 0)
    return 0;
  return gmbJoy & 0xFF;
}

bool gmbBootloader(bool isCgb, unsigned char *data, unsigned bufSize)
{
  const char *name = isCgb ? gambatteCgbBiosFileName : gbBiosFileName;
  const unsigned want = isCgb ? 0x900 : 0x100;
  if (!name[0] || want > bufSize)
    return false;
  FILE *f = fopen(name, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  // A wrong file (a Game Boy boot ROM for a Color game...) is ignored.
  bool ok = size == (long)want && fread(data, 1, want, f) == want;
  fclose(f);
  return ok;
}

u32 hashBytes(const u8 *p, unsigned n)
{
  u32 h = 2166136261u;
  for (unsigned i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

u32 saveHash()
{
  const u8 *p = (const u8 *)gmbBridge::saveData();
  if (!p)
    return 0;
  return hashBytes(p, gmbBridge::saveSize());
}

void soundSetup()
{
  long rate = soundGetSampleRate();
  if (gmbStereo && rate == gmbSampleRate)
    return;
  delete gmbSynthL;
  delete gmbSynthR;
  delete gmbStereo;
  gmbStereo = new Stereo_Buffer;
  gmbStereo->set_sample_rate(rate, 250);
  gmbStereo->clock_rate(NATIVE_RATE);
  gmbStereo->bass_freq(20);
  gmbSynthL = new GmbSynth;
  gmbSynthR = new GmbSynth;
  gmbSynthL->output(gmbStereo->left());
  gmbSynthR->output(gmbStereo->right());
  gmbLastL = gmbLastR = 0;
  gmbVolume = -1.0f;
  gmbSampleRate = rate;
}

void soundMix(unsigned samples)
{
  float vol = soundGetVolume();
  if (vol != gmbVolume) {
    gmbSynthL->volume(vol);
    gmbSynthR->volume(vol);
    gmbVolume = vol;
  }
  const unsigned *s = gmbSound;
  int l = gmbLastL, r = gmbLastR;
  for (unsigned t = 0; t < samples; t++) {
    // Left sample first in memory (high half on big-endian machines).
    u32 v = s[t];
#ifdef WORDS_BIGENDIAN
    int nl = (s16)(v >> 16), nr = (s16)(v & 0xFFFF);
#else
    int nl = (s16)(v & 0xFFFF), nr = (s16)(v >> 16);
#endif
    if (nl != l) { gmbSynthL->offset_inline(t, nl - l); l = nl; }
    if (nr != r) { gmbSynthR->offset_inline(t, nr - r); r = nr; }
  }
  gmbLastL = l;
  gmbLastR = r;
  // Stereo_Buffer only mixes the channels marked as having sound.
  gmbStereo->left()->set_modified();
  gmbStereo->right()->set_modified();
  gmbStereo->end_frame(samples);
}

void applyCheats()
{
  std::string gs, gg;
  if (cheatsEnabled) {
    for (int i = 0; i < gbCheatNumber; i++) {
      if (!gbCheatList[i].enabled)
        continue;
      std::string &dst = gbCheatList[i].code == 0x101 ? gg : gs;
      if (!dst.empty())
        dst += ';';
      dst += gbCheatList[i].cheatCode;
    }
  }
  std::string key = gs + "|" + gg;
  if (key == gmbCheatKey)
    return;
  gmbCheatKey = key;
  gmbBridge::setCheats(gg.c_str(), gs.c_str());
}

void drawFrame()
{
  if (gmbSgbOn) {
    // 256x224 with the border, laid out like the old core's border mode
    // (pitch 257, one spare row on top).
    gmbSgb::render((unsigned *)pix + 257, 257, (const unsigned *)systemColorMap32);
    return;
  }
  // The Xbox frontend always uses 32-bit color (like the other cores here).
  const unsigned short *src = gmbVideo;
  for (int y = 0; y < 144; y++) {
    u32 *dest = (u32 *)pix + 161 * (y + 1);
    for (int x = 0; x < 160; x++) {
      unsigned c = *src++;
      *dest++ = systemColorMap32[(c >> 11) | ((c >> 1) & 0x3E0) | ((c & 0x1F) << 10)];
    }
  }
}

void endOfFrame()
{
  gmbFrameCount++;
  systemFrame();
  if ((gmbFrameCount % 10) == 0)
    system10Frames(60);
  if (gmbFrameCount >= 60) {
    u32 now = systemGetClock();
    systemShowSpeed(now != gmbLastTime ? 100000 / (now - gmbLastTime) : 0);
    gmbLastTime = now;
    gmbFrameCount = 0;
  }

  if (systemReadJoypads())
    gmbJoy = systemReadJoypad(-1);
  unsigned extra = gmbJoy >> 10;
  speedup = (extra & 1) != 0;
  bool capture = (extra & 2) != 0;
  if (capture && !gmbCapturePrev)
    systemScreenCapture(++gmbCaptureNumber);
  gmbCapturePrev = capture;

  if (++gmbSaveCheck >= 30) {
    gmbSaveCheck = 0;
    u32 h = saveHash();
    if (h != gmbSaveHash) {
      gmbSaveHash = h;
      systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
    }
  }
  if (++gmbCheatCheck >= 30) {
    gmbCheatCheck = 0;
    applyCheats();
  }

  int framesToSkip = speedup ? 9 : systemFrameSkip;
  if (gmbSkipCount >= framesToSkip) {
    drawFrame();
    systemDrawScreen();
    gmbSkipCount = 0;
  } else {
    gmbSkipCount++;
  }
}

// ---- EmulatedSystem ----

void gmbEmulate(int)
{
  if (!gmb)
    return;
  soundSetup();
  if (gmbBridge::isMbc7()) {
    // Kirby Tilt 'n' Tumble: the tilt mapped in Settings > Controls
    // (sensor values are 2047 at rest, about 300 per full push).
    systemUpdateMotionSensor();
    gmbBridge::setAccelerometer((systemGetSensorX() - 2047) / 300.0,
                                (2047 - systemGetSensorY()) / 300.0 + xboxShakeY());   // + the jump flick
  }
  // Run until Gambatte finishes a frame. With the screen off it may not
  // signal one, so stop after two frames' worth of time anyway.
  unsigned total = 0;
  for (;;) {
    unsigned samples = SAMPLES_PER_RUN;
    long frameAt = gmbBridge::runFor(gmbVideo, 160, gmbSound, SOUND_BUF_SIZE, samples);
    if (samples > SOUND_BUF_SIZE)
      samples = SOUND_BUF_SIZE;
    soundMix(samples);
    total += samples;
    if (frameAt >= 0 || total >= 2 * SAMPLES_PER_FRAME)
      break;
  }
  if (gmbSgbOn)
    gmbSgb::frame(gmbVideo);   // the picture holds shades 0-3 in this mode
  flush_samples(gmbStereo);
  endOfFrame();
}

void gmbReset()
{
  gmbBridge::reset();
  if (gmbSgbOn)
    gmbSgb::reset();
  gmbCheatKey = "?";
}

void gmbCleanUp()
{
  gmbBridge::unload();
  gmbBridge::setJoypHooks(0, 0);
  gmb = false;
  gmbSgbOn = false;
  delete gmbSynthL;
  delete gmbSynthR;
  delete gmbStereo;
  gmbSynthL = gmbSynthR = 0;
  gmbStereo = 0;
  gmbSampleRate = 0;
  gmbCheatKey.clear();
  // The ROM and the rest loaded by gbLoadRom().
  gbCleanUp();
}

// Save file: the save memory as is (the same layout as the old core and other
// emulators), then for clock cartridges a "GMBR" tag and Gambatte's clock.
const char kRtcTag[4] = { 'G', 'M', 'B', 'R' };

bool gmbReadBattery(const char *file)
{
  void *ram = gmbBridge::saveData();
  unsigned ramSize = gmbBridge::saveSize();
  if (!ram || !ramSize)
    return false;
  FILE *f = fopen(file, "rb");
  if (!f)
    return false;
  size_t got = fread(ram, 1, ramSize, f);
  void *rtc = gmbBridge::rtcData();
  unsigned rtcSize = gmbBridge::rtcSize();
  char tag[4];
  if (got == ramSize && rtc && rtcSize && fread(tag, 1, 4, f) == 4 && !memcmp(tag, kRtcTag, 4)) {
    u8 buf[64];
    if (rtcSize <= sizeof(buf) && fread(buf, 1, rtcSize, f) == rtcSize)
      memcpy(rtc, buf, rtcSize);
  }
  fclose(f);
  gmbSaveHash = saveHash();
  return got > 0;
}

bool gmbWriteBattery(const char *file)
{
  void *ram = gmbBridge::saveData();
  unsigned ramSize = gmbBridge::saveSize();
  if (!ram || !ramSize)
    return true;   // nothing to save: not an error
  FILE *f = fopen(file, "wb");
  if (!f)
    return false;
  bool ok = fwrite(ram, 1, ramSize, f) == ramSize;
  void *rtc = gmbBridge::rtcData();
  unsigned rtcSize = gmbBridge::rtcSize();
  if (ok && rtc && rtcSize)
    ok = fwrite(kRtcTag, 1, 4, f) == 4 && fwrite(rtc, 1, rtcSize, f) == rtcSize;
  ok = fclose(f) == 0 && ok;
  gmbSaveHash = saveHash();
  return ok;
}

// Save states: "VBAGMB02", the state size (4 bytes, big-endian), the mode
// it was saved in (1 byte: 1 = Game Boy Color, 2 = Super Game Boy; 3 spare),
// the state; in Super Game Boy mode then "SGB1", its size and the Super Game
// Boy state. "VBAGMB01" states (no mode bytes) still load.
//
// A state only loads in the mode it was saved in: a Game Boy-mode state put
// into a game running in Color mode (or the other way round) crashed the
// emulator, e.g. after switching the Super Game Boy option with "Resume
// where I left off" on.
const char kStateTag[8] = { 'V', 'B', 'A', 'G', 'M', 'B', '0', '2' };
const char kStateTagV1[8] = { 'V', 'B', 'A', 'G', 'M', 'B', '0', '1' };
const int kHeader = 16;

unsigned stateMode()
{
  return (gmbCgb ? 1u : 0u) | (gmbSgbOn ? 2u : 0u);
}
const char kSgbTag[4] = { 'S', 'G', 'B', '1' };

void putSize(char *p, size_t size)
{
  p[0] = (char)(size >> 24); p[1] = (char)(size >> 16);
  p[2] = (char)(size >> 8); p[3] = (char)size;
}

size_t getSize(const char *p)
{
  return ((size_t)(u8)p[0] << 24) | ((size_t)(u8)p[1] << 16) |
         ((size_t)(u8)p[2] << 8) | (size_t)(u8)p[3];
}

size_t stateBytes()
{
  return kHeader + gmbBridge::stateSize() + (gmbSgbOn ? 8 + gmbSgb::stateSize() : 0);
}

bool gmbWriteStateMem(char *mem, int available, int *used)
{
  size_t size = gmbBridge::stateSize();
  size_t total = stateBytes();
  if (!size || (size_t)available < total)
    return false;
  memcpy(mem, kStateTag, 8);
  putSize(mem + 8, size);
  mem[12] = (char)stateMode();
  mem[13] = mem[14] = mem[15] = 0;
  gmbBridge::saveState(mem + kHeader);
  if (gmbSgbOn) {
    char *p = mem + kHeader + size;
    memcpy(p, kSgbTag, 4);
    putSize(p + 4, gmbSgb::stateSize());
    gmbSgb::saveState((unsigned char *)p + 8);
  }
  if (used)
    *used = (int)total;
  return true;
}

bool gmbReadStateMem(const char *mem, int available)
{
  if (available < 12)
    return false;
  int header;
  if (!memcmp(mem, kStateTag, 8)) {
    header = kHeader;
    if (available < header)
      return false;
    unsigned mode = (u8)mem[12];
    if (mode != stateMode()) {
      fprintf(stdout, "Save state not loaded: saved in mode %u, the game runs in mode %u\n",
              mode, stateMode());
      return false;
    }
  } else if (!memcmp(mem, kStateTagV1, 8)) {
    header = 12;
    if (gmbSgbOn)
      return false;   // older states were never Super Game Boy ones
  } else {
    return false;     // not a Gambatte state (e.g. one from the old core)
  }
  size_t size = getSize(mem + 8);
  if (size == 0 || size > (size_t)available - header)
    return false;
  bool ok = gmbBridge::loadState(mem + header, (unsigned)size);
  if (ok && gmbSgbOn) {
    const char *p = mem + header + size;
    size_t rest = (size_t)available - header - size;
    if (rest >= 8 && !memcmp(p, kSgbTag, 4) && getSize(p + 4) <= rest - 8)
      gmbSgb::loadState((const unsigned char *)p + 8, (unsigned)getSize(p + 4));
  }
  if (ok) {
    gmbCheatKey = "?";   // re-apply cheats over the restored memory
    gmbSkipCount = 0;
  }
  return ok;
}

bool gmbWriteState(const char *file)
{
  if (!gmb)
    return false;
  size_t size = stateBytes();
  char *buf = (char *)malloc(size);
  if (!buf)
    return false;
  int used = 0;
  bool ok = gmbWriteStateMem(buf, (int)size, &used);
  if (ok) {
    FILE *f = fopen(file, "wb");
    ok = f && fwrite(buf, 1, used, f) == (size_t)used;
    if (f)
      ok = fclose(f) == 0 && ok;
  }
  free(buf);
  return ok;
}

bool gmbReadState(const char *file)
{
  if (!gmb)
    return false;
  FILE *f = fopen(file, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  bool ok = false;
  if (size > 12 && size < 16 * 1024 * 1024) {
    char *buf = (char *)malloc(size);
    if (buf && fread(buf, 1, size, f) == (size_t)size)
      ok = gmbReadStateMem(buf, (int)size);
    free(buf);
  }
  fclose(f);
  return ok;
}

bool gmbWriteMemState(char *mem, int available)
{
  return gmb && gmbWriteStateMem(mem, available, 0);
}

bool gmbReadMemState(char *mem, int available)
{
  return gmb && gmbReadStateMem(mem, available);
}

bool gmbWritePNG(const char *file)
{
  return gmbSgbOn ? utilWritePNGFile(file, 256, 224, pix) : utilWritePNGFile(file, 160, 144, pix);
}

bool gmbWriteBMP(const char *file)
{
  return gmbSgbOn ? utilWriteBMPFile(file, 256, 224, pix) : utilWriteBMPFile(file, 160, 144, pix);
}

void sgbJoypWrite(unsigned data, unsigned old) { gmbSgb::joypWrite(data, old); }
unsigned sgbJoypIdle() { return gmbSgb::joypIdle(); }

// Does this game use Super Game Boy features, in the chosen mode?
bool wantsSgb()
{
  if (gambatteSgbMode <= 0)
    return false;
  if (gbRom[0x146] != 0x03 || gbRom[0x14B] != 0x33)
    return false;               // not a Super Game Boy game
  unsigned cgbFlag = gbRom[0x143];
  if (cgbFlag == 0xC0)
    return false;               // Game Boy Color only
  if (cgbFlag & 0x80)
    return gambatteSgbMode >= 2;   // made for both: only if asked
  return true;
}

void setPalette12(const unsigned short *bgr15)
{
  for (unsigned pal = 0; pal < 3; pal++)
    for (unsigned c = 0; c < 4; c++)
      gmbBridge::setDmgColorGbc(pal, c, bgr15[pal * 4 + c]);
}

} // namespace

struct EmulatedSystem GambatteSystem = {
  gmbEmulate,
  gmbReset,
  gmbCleanUp,
  gmbReadBattery,
  gmbWriteBattery,
  gmbReadState,
  gmbWriteState,
  gmbReadMemState,
  gmbWriteMemState,
  gmbWritePNG,
  gmbWriteBMP,
  NULL,     // emuUpdateCPSR
  false,    // emuHasDebugger
  70224     // emuCount (unused)
};

bool gambatteLoad()
{
  gmbBridge::unload();
  gmb = false;
  if (!gbRom || gbRomSize < 0x150)
    return false;
  gmbBridge::setInput(gmbButtons);
  gmbBridge::setBootloader(gmbBootloader);
  gmbBridge::setRumble(systemCartridgeRumble);
  gbPrinterReset();
  gmbBridge::setSerialDevice(gbSerialFunction);   // set by the host: printer on or off
  bool sgb = wantsSgb();
  gmbBridge::setJoypHooks(sgb ? sgbJoypWrite : 0, sgb ? sgbJoypIdle : 0);
  if (!gmbBridge::load(gbRom, (unsigned)gbRomSize, sgb)) {
    gmbBridge::setJoypHooks(0, 0);
    return false;
  }
  gmb = true;
  gmbCgb = gmbBridge::isCgb();
  gmbSgbOn = sgb && !gmbCgb;
  if (gmbSgbOn) {
    // The Super Game Boy colors the screen itself: Gambatte outputs the
    // plain shades 0-3 and gmbSgb picks the colors.
    gmbSgb::reset();
    for (unsigned pal = 0; pal < 3; pal++)
      for (unsigned c = 0; c < 4; c++)
        gmbBridge::setDmgColorRaw(pal, c, (unsigned short)c);
  }
  gmbJoy = 0;
  gmbFrameCount = gmbSkipCount = gmbSaveCheck = gmbCheatCheck = 0;
  gmbLastTime = systemGetClock();
  gmbCapturePrev = false;
  gmbCheatKey = "?";
  gmbSaveHash = saveHash();
  memset(gmbVideo, 0, sizeof(gmbVideo));
  return true;
}

bool gambatteHasTilt()
{
  return gmb && gmbBridge::isMbc7();
}

bool gambatteActive()
{
  return gmb != 0;
}

bool gambatteIsCgb()
{
  return gmb && gmbCgb;
}

bool gambatteSgbActive()
{
  return gmb && gmbSgbOn;
}

void gambatteSetDmgPalette(const unsigned int *rgb4)
{
  if (!gmb || gmbCgb || gmbSgbOn)
    return;
  gmbBridge::setColorCorrection(false);
  for (unsigned pal = 0; pal < 3; pal++)
    for (unsigned c = 0; c < 4; c++)
      gmbBridge::setDmgColor(pal, c, rgb4[c] & 0xFFFFFF);
}

bool gambatteSetAutoPalette()
{
  if (!gmb || gmbCgb)
    return false;
  if (gmbSgbOn)
    return true;   // the Super Game Boy picks the colors
  // The title in the header (0x134, up to 16 characters), as the Game Boy
  // Color boot ROM reads it.
  char title[17];
  int n = 0;
  for (; n < 16 && gbRom[0x134 + n]; n++)
    title[n] = (char)gbRom[0x134 + n];
  title[n] = 0;

  bool isGbc = false;
  const unsigned short *pal = gmbBridge::autoPalette(title, isGbc);
  if (!pal)
    return false;
  // The Color's own palettes look like its screen; the Super Game Boy ones
  // were made for a TV.
  gmbBridge::setColorCorrection(isGbc);
  setPalette12(pal);
  return true;
}
