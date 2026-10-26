#include "UiConfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const int uiFastForwardSpeeds[] = { 2, 3, 4, 6, 8, 0 };
const int uiFastForwardSpeedCount = sizeof(uiFastForwardSpeeds) / sizeof(uiFastForwardSpeeds[0]);
const int uiBgVeilAlpha[BG_VIS_COUNT] = { 0xE6, 0xC8, 0x90, 0x50 };
const int uiPanelOpacityPercent[PANEL_OPACITY_COUNT] = { 130, 100, 70, 45 };
const unsigned int uiAccentTints[ACCENT_TINT_COUNT] = {
  0, 0xFF27A7E8, 0xFF1FC8B4, 0xFF4CC26A, 0xFFE3B341, 0xFFF08A24, 0xFFE5534B, 0xFFE0457B, 0xFF7B6CFF };
const int uiMusicVolumePercent[MUSIC_VOLUME_COUNT] = { 25, 50, 75, 100 };
const int uiVolumeLevels[] = { 25, 50, 75, 100, 125, 150, 200 };
const int uiVolumeLevelCount = sizeof(uiVolumeLevels) / sizeof(uiVolumeLevels[0]);
const int uiAudioLatencyMs[] = { 100, 60, 40 };
const int uiAudioLatencyCount = sizeof(uiAudioLatencyMs) / sizeof(uiAudioLatencyMs[0]);

