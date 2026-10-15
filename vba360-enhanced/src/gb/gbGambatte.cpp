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
#include "gbGlobals.h"
#include "gbCheats.h"
#include "gbGambatte.h"
#include "gbGambatteBridge.h"

extern u8 *pix;
extern bool speedup;
extern bool cheatsEnabled;
extern int systemFrameSkip;
void flush_samples(Multi_Buffer *buffer);

// Game Boy Color boot ROM (bios\gbc_bios.bin), set by the frontend.
char gambatteCgbBiosFileName[2048];
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

unsigned gmbButtons() { return gmbJoy & 0xFF; }

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
  flush_samples(gmbStereo);
  endOfFrame();
}

void gmbReset()
{
  gmbBridge::reset();
  gmbCheatKey = "?";
}

void gmbCleanUp()
{
  gmbBridge::unload();
  gmb = false;
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

// Save states: "VBAGMB01", the state size (4 bytes, big-endian), the state.
const char kStateTag[8] = { 'V', 'B', 'A', 'G', 'M', 'B', '0', '1' };

bool gmbWriteStateMem(char *mem, int available, int *used)
{
  size_t size = gmbBridge::stateSize();
  if (!size || (size_t)available < size + 12)
    return false;
  memcpy(mem, kStateTag, 8);
  mem[8] = (char)(size >> 24); mem[9] = (char)(size >> 16);
  mem[10] = (char)(size >> 8); mem[11] = (char)size;
  gmbBridge::saveState(mem + 12);
  if (used)
    *used = (int)size + 12;
  return true;
}

bool gmbReadStateMem(const char *mem, int available)
{
  if (available < 12 || memcmp(mem, kStateTag, 8))
    return false;
  size_t size = ((size_t)(u8)mem[8] << 24) | ((size_t)(u8)mem[9] << 16) |
                ((size_t)(u8)mem[10] << 8) | (size_t)(u8)mem[11];
  if (size + 12 > (size_t)available)
    return false;
  bool ok = gmbBridge::loadState(mem + 12, (unsigned)size);
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
  size_t size = gmbBridge::stateSize() + 12;
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
  return utilWritePNGFile(file, 160, 144, pix);
}

bool gmbWriteBMP(const char *file)
{
  return utilWriteBMPFile(file, 160, 144, pix);
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
  if (!gmbBridge::load(gbRom, (unsigned)gbRomSize))
    return false;
  gmb = true;
  gmbCgb = gmbBridge::isCgb();
  gmbJoy = 0;
  gmbFrameCount = gmbSkipCount = gmbSaveCheck = gmbCheatCheck = 0;
  gmbLastTime = systemGetClock();
  gmbCapturePrev = false;
  gmbCheatKey = "?";
  gmbSaveHash = saveHash();
  memset(gmbVideo, 0, sizeof(gmbVideo));
  return true;
}

bool gambatteActive()
{
  return gmb != 0;
}

bool gambatteIsCgb()
{
  return gmb && gmbCgb;
}

void gambatteSetDmgPalette(const unsigned int *rgb4)
{
  if (!gmb || gmbCgb)
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
