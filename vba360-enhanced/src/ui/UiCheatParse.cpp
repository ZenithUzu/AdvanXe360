#include "UiCheatParse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool isHex(const std::string &s)
{
  if (s.empty())
    return false;
  for (size_t i = 0; i < s.size(); i++)
    if (!isxdigit((unsigned char)s[i]))
      return false;
  return true;
}

std::string upper(const std::string &s)
{
  std::string r = s;
  for (size_t i = 0; i < r.size(); i++)
    r[i] = (char)toupper((unsigned char)r[i]);
  return r;
}

std::string trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while (a < b && isspace((unsigned char)s[a])) a++;
  while (b > a && isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}

void splitTokens(const std::string &text, std::vector<std::string> &out)
{
  std::string cur;
  for (size_t i = 0; i <= text.size(); i++) {
    char c = i < text.size() ? text[i] : ' ';
    if (isspace((unsigned char)c) || c == '+' || c == ',') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur += c;
    }
  }
}

UiParsedCode make(int kind, const std::string &code)
{
  UiParsedCode p;
  p.kind = kind;
  p.code = code;
  return p;
}

// "AAAAAAAA:VV" style (address optional, value 2/4/8 digits)
bool isRaw(const std::string &t)
{
  size_t colon = t.find(':');
  if (colon == std::string::npos || t.find(':', colon + 1) != std::string::npos)
    return false;
  std::string a = t.substr(0, colon), v = t.substr(colon + 1);
  if (!(a.empty() || (a.size() == 8 && isHex(a))))
    return false;
  return isHex(v) && (v.size() == 2 || v.size() == 4 || v.size() == 8);
}

bool isGameGenie(const std::string &t)
{
  // XXX-XXX or XXX-XXX-XXX
  if (t.size() != 7 && t.size() != 11)
    return false;
  for (size_t i = 0; i < t.size(); i++) {
    bool dash = i == 3 || i == 7;
    if (dash ? t[i] != '-' : !isxdigit((unsigned char)t[i]))
      return false;
  }
  return true;
}

} // namespace

bool uiParseCheatCodes(const std::string &text, bool gba, std::vector<UiParsedCode> &out)
{
  std::vector<std::string> tok;
  splitTokens(upper(text), tok);
  if (tok.empty())
    return false;
  std::vector<UiParsedCode> found;
  for (size_t i = 0; i < tok.size(); i++) {
    const std::string &t = tok[i];
    if (!gba) {
      if (isGameGenie(t)) found.push_back(make(CODE_GB_GG, t));
      else if (t.size() == 8 && isHex(t)) found.push_back(make(CODE_GB_GS, t));
      else return false;
      continue;
    }
    if (isRaw(t)) {
      found.push_back(make(CODE_GBA_RAW, t));
    } else if (t.size() == 8 && isHex(t) && i + 1 < tok.size() && isHex(tok[i + 1]) &&
               (tok[i + 1].size() == 4 || tok[i + 1].size() == 8)) {
      // "AAAAAAAA VVVV" = CodeBreaker, "AAAAAAAA VVVVVVVV" = GameShark/AR
      const std::string &v = tok[i + 1];
      if (v.size() == 4)
        found.push_back(make(CODE_GBA_CBA, t + " " + v));
      else
        found.push_back(make(CODE_GBA_GSA, t + v));
      i++;
    } else if (t.size() == 16 && isHex(t)) {
      found.push_back(make(CODE_GBA_GSA, t));
    } else if (t.size() == 12 && isHex(t)) {
      found.push_back(make(CODE_GBA_CBA, t.substr(0, 8) + " " + t.substr(8)));
    } else if (t.size() > 16 && t.size() % 16 == 0 && isHex(t)) {
      for (size_t k = 0; k < t.size(); k += 16)
        found.push_back(make(CODE_GBA_GSA, t.substr(k, 16)));
    } else {
      return false;
    }
  }
  out.insert(out.end(), found.begin(), found.end());
  return true;
}

namespace {
const char *const kTypeKeys[CT_COUNT] = { "auto", "gsa", "gsa3", "cba", "vba", "gs", "gg" };
}

const char *uiCheatTypeKey(int type)
{
  return type >= 0 && type < CT_COUNT ? kTypeKeys[type] : kTypeKeys[0];
}

int uiCheatTypeFromKey(const std::string &key)
{
  for (int i = 0; i < CT_COUNT; i++)
    if (key == kTypeKeys[i])
      return i;
  return CT_AUTO;
}

