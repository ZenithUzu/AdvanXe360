# Converts non-ASCII characters in C/C++ sources into Latin-1 hex escapes so
# the files are plain ASCII (safe for Visual Studio 2008 with any codepage).
# "\xe1" followed by a hex digit would swallow it, so a "" split is added then.
import sys, re
HEX = set("0123456789abcdefABCDEF")
for path in sys.argv[1:]:
    s = open(path, encoding="utf-8").read()
    out = []
    for i, ch in enumerate(s):
        if ord(ch) < 128:
            out.append(ch); continue
        code = ch.encode("latin-1")[0]   # fails loudly if not Latin-1
        out.append("\\x%02x" % code)
        nxt = s[i + 1] if i + 1 < len(s) else ""
        if nxt in HEX:
            out.append('""')
    t = "".join(out)
    if t != s:
        open(path, "w", encoding="ascii").write(t)
        print("escaped", path)