namespace {

const char *kActionKeys[ACT_COUNT] = {
  "map_a", "map_b", "map_l", "map_r", "map_start", "map_select",
  "map_up", "map_down", "map_left", "map_right",
  "map_turbo_a", "map_turbo_b",
  "map_fast_forward", "map_menu", "map_quick_save", "map_quick_load"
};

const char *kSensorKeys[SENS_COUNT] = {
  "sensor_tilt_right", "sensor_tilt_left", "sensor_tilt_up", "sensor_tilt_down",
  "sensor_sun_up", "sensor_sun_down"
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
    else if (k == "menu_theme") c.menuTheme = clampInt(v, 0, 6, c.menuTheme);
    else if (k == "option_style") c.optionStyle = clampInt(v, 0, OPT_STYLE_COUNT - 1, c.optionStyle);
    else if (k == "menu_background") c.menuBackground = v;
    else if (k == "cover_server") { if (!v.empty()) c.coverServer = v; }
    else if (k == "preview_mode") c.previewMode = clampInt(v, 0, PREVIEW_COUNT - 1, c.previewMode);
    else if (k == "list_view") c.listView = clampInt(v, 0, LIST_VIEW_COUNT - 1, c.listView);
    else if (k == "bg_effect") c.bgEffect = clampInt(v, 0, UI_EFFECT_COUNT - 1, c.bgEffect);
    else if (k == "start_screen") c.startScreen = clampInt(v, 0, 1, c.startScreen);
    else if (k == "kinect") c.kinect = clampInt(v, 0, 1, c.kinect);
    else if (k == "ra_enabled") c.raEnabled = clampInt(v, 0, 1, c.raEnabled);
    else if (k == "ra_user") c.raUser = v;
    else if (k == "ra_token") c.raToken = v;
    else if (k == "ra_sound") c.raSound = clampInt(v, 0, 1, c.raSound);
    else if (k == "kinect_hands") c.kinectHands = clampInt(v, 0, 1, c.kinectHands);
    else if (k == "kinect_camera") c.kinectCamera = clampInt(v, 0, 1, c.kinectCamera);
    else if (k == "library_dir") c.libraryDir = v;
    else if (k == "bg_visibility") c.bgVisibility = clampInt(v, 0, BG_VIS_COUNT - 1, c.bgVisibility);
    else if (k == "panel_opacity") c.panelOpacity = clampInt(v, 0, PANEL_OPACITY_COUNT - 1, c.panelOpacity);
    else if (k == "accent_tint") c.accentTint = clampInt(v, 0, ACCENT_TINT_COUNT - 1, c.accentTint);
    else if (k == "bubble_amount") c.bubbleAmount = clampInt(v, 0, EFFECT_LEVEL_COUNT - 1, c.bubbleAmount);
    else if (k == "water_strength") c.waterStrength = clampInt(v, 0, EFFECT_LEVEL_COUNT - 1, c.waterStrength);
    else if (k == "menu_music") c.menuMusic = clampInt(v, 0, MUSIC_MODE_COUNT - 1, c.menuMusic);
    else if (k == "music_song") c.musicSong = v;
    else if (k == "music_volume") c.musicVolume = clampInt(v, 0, MUSIC_VOLUME_COUNT - 1, c.musicVolume);
    else if (k == "filter") c.filter = clampInt(v, 0, FILTER_COUNT - 1, c.filter);
    else if (k == "stretch") c.stretch = clampInt(v, 0, SIZE_COUNT - 1, c.stretch);
    else if (k == "border") c.border = clampInt(v, 0, 1, c.border);
    else if (k == "border_image") c.borderImage = v;
    else if (k == "border_fit") c.borderFit = clampInt(v, 0, FIT_COUNT - 1, c.borderFit);
    else if (k == "screen_effect") c.screenEffect = clampInt(v, 0, EFFECT_COUNT - 1, c.screenEffect);
    else if (k == "gb_palette") c.gbPalette = clampInt(v, 0, GB_PALETTE_COUNT - 1, c.gbPalette);
    else if (k == "gb_printer") c.gbPrinter = clampInt(v, 0, 1, c.gbPrinter);
    else if (k == "sgb_borders") c.sgbBorders = clampInt(v, 0, 2, c.sgbBorders);
    else if (k == "gba_color") c.gbaColor = clampInt(v, 0, 2, c.gbaColor);
    else if (k == "frame_blend") c.frameBlend = clampInt(v, 0, 2, c.frameBlend);
    else if (k == "save_backups") c.saveBackups = clampInt(v, 0, 1, c.saveBackups);
    else if (k == "list_sort") c.listSort = clampInt(v, 0, SORT_COUNT - 1, c.listSort);
    else if (k == "show_fps") c.showFps = clampInt(v, 0, 1, c.showFps);
    else if (k == "frame_skip") c.frameSkip = clampInt(v, 0, 4, c.frameSkip);
    else if (k == "sound") c.sound = clampInt(v, 0, 1, c.sound);
    else if (k == "mute_fast_forward") c.muteFastForward = clampInt(v, 0, 1, c.muteFastForward);
    else if (k == "volume") c.volume = clampInt(v, 0, uiVolumeLevelCount - 1, c.volume);
    else if (k == "audio_latency") c.audioLatency = clampInt(v, 0, uiAudioLatencyCount - 1, c.audioLatency);
    else if (k == "ff_speed") c.ffSpeed = clampInt(v, 0, uiFastForwardSpeedCount - 1, c.ffSpeed);
    else if (k == "ff_toggle") c.ffToggle = clampInt(v, 0, 1, c.ffToggle);
    else if (k == "use_bios") c.useBios = clampInt(v, 0, 1, c.useBios);
    else if (k == "gsa_v3") c.gsaV3 = clampInt(v, 0, 1, c.gsaV3);
    else if (k == "sun_level") c.sunLevel = clampInt(v, 0, 10, c.sunLevel);
    else if (k == "sensor_sensitivity") c.sensorSens = clampInt(v, 0, SENSOR_SENS_COUNT - 1, c.sensorSens);
    else if (k == "sensor_deadzone") c.sensorDeadzone = clampInt(v, 0, SENSOR_DEADZONE_COUNT - 1, c.sensorDeadzone);
    else if (k == "clock_mode") c.clockMode = clampInt(v, 0, 1, c.clockMode);
    else if (k == "clock_offset") c.clockOffset = clampInt(v, -2000000000, 2000000000, c.clockOffset);
    else if (k == "save_dir") c.saveDirMode = clampInt(v, 0, 1, c.saveDirMode);
    else if (k == "state_dir") c.stateDirMode = clampInt(v, 0, 1, c.stateDirMode);
    else if (k == "auto_save_state") c.autoSaveState = clampInt(v, 0, 1, c.autoSaveState);
    else if (k == "auto_load_state") c.autoLoadState = clampInt(v, 0, 1, c.autoLoadState);
    else if (k == "state_slot") c.stateSlot = clampInt(v, 0, 9, c.stateSlot);
    else {
      for (int i = 0; i < ACT_COUNT; i++)
        if (k == kActionKeys[i])
          c.map[i] = clampInt(v, 0, PAD_NONE, c.map[i]);
      for (int i = 0; i < SENS_COUNT; i++)
        if (k == kSensorKeys[i])
          c.sensorMap[i] = clampInt(v, 0, SDIR_COUNT - 1, c.sensorMap[i]);
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
    else if (k == "custom") c.custom = clampInt(v, 0, 1, c.custom);
    else if (k == "filter") c.filter = clampInt(v, 0, FILTER_COUNT - 1, c.filter);
    else if (k == "stretch") c.stretch = clampInt(v, 0, SIZE_COUNT - 1, c.stretch);
    else if (k == "screen_effect") c.screenEffect = clampInt(v, 0, EFFECT_COUNT - 1, c.screenEffect);
    else if (k == "gb_palette") c.gbPalette = clampInt(v, 0, GB_PALETTE_COUNT - 1, c.gbPalette);
    else if (k == "sgb_mode") c.sgbMode = clampInt(v, 0, 1, c.sgbMode);
    else if (k == "gba_color") c.gbaColor = clampInt(v, 0, 2, c.gbaColor);
    else if (k == "frame_blend") c.frameBlend = clampInt(v, 0, 2, c.frameBlend);
    else if (k == "show_fps") c.showFps = clampInt(v, 0, 1, c.showFps);
    else if (k == "frame_skip") c.frameSkip = clampInt(v, 0, 4, c.frameSkip);
    else if (k == "custom_controls") c.customControls = clampInt(v, 0, 1, c.customControls);
    else if (k.compare(0, 4, "ctl_") == 0) {
      for (int i = 0; i < ACT_COUNT; i++)
        if (k.compare(4, std::string::npos, kActionKeys[i]) == 0)
          c.map[i] = clampInt(v, 0, PAD_NONE, c.map[i]);
    }
  }
};

} // namespace

