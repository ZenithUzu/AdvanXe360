/* Minimal stand-in for libretro.h: the Gambatte core only needs the log
 * levels. VBA360 is not a libretro frontend. */
#ifndef GAMBATTE_VBA360_LIBRETRO_H
#define GAMBATTE_VBA360_LIBRETRO_H

enum retro_log_level {
   RETRO_LOG_DEBUG = 0,
   RETRO_LOG_INFO,
   RETRO_LOG_WARN,
   RETRO_LOG_ERROR,
   RETRO_LOG_DUMMY = 0x7fffffff
};

#endif