const char *uiCheatTypeExample(int type)
{
  switch (type) {
  case CT_GBA_GSA12:
  case CT_GBA_GSA3: return "XXXXXXXX YYYYYYYY";
  case CT_GBA_CBA: return "XXXXXXXX YYYY";
  case CT_GBA_RAW: return "XXXXXXXX:YY";
  case CT_GB_GS: return "01XXXXXX";
  case CT_GB_GG: return "XXX-XXX-XXX";
  default: return "";
  }
}

bool uiParseCheatCodesAs(const std::string &text, bool gba, int type, std::vector<UiParsedCode> &out)
{
  std::vector<UiParsedCode> found;
  if (!uiParseCheatCodes(text, gba, found))
    return false;
  if (type == CT_AUTO) {
    out.insert(out.end(), found.begin(), found.end());
    return true;
  }
  for (size_t i = 0; i < found.size(); i++) {
    int k = found[i].kind;
    bool ok = false;
    switch (type) {
    case CT_GBA_GSA12: ok = k == CODE_GBA_GSA; found[i].kind = CODE_GBA_GSA12; break;
    case CT_GBA_GSA3:  ok = k == CODE_GBA_GSA; found[i].kind = CODE_GBA_GSA3; break;
    case CT_GBA_CBA:   ok = k == CODE_GBA_CBA; break;
    case CT_GBA_RAW:   ok = k == CODE_GBA_RAW; break;
    case CT_GB_GS:     ok = k == CODE_GB_GS; break;
    case CT_GB_GG:     ok = k == CODE_GB_GG; break;
    }
    if (!ok)
      return false;   // e.g. a Game Genie code with "GameShark" chosen
  }
  out.insert(out.end(), found.begin(), found.end());
  return true;
}

bool uiLoadCheatStore(const std::string &path, std::vector<UiStoredCheat> &out, int &system)
{
  out.clear();
  system = 0;
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;
  char buf[1024];
  while (fgets(buf, sizeof(buf), f)) {
    std::string line = trim(buf);
    if (line.empty() || line[0] == '#')
      continue;
    if (line == "system=gba") { system = 1; continue; }
    if (line == "system=gb") { system = 2; continue; }
    if (line.compare(0, 6, "cheat=") != 0)
      continue;
    // cheat=<0|1>|<type>|<codes>|<description>
    std::string rest = line.substr(6);
    size_t a = rest.find('|');
    size_t b = a == std::string::npos ? a : rest.find('|', a + 1);
    size_t c = b == std::string::npos ? b : rest.find('|', b + 1);
    if (c == std::string::npos)
      continue;
    UiStoredCheat ch;
    ch.enabled = rest.substr(0, a) == "1";
    ch.type = uiCheatTypeFromKey(rest.substr(a + 1, b - a - 1));
    ch.code = rest.substr(b + 1, c - b - 1);
    ch.desc = rest.substr(c + 1);
    if (!ch.code.empty())
      out.push_back(ch);
  }
  fclose(f);
  return true;
}

bool uiSaveCheatStore(const std::string &path, const std::vector<UiStoredCheat> &list, int system)
{
  FILE *f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  fprintf(f, "# VBA360 cheats: cheat=<on>|<type>|<codes>|<description>\n");
  if (system == 1) fprintf(f, "system=gba\n");
  if (system == 2) fprintf(f, "system=gb\n");
  for (size_t i = 0; i < list.size(); i++) {
    std::string code = list[i].code, desc = list[i].desc;
    for (size_t k = 0; k < code.size(); k++) if (code[k] == '|' || code[k] == '\n') code[k] = ' ';
    for (size_t k = 0; k < desc.size(); k++) if (desc[k] == '\n' || desc[k] == '\r') desc[k] = ' ';
    fprintf(f, "cheat=%d|%s|%s|%s\n", list[i].enabled ? 1 : 0, uiCheatTypeKey(list[i].type),
            code.c_str(), desc.c_str());
  }
  fclose(f);
  return true;
}

bool uiCheatHasBlank(const std::string &code)
{
  for (size_t i = 0; i < code.size(); i++)
    if (code[i] == '?' || code[i] == 'X' || code[i] == 'x')
      return true;
  return false;
}

