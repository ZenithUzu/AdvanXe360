#include "UiConfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const int uiFastForwardSpeeds[] = { 2, 3, 4, 6, 8, 0 };
const int uiFastForwardSpeedCount = sizeof(uiFastForwardSpeeds) / sizeof(uiFastForwardSpeeds[0]);

namespace {

const char *kActionKeys[ACT_COUNT] = {
  "map_a", "map_b", "map_l", "map_r", "map_start", "map_select",
  "map_up", "map_down", "map_left", "map_right",
  "map_turbo_a", "map_turbo_b",
  "map_fast_forward", "map_menu", "map_quick_save", "map_quick_load"
};

std::string trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while (a < b && isspace((unsigned char)s[a])) a++;
  while (b > a && isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}

// Reads "key=value" lines; calls back for each pair.
template <class F>
bool readIni(const std::string &path, F &onPair)
{
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#' || s[0] == ';' || s[0] == '[')
      continue;
    size_t eq = s.find('=');
    if (eq == std::string::npos)
      continue;
    onPair(trim(s.substr(0, eq)), trim(s.substr(eq + 1)));
  }
  fclose(f);
  return true;
}

int clampInt(const std::string &v, int lo, int hi, int def)
{
  if (v.empty())
    return def;
  int n = atoi(v.c_str());
  if (n < lo || n > hi)
    return def;
  return n;
}

struct ConfigReader
{
  UiConfig &c;
  explicit ConfigReader(UiConfig &cfg) : c(cfg) {}
  void operator()(const std::string &k, const std::string &v)
  {
    if (k == "language") c.language = clampInt(v, 0, 1, c.language);
    else if (k == "hide_non_games") c.hideNonGames = clampInt(v, 0, 1, c.hideNonGames);
    else if (k == "last_dir") c.lastDir = v;
    else if (k == "filter") c.filter = clampInt(v, 0, FILTER_COUNT - 1, c.filter);
    else if (k == "stretch") c.stretch = clampInt(v, 0, SIZE_COUNT - 1, c.stretch);
    else if (k == "border") c.border = clampInt(v, 0, 1, c.border);
    else if (k == "border_image") c.borderImage = v;
    else if (k == "border_fit") c.borderFit = clampInt(v, 0, FIT_COUNT - 1, c.borderFit);
    else if (k == "screen_effect") c.screenEffect = clampInt(v, 0, EFFECT_COUNT - 1, c.screenEffect);
    else if (k == "gb_palette") c.gbPalette = clampInt(v, 0, GB_PALETTE_COUNT - 1, c.gbPalette);
    else if (k == "show_fps") c.showFps = clampInt(v, 0, 1, c.showFps);
    else if (k == "frame_skip") c.frameSkip = clampInt(v, 0, 4, c.frameSkip);
    else if (k == "sound") c.sound = clampInt(v, 0, 1, c.sound);
    else if (k == "mute_fast_forward") c.muteFastForward = clampInt(v, 0, 1, c.muteFastForward);
    else if (k == "ff_speed") c.ffSpeed = clampInt(v, 0, uiFastForwardSpeedCount - 1, c.ffSpeed);
    else if (k == "ff_toggle") c.ffToggle = clampInt(v, 0, 1, c.ffToggle);
    else if (k == "use_bios") c.useBios = clampInt(v, 0, 1, c.useBios);
    else if (k == "gsa_v3") c.gsaV3 = clampInt(v, 0, 1, c.gsaV3);
    else if (k == "save_dir") c.saveDirMode = clampInt(v, 0, 1, c.saveDirMode);
    else if (k == "state_dir") c.stateDirMode = clampInt(v, 0, 1, c.stateDirMode);
    else if (k == "auto_save_state") c.autoSaveState = clampInt(v, 0, 1, c.autoSaveState);
    else if (k == "auto_load_state") c.autoLoadState = clampInt(v, 0, 1, c.autoLoadState);
    else if (k == "state_slot") c.stateSlot = clampInt(v, 0, 9, c.stateSlot);
    else {
      for (int i = 0; i < ACT_COUNT; i++)
        if (k == kActionKeys[i])
          c.map[i] = clampInt(v, 0, PAD_NONE, c.map[i]);
    }
  }
};

struct GameConfigReader
{
  UiGameConfig &c;
  explicit GameConfigReader(UiGameConfig &cfg) : c(cfg) {}
  void operator()(const std::string &k, const std::string &v)
  {
    if (k == "save_type") c.saveType = clampInt(v, 0, SAVE_TYPE_COUNT - 1, c.saveType);
    else if (k == "rtc") c.rtc = clampInt(v, 0, 2, c.rtc);
    else if (k == "patch") c.patch = clampInt(v, 0, 1, c.patch);
  }
};

} // namespace

