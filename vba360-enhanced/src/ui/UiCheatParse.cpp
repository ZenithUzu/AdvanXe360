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
          else if (key == "code") uiParseCheatCodes(val, gba, rarch[n].codes);
          else if (key == "enable") rarch[n].enabled = val == "true" || val == "1";
        }
        continue;
      }
    }
    if (line.compare(0, 6, "cheats") == 0 && line.find('=') != std::string::npos)
      continue;   // "cheats = 12"

    // Plain text: a code line, or a description for the codes below it.
    std::vector<UiParsedCode> codes;
    if (uiParseCheatCodes(line, gba, codes)) {
      if (!lastWasCode || out.empty()) {
        UiCheatEntry e;
        e.desc = pendingDesc;
        out.push_back(e);
      }
      out.back().codes.insert(out.back().codes.end(), codes.begin(), codes.end());
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
