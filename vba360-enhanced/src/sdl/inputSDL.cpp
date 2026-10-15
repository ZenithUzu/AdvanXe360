// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// Xbox 360 version: controllers are read directly with XInput and mapped
// through the remappable controls in the settings (see Xbox/XboxHost.cpp),
// so the SDL keymap functions below are kept only for API compatibility.
#include <xtl.h>
#include "inputSDL.h"
#include "../Xbox/XboxHost.h"

static EPad sdlDefaultJoypad = PAD_MAIN;

// Read by sdl/SDL.cpp's option parser; turbo now lives in XboxHost.cpp.
int autoFireMaxCount = 1;
static int sensorX = 2047;
static int sensorY = 2047;
static int sensorZ = 0;

void inputInitJoysticks() {}
void inputSetKeymap(EPad, EKey, uint32_t) {}
uint32_t inputGetKeymap(EPad, EKey) { return 0; }
void inputSetMotionKeymap(EKey, uint32_t) {}
bool inputToggleAutoFire(EKey) { return false; }
bool inputGetAutoFire(EKey) { return false; }
void inputProcessSDLEvent(const SDL_Event &) {}
uint32_t inputGetEventCode(const SDL_Event &) { return 0; }

uint32_t inputReadJoypad(int)
{
  return xboxReadJoypad();
}

// Tilt sensor (WarioWare: Twisted!, Yoshi Topsy-Turvy...): right stick.
void inputUpdateMotionSensor()
{
  int x, y;
  xboxGetRightStick(x, y);
  const int dz = 6000;
  int tx = (x > dz || x < -dz) ? 2047 - x * 300 / 32767 : 2047;
  int ty = (y > dz || y < -dz) ? 2047 - y * 300 / 32767 : 2047;
  // Ease towards the target so movement is smooth.
  sensorX += (tx - sensorX) / 4;
  sensorY += (ty - sensorY) / 4;
  // Rotation (WarioWare: Twisted!): right stick left/right, about -180..180
  // like VBA-M's gyro (its +-1800 range divided by 10).
  int tz = (x > dz || x < -dz) ? x * 180 / 32767 : 0;
  sensorZ += (tz - sensorZ) / 2;
}

int inputGetSensorZ() { return sensorZ; }

int inputGetSensorX() { return sensorX; }
int inputGetSensorY() { return sensorY; }

void inputSetDefaultJoypad(EPad pad) { sdlDefaultJoypad = pad; }
EPad inputGetDefaultJoypad() { return sdlDefaultJoypad; }
