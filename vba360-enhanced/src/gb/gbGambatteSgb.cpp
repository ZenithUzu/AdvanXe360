// Super Game Boy high-level emulation for the Gambatte core.
//
// Adapted from SameBoy's Core/sgb.c and Core/joypad.c:
//   Copyright (c) 2015-2026 Lior Halphon
//   Permission is hereby granted, free of charge, to any person obtaining a
//   copy of this software and associated documentation files (the
//   "Software"), to deal in the Software without restriction, including
//   without limitation the rights to use, copy, modify, merge, publish,
//   distribute, sublicense, and/or sell copies of the Software, and to permit
//   persons to whom the Software is furnished to do so, subject to the
//   following conditions: The above copyright notice and this permission
//   notice shall be included in all copies or substantial portions of the
//   Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
//
// Changes for VBA360: plain C++03 without SameBoy's structures, explicit
// little-endian reads (the Xbox 360 is big-endian), no boot animation,
// jingle or built-in border (a game's own border appears when it sends it;
// until then the frame is black), the border is swapped in without the
// fade.

#include <string.h>
#include "gbGambatteSgb.h"

namespace gmbSgb {

namespace {

enum {
  PAL01 = 0x00, PAL23 = 0x01, PAL03 = 0x02, PAL12 = 0x03,
  ATTR_BLK = 0x04, ATTR_LIN = 0x05, ATTR_DIV = 0x06, ATTR_CHR = 0x07,
  PAL_SET = 0x0A, PAL_TRN = 0x0B, DATA_SND = 0x0F, MLT_REQ = 0x11,
  CHR_TRN = 0x13, PCT_TRN = 0x14, ATTR_TRN = 0x15, ATTR_SET = 0x16,
  MASK_EN = 0x17
};

enum { MASK_DISABLED, MASK_FREEZE, MASK_BLACK, MASK_COLOR_0 };

enum {
  TRANSFER_LOW_TILES, TRANSFER_HIGH_TILES, TRANSFER_BORDER_DATA,
  TRANSFER_PALETTES, TRANSFER_ATTRIBUTES
};

const unsigned PACKET_BITS = 16 * 8;

// Everything a save state needs, in one block of plain bytes.
struct State {
  unsigned char command[16 * 7];
  unsigned short commandWriteIndex;
  unsigned char readyForWrite, readyForStop, disableCommands;

  unsigned char screen[160 * 144];            // last Game Boy frame
  unsigned char effectiveScreen[160 * 144];   // what is shown (mask freeze)

  unsigned char playerCount, curPlayer;
  unsigned char maskMode;
  unsigned char transferCountdown, transferDest;

  unsigned char borderTiles[0x100 * 32];      // 256 tiles, 4 bits per pixel
  unsigned char borderData[0x880];            // map (32x32) + 4 palettes, little-endian
  unsigned char pendingTiles[0x100 * 32];
  unsigned char pendingData[0x880];
  unsigned char haveBorder, pendingBorder;

