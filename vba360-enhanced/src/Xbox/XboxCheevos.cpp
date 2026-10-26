// RetroAchievements: see XboxCheevos.h.
//
// rc_client (rcheevos, MIT) does the achievements logic. This file gives it:
//  - the network: each request goes to a thread of its own (HTTPS can take a
//    second, the game must not stop); the answer is handed back to rc_client
//    on the emulator's thread (cheevosPump), so rc_client is only ever used
//    from one thread;
//  - the game's memory, laid out as RetroAchievements expects it;
//  - pop-ups over the game, like RetroArch's: the badge, then a dark box
//    unfolding to its right with "Achievement unlocked" and the title.
#include "XboxCheevos.h"

#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>

#define RC_CLIENT_SUPPORTS_HASH   // as the rcheevos library is built (rcheevos.vcxproj)
#include "rc_client.h"
#include "rc_consoles.h"

#include "../ui/UiHost.h"
#include "../ui/UiCanvas.h"
#include "../ui/UiConfig.h"
#include "XboxCovers.h"

#ifdef _XBOX
#include <xtl.h>
#include "XboxHost.h"
#include "XboxLog.h"
#include "../gb/gbGambatteBridge.h"
typedef unsigned char u8;
extern u8 *internalRAM;
extern u8 *workRAM;
extern u8 flashSaveMemory[0x20000];
std::string xboxEmulatorDir();
bool xboxLoadImagePixels(const char *path, int w, int h, std::vector<UiColor> &out, bool smooth);
#define CHEEVOS_LOG xboxLog
#else
#define CHEEVOS_LOG(...) ((void)0)
#endif

extern UiConfig g_cfg;

