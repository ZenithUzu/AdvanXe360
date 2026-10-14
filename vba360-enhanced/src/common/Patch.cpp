#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "Patch.h"


#ifdef __GNUC__
#if defined(__APPLE__) || defined (BSD)
typedef off_t __off64_t; /* off_t is 64 bits on BSD. */
#define	fseeko64 fseeko
#define	ftello64 ftello
#else
typedef off64_t __off64_t;
#endif /* __APPLE__ || BSD */
#endif /* __GNUC__ */

#ifndef _MSC_VER
#define _stricmp strcasecmp
#endif // ! _MSC_VER

#ifdef _MSC_VER
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
typedef	__int64 __off64_t;
#endif

static int readInt2(FILE *f)
{
  int res = 0;
  int c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c;
  c = fgetc(f);
  if(c == EOF)
    return -1;
  return c + (res<<8);
}


static s64 readInt4(FILE *f)
{
  s64 tmp, res = 0;
  int c;

  for (int i = 0; i < 4; i++) {
    c = fgetc(f);
    if (c == EOF)
      return -1;
    tmp = c;
    res = res + (tmp << (i*8));
  }

  return res;
}

static s64 readInt8(FILE *f)
{
  s64 tmp, res = 0;
  int c;

  for (int i = 0; i < 8; i++) {
    c = fgetc(f);
    if (c == EOF)
      return -1;
    tmp = c;
    res = res + (tmp << (i*8));
  }

  return res;
}


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// When non-zero, the ROM buffer is a fixed allocation of this many bytes.
// On Xbox 360 the GBA ROM lives in XPhysicalAlloc memory, which must never
// be passed to realloc; patches may only grow the ROM up to this capacity.
static int patchFixedCapacity = 0;

void patchSetFixedCapacity(int capacity)
{
  patchFixedCapacity = capacity;
}

// Grows the ROM to newSize bytes, zero-filling the new area.
static bool patchGrowRom(u8 **rom, int *size, s64 newSize)
{
  if (newSize <= *size)
    return true;
  if (patchFixedCapacity) {
    if (newSize > patchFixedCapacity)
      return false;
  } else {
    u8 *p = (u8 *)realloc(*rom, (size_t)newSize);
    if (!p)
      return false;
    *rom = p;
  }
  memset(*rom + *size, 0, (size_t)(newSize - *size));
  *size = (int)newSize;
  return true;
}


// Reads a whole patch file into memory. Parsing from memory is much faster
// on the Xbox 360 than reading the patch byte by byte with fgetc/ftell.
static u8 *patchLoadFile(const char *patchname, size_t &len)
{
  len = 0;
  FILE *f = fopen(patchname, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 0x4000000) {
    fclose(f);
    return NULL;
  }
  u8 *data = (u8 *)malloc((size_t)n);
  if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
    free(data);
    data = NULL;
  }
  fclose(f);
  if (data)
    len = (size_t)n;
  return data;
}

static bool patchApplyIPS(const char *patchname, u8 **r, int *s)
{
  // from the IPS spec at http://zerosoft.zophar.net/ips.htm
  size_t len = 0;
  u8 *p = patchLoadFile(patchname, len);
  if (!p)
    return false;

  bool result = false;
  u8 *rom = *r;
  int size = *s;
  if (len >= 5 && memcmp(p, "PATCH", 5) == 0) {
    result = true;
    size_t pos = 5;
    for (;;) {
      // offset (3 bytes, big endian); "EOF" or the end of the file ends it
      if (pos + 3 > len)
        break;
      int offset = (p[pos] << 16) | (p[pos + 1] << 8) | p[pos + 2];
      pos += 3;
      if (offset == 0x454f46)
        break;
      if (pos + 2 > len)
        break;
      int count = (p[pos] << 8) | p[pos + 1];
      pos += 2;
      int fill = -1;
      if (!count) {
        // RLE block: length, then the byte to fill with
        if (pos + 3 > len)
          break;
        count = (p[pos] << 8) | p[pos + 1];
        fill = p[pos + 2];
        pos += 3;
      } else if (pos + (size_t)count > len) {
        break;
      }
      // grow the ROM if this record writes past its end
      if (offset + count > size) {
        if (!patchGrowRom(&rom, &size, (s64)offset + count)) {
          result = false;
          break;
        }
        *r = rom;
        *s = size;
      }
      if (fill < 0) {
        memcpy(&rom[offset], p + pos, (size_t)count);
        pos += (size_t)count;
      } else {
        memset(&rom[offset], fill, (size_t)count);
      }
    }
  }
  free(p);
  return result;
}

