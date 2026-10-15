// Super Game Boy features for games running on Gambatte: the commands a game
// sends through the joypad port (colors per screen area, its own border,
// multiplayer detection) and the 256x224 picture with the border.
//
// High-level emulation adapted from SameBoy's Core/sgb.c
// (Copyright (c) 2015-2026 Lior Halphon, Expat/MIT license).

#ifndef GB_GAMBATTE_SGB_H
#define GB_GAMBATTE_SGB_H

namespace gmbSgb {

void reset();            // power on: no border, default colors

// Joypad port (FF00) hooks.
void joypWrite(unsigned data, unsigned old);
unsigned joypIdle();     // low nibble read when neither button row is selected
int currentPlayer();     // 0 = player 1 (the only controller here)

// Once per Game Boy frame. 'shades' is the 160x144 picture as Game Boy
// shades 0 (lightest) .. 3, after the game's own palette registers.
void frame(const unsigned short *shades);

// The 256x224 picture with the border. colorMap turns a 15-bit color
// (red in the low bits) into the frontend's pixel format; pitch in pixels.
void render(unsigned *dest, int pitch, const unsigned *colorMap);

// Save states.
unsigned stateSize();
void saveState(unsigned char *out);
bool loadState(const unsigned char *in, unsigned size);

}

#endif