UiConfig::UiConfig()
  : language(0), hideNonGames(1), menuTheme(0), optionStyle(OPT_STYLE_PANEL), listSort(SORT_NAME),
    filter(FILTER_SMOOTH), stretch(SIZE_ORIGINAL), border(0), borderFit(FIT_CENTER),
    screenEffect(EFFECT_NONE), gbPalette(0), sgbBorders(0), gbPrinter(0), gbaColor(0), frameBlend(0), showFps(0), frameSkip(0),
    sound(1), muteFastForward(1), volume(VOLUME_DEFAULT), audioLatency(0),
    ffSpeed(1), ffToggle(0), useBios(0), gsaV3(0), sunLevel(7),
    sensorSens(2), sensorDeadzone(1), clockMode(0), clockOffset(0),
    saveDirMode(DIR_NEXT_TO_ROM), stateDirMode(DIR_EMULATOR),
    autoSaveState(0), autoLoadState(0), stateSlot(1), saveBackups(1)
{
  coverServer = "http://thumbnails.libretro.com";
  previewMode = PREVIEW_COVER_OR_LAST;
  listView = LIST_VIEW_CAROUSEL;
  bgEffect = UI_EFFECT_BUBBLES;
  startScreen = 0;
  kinect = 0;
  raEnabled = 0;
  raSound = 1;
  kinectHands = 0;
  kinectCamera = 1;
  bgVisibility = 1;
  panelOpacity = 1;
  accentTint = 0;
  bubbleAmount = 1;
  waterStrength = 1;
  menuMusic = MUSIC_IN_ORDER;
  musicVolume = 2;
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
  // Sensors: the right stick, the way you would tilt the console.
  sensorMap[SENS_TILT_RIGHT] = SDIR_RS_RIGHT;
  sensorMap[SENS_TILT_LEFT] = SDIR_RS_LEFT;
  sensorMap[SENS_TILT_UP] = SDIR_RS_UP;
  sensorMap[SENS_TILT_DOWN] = SDIR_RS_DOWN;
  sensorMap[SENS_SUN_UP] = SDIR_RS_UP;
  sensorMap[SENS_SUN_DOWN] = SDIR_RS_DOWN;
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
  fprintf(f, "menu_theme=%d\nmenu_background=%s\noption_style=%d\nlist_sort=%d\n",
          menuTheme, menuBackground.c_str(), optionStyle, listSort);
  fprintf(f, "cover_server=%s\npreview_mode=%d\nlist_view=%d\nbg_effect=%d\n", coverServer.c_str(), previewMode, listView, bgEffect);
  fprintf(f, "start_screen=%d\nlibrary_dir=%s\nkinect=%d\nkinect_hands=%d\nkinect_camera=%d\n", startScreen, libraryDir.c_str(),
          kinect, kinectHands, kinectCamera);
  fprintf(f, "ra_enabled=%d\nra_user=%s\nra_token=%s\nra_sound=%d\n", raEnabled, raUser.c_str(), raToken.c_str(), raSound);
  fprintf(f, "bg_visibility=%d\npanel_opacity=%d\naccent_tint=%d\nbubble_amount=%d\nwater_strength=%d\n",
          bgVisibility, panelOpacity, accentTint, bubbleAmount, waterStrength);
  fprintf(f, "menu_music=%d\nmusic_volume=%d\nmusic_song=%s\n", menuMusic, musicVolume, musicSong.c_str());
  fprintf(f, "filter=%d\nstretch=%d\nborder=%d\nborder_image=%s\nborder_fit=%d\n",
          filter, stretch, border, borderImage.c_str(), borderFit);
  fprintf(f, "screen_effect=%d\ngb_palette=%d\nsgb_borders=%d\nshow_fps=%d\nframe_skip=%d\n",
          screenEffect, gbPalette, sgbBorders, showFps, frameSkip);
  fprintf(f, "gb_printer=%d\n", gbPrinter);
  fprintf(f, "gba_color=%d\nframe_blend=%d\nsave_backups=%d\n", gbaColor, frameBlend, saveBackups);
  fprintf(f, "sound=%d\nmute_fast_forward=%d\nvolume=%d\naudio_latency=%d\n", sound, muteFastForward, volume, audioLatency);
  fprintf(f, "ff_speed=%d\nff_toggle=%d\nuse_bios=%d\ngsa_v3=%d\nsun_level=%d\n", ffSpeed, ffToggle, useBios, gsaV3, sunLevel);
  fprintf(f, "sensor_sensitivity=%d\nsensor_deadzone=%d\n", sensorSens, sensorDeadzone);
  for (int i = 0; i < SENS_COUNT; i++)
    fprintf(f, "%s=%d\n", kSensorKeys[i], sensorMap[i]);
  fprintf(f, "clock_mode=%d\nclock_offset=%d\n", clockMode, clockOffset);
  fprintf(f, "save_dir=%d\nstate_dir=%d\nauto_save_state=%d\nauto_load_state=%d\nstate_slot=%d\n",
          saveDirMode, stateDirMode, autoSaveState, autoLoadState, stateSlot);
  for (int i = 0; i < ACT_COUNT; i++)
    fprintf(f, "%s=%d\n", kActionKeys[i], map[i]);
  fclose(f);
  return true;
}

