// VBA360 entry point (Main.cpp). The menus live in src/ui and the Xbox glue
// in XboxHost.cpp; the old XUI scenes are no longer used.
#ifndef MAIN_H
#define MAIN_H

#include <xtl.h>

extern IDirect3DDevice9 *pDevice;

// Starts a game; returns when the player leaves it (sdl/SDL.cpp).
int gba_main(char *path, char *fname);

#endif