  unsigned short palettes[4 * 4];             // the 4 screen palettes
  unsigned short ramPalettes[4 * 512];        // sent with PAL_TRN
  unsigned char attributeMap[20 * 18];        // palette per 8x8 area
  unsigned char attributeFiles[0xFE0];        // sent with ATTR_TRN
};

State s;

const unsigned short kDefaultPalette[4] = { 0x67BF, 0x265B, 0x10B5, 0x2866 };

inline unsigned le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

void palCommand(unsigned first, unsigned second)
{
  const unsigned char *c = s.command;
  s.palettes[0] = s.palettes[4] = s.palettes[8] = s.palettes[12] = (unsigned short)le16(c + 1);
  for (unsigned i = 0; i < 3; i++)
    s.palettes[first * 4 + i + 1] = (unsigned short)le16(c + 3 + i * 2);
  for (unsigned i = 0; i < 3; i++)
    s.palettes[second * 4 + i + 1] = (unsigned short)le16(c + 9 + i * 2);
}

void loadAttributeFile(unsigned index)
{
  if (index > 0x2C)
    return;
  unsigned char *out = s.attributeMap;
  for (unsigned i = 0; i < 90; i++) {
    unsigned char b = s.attributeFiles[index * 90 + i];
    for (unsigned j = 0; j < 4; j++) {
      *out++ = b >> 6;
      b = (unsigned char)(b << 2);
    }
  }
}

void ramPalette(unsigned dst, unsigned lo, unsigned hi)
{
  unsigned index = (lo + (hi & 1) * 0x100) * 4;
  for (unsigned i = 0; i < 4; i++)
    s.palettes[dst * 4 + i] = s.ramPalettes[index + i];
}

void commandReady()
{
  const unsigned char *c = s.command;
  // Header packets (F1, F3 ... FB) are the SNES checking the cartridge.
  if ((c[0] & 0xF1) == 0xF1)
    return;
  if ((c[0] & 7) == 0)
    return;   // malformed (0 packets)

  switch (c[0] >> 3) {
  case PAL01: palCommand(0, 1); break;
  case PAL23: palCommand(2, 3); break;
  case PAL03: palCommand(0, 3); break;
  case PAL12: palCommand(1, 2); break;

  case ATTR_BLK: {
    unsigned count = c[1];
    if (count > 0x12)
      return;
    for (unsigned i = 0; i < count; i++) {
      const unsigned char *d = c + 2 + i * 6;
      bool inside = (d[0] & 1) != 0, middle = (d[0] & 2) != 0, outside = (d[0] & 4) != 0;
      unsigned inPal = d[1] & 3, midPal = (d[1] >> 2) & 3, outPal = (d[1] >> 4) & 3;
      if (inside && !middle && !outside) {
        middle = true;
        midPal = inPal;
      } else if (outside && !middle && !inside) {
        middle = true;
        midPal = outPal;
      }
      unsigned left = d[2] & 0x1F, top = d[3] & 0x1F, right = d[4] & 0x1F, bottom = d[5] & 0x1F;
      for (unsigned y = 0; y < 18; y++)
        for (unsigned x = 0; x < 20; x++) {
          if (x < left || x > right || y < top || y > bottom) {
            if (outside)
              s.attributeMap[x + 20 * y] = (unsigned char)outPal;
          } else if (x > left && x < right && y > top && y < bottom) {
            if (inside)
              s.attributeMap[x + 20 * y] = (unsigned char)inPal;
          } else if (middle) {
            s.attributeMap[x + 20 * y] = (unsigned char)midPal;
          }
        }
    }
    break;
  }

  case ATTR_CHR: {
    unsigned x = c[1], y = c[2];
    unsigned count = le16(c + 3);
    unsigned vertical = c[5];
    if (x >= 20 || y >= 18)
      break;
    const unsigned maxCount = (sizeof(s.command) - 6) * 4;
    if (count > maxCount)
      count = maxCount;
    for (unsigned i = 0; i < count; i++) {
      unsigned pal = (c[6 + i / 4] >> (((~i) & 3) << 1)) & 3;
      s.attributeMap[x + 20 * y] = (unsigned char)pal;
      if (vertical) {
        if (++y == 18) {
          y = 0;
          if (++x == 20)
            break;
        }
      } else {
        if (++x == 20) {
          x = 0;
          if (++y == 18)
            break;
        }
      }
    }
    break;
  }

  case ATTR_LIN: {
    unsigned count = c[1];
    if (count > sizeof(s.command) - 2)
      return;
    for (unsigned i = 0; i < count; i++) {
      unsigned d = c[2 + i];
      unsigned pal = (d >> 5) & 3, line = d & 0x1F;
      if (d & 0x80) {
        if (line >= 18) continue;
        for (unsigned x = 0; x < 20; x++)
          s.attributeMap[x + 20 * line] = (unsigned char)pal;
      } else {
        if (line >= 20) continue;
        for (unsigned y = 0; y < 18; y++)
          s.attributeMap[line + 20 * y] = (unsigned char)pal;
      }
    }
    break;
  }

  case ATTR_DIV: {
    unsigned high = c[1] & 3, low = (c[1] >> 2) & 3, mid = (c[1] >> 4) & 3;
    bool horizontal = (c[1] & 0x40) != 0;
    unsigned line = c[2] & 0x1F;
    for (unsigned y = 0; y < 18; y++)
      for (unsigned x = 0; x < 20; x++) {
        unsigned v = horizontal ? y : x;
        s.attributeMap[x + 20 * y] = (unsigned char)(v < line ? low : v == line ? mid : high);
      }
    break;
  }

  case PAL_SET:
    ramPalette(0, c[1], c[2]);
    ramPalette(1, c[3], c[4]);
    ramPalette(2, c[5], c[6]);
    ramPalette(3, c[7], c[8]);
    s.palettes[12] = s.palettes[8] = s.palettes[4] = s.palettes[0];
    if (c[9] & 0x80)
      loadAttributeFile(c[9] & 0x3F);
    if (c[9] & 0x40)
      s.maskMode = MASK_DISABLED;
    break;

  case PAL_TRN:  s.transferCountdown = 3; s.transferDest = TRANSFER_PALETTES; break;
  case CHR_TRN:  s.transferCountdown = 3; s.transferDest = (c[1] & 1) ? TRANSFER_HIGH_TILES : TRANSFER_LOW_TILES; break;
  case PCT_TRN:  s.transferCountdown = 3; s.transferDest = TRANSFER_BORDER_DATA; break;
  case ATTR_TRN: s.transferCountdown = 3; s.transferDest = TRANSFER_ATTRIBUTES; break;

  case ATTR_SET:
    loadAttributeFile(c[1] & 0x3F);
    if (c[1] & 0x40)
      s.maskMode = MASK_DISABLED;
    break;

  case MASK_EN:
    s.maskMode = c[1] & 3;
    break;

  case MLT_REQ:
    // Player count 1, 2 or 4 (3 behaves oddly on the real thing too).
    s.playerCount = (unsigned char)((c[1] & 3) + 1);
    if (s.playerCount == 3)
      s.curPlayer++;
    s.curPlayer &= s.playerCount - 1;
    break;

  default:
    // DATA_SND, sound commands and the rest: nothing to show.
    break;
  }
}

// The bytes a transfer sends: the screen read as Game Boy tiles (20 per
// row), 16 bytes per tile, the way the Super Game Boy reads it.
void readTransfer(unsigned char *dst, unsigned tiles)
{
  for (unsigned tile = 0; tile < tiles; tile++) {
    unsigned tx = (tile % 20) * 8, ty = (tile / 20) * 8;
    for (unsigned y = 0; y < 8; y++) {
      unsigned lo = 0, hi = 0;
      const unsigned char *row = s.screen + (ty + y) * 160 + tx;
      for (unsigned x = 0; x < 8; x++) {
        lo |= (row[x] & 1) << (7 - x);
        hi |= ((row[x] >> 1) & 1) << (7 - x);
      }
      *dst++ = (unsigned char)lo;
      *dst++ = (unsigned char)hi;
    }
  }
}

void finishTransfer()
{
  switch (s.transferDest) {
  case TRANSFER_LOW_TILES:
    readTransfer(s.pendingTiles, 0x100);
    break;
  case TRANSFER_HIGH_TILES:
    readTransfer(s.pendingTiles + 0x1000, 0x100);
    break;
  case TRANSFER_BORDER_DATA:
    readTransfer(s.pendingData, 0x88);
    // The new border is shown with the tiles sent so far.
    memcpy(s.borderTiles, s.pendingTiles, sizeof(s.borderTiles));
    memcpy(s.borderData, s.pendingData, sizeof(s.borderData));
    s.haveBorder = 1;
    break;
  case TRANSFER_PALETTES: {
    unsigned char buf[0x1000];
    readTransfer(buf, 0x100);
    for (unsigned i = 0; i < 4 * 512; i++)
      s.ramPalettes[i] = (unsigned short)le16(buf + i * 2);
    break;
  }
  case TRANSFER_ATTRIBUTES:
    readTransfer(s.attributeFiles, 0xFE);
    break;
  }
}

} // namespace

void reset()
{
  memset(&s, 0, sizeof(s));
  s.playerCount = 1;
  for (unsigned i = 0; i < 4; i++)
    s.palettes[i] = s.palettes[4 + i] = s.palettes[8 + i] = s.palettes[12 + i] = kDefaultPalette[i];
}

void joypWrite(unsigned value, unsigned oldReg)
{
  if (s.disableCommands)
    return;
  unsigned commandSize = ((s.command[0] & 7) ? (s.command[0] & 7) : 1) * PACKET_BITS;
  if ((s.command[0] & 0xF1) == 0xF1)
    commandSize = PACKET_BITS;
  unsigned old = (oldReg >> 4) & 3;

  // P15 rising with 2 or 4 players: the next controller answers.
  if ((value & 0x20) && !(old & 2) && (s.playerCount & 1) == 0) {
    s.curPlayer++;
    s.curPlayer &= s.playerCount - 1;
  }

  switch ((value >> 4) & 3) {
  case 3:
    if (old == 0) {
      s.readyForWrite = 1;
    } else if ((old == 1 || old == 2) && s.readyForWrite) {
      if (s.readyForStop) {
        if (s.commandWriteIndex == commandSize) {
          commandReady();
          s.commandWriteIndex = 0;
          memset(s.command, 0, sizeof(s.command));
        }
        s.readyForWrite = 0;
        s.readyForStop = 0;
      } else if (s.commandWriteIndex < sizeof(s.command) * 8) {
        if (old == 1)
          s.command[s.commandWriteIndex / 8] |= (unsigned char)(1 << (s.commandWriteIndex & 7));
        s.commandWriteIndex++;
        if ((s.commandWriteIndex & (PACKET_BITS - 1)) == 0)
          s.readyForStop = 1;
      }
    }
    break;
  case 0:
    s.readyForWrite = 0;
    if ((s.commandWriteIndex & (PACKET_BITS - 1)) != 0 || s.commandWriteIndex == 0 || s.readyForStop) {
      s.commandWriteIndex = 0;
      memset(s.command, 0, sizeof(s.command));
      s.readyForStop = 0;
    }
    break;
  default:
    break;   // a 0 or 1 bit: counted on the following 3
  }
}

unsigned joypIdle()
{
  return s.playerCount > 1 ? (0xF - s.curPlayer) & 0xF : 0xF;
}

int currentPlayer()
{
  return s.curPlayer;
}

void frame(const unsigned short *shades)
{
  for (unsigned i = 0; i < 160 * 144; i++)
    s.screen[i] = (unsigned char)(shades[i] & 3);
  if (s.transferCountdown && --s.transferCountdown == 0)
    finishTransfer();
  if (s.maskMode != MASK_FREEZE)
    memcpy(s.effectiveScreen, s.screen, sizeof(s.screen));
}

void render(unsigned *dest, int pitch, const unsigned *colorMap)
{
  unsigned colors[16];
  for (unsigned i = 0; i < 16; i++)
    colors[i] = colorMap[s.palettes[i] & 0x7FFF];
  const unsigned black = colorMap[0];

  // Without a border from the game the frame stays black.
  unsigned back = s.haveBorder ? colors[0] : black;
  for (int y = 0; y < 224; y++) {
    unsigned *row = dest + y * pitch;
    for (int x = 0; x < 256; x++)
      row[x] = back;
  }

  // The Game Boy picture, in the middle (48, 40).
  const unsigned char *in = s.effectiveScreen;
  for (unsigned y = 0; y < 144; y++) {
    unsigned *out = dest + (40 + y) * pitch + 48;
    for (unsigned x = 0; x < 160; x++, in++) {
      if (s.maskMode == MASK_BLACK)
        out[x] = black;
      else if (s.maskMode == MASK_COLOR_0)
        out[x] = colors[0];
      else
        out[x] = colors[(*in & 3) + (s.attributeMap[x / 8 + (y / 8) * 20] & 3) * 4];
    }
  }

  if (!s.haveBorder)
    return;

  unsigned borderColors[16 * 4];
  for (unsigned i = 0; i < 16 * 4; i++)
    borderColors[i] = colorMap[le16(s.borderData + 0x800 + i * 2) & 0x7FFF];

  for (unsigned ty = 0; ty < 28; ty++) {
    for (unsigned tx = 0; tx < 32; tx++) {
      bool gbArea = tx >= 6 && tx < 26 && ty >= 5 && ty < 23;
      unsigned tile = le16(s.borderData + (tx + ty * 32) * 2);
      if (tile & 0x300)
        continue;   // unused tile
      unsigned flipX = (tile & 0x4000) ? 0 : 7;
      unsigned flipY = (tile & 0x8000) ? 7 : 0;
      unsigned pal = (tile >> 10) & 3;
      for (unsigned y = 0; y < 8; y++) {
        unsigned base = (tile & 0xFF) * 32 + (y ^ flipY) * 2;
        unsigned *out = dest + (ty * 8 + y) * pitch + tx * 8;
        for (unsigned x = 0; x < 8; x++) {
          unsigned bit = 1u << (x ^ flipX);
          unsigned color = ((s.borderTiles[base] & bit) ? 1 : 0) |
                           ((s.borderTiles[base + 1] & bit) ? 2 : 0) |
                           ((s.borderTiles[base + 16] & bit) ? 4 : 0) |
                           ((s.borderTiles[base + 17] & bit) ? 8 : 0);
          if (color == 0) {
            if (!gbArea)
              out[x] = colors[0];
          } else {
            out[x] = borderColors[color + pal * 16];
          }
        }
      }
    }
  }
}

unsigned stateSize()
{
  return sizeof(State);
}

void saveState(unsigned char *out)
{
  memcpy(out, &s, sizeof(State));
}

bool loadState(const unsigned char *in, unsigned size)
{
  if (size != sizeof(State))
    return false;
  memcpy(&s, in, sizeof(State));
  return true;
}

}