UiGameConfig::UiGameConfig()
  : saveType(SAVE_AUTO), rtc(TRI_AUTO), patch(1), custom(0),
    filter(FILTER_SMOOTH), stretch(SIZE_ORIGINAL), screenEffect(EFFECT_NONE), gbPalette(0), sgbMode(1),
    gbaColor(0), frameBlend(0), showFps(0), frameSkip(0),
    customControls(0)
{
  UiConfig defaults;
  for (int i = 0; i < ACT_COUNT; i++)
    map[i] = defaults.map[i];
}

void UiGameConfig::copyControlsFrom(const UiConfig &cfg)
{
  for (int i = 0; i < ACT_COUNT; i++)
    map[i] = cfg.map[i];
}

void UiGameConfig::copyVideoFrom(const UiConfig &cfg)
{
  filter = cfg.filter;
  stretch = cfg.stretch;
  screenEffect = cfg.screenEffect;
  gbPalette = cfg.gbPalette;
  sgbMode = cfg.sgbBorders ? 1 : 0;
  gbaColor = cfg.gbaColor;
  frameBlend = cfg.frameBlend;
  showFps = cfg.showFps;
  frameSkip = cfg.frameSkip;
}

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
  fprintf(f, "custom=%d\nfilter=%d\nstretch=%d\nscreen_effect=%d\ngb_palette=%d\nsgb_mode=%d\n",
          custom, filter, stretch, screenEffect, gbPalette, sgbMode);
  fprintf(f, "gba_color=%d\nframe_blend=%d\nshow_fps=%d\nframe_skip=%d\n", gbaColor, frameBlend, showFps, frameSkip);
  fprintf(f, "custom_controls=%d\n", customControls);
  if (customControls)
    for (int i = 0; i < ACT_COUNT; i++)
      fprintf(f, "ctl_%s=%d\n", kActionKeys[i], map[i]);
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