UiConfig::UiConfig()
  : language(0), hideNonGames(1),
    filter(FILTER_SMOOTH), stretch(SIZE_ORIGINAL), border(0), borderFit(FIT_CENTER),
    screenEffect(EFFECT_NONE), gbPalette(0), showFps(0), frameSkip(0),
    sound(1), muteFastForward(1),
    ffSpeed(1), ffToggle(0), useBios(0), gsaV3(0),
    saveDirMode(DIR_NEXT_TO_ROM), stateDirMode(DIR_EMULATOR),
    autoSaveState(0), autoLoadState(0), stateSlot(1)
{
  resetControls();
}

void UiConfig::resetControls()
{
  // Nintendo layout: GBA A on the right face button, B on the bottom one.
  map[ACT_GBA_A] = PAD_B;
  map[ACT_GBA_B] = PAD_A;
  map[ACT_GBA_L] = PAD_LB;
  map[ACT_GBA_R] = PAD_RB;
  map[ACT_GBA_START] = PAD_START;
  map[ACT_GBA_SELECT] = PAD_BACK;
  map[ACT_GBA_UP] = PAD_UP;
  map[ACT_GBA_DOWN] = PAD_DOWN;
  map[ACT_GBA_LEFT] = PAD_LEFT;
  map[ACT_GBA_RIGHT] = PAD_RIGHT;
  map[ACT_TURBO_A] = PAD_Y;
  map[ACT_TURBO_B] = PAD_X;
  map[ACT_FAST_FORWARD] = PAD_RT;
  map[ACT_MENU] = PAD_RS;
  map[ACT_QUICK_SAVE] = PAD_NONE;
  map[ACT_QUICK_LOAD] = PAD_NONE;
}

bool UiConfig::load(const std::string &path)
{
  ConfigReader r(*this);
  bool ok = readIni(path, r);
  // The in-game menu must always be reachable.
  if (map[ACT_MENU] >= PAD_BUTTON_COUNT)
    map[ACT_MENU] = PAD_RS;
  return ok;
}

bool UiConfig::save(const std::string &path) const
{
  FILE *f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  fprintf(f, "# VBA360 settings\n");
  fprintf(f, "language=%d\nhide_non_games=%d\nlast_dir=%s\n", language, hideNonGames, lastDir.c_str());
  fprintf(f, "filter=%d\nstretch=%d\nborder=%d\nborder_image=%s\nborder_fit=%d\n",
          filter, stretch, border, borderImage.c_str(), borderFit);
  fprintf(f, "screen_effect=%d\ngb_palette=%d\nshow_fps=%d\nframe_skip=%d\n",
          screenEffect, gbPalette, showFps, frameSkip);
  fprintf(f, "sound=%d\nmute_fast_forward=%d\n", sound, muteFastForward);
  fprintf(f, "ff_speed=%d\nff_toggle=%d\nuse_bios=%d\ngsa_v3=%d\n", ffSpeed, ffToggle, useBios, gsaV3);
  fprintf(f, "save_dir=%d\nstate_dir=%d\nauto_save_state=%d\nauto_load_state=%d\nstate_slot=%d\n",
          saveDirMode, stateDirMode, autoSaveState, autoLoadState, stateSlot);
  for (int i = 0; i < ACT_COUNT; i++)
    fprintf(f, "%s=%d\n", kActionKeys[i], map[i]);
  fclose(f);
  return true;
}

UiGameConfig::UiGameConfig() : saveType(SAVE_AUTO), rtc(TRI_AUTO), patch(1) {}

bool UiGameConfig::load(const std::string &path)
{
  GameConfigReader r(*this);
  return readIni(path, r);
}

bool UiGameConfig::save(const std::string &path) const
{
  FILE *f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  fprintf(f, "save_type=%d\nrtc=%d\npatch=%d\n", saveType, rtc, patch);
  fclose(f);
  return true;
}

// --- UiPathList ------------------------------------------------------------

bool UiPathList::load(const std::string &path)
{
  m_items.clear();
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;
  char line[1024];
  while (fgets(line, sizeof(line), f) && m_items.size() < m_max) {
    std::string s = trim(line);
    if (!s.empty() && !contains(s))
      m_items.push_back(s);
  }
  fclose(f);
  return true;
}

bool UiPathList::save(const std::string &path) const
{
  FILE *f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  for (size_t i = 0; i < m_items.size(); i++)
    fprintf(f, "%s\n", m_items[i].c_str());
  fclose(f);
  return true;
}

void UiPathList::pushFront(const std::string &item)
{
  remove(item);
  m_items.insert(m_items.begin(), item);
  if (m_items.size() > m_max)
    m_items.resize(m_max);
}

void UiPathList::remove(const std::string &item)
{
  for (size_t i = 0; i < m_items.size(); i++) {
    if (m_items[i] == item) {
      m_items.erase(m_items.begin() + i);
      return;
    }
  }
}

bool UiPathList::contains(const std::string &item) const
{
  for (size_t i = 0; i < m_items.size(); i++)
    if (m_items[i] == item)
      return true;
  return false;
}

