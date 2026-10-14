#include "UiThumb.h"

#include <stdio.h>

bool uiSaveThumb(const std::string &path, const UiColor *src, int w, int h, int pitch)
{
  if (!src || w <= 0 || h <= 0 || w > 1024 || h > 1024)
    return false;
  FILE *f = fopen(path.c_str(), "wb");
  if (!f)
    return false;
  unsigned char hdr[8] = { 'V', 'T', 'M', 'B',
                           (unsigned char)(w & 0xFF), (unsigned char)(w >> 8),
                           (unsigned char)(h & 0xFF), (unsigned char)(h >> 8) };
  bool ok = fwrite(hdr, 1, 8, f) == 8;
  std::vector<unsigned char> row(w * 2);
  for (int y = 0; ok && y < h; y++) {
    const UiColor *s = src + y * pitch;
    for (int x = 0; x < w; x++) {
      UiColor c = s[x];
      unsigned int v = ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F);
      row[x * 2] = (unsigned char)(v & 0xFF);
      row[x * 2 + 1] = (unsigned char)(v >> 8);
    }
    ok = fwrite(&row[0], 1, row.size(), f) == row.size();
  }
  fclose(f);
  return ok;
}

bool uiLoadThumb(const std::string &path, UiThumb &out)
{
  out.w = out.h = 0;
  out.px.clear();
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return false;
  unsigned char hdr[8];
  bool ok = fread(hdr, 1, 8, f) == 8 && hdr[0] == 'V' && hdr[1] == 'T' &&
            hdr[2] == 'M' && hdr[3] == 'B';
  int w = ok ? (hdr[4] | (hdr[5] << 8)) : 0;
  int h = ok ? (hdr[6] | (hdr[7] << 8)) : 0;
  ok = ok && w > 0 && h > 0 && w <= 1024 && h <= 1024;
  if (ok) {
    std::vector<unsigned char> data(w * h * 2);
    ok = fread(&data[0], 1, data.size(), f) == data.size();
    if (ok) {
      out.px.resize(w * h);
      for (int i = 0; i < w * h; i++) {
        unsigned int v = data[i * 2] | (data[i * 2 + 1] << 8);
        int r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
        out.px[i] = ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
      }
      out.w = w;
      out.h = h;
    }
  }
  fclose(f);
  return ok;
}
