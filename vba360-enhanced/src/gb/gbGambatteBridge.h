// Thin interface to the Gambatte core with plain C++ types only.
//
// Gambatte is built with <stdint.h>, while VBA-M's common/Types.h defines
// int32_t & co. itself (as long); the two can't be in the same file. So
// gbGambatteBridge.cpp is the only file that includes Gambatte's headers, and
// the rest of the emulator talks to it through these functions.

#ifndef GB_GAMBATTE_BRIDGE_H
#define GB_GAMBATTE_BRIDGE_H

namespace gmbBridge {

// Video: 160x144 pixels, RGB565 (red in the top 5 bits; green's lowest bit
// is always 0, so (p >> 6) & 31 is a 5-bit green). Sound: one stereo sample per 32-bit
// word, the left one in the high half on big-endian machines (as in memory).

// forceDmg: run a Game Boy Color-compatible game as a plain Game Boy (Super
// Game Boy mode).
bool load(const void *rom, unsigned size, bool forceDmg);
void unload();
bool loaded();
bool isCgb();
void reset();

// Runs until a frame is done or about 'samples' sound samples are made.
// Returns -1 when no frame was finished; 'samples' gets the samples made.
long runFor(unsigned short *video, int pitch, unsigned *sound, unsigned soundSize, unsigned &samples);

void setInput(unsigned (*getButtons)());   // A=1 B=2 Select=4 Start=8 Right Left Up Down
void setBootloader(bool (*get)(bool isCgb, unsigned char *data, unsigned size));
void setRumble(void (*set)(bool on));   // rumble cartridges
// MBC7 cartridges (Kirby Tilt 'n' Tumble): accelerometer in g (about -1..1;
// x > 0 = tilted left, y > 0 = tilted away / up, as on the real cartridge).
bool isMbc7();
void setAccelerometer(double x, double y);
// Super Game Boy: called on every write to the joypad port (FF00) with the
// new and old values, and for the low nibble read when no button row is
// selected. Null functions = a plain Game Boy.
void setJoypHooks(void (*write)(unsigned data, unsigned oldValue), unsigned (*idle)());

void *saveData();
unsigned saveSize();
void *rtcData();
unsigned rtcSize();

unsigned stateSize();
void saveState(void *data);
bool loadState(const void *data, unsigned size);

void setColorCorrection(bool on);
// Colors of Game Boy (not Color) games. palette: 0 background, 1 and 2
// sprites; color: 0 (lightest) .. 3.
void setDmgColor(unsigned palette, unsigned color, unsigned rgb);          // 0xRRGGBB
void setDmgColorGbc(unsigned palette, unsigned color, unsigned bgr15);     // a Game Boy Color color
void setDmgColorRaw(unsigned palette, unsigned color, unsigned short pixel); // output value as is

// 12 colors (background, sprites 0, sprites 1; 15-bit BGR) the Game Boy Color
// boot ROM picks for this header title, or 0. 'isGbc' is false for the
// Super Game Boy ones.
const unsigned short *autoPalette(const char *title, bool &isGbc);

// Codes separated by ';' (Game Genie "XXX-XXX-XXX", GameShark "01XXXXXX").
void setCheats(const char *gameGenie, const char *gameShark);

}

#endif