namespace {

rc_client_t *g_client = 0;
int g_console = -1;             // CHEEVOS_*, while a game runs
int g_state = UI_RA_LOGGED_OUT;
std::string g_message;
std::string g_userAgent;

// --- Network ------------------------------------------------------------------

struct Job
{
  std::string url, post;
  std::string saveTo;           // a file download (badges): where it goes
  rc_client_server_callback_t callback;
  void *callbackData;
  std::string body;
  int status;
  Job() : callback(0), callbackData(0), status(0) {}
};

std::deque<Job *> g_todo, g_done;

#ifdef _XBOX
CRITICAL_SECTION g_lock;
HANDLE g_wake = 0;
HANDLE g_thread = 0;

DWORD WINAPI netThread(void *)
{
  for (;;) {
    WaitForSingleObject(g_wake, INFINITE);
    for (;;) {
      EnterCriticalSection(&g_lock);
      Job *j = g_todo.empty() ? 0 : g_todo.front();
      if (j)
        g_todo.pop_front();
      LeaveCriticalSection(&g_lock);
      if (!j)
        break;
      std::string err;
      if (!xboxNetStart(err) || !httpRequestStatus(j->url, j->post, j->body, j->status, err, g_userAgent)) {
        j->status = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;   // rc_client will try again
        j->body.clear();
        CHEEVOS_LOG("RetroAchievements: %s failed (%s)", j->saveTo.empty() ? "request" : "download", err.c_str());
      }
      if (!j->saveTo.empty() && j->status == 200 && !j->body.empty()) {
        FILE *f = fopen(j->saveTo.c_str(), "wb");
        if (f) {
          fwrite(j->body.data(), 1, j->body.size(), f);
          fclose(f);
        }
      }
      EnterCriticalSection(&g_lock);
      g_done.push_back(j);
      LeaveCriticalSection(&g_lock);
    }
  }
  return 0;
}

void queueJob(Job *j)
{
  if (!g_thread) {
    InitializeCriticalSection(&g_lock);
    g_wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_thread = CreateThread(NULL, 64 * 1024, netThread, NULL, CREATE_SUSPENDED, NULL);
    XSetThreadProcessor(g_thread, 2);   // away from the emulator's core
    ResumeThread(g_thread);
  }
  EnterCriticalSection(&g_lock);
  g_todo.push_back(j);
  LeaveCriticalSection(&g_lock);
  SetEvent(g_wake);
}

Job *takeDone()
{
  if (!g_thread)
    return 0;
  EnterCriticalSection(&g_lock);
  Job *j = g_done.empty() ? 0 : g_done.front();
  if (j)
    g_done.pop_front();
  LeaveCriticalSection(&g_lock);
  return j;
}
#else
void queueJob(Job *j) { g_done.push_back(j); }   // PC check build: no network
Job *takeDone()
{
  Job *j = g_done.empty() ? 0 : g_done.front();
  if (j)
    g_done.pop_front();
  return j;
}
#endif

void RC_CCONV serverCall(const rc_api_request_t *request, rc_client_server_callback_t callback, void *callbackData,
                         rc_client_t *)
{
  Job *j = new Job;
  j->url = request->url;
  if (request->post_data)
    j->post = request->post_data;
  j->callback = callback;
  j->callbackData = callbackData;
  queueJob(j);
}

// --- Memory ---------------------------------------------------------------------

// One byte at a RetroAchievements address; false past the console's memory.
bool readByte(uint32_t a, uint8_t &out)
{
#ifdef _XBOX
  if (g_console == CHEEVOS_GBA) {
    // 0x00000 IWRAM (32 KB), 0x08000 EWRAM (256 KB), 0x48000 save RAM (64 KB).
    if (a < 0x8000) { out = internalRAM ? internalRAM[a] : 0; return true; }
    if (a < 0x48000) { out = workRAM ? workRAM[a - 0x8000] : 0; return true; }
    if (a < 0x58000) { out = flashSaveMemory[a - 0x48000]; return true; }
    return false;
  }
  if (g_console == CHEEVOS_GB || g_console == CHEEVOS_GBC) {
    if (a >= 0x16000)
      return false;
    out = gmbBridge::peek(a);
    return true;
  }
#else
  (void)a;
  (void)out;
#endif
  return false;
}

uint32_t RC_CCONV readMemory(uint32_t address, uint8_t *buffer, uint32_t num, rc_client_t *)
{
  for (uint32_t i = 0; i < num; i++)
    if (!readByte(address + i, buffer[i]))
      return i;
  return num;
}

// --- Badges -----------------------------------------------------------------------

std::string badgeDir()
{
#ifdef _XBOX
  return uiJoinPath(uiJoinPath(xboxEmulatorDir(), "cheevos"), "badges");
#else
  return "badges";
#endif
}

std::string badgeFile(const std::string &name) { return uiJoinPath(badgeDir(), name + ".png"); }

bool fileExists(const std::string &path)
{
  FILE *f = fopen(path.c_str(), "rb");
  if (f)
    fclose(f);
  return f != 0;
}

std::map<std::string, bool> g_badgeAsked;

// Downloads a badge once (in the background); the pop-up uses it if it's there.
void fetchBadge(const std::string &name, const char *url)
{
  if (name.empty() || !url || !*url || g_badgeAsked[name])
    return;
  g_badgeAsked[name] = true;
  std::string path = badgeFile(name);
  if (fileExists(path))
    return;
  Job *j = new Job;
  j->url = url;
  j->saveTo = path;
  queueJob(j);
}

// --- Pop-ups ------------------------------------------------------------------------

struct Popup
{
  std::string title, subtitle, badge;   // badge: its file
  bool sound;
};
std::deque<Popup> g_popups;
bool g_popupOn = false;
int g_popupFrame = 0;
int g_popupBoxW = 0;                    // width of the box right of the badge

// RetroArch's timing: slide in, unfold, 4 seconds, fold, slide out (60 frames a second).
enum { POP_H = 84, POP_SLIDE = 18, POP_UNFOLD = 18, POP_HOLD = 240, POP_MARGIN = 24 };
const int kPopTotal = POP_SLIDE + POP_UNFOLD + POP_HOLD + POP_UNFOLD + POP_SLIDE;

void popup(const std::string &title, const std::string &subtitle, const std::string &badge, bool sound)
{
  Popup p;
  p.title = title;
  p.subtitle = subtitle;
  p.badge = badge;
  p.sound = sound;
  if (g_popups.size() < 8)
    g_popups.push_back(p);
}

// Draws the pop-up once into a canvas the size of the whole thing; the frames
// only move it and show more or less of it.
void buildPopup(const Popup &p)
{
#ifdef _XBOX
  UiCanvas probe;
  probe.create(4, 4);
  int tw = probe.textWidth(UI_FONT_SMALL, p.title.c_str());
  int sw = probe.textWidth(UI_FONT_BODY, p.subtitle.c_str());
  g_popupBoxW = (tw > sw ? tw : sw) + 40;
  if (g_popupBoxW > 760)
    g_popupBoxW = 760;
  UiCanvas c;
  c.create(POP_H + g_popupBoxW, POP_H);
  c.fillRect(0, 0, c.width(), c.height(), 0xFF000000);
  c.fillRect(POP_H, 0, g_popupBoxW, POP_H, 0xF0161616);   // RetroArch's dark backdrop
  std::vector<UiColor> px;
  if (!p.badge.empty() && fileExists(p.badge) && xboxLoadImagePixels(p.badge.c_str(), POP_H, POP_H, px, true))
    c.blitAlpha(&px[0], POP_H, POP_H, 0, 0);
  else
    c.fillGradient(0, 0, POP_H, POP_H, 0xFF3A3F55, 0xFF1C1F2B);   // no badge (yet): a plain square
  c.drawTextFit(UI_FONT_SMALL, POP_H + 20, 12, g_popupBoxW - 40, p.title.c_str(), 0xFF878787);
  c.drawTextFit(UI_FONT_BODY, POP_H + 20, 38, g_popupBoxW - 40, p.subtitle.c_str(), 0xFFF0F0F0);
  xboxOverlaySet(&c);
  if (p.sound && g_cfg.raSound)
    xboxPlaySound(uiJoinPath(uiJoinPath(xboxEmulatorDir(), "sounds"), "unlock.wav"));
#else
  (void)p;
#endif
}

float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

void tickPopup()
{
#ifdef _XBOX
  if (!g_popupOn) {
    if (g_popups.empty())
      return;
    buildPopup(g_popups.front());
    g_popups.pop_front();
    g_popupOn = true;
    g_popupFrame = 0;
  }
  int f = g_popupFrame++;
  float slide, unfold;
  if (f < POP_SLIDE) { slide = easeOut((float)f / POP_SLIDE); unfold = 0; }
  else if ((f -= POP_SLIDE) < POP_UNFOLD) { slide = 1; unfold = easeOut((float)f / POP_UNFOLD); }
  else if ((f -= POP_UNFOLD) < POP_HOLD) { slide = 1; unfold = 1; }
  else if ((f -= POP_HOLD) < POP_UNFOLD) { slide = 1; unfold = 1 - easeOut((float)f / POP_UNFOLD); }
  else if ((f -= POP_UNFOLD) < POP_SLIDE) { slide = 1 - easeOut((float)f / POP_SLIDE); unfold = 0; }
  else {
    xboxOverlaySet(0);
    g_popupOn = false;
    return;
  }
  int shown = POP_H + (int)(g_popupBoxW * unfold);
  int x = (1280 - shown) / 2;   // centered at the top, as RetroArch puts it
  int y = -POP_H + (int)((POP_H + POP_MARGIN) * slide);
  xboxOverlayPlace(x, y, shown);
#endif
}

// --- Events -------------------------------------------------------------------------

const char *tr(const char *es, const char *en) { return g_cfg.language ? en : es; }

void RC_CCONV onEvent(const rc_client_event_t *e, rc_client_t *client)
{
  char buf[256];
  switch (e->type) {
  case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED: {
    const rc_client_achievement_t *a = e->achievement;
    sprintf(buf, tr("Logro desbloqueado (%u puntos)", "Achievement unlocked (%u points)"), (unsigned)a->points);
    popup(buf, a->title ? a->title : "", badgeFile(a->badge_name), true);
    CHEEVOS_LOG("RetroAchievements: unlocked %u \"%s\"", (unsigned)a->id, a->title ? a->title : "");
    break;
  }
  case RC_CLIENT_EVENT_GAME_COMPLETED: {
    const rc_client_game_t *g = rc_client_get_game_info(client);
    popup(tr("\xa1Juego dominado!", "Game mastered!"), g && g->title ? g->title : "",
          g && g->badge_name ? badgeFile(std::string("game-") + g->badge_name) : std::string(), true);
    break;
  }
  case RC_CLIENT_EVENT_SERVER_ERROR:
    if (e->server_error && e->server_error->error_message)
      popup("RetroAchievements", e->server_error->error_message, "", false);
    break;
  case RC_CLIENT_EVENT_DISCONNECTED:
    popup("RetroAchievements", tr("Sin conexi\xf3n: los logros se enviar\xe1n despu\xe9s",
                                  "Offline: achievements will be sent later"), "", false);
    break;
  case RC_CLIENT_EVENT_RECONNECTED:
    popup("RetroAchievements", tr("Conectado de nuevo: logros enviados", "Back online: achievements sent"), "", false);
    break;
  default:
    break;
  }
}

void RC_CCONV onLogin(int result, const char *error, rc_client_t *client, void *)
{
  if (result == RC_OK) {
    const rc_client_user_t *u = rc_client_get_user_info(client);
    g_state = UI_RA_LOGGED_IN;
    g_message.clear();
    if (u) {
      g_cfg.raUser = u->username ? u->username : g_cfg.raUser;
      g_cfg.raToken = u->token ? u->token : g_cfg.raToken;   // saved instead of the password
    }
    CHEEVOS_LOG("RetroAchievements: logged in as %s", g_cfg.raUser.c_str());
  } else {
    g_state = UI_RA_FAILED;
    g_message = error ? error : rc_error_str(result);
    CHEEVOS_LOG("RetroAchievements: login failed (%s)", g_message.c_str());
    if (result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN)
      g_cfg.raToken.clear();   // the next login needs the password again
  }
}

// After loading, the badges of the game and its achievements are fetched, so
// the pop-ups can show them at once.
void fetchBadges()
{
  const rc_client_game_t *g = rc_client_get_game_info(g_client);
  if (g && g->badge_name && g->badge_url)
    fetchBadge(std::string("game-") + g->badge_name, g->badge_url);
  rc_client_achievement_list_t *list = rc_client_create_achievement_list(
      g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  if (!list)
    return;
  for (uint32_t b = 0; b < list->num_buckets; b++)
    for (uint32_t i = 0; i < list->buckets[b].num_achievements; i++) {
      const rc_client_achievement_t *a = list->buckets[b].achievements[i];
      fetchBadge(a->badge_name, a->badge_url);
    }
  rc_client_destroy_achievement_list(list);
}

void RC_CCONV onGameLoaded(int result, const char *error, rc_client_t *client, void *)
{
  if (result != RC_OK) {
    if (result == RC_NO_GAME_LOADED)
      popup("RetroAchievements", tr("Este juego no est\xe1 en RetroAchievements", "This game isn't on RetroAchievements"),
            "", false);
    else if (result != RC_ABORTED)
      popup("RetroAchievements", error ? error : rc_error_str(result), "", false);
    CHEEVOS_LOG("RetroAchievements: game not loaded (%d %s)", result, error ? error : "");
    return;
  }
  const rc_client_game_t *g = rc_client_get_game_info(client);
  rc_client_user_game_summary_t s;
  rc_client_get_user_game_summary(client, &s);
  char buf[160];
  if (s.num_core_achievements == 0)
    strcpy(buf, tr("Este juego a\xfan no tiene logros", "This game has no achievements yet"));
  else
    sprintf(buf, tr("Tienes %u de %u logros", "You have %u of %u achievements"), (unsigned)s.num_unlocked_achievements,
            (unsigned)s.num_core_achievements);
  fetchBadges();
  popup(g && g->title ? g->title : "RetroAchievements", buf,
        g && g->badge_name ? badgeFile(std::string("game-") + g->badge_name) : std::string(), false);
  CHEEVOS_LOG("RetroAchievements: %s (%u/%u)", g && g->title ? g->title : "?", (unsigned)s.num_unlocked_achievements,
              (unsigned)s.num_core_achievements);
}

void makeClient()
{
  if (g_client)
    return;
  g_client = rc_client_create(readMemory, serverCall);
  rc_client_set_event_handler(g_client, onEvent);
  rc_client_set_hardcore_enabled(g_client, 0);   // softcore: hardcore is for approved emulators
  char clause[64];
  rc_client_get_user_agent_clause(g_client, clause, sizeof(clause));
  g_userAgent = std::string("AdvanXe360/1.0 (Xbox 360) ") + clause;
#ifdef _XBOX
  CreateDirectory(uiJoinPath(xboxEmulatorDir(), "cheevos").c_str(), NULL);
  CreateDirectory(badgeDir().c_str(), NULL);
#endif
}

} // namespace

void cheevosStart()
{
  if (!g_cfg.raEnabled) {
    if (g_client) {
      rc_client_destroy(g_client);
      g_client = 0;
    }
    g_state = UI_RA_LOGGED_OUT;
    return;
  }
  makeClient();
  if (g_state != UI_RA_LOGGED_IN && g_state != UI_RA_LOGGING_IN && !g_cfg.raUser.empty() && !g_cfg.raToken.empty()) {
    g_state = UI_RA_LOGGING_IN;
    rc_client_begin_login_with_token(g_client, g_cfg.raUser.c_str(), g_cfg.raToken.c_str(), onLogin, 0);
  }
}

void cheevosLogin(const std::string &user, const std::string &password)
{
  makeClient();
  rc_client_logout(g_client);
  g_state = UI_RA_LOGGING_IN;
  g_message.clear();
  rc_client_begin_login_with_password(g_client, user.c_str(), password.c_str(), onLogin, 0);
}

void cheevosLogout()
{
  if (g_client)
    rc_client_logout(g_client);
  g_cfg.raToken.clear();
  g_state = UI_RA_LOGGED_OUT;
}

void cheevosPump()
{
  Job *j;
  while ((j = takeDone()) != 0) {
    if (j->callback) {
      rc_api_server_response_t r;
      r.body = j->body.c_str();
      r.body_length = j->body.size();
      r.http_status_code = j->status;
      j->callback(&r, j->callbackData);
    }
    delete j;
  }
}

void cheevosGameStarted(int console, const unsigned char *rom, unsigned size)
{
  g_console = console;
  if (!g_cfg.raEnabled || !g_client || !rom || !size || (g_state != UI_RA_LOGGED_IN && g_state != UI_RA_LOGGING_IN))
    return;   // not logged in: no "login required" message on every game
  uint32_t id = console == CHEEVOS_GBA ? RC_CONSOLE_GAMEBOY_ADVANCE
              : console == CHEEVOS_GBC ? RC_CONSOLE_GAMEBOY_COLOR : RC_CONSOLE_GAMEBOY;
  // Identified by the hash of the ROM in memory (works for .zip / .7z too).
  rc_client_begin_identify_and_load_game(g_client, id, NULL, rom, size, onGameLoaded, 0);
}

void cheevosFrame()
{
  cheevosPump();
  if (g_client && g_console >= 0)
    rc_client_do_frame(g_client);
  tickPopup();
}

void cheevosIdle()
{
  cheevosPump();
  if (g_client)
    rc_client_idle(g_client);
}

void cheevosReset()
{
  if (g_client && g_console >= 0)
    rc_client_reset(g_client);
}

void cheevosGameExited()
{
  if (g_client)
    rc_client_unload_game(g_client);
  g_console = -1;
  g_popups.clear();
  if (g_popupOn) {
#ifdef _XBOX
    xboxOverlaySet(0);
#endif
    g_popupOn = false;
  }
}

void cheevosInfo(UiRaInfo &out)
{
  out = UiRaInfo();
  if (!g_cfg.raEnabled)
    return;
  out.state = g_state;
  out.user = g_cfg.raUser;
  out.message = g_message;
  if (g_client && rc_client_is_game_loaded(g_client)) {
    const rc_client_game_t *g = rc_client_get_game_info(g_client);
    rc_client_user_game_summary_t s;
    rc_client_get_user_game_summary(g_client, &s);
    out.game = g && g->title ? g->title : "";
    out.unlocked = (int)s.num_unlocked_achievements;
    out.total = (int)s.num_core_achievements;
    out.points = (int)s.points_unlocked;
    out.pointsTotal = (int)s.points_core;
  }
}

void cheevosList(std::vector<UiRaAchievement> &out)
{
  out.clear();
  if (!g_client || !rc_client_is_game_loaded(g_client))
    return;
  rc_client_achievement_list_t *list = rc_client_create_achievement_list(
      g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  if (!list)
    return;
  for (uint32_t b = 0; b < list->num_buckets; b++)
    for (uint32_t i = 0; i < list->buckets[b].num_achievements; i++) {
      const rc_client_achievement_t *a = list->buckets[b].achievements[i];
      UiRaAchievement e;
      e.title = a->title ? a->title : "";
      e.description = a->description ? a->description : "";
      e.progress = a->measured_progress;
      e.points = (int)a->points;
      e.unlocked = a->unlocked != 0;
      out.push_back(e);
    }
  rc_client_destroy_achievement_list(list);
}
