// The only file that includes Gambatte's headers (see gbGambatteBridge.h).
// Built with the same settings as the Gambatte files.

#ifndef HAVE_STDINT_H
#define HAVE_STDINT_H
#endif
#ifndef __LIBRETRO__
#define __LIBRETRO__
#endif
// Gambatte is built for 16-bit RGB565 output, its main (best tested) mode.
#ifndef VIDEO_RGB565
#error "Build the Gambatte files and this one with VIDEO_RGB565 defined"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string>

#include "../gambatte/include/gambatte.h"
#include "../gambatte/include/gambatte_log.h"
#include "../gambatte/include/gbcpalettes.h"
#include "../gambatte/src/mem/mbc7.h"
#include "gbGambatteBridge.h"
#define XBOX_LOG_NO_REDIRECT
#include "../Xbox/XboxLog.h"

// Pixels are 16-bit, samples uint_least32_t; the bridge uses unsigned types.
typedef char gmbCheckPixelSize[sizeof(gambatte::video_pixel_t) == sizeof(unsigned short) ? 1 : -1];
typedef char gmbCheckSampleSize[sizeof(gambatte::uint_least32_t) == sizeof(unsigned) ? 1 : -1];

extern "C" void gambatte_log(enum retro_log_level level, const char *format, ...)
{
  if (level < RETRO_LOG_INFO)
    return;
  char line[512];
  va_list args;
  va_start(args, format);
  _vsnprintf(line, sizeof(line) - 1, format, args);
  va_end(args);
  line[sizeof(line) - 1] = 0;
  xboxLog("Gambatte: %s", line);
}

namespace {
void (*rumbleFn)(bool) = 0;
}

namespace gambatte {
// In gambatte-memory.cpp (VBA360 addition).
extern void (*sgbJoypWriteHook)(unsigned data, unsigned oldValue);
extern unsigned (*sgbJoypIdleHook)();
}

// Called by Gambatte's MBC5 for rumble cartridges (Pokemon Pinball...).
void cartridge_set_rumble(unsigned active)
{
  if (rumbleFn)
    rumbleFn(active != 0);
}

namespace {

gambatte::GB *gb = 0;
unsigned (*inputFn)() = 0;
bool (*bootFn)(bool, unsigned char *, unsigned) = 0;

class Input : public gambatte::InputGetter {
public:
  unsigned operator()() { return inputFn ? inputFn() : 0; }
};
Input input;

bool bootloader(void *, bool isCgb, uint8_t *data, uint32_t size)
{
  return bootFn && bootFn(isCgb, data, size);
}

}

namespace gmbBridge {

bool load(const void *rom, unsigned size, bool forceDmg)
{
  unload();
  gb = new gambatte::GB;
  gb->setInputGetter(&input);
  gb->setBootloaderGetter(bootloader);
  unsigned flags = gambatte::GB::MULTICART_COMPAT | (forceDmg ? gambatte::GB::FORCE_DMG : 0);
  if (gb->load(rom, size, flags) != 0) {
    unload();
    return false;
  }
  // Game Boy Color colors as its screen shows them ("accurate", central
  // frontlight: gambatte-libretro's defaults).
  gb->setColorCorrectionMode(0);
  gb->setColorCorrectionBrightness(0.5f);
  gb->setColorCorrection(gb->isCgb());
  return true;
}

void unload()
{
  delete gb;
  gb = 0;
}

bool loaded() { return gb != 0; }
bool isCgb() { return gb && gb->isCgb(); }
void reset() { if (gb) gb->reset(); }

long runFor(unsigned short *video, int pitch, unsigned *sound, unsigned soundSize, unsigned &samples)
{
  return gb->runFor((gambatte::video_pixel_t *)video, pitch,
                    (gambatte::uint_least32_t *)sound, soundSize, samples);
}

void setInput(unsigned (*getButtons)()) { inputFn = getButtons; }
void setBootloader(bool (*get)(bool, unsigned char *, unsigned)) { bootFn = get; }
void setRumble(void (*set)(bool on)) { rumbleFn = set; }

bool isMbc7() { return gb && gambatte::mbc7Present; }

void setAccelerometer(double x, double y)
{
  if (x < -2) x = -2;
  if (x > 2) x = 2;
  if (y < -2) y = -2;
  if (y > 2) y = 2;
  gambatte::mbc7AccelX = (unsigned)(0x81D0 + (int)(0x70 * x));
  gambatte::mbc7AccelY = (unsigned)(0x81D0 + (int)(0x70 * y));
}

void setJoypHooks(void (*write)(unsigned, unsigned), unsigned (*idle)())
{
  gambatte::sgbJoypWriteHook = write;
  gambatte::sgbJoypIdleHook = idle;
}

void *saveData() { return gb ? gb->savedata_ptr() : 0; }
unsigned saveSize() { return gb ? gb->savedata_size() : 0; }
void *rtcData() { return gb ? gb->rtcdata_ptr() : 0; }
unsigned rtcSize() { return gb ? gb->rtcdata_size() : 0; }

unsigned stateSize() { return gb ? (unsigned)gb->stateSize() : 0; }
void saveState(void *data) { gb->saveState(data); }
bool loadState(const void *data, unsigned size) { return gb->loadState(data, size); }

void setColorCorrection(bool on) { if (gb) gb->setColorCorrection(on); }
void setDmgColor(unsigned palette, unsigned color, unsigned rgb)
{
  if (gb)
    gb->setDmgPaletteColor(palette, color,
        (gambatte::video_pixel_t)((rgb >> 19 & 0x1F) << 11 | (rgb >> 10 & 0x3F) << 5 | (rgb >> 3 & 0x1F)));
}

void setDmgColorRaw(unsigned palette, unsigned color, unsigned short pixel)
{
  if (gb)
    gb->setDmgPaletteColor(palette, color, (gambatte::video_pixel_t)pixel);
}

void setDmgColorGbc(unsigned palette, unsigned color, unsigned bgr15)
{
  if (gb)
    gb->setDmgPaletteColor(palette, color, gb->gbcToRgb32(bgr15));
}

const unsigned short *autoPalette(const char *title, bool &isGbc)
{
  // gambatte-libretro's "Auto" order: the Game Boy Color palette, or the
  // Super Game Boy one when the Color one only has a few colors.
  const unsigned short *gbc = findGbcTitlePal(title);
  const unsigned short *sgb = findSgbTitlePal(title);
  isGbc = gbc != 0;
  if (gbc && sgb && gbc != p005 && gbc != p006 && gbc != p007 && gbc != p008 &&
      gbc != p012 && gbc != p013 && gbc != p016 && gbc != p017 && gbc != p01B) {
    isGbc = false;
    return sgb;
  }
  if (gbc)
    return gbc;
  isGbc = false;
  return sgb;
}

void setCheats(const char *gameGenie, const char *gameShark)
{
  if (!gb)
    return;
  gb->clearCheats();
  if (gameGenie && gameGenie[0])
    gb->setGameGenie(gameGenie);
  if (gameShark && gameShark[0])
    gb->setGameShark(gameShark);
}

}
