#ifndef PATCH_H
#define PATCH_H

#include "Types.h"

bool applyPatch(const char *patchname, u8 **rom, int *size);

// Tell the patcher the ROM buffer is a fixed allocation of `capacity` bytes
// that must not be realloc'd (0 = normal malloc'd buffer).
void patchSetFixedCapacity(int capacity);

#endif // PATCH_H