// --- Path helpers ------------------------------------------------------------

std::string uiJoinPath(const std::string &dir, const std::string &name)
{
  if (dir.empty())
    return name;
  char last = dir[dir.size() - 1];
  if (last == '\\' || last == '/')
    return dir + name;
  return dir + "\\" + name;
}

std::string uiParentDir(const std::string &path)
{
  std::string p = path;
  while (!p.empty() && (p[p.size() - 1] == '\\' || p[p.size() - 1] == '/'))
    p.erase(p.size() - 1);
  size_t slash = p.find_last_of("\\/");
  if (slash == std::string::npos)
    return "";                      // "hdd:" -> back to the drive list
  return p.substr(0, slash + 1);    // keep the trailing separator
}

std::string uiFileName(const std::string &path)
{
  size_t slash = path.find_last_of("\\/");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string uiStripExtension(const std::string &name)
{
  size_t dot = name.find_last_of('.');
  size_t slash = name.find_last_of("\\/");
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    return name;
  return name.substr(0, dot);
}

std::string uiExtension(const std::string &name)
{
  size_t dot = name.find_last_of('.');
  if (dot == std::string::npos)
    return "";
  std::string e = name.substr(dot);
  for (size_t i = 0; i < e.size(); i++)
    e[i] = (char)tolower((unsigned char)e[i]);
  return e;
}

std::string uiSafeFileName(const std::string &base, const std::string &ext)
{
  std::string clean;
  for (size_t i = 0; i < base.size(); i++) {
    unsigned char c = (unsigned char)base[i];
    // FATX rejects these; also avoid control characters.
    if (c < 32 || strchr("\\/:*?\"<>|", c))
      clean += '_';
    else
      clean += (char)c;
  }
  const size_t kMax = 42;
  if (clean.size() + ext.size() <= kMax)
    return clean + ext;
  // FNV-1a of the full name keeps shortened names unique.
  unsigned int h = 2166136261u;
  for (size_t i = 0; i < base.size(); i++) {
    h ^= (unsigned char)base[i];
    h *= 16777619u;
  }
  char suffix[16];
  sprintf(suffix, "~%06X", h & 0xFFFFFF);
  size_t keep = kMax - ext.size() - strlen(suffix);
  return clean.substr(0, keep) + suffix + ext;
}

std::string uiGameFile(const std::string &emuDir, const char *folder,
                       const std::string &rom, const std::string &suffix,
                       const std::string &ext)
{
  std::string base = uiStripExtension(uiFileName(rom)) + suffix;
  return uiJoinPath(uiJoinPath(emuDir, folder), uiSafeFileName(base, ext));
}

const char *const uiGbPaletteNames[GB_PALETTE_COUNT][2] = {
  { "Gris (original)", "Gray (original)" },
  { "Verde cl\xe1sico", "Classic green" },
  { "Game Boy Pocket", "Game Boy Pocket" },
  { "Super Game Boy 1-A", "Super Game Boy 1-A" },
  { "Super Game Boy 2-C", "Super Game Boy 2-C" },
  { "Super Game Boy 3-B", "Super Game Boy 3-B" },
  { "Sepia", "Sepia" },
  { "Azul", "Blue" },
  { "Rojo", "Red" },
};

// Imitations of the classic screens and Super Game Boy presets.
const unsigned int uiGbPalettes[GB_PALETTE_COUNT][4] = {
  { 0xF8F8F8, 0xA8A8A8, 0x606060, 0x000000 },
  { 0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F },
  { 0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F },
  { 0xF8E8C8, 0xD89048, 0xA82820, 0x301850 },
  { 0xF8D8B0, 0x78C078, 0x688840, 0x583820 },
  { 0xF8D8A8, 0xE0A878, 0x785888, 0x002030 },
  { 0xF8F0D8, 0xC8A878, 0x806040, 0x302010 },
  { 0xE0F8F8, 0x88C0F0, 0x4060A8, 0x102040 },
  { 0xF8E0E0, 0xE08888, 0xA03030, 0x300808 },
};

bool uiIsImageFile(const std::string &name)
{
  static const char *exts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".dib", 0 };
  std::string e = uiExtension(name);
  for (int i = 0; exts[i]; i++)
    if (e == exts[i])
      return true;
  return false;
}

bool uiIsRomFile(const std::string &name)
{
  static const char *exts[] = { ".gba", ".agb", ".gb", ".gbc", ".cgb", ".sgb",
                                ".dmg", ".bin", ".mb", ".zip", ".7z", 0 };
  std::string e = uiExtension(name);
  for (int i = 0; exts[i]; i++)
    if (e == exts[i])
      return true;
  return false;
}

bool uiIsPatchFile(const std::string &name)
{
  std::string e = uiExtension(name);
  return e == ".bps" || e == ".ups" || e == ".ips";
}
