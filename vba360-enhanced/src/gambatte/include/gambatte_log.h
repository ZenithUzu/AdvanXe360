#ifndef _GAMBATTE_LOG_H
#define _GAMBATTE_LOG_H

#include "libretro.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented in gb/gbGambatte.cpp (writes to the emulator log). */
void gambatte_log(enum retro_log_level level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