bool uiVersionNewer(const std::string &a, const std::string &b)
{
  const char *p = a.c_str(), *q = b.c_str();
  if (*p == 'v' || *p == 'V') p++;
  if (*q == 'v' || *q == 'V') q++;
  for (int i = 0; i < 4; i++) {
    long x = strtol(p, (char **)&p, 10), y = strtol(q, (char **)&q, 10);
    if (x != y)
      return x > y;
    if (*p == '.') p++;
    if (*q == '.') q++;
  }
  return false;
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
  { "Game Boy (DMG)", "Game Boy (DMG)" },
  { "Game Boy Pocket", "Game Boy Pocket" },
  { "Game Boy Light", "Game Boy Light" },
  { "Super Game Boy 1-A", "Super Game Boy 1-A" },
  { "Super Game Boy 1-B", "Super Game Boy 1-B" },
  { "Super Game Boy 1-C", "Super Game Boy 1-C" },
  { "Super Game Boy 1-D", "Super Game Boy 1-D" },
  { "Super Game Boy 1-E", "Super Game Boy 1-E" },
  { "Super Game Boy 1-F", "Super Game Boy 1-F" },
  { "Super Game Boy 1-G", "Super Game Boy 1-G" },
  { "Super Game Boy 1-H", "Super Game Boy 1-H" },
  { "Super Game Boy 2-A", "Super Game Boy 2-A" },
  { "Super Game Boy 2-B", "Super Game Boy 2-B" },
  { "Super Game Boy 2-C", "Super Game Boy 2-C" },
  { "Super Game Boy 2-D", "Super Game Boy 2-D" },
  { "Super Game Boy 2-E", "Super Game Boy 2-E" },
  { "Super Game Boy 2-F", "Super Game Boy 2-F" },
  { "Super Game Boy 2-G", "Super Game Boy 2-G" },
  { "Super Game Boy 2-H", "Super Game Boy 2-H" },
  { "Super Game Boy 3-A", "Super Game Boy 3-A" },
  { "Super Game Boy 3-B", "Super Game Boy 3-B" },
  { "Super Game Boy 3-C", "Super Game Boy 3-C" },
  { "Super Game Boy 3-D", "Super Game Boy 3-D" },
  { "Super Game Boy 3-E", "Super Game Boy 3-E" },
  { "Super Game Boy 3-F", "Super Game Boy 3-F" },
  { "Super Game Boy 3-G", "Super Game Boy 3-G" },
  { "Super Game Boy 3-H", "Super Game Boy 3-H" },
  { "Super Game Boy 4-A", "Super Game Boy 4-A" },
  { "Super Game Boy 4-B", "Super Game Boy 4-B" },
  { "Super Game Boy 4-C", "Super Game Boy 4-C" },
  { "Super Game Boy 4-D", "Super Game Boy 4-D" },
  { "Super Game Boy 4-E", "Super Game Boy 4-E" },
  { "Super Game Boy 4-F", "Super Game Boy 4-F" },
  { "Super Game Boy 4-G", "Super Game Boy 4-G" },
  { "Super Game Boy 4-H", "Super Game Boy 4-H" },
  { "Sepia", "Sepia" },
  { "Azul", "Blue" },
  { "Rojo", "Red" },
  { "Autom\xe1tica (Game Boy Color)", "Automatic (Game Boy Color)" },
};

