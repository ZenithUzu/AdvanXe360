// Game Boy / Game Boy Color through the Gambatte core (libretro's
// gambatte-libretro, by Sindre Aamås and contributors, GPL v2).
//
// The ROM is still loaded (and patched) by gbLoadRom() so the rest of the
// frontend keeps using gbRom / gbRomSize; Gambatte runs a copy of it.

#ifndef GB_GAMBATTE_H
#define GB_GAMBATTE_H

#include "../System.h"

extern struct EmulatedSystem GambatteSystem;

// Starts Gambatte with the ROM already in gbRom. Returns false when Gambatte
// can't run it (unsupported cartridge); the old core is used then.
bool gambatteLoad();

// True while a game runs on Gambatte.
bool gambatteActive();

// True for MBC7 cartridges (Kirby Tilt 'n' Tumble): tilt sensor.
bool gambatteHasTilt();

// True when the running game uses Game Boy Color mode.
bool gambatteIsCgb();

// Super Game Boy mode for the games loaded next: 0 off, 1 Game Boy games
// made for the Super Game Boy, 2 also the ones made for the Game Boy Color
// too (they then run as Game Boy games, with the Super Game Boy colors).
extern int gambatteSgbMode;
// True when the running game is in Super Game Boy mode: the picture is
// 256x224 with the border (the frontend's gbBorderOn layout).
bool gambatteSgbActive();

// Colors for Game Boy (not Color) games: 4 shades 0xRRGGBB, lightest first,
// used for the background and both sprite palettes.
void gambatteSetDmgPalette(const unsigned int *rgb4);

// Colors the Game Boy Color gives this game when it starts (its built-in
// per-game table, or the Super Game Boy one). Returns false for games that
// have no entry in either table.
bool gambatteSetAutoPalette();

#endif
