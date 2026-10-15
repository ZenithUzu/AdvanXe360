Gambatte core used by VBA360 enhanced-ai for Game Boy and Game Boy Color
==========================================================================

Source: https://github.com/libretro/gambatte-libretro
Commit: d9d6cd06382d1ced30de34d56d3609452323dab1 (2026-08-21)
License: GNU GPL version 2 (see COPYING)
Authors: Sindre Aamås and the gambatte-libretro contributors.

Copied from that repository:
  libgambatte/src/       -> src/
  libgambatte/include/   -> include/
  common/*.h             -> include/
  libgambatte/libretro/gbcpalettes.h -> include/gbcpalettes.h

Changes made for VBA360 (all small):
  - include/gambatte-array.h, src/statesaver.cpp: the class "Array" is
    renamed "GambatteArray" (VBA-M has its own "Array" template).
  - include/gbcpalettes.h: the palette lookups use a plain table search
    instead of libretro-common's hash map (rhmap.h).
  - include/libretro.h, include/gambatte_log.h: small stand-ins, since
    VBA360 is not a libretro frontend. gambatte_log() and
    cartridge_set_rumble() are implemented in ../gb/gbGambatteBridge.cpp.

Build settings (see vbam.vcxproj): __LIBRETRO__, HAVE_STDINT_H,
VIDEO_RGB565, WORDS_BIGENDIAN (Xbox 360), include paths include/ and src/.
The connection to the emulator is ../gb/gbGambatte.cpp and
../gb/gbGambatteBridge.cpp.
