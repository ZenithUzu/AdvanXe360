# Builds ui/UiGlyphs.cpp: the Xbox 360 controller button pictures for the
# menu's button hints, from Microsoft's XNA "Xbox 360 Controller Button
# Images" pack (Ms-PL):
# https://github.com/SimonDarksideJ/XNAGameStudio/wiki/Xbox-360-Controller-Button-Images
#
# Usage: python3 tools/gen_button_glyphs.py <the pack's ButtonImages folder> <output UiGlyphs.cpp> [license file, as the comment names it]
# Needs Pillow and the DejaVu Sans Bold font (for the LB / RB labels).
#
# What it does, and why:
# - Scales in PREMULTIPLIED alpha ("RGBa"): scaling plain RGBA mixes the
#   colour of fully transparent pixels (black) into the edges, which then
#   show a dark rim over the menu.
# - Lanczos, then a light unsharp mask: the pictures are shrunk a lot (from
#   ~100 px to ~32), and plain bilinear left them blurry.
# - LB / RB: the pack draws the bumpers with the square end outward; on the
#   controller (and as asked) the round end is outward. The label is erased,
#   the shape mirrored and the label written again, so it doesn't read
#   backwards.
# - Stored as 0xAARRGGBB, NOT premultiplied (what UiCanvas::blitAlpha takes).
import sys
from PIL import Image, ImageFilter, ImageOps, ImageDraw, ImageFont

SRC = sys.argv[1].rstrip('/\\') + '/'
OUT = sys.argv[2]
LICENSE = sys.argv[3] if len(sys.argv) > 3 else 'ui/LICENSE-ButtonImages.txt'
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def scaled(im, w, h, sharpen):
    im = im.convert('RGBA').convert('RGBa')   # premultiplied: no dark fringes
    im = im.resize((w, h), Image.LANCZOS)
    if sharpen:
        im = im.filter(ImageFilter.UnsharpMask(radius=0.8, percent=sharpen, threshold=0))
    return im.convert('RGBA')


def carray(name, im):
    w, h = im.size
    vals = ['0x%02X%02X%02X%02X' % (a, r, g, b) for (r, g, b, a) in (im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata())]
    lines = [','.join(vals[i:i + 10]) for i in range(0, len(vals), 10)]
    return 'const UiColor %s[%d * %d] = {\n  %s\n};\n' % (name, w, h, ',\n  '.join(lines))


def bumper(name, label, box):
    """The pack's bumper mirrored (round end outward), its label redrawn.
    box: where the label is in the original picture (x0, y0, x1, y1)."""
    src = Image.open(SRC + 'xboxController%s.tga' % name).convert('RGBA')
    px = src.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1):   # erase the label: blend the colours at its left and right edges
        a = px[x0 - 1, y]
        b = px[x1, y]
        for x in range(x0, x1):
            t = (x - x0 + 1) / (x1 - x0 + 1)
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))
    fl = ImageOps.mirror(src)
    d = ImageDraw.Draw(fl)
    f = ImageFont.truetype(FONT, 15)
    cx = fl.size[0] - (x0 + x1) // 2
    cy = (y0 + y1) // 2
    tw = d.textlength(label, font=f)
    d.text((cx - tw / 2, cy - 8), label, font=f, fill=(255, 255, 255, 255))   # a light edge under it
    d.text((cx - tw / 2, cy - 9), label, font=f, fill=(120, 120, 124, 255))
    return fl


special = {'LeftShoulder': lambda: bumper('LeftShoulder', 'LB', (66, 40, 100, 62)),
           'RightShoulder': lambda: bumper('RightShoulder', 'RB', (116, 40, 150, 62))}

# name in the hints, file in the pack, size in the menu
glyphs = [('A', 'ButtonA', 32, 32), ('B', 'ButtonB', 32, 32), ('X', 'ButtonX', 32, 32), ('Y', 'ButtonY', 32, 32),
          ('BACK', 'Back', 34, 32), ('START', 'Start', 35, 32), ('LB', 'LeftShoulder', 62, 26),
          ('RB', 'RightShoulder', 62, 26), ('LT', 'LeftTrigger', 18, 34), ('RT', 'RightTrigger', 18, 34),
          ('LS', 'LeftThumbstick', 31, 32), ('RS', 'RightThumbstick', 31, 32), ('DPAD', 'DPad', 32, 32)]

out = ['''// Xbox 360 controller button images, from Microsoft's XNA "Xbox 360 Controller
// Button Images" pack (Microsoft Permissive License, Ms-PL; see
// ''' + LICENSE + '''). Scaled in premultiplied alpha with Lanczos and
// sharpened; LB / RB mirrored with their labels redrawn. 0xAARRGGBB, not premultiplied.
// Generated from the pack's .tga files; regenerate instead of editing.
#include "UiGlyphs.h"

namespace {
''']
for n, f, w, h in glyphs:
    img = special[f]() if f in special else Image.open(SRC + 'xboxController%s.tga' % f)
    out.append(carray('k' + n, scaled(img, w, h, 90)))
out.append('''} // namespace

static const UiGlyph kGlyphs[] = {
''' + ''.join('  { "%s", %d, %d, k%s },\n' % (n, w, h, n) for n, f, w, h in glyphs) + '''  { 0, 0, 0, 0 }
};

const UiGlyph *uiGlyph(const char *name)
{
  for (const UiGlyph *g = kGlyphs; g->name; g++)
    if (strcmp(g->name, name) == 0)
      return g;
  return 0;
}
''')
open(OUT, 'w', newline='\n').write('\n'.join(out))