static u32 upsRead32(const u8 *p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static s64 upsReadVar(const u8 *p, size_t end, size_t &pos)
{
  s64 offset = 0, shift = 1;
  for (;;) {
    if (pos >= end)
      return 0;
    u8 c = p[pos++];
    offset += (c & 0x7F) * shift;
    if (c & 0x80)
      break;
    shift <<= 7;
    offset += shift;
  }
  return offset;
}

static bool patchApplyUPS(const char *patchname, u8 **rom, int *size)
{
  size_t len = 0;
  u8 *p = patchLoadFile(patchname, len);
  if (!p)
    return false;
  if (len < 20 || memcmp(p, "UPS1", 4) != 0) {
    free(p);
    return false;
  }

  u32 srcCRC = upsRead32(p + len - 12);
  u32 dstCRC = upsRead32(p + len - 8);
  u32 patchCRC = upsRead32(p + len - 4);
  if (crc32(crc32(0L, Z_NULL, 0), p, (uInt)(len - 4)) != patchCRC) {
    free(p);
    return false;
  }

  u32 crc = crc32(crc32(0L, Z_NULL, 0), *rom, *size);
  size_t pos = 4;
  const size_t end = len - 12;
  s64 srcSize = upsReadVar(p, end, pos);
  s64 dstSize = upsReadVar(p, end, pos);
  s64 dataSize;
  if (crc == srcCRC && srcSize == *size) {
    dataSize = dstSize;          // apply
  } else if (crc == dstCRC && dstSize == *size) {
    free(p);                     // the ROM is already patched: keep it
    return true;
  } else {
    free(p);
    return false;
  }
  if (!patchGrowRom(rom, size, dataSize)) {
    free(p);
    return false;
  }

  // Each block: distance to skip, then bytes to XOR up to a 0 terminator.
  // Bytes past the output size are still consumed (when undoing a patch
  // that grew the ROM); skipping them would misread the rest of the patch.
  s64 relative = 0;
  bool ok = true;
  while (pos < end) {
    relative += upsReadVar(p, end, pos);
    if (relative < 0) {
      ok = false;
      break;
    }
    while (pos < end) {
      u8 x = p[pos++];
      if (relative < dataSize)
        (*rom)[relative] ^= x;
      relative++;
      if (!x)
        break;
    }
  }
  if (ok && dataSize < *size)
    *size = (int)dataSize;
  free(p);
  return ok;
}

// BPS (beat) patches, the most common format for modern ROM hacks.
// Spec: https://www.romhacking.net/documents/746/
static u64 bpsReadVar(const u8 *p, size_t end, size_t &pos, bool &ok)
{
  u64 data = 0, shift = 1;
  for (;;) {
    if (pos >= end) { ok = false; return 0; }
    u8 x = p[pos++];
    data += (u64)(x & 0x7f) * shift;
    if (x & 0x80) break;
    shift <<= 7;
    data += shift;
  }
  return data;
}

static u32 bpsRead32(const u8 *p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static bool patchApplyBPS(const char *patchname, u8 **rom, int *size)
{
  FILE *f = fopen(patchname, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long patchSize = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (patchSize < 19) {
    fclose(f);
    return false;
  }
  u8 *patch = (u8 *)malloc(patchSize);
  if (!patch) {
    fclose(f);
    return false;
  }
  bool readOk = fread(patch, 1, patchSize, f) == (size_t)patchSize;
  fclose(f);

  u8 *source = NULL;
  bool ok = readOk && memcmp(patch, "BPS1", 4) == 0;

  // Footer: source CRC, target CRC, patch CRC (all little endian)
  const size_t footer = patchSize - 12;
  if (ok) {
    u32 patchCRC = bpsRead32(patch + patchSize - 4);
    ok = crc32(crc32(0L, Z_NULL, 0), patch, patchSize - 4) == patchCRC;
  }
  if (ok) {
    u32 srcCRC = bpsRead32(patch + footer);
    ok = crc32(crc32(0L, Z_NULL, 0), *rom, *size) == srcCRC;
  }

  size_t pos = 4;
  u64 sourceSize = 0, targetSize = 0;
  if (ok) {
    sourceSize = bpsReadVar(patch, footer, pos, ok);
    targetSize = bpsReadVar(patch, footer, pos, ok);
    u64 metaSize = bpsReadVar(patch, footer, pos, ok);
    pos += (size_t)metaSize;
    ok = ok && sourceSize == (u64)*size && pos <= footer;
  }

  // Keep an untouched copy of the source; the ROM buffer becomes the target.
  if (ok) {
    source = (u8 *)malloc((size_t)sourceSize);
    ok = source != NULL;
  }
  if (ok) {
    memcpy(source, *rom, (size_t)sourceSize);
    ok = patchGrowRom(rom, size, (s64)targetSize);
  }

  u8 *target = ok ? *rom : NULL;
  u64 out = 0, srcRel = 0, tgtRel = 0;
  while (ok && pos < footer) {
    u64 data = bpsReadVar(patch, footer, pos, ok);
    u64 length = (data >> 2) + 1;
    if (!ok || out + length > targetSize) { ok = false; break; }
    switch (data & 3) {
    case 0: // SourceRead
      if (out + length > sourceSize) { ok = false; break; }
      memcpy(target + out, source + out, (size_t)length);
      out += length;
      break;
    case 1: // TargetRead
      if (pos + length > footer) { ok = false; break; }
      memcpy(target + out, patch + pos, (size_t)length);
      pos += (size_t)length;
      out += length;
      break;
    case 2: { // SourceCopy
      u64 d = bpsReadVar(patch, footer, pos, ok);
      srcRel += (d & 1) ? -(s64)(d >> 1) : (s64)(d >> 1);
      if (!ok || srcRel + length > sourceSize) { ok = false; break; }
      memcpy(target + out, source + srcRel, (size_t)length);
      srcRel += length;
      out += length;
      break;
    }
    case 3: { // TargetCopy (may overlap, so copy byte by byte)
      u64 d = bpsReadVar(patch, footer, pos, ok);
      tgtRel += (d & 1) ? -(s64)(d >> 1) : (s64)(d >> 1);
      if (!ok || tgtRel >= out) { ok = false; break; }
      while (length--)
        target[out++] = target[tgtRel++];
      break;
    }
    }
  }

  if (ok) {
    u32 tgtCRC = bpsRead32(patch + footer + 4);
    ok = out == targetSize &&
         crc32(crc32(0L, Z_NULL, 0), target, (uInt)targetSize) == tgtCRC;
    if (ok)
      *size = (int)targetSize;
  }

  // On failure after the ROM was touched, put the original back.
  if (!ok && source && target) {
    memcpy(*rom, source, (size_t)sourceSize);
    *size = (int)sourceSize;
  }

  free(source);
  free(patch);
  return ok;
}

static int ppfVersion(FILE *f)
{
  fseeko64(f, 0, SEEK_SET);
  if (fgetc(f) != 'P' || fgetc(f) != 'P' || fgetc(f) != 'F')
    return 0;
  switch(fgetc(f)){
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    default: return 0;
  }
}

static int ppfFileIdLen(FILE *f, int version)
{
  if (version == 2) {
    fseeko64(f, -8, SEEK_END);
  } else {
    fseeko64(f, -6, SEEK_END);
  }

  if (fgetc(f) != '.' || fgetc(f) != 'D' || fgetc(f) != 'I' || fgetc(f) != 'Z')
    return 0;

  return (version == 2) ? readInt4(f) : readInt2(f);
}

static bool patchApplyPPF1(FILE *f, u8 **rom, int *size)
{
  fseek(f, 0, SEEK_END);
  int count = ftell(f);
  if (count < 56)
    return false;
  count -= 56;

  fseek(f, 56, SEEK_SET);

  u8 *mem = *rom;

  while (count > 0) {
    int offset = readInt4(f);
    if (offset == -1)
      break;
    int len = fgetc(f);
    if (len == EOF)
      break;
    if (offset+len > *size)
      break;
    if (fread(&mem[offset], 1, len, f) != (size_t)len)
      break;
    count -= 4 + 1 + len;
  }

  return (count == 0);
}

static bool patchApplyPPF2(FILE *f, u8 **rom, int *size)
{
  fseek(f, 0, SEEK_END);
  int count = ftell(f);
  if (count < 56+4+1024)
    return false;
  count -= 56+4+1024;

  fseek(f, 56, SEEK_SET);

  int datalen = readInt4(f);
  if (datalen != *size)
    return false;

  u8 *mem = *rom;

  u8 block[1024];
  fread(&block, 1, 1024, f);
  if (memcmp(&mem[0x9320], &block, 1024) != 0)
    return false;

  int idlen = ppfFileIdLen(f, 2);
  if (idlen > 0)
    count -= 16 + 16 + idlen;

  fseek(f, 56+4+1024, SEEK_SET);

  while (count > 0) {
    int offset = readInt4(f);
    if (offset == -1)
      break;
    int len = fgetc(f);
    if (len == EOF)
      break;
    if (offset+len > *size)
      break;
    if (fread(&mem[offset], 1, len, f) != (size_t)len)
      break;
    count -= 4 + 1 + len;
  }

  return (count == 0);
}

static bool patchApplyPPF3(FILE *f, u8 **rom, int *size)
{
  fseek(f, 0, SEEK_END);
  int count = ftell(f);
  if (count < 56+4+1024)
    return false;
  count -= 56+4;

  fseek(f, 56, SEEK_SET);

  int imagetype = fgetc(f);
  int blockcheck = fgetc(f);
  int undo = fgetc(f);
  fgetc(f);

  u8 *mem = *rom;

  if (blockcheck) {
    u8 block[1024];
    fread(&block, 1, 1024, f);
    if (memcmp(&mem[(imagetype == 0) ? 0x9320 : 0x80A0], &block, 1024) != 0)
      return false;
    count -= 1024;
  }

  int idlen = ppfFileIdLen(f, 2);
  if (idlen > 0)
    count -= 16 + 16 + idlen;

  fseek(f, 56+4+(blockcheck ? 1024 : 0), SEEK_SET);

  while (count > 0) {
    __off64_t offset = readInt8(f);
    if (offset == -1)
      break;
    int len = fgetc(f);
    if (len == EOF)
      break;
    if (offset+len > *size)
      break;
    if (fread(&mem[offset], 1, len, f) != (size_t)len)
      break;
    if (undo) fseeko64(f, len, SEEK_CUR);
    count -= 8 + 1 + len;
    if (undo) count -= len;
  }

  return (count == 0);
}

static bool patchApplyPPF(const char *patchname, u8 **rom, int *size)
{
  FILE *f = fopen(patchname, "rb");
  if (!f)
    return false;

  bool res = false;

  int version = ppfVersion(f);
  switch (version) {
    case 1: res = patchApplyPPF1(f, rom, size); break;
    case 2: res = patchApplyPPF2(f, rom, size); break;
    case 3: res = patchApplyPPF3(f, rom, size); break;
  }

  fclose(f);
  return res;
}

bool applyPatch(const char *patchname, u8 **rom, int *size)
{
  if (strlen(patchname) < 5)
    return false;
  const char * p = strrchr(patchname, '.');
  if (p == NULL)
    return false;
  if (_stricmp(p, ".ips") == 0)
    return patchApplyIPS(patchname, rom, size);
  if (_stricmp(p, ".ups") == 0)
    return patchApplyUPS(patchname, rom, size);
  if (_stricmp(p, ".bps") == 0)
    return patchApplyBPS(patchname, rom, size);
  if (_stricmp(p, ".ppf") == 0)
    return patchApplyPPF(patchname, rom, size);
  return false;
}
