# Usage: python3 tools/cheats_crc_index.py <unzipped pack folder> <folder with libretro-database's
# metadat/no-intro "Nintendo - Game Boy.dat" and "Nintendo - Game Boy Color.dat">
# Output goes into _gamecodes.txt of extras/cheats/libretro-cheats.zip.
# Builds "CRC:XXXXXXXX=<folder>/<file>.cht" lines: each Game Boy / Color dump
# in libretro's No-Intro lists, to the cheat file of that same game and region.
import os, re, sys
pack, datdir = sys.argv[1], sys.argv[2]
DEVICE = ('gameshark', 'game genie', 'xploder', 'miyoo', 'codebreaker', 'code breaker', 'action replay', 'diff', 'eng1', 'eng2')
def key(name):
    m = re.match(r'([^(]*?)\s*(\([^)]*\))', name)
    title, region = (m.group(1), m.group(2)) if m else (name, '')
    t = re.sub(r'\s+-\s+(ENG\d)$', '', title, flags=re.I)
    return (re.sub(r'[^a-z0-9]', '', t.lower()), region.lower())
def plain(name):
    return not any(d in name.lower() for d in DEVICE)
lines = []
for system in ('Nintendo - Game Boy', 'Nintendo - Game Boy Color'):
    chts = {}
    for folder in ('Nintendo - Game Boy', 'Nintendo - Game Boy Color'):
        for f in os.listdir(os.path.join(pack, folder)):
            if f.endswith('.cht'):
                chts.setdefault(key(f[:-4]), []).append((folder, f))
    dat = open(os.path.join(datdir, system + '.dat'), encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'game \(\s*name "([^"]+)".*?crc ([0-9A-Fa-f]{8})', dat, re.S):
        name, crc = m.group(1), m.group(2).upper()
        cands = chts.get(key(name))
        if not cands:
            # A known dump with no cheats of its own: say so ("-"), so the
            # emulator doesn't guess by name (another language's codes).
            lines.append('CRC:%s=-' % crc)
            continue
        exact = [c for c in cands if c[1][:-4] == name]
        best = exact or sorted([c for c in cands if plain(c[1])] or cands, key=lambda c: (c[0] != system, len(c[1])))
        lines.append('CRC:%s=%s/%s' % (crc, best[0][0], best[0][1]))
best = {}
for l in lines:   # the same dump in both lists: a cheat file wins over "-"
    k, v = l.split('=', 1)
    if k not in best or best[k] == '-':
        best[k] = v
print('\n'.join('%s=%s' % kv for kv in sorted(best.items())))