// 4 shades, lightest first (0xRRGGBB). The Game Boy, Pocket, Light and the
// 32 Super Game Boy presets (1-A ... 4-H) are taken from the Gambatte
// libretro core (libgambatte/libretro/gbcpalettes.h, GPL v2).
const unsigned int uiGbPalettes[GB_PALETTE_COUNT][4] = {
  { 0xF8F8F8, 0xA8A8A8, 0x606060, 0x000000 },
  { 0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F },
  { 0x578200, 0x317400, 0x005121, 0x00420C },
  { 0xA7B19A, 0x86927C, 0x535F49, 0x2A3325 },
  { 0x01CBDF, 0x01B6D5, 0x269BAD, 0x00778D },
  { 0xF8E8C8, 0xD89048, 0xA82820, 0x301850 },
  { 0xD8D8C0, 0xC8B070, 0xB05010, 0x000000 },
  { 0xF8C0F8, 0xE89850, 0x983860, 0x383898 },
  { 0xF8F8A8, 0xC08048, 0xF80000, 0x501800 },
  { 0xF8D8B0, 0x78C078, 0x688840, 0x583820 },
  { 0xD8E8F8, 0xE08850, 0xA80000, 0x004010 },
  { 0x000050, 0x00A0E8, 0x787800, 0xF8F858 },
  { 0xF8E8E0, 0xF8B888, 0x804000, 0x301800 },
  { 0xF0C8A0, 0xC08848, 0x287800, 0x000000 },
  { 0xF8F8F8, 0xF8E850, 0xF83000, 0x500058 },
  { 0xF8C0F8, 0xE88888, 0x7830E8, 0x282898 },
  { 0xF8F8A0, 0x00F800, 0xF83000, 0x000050 },
  { 0xF8C880, 0x90B0E0, 0x281060, 0x100810 },
  { 0xD0F8F8, 0xF89050, 0xA00000, 0x180000 },
  { 0x68B838, 0xE05040, 0xE0B880, 0x001800 },
  { 0xF8F8F8, 0xB8B8B8, 0x707070, 0x000000 },
  { 0xF8D098, 0x70C0C0, 0xF86028, 0x304860 },
  { 0xD8D8C0, 0xE08020, 0x005000, 0x001010 },
  { 0xE0A8C8, 0xF8F878, 0x00B8F8, 0x202058 },
  { 0xF0F8B8, 0xE0A878, 0x08C800, 0x000000 },
  { 0xF8F8C0, 0xE0B068, 0xB07820, 0x504870 },
  { 0x7878C8, 0xF868F8, 0xF8D000, 0x404040 },
  { 0x60D850, 0xF8F8F8, 0xC83038, 0x380000 },
  { 0xE0F8A0, 0x78C838, 0x488818, 0x081800 },
  { 0xF0A868, 0x78A8F8, 0xD000D0, 0x000078 },
  { 0xF0E8F0, 0xE8A060, 0x407838, 0x180808 },
  { 0xF8E0E0, 0xD8A0D0, 0x98A0E0, 0x080000 },
  { 0xF8F8B8, 0x90C8C8, 0x486878, 0x082048 },
  { 0xF8D8A8, 0xE0A878, 0x785888, 0x002030 },
  { 0xB8D0D0, 0xD880D8, 0x8000A0, 0x380000 },
  { 0xB0E018, 0xB82058, 0x281000, 0x008060 },
  { 0xF8F8C8, 0xB8C058, 0x808840, 0x405028 },
  // Extras (not from any console)
  { 0xF8F0D8, 0xC8A878, 0x806040, 0x302010 },
  { 0xE0F8F8, 0x88C0F0, 0x4060A8, 0x102040 },
  { 0xF8E0E0, 0xE08888, 0xA03030, 0x300808 },
  // Automatic: games with no Game Boy Color colors use gray.
  { 0xF8F8F8, 0xA8A8A8, 0x606060, 0x000000 },
};

const int uiSensorSensPercent[SENSOR_SENS_COUNT] = { 50, 75, 100, 150, 200 };
const int uiSensorDeadzone[SENSOR_DEADZONE_COUNT] = { 3000, 6000, 9000, 12000 };

int uiStickAmount(int dir, int lx, int ly, int rx, int ry)
{
  int v;
  switch (dir) {
  case SDIR_RS_RIGHT: v = rx; break;
  case SDIR_RS_LEFT: v = -rx; break;
  case SDIR_RS_UP: v = ry; break;
  case SDIR_RS_DOWN: v = -ry; break;
  case SDIR_LS_RIGHT: v = lx; break;
  case SDIR_LS_LEFT: v = -lx; break;
  case SDIR_LS_UP: v = ly; break;
  case SDIR_LS_DOWN: v = -ly; break;
  default: return 0;
  }
  if (v < 0) return 0;
  return v > 32767 ? 32767 : v;
}

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