std::string uiCheatFillBlank(const std::string &code, const std::string &value)
{
  std::string out;
  for (size_t i = 0; i < code.size();) {
    if (code[i] != '?' && code[i] != 'X' && code[i] != 'x') {
      out += code[i++];
      continue;
    }
    size_t n = 0;
    while (i + n < code.size() && (code[i + n] == '?' || code[i + n] == 'X' || code[i + n] == 'x'))
      n++;
    std::string v = value.size() > n ? value.substr(value.size() - n) : std::string(n - value.size(), '0') + value;
    for (size_t k = 0; k < v.size(); k++)
      v[k] = (char)toupper((unsigned char)v[k]);
    out += v;
    i += n;
  }
  return out;
}

// A code with a value to fill in: kept as written (blank) if it is a valid
// code once filled, so the player can choose the value.
static bool parseWithBlank(const std::string &text, bool gba, UiCheatEntry &e)
{
  std::vector<UiParsedCode> codes;
  if (uiParseCheatCodes(text, gba, codes)) {
    e.codes.insert(e.codes.end(), codes.begin(), codes.end());
    return true;
  }
  if (!uiCheatHasBlank(text) || !uiParseCheatCodes(uiCheatFillBlank(text, "0"), gba, codes))
    return false;
  e.codes.insert(e.codes.end(), codes.begin(), codes.end());
  e.blank += (e.blank.empty() ? "" : " ") + text;
  return true;
}

bool uiParseCheatFile(const std::string &path, bool gba, std::vector<UiCheatEntry> &out)
{
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;

  std::vector<UiCheatEntry> rarch;    // RetroArch .cht, indexed by N
  std::string pendingDesc;
  bool lastWasCode = false;
  char buf[1024];
  while (fgets(buf, sizeof(buf), f)) {
    std::string line = trim(buf);
    if (line.empty() || line[0] == '#' || line[0] == ';' || (line.size() > 1 && line[0] == '/' && line[1] == '/'))
      continue;

    // RetroArch: cheat3_desc = "Infinite money"
    if (line.compare(0, 5, "cheat") == 0 && isdigit((unsigned char)line[5 < line.size() ? 5 : 0])) {
      size_t us = line.find('_'), eq = line.find('=');
      if (us != std::string::npos && eq != std::string::npos && us < eq) {
        int n = atoi(line.c_str() + 5);
        std::string key = trim(line.substr(us + 1, eq - us - 1));
        std::string val = trim(line.substr(eq + 1));
        if (val.size() >= 2 && val[0] == '"' && val[val.size() - 1] == '"')
          val = val.substr(1, val.size() - 2);
        if (n >= 0 && n < 1000) {
          if ((int)rarch.size() <= n)
            rarch.resize(n + 1);
          if (key == "desc") rarch[n].desc = val;
          else if (key == "code") parseWithBlank(val, gba, rarch[n]);
          else if (key == "enable") rarch[n].enabled = val == "true" || val == "1";
        }
        continue;
      }
    }
    if (line.compare(0, 6, "cheats") == 0 && line.find('=') != std::string::npos)
      continue;   // "cheats = 12"

    // Plain text: a code line, or a description for the codes below it.
    UiCheatEntry probe;
    if (parseWithBlank(line, gba, probe)) {
      if (!lastWasCode || out.empty()) {
        UiCheatEntry e;
        e.desc = pendingDesc;
        out.push_back(e);
      }
      UiCheatEntry &e = out.back();
      // Once a cheat has a blank, all of it is kept as written (blank), the
      // lines before included.
      if (!probe.blank.empty() && e.blank.empty())
        for (size_t k = 0; k < e.codes.size(); k++)
          e.blank += (k ? " " : "") + e.codes[k].code;
      if (!e.blank.empty() || !probe.blank.empty())
        e.blank += (e.blank.empty() ? "" : " ") + (probe.blank.empty() ? line : probe.blank);
      e.codes.insert(e.codes.end(), probe.codes.begin(), probe.codes.end());
      lastWasCode = true;
    } else {
      // Strip decorations like "[Infinite money]" or "Infinite money:"
      std::string d = line;
      if (d.size() > 2 && d[0] == '[' && d[d.size() - 1] == ']')
        d = d.substr(1, d.size() - 2);
      while (!d.empty() && (d[d.size() - 1] == ':' || d[d.size() - 1] == '='))
        d.erase(d.size() - 1);
      pendingDesc = trim(d);
      lastWasCode = false;
    }
  }
  fclose(f);

  for (size_t i = 0; i < rarch.size(); i++)
    if (!rarch[i].codes.empty())
      out.push_back(rarch[i]);
  return true;
}
