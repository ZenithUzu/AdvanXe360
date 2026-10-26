// RetroAchievements on the console: rcheevos (rc_client) plus the network
// (HTTPS, on a thread of its own), the game's memory and the pop-ups shown
// during the game. Softcore only (hardcore is for emulators the site approves).
#ifndef XBOX_CHEEVOS_H
#define XBOX_CHEEVOS_H

#include <string>
#include <vector>

struct UiRaInfo;
struct UiRaAchievement;

enum { CHEEVOS_GBA = 0, CHEEVOS_GB, CHEEVOS_GBC };

// At start-up, and when the option changes: logs in with the saved token.
void cheevosStart();
// Finishes the network replies that came back (login, game data, unlocks).
// Call it often, from the emulator's thread: every menu and game frame.
void cheevosPump();
// The game started: finds it on RetroAchievements (by the ROM's hash).
void cheevosGameStarted(int console, const unsigned char *rom, unsigned size);
// After every emulated frame: checks the achievements, moves the pop-ups.
void cheevosFrame();
// While the game is paused (in-game menu).
void cheevosIdle();
void cheevosGameExited();
// The game was reset or a saved state loaded: its memory changed at once, so
// achievements wait for fresh conditions instead of firing by mistake.
void cheevosReset();

// The running game as RetroAchievements identified it, by its ROM's hash:
// its title ("Pokemon Crystal Version"), also for versions it knows but
// doesn't support (other languages, fan translations). "" when there is no
// game, or it's a hack or homebrew ("~Hack~ ..."). For finding cheats.
std::string cheevosGameTitle();

void cheevosLogin(const std::string &user, const std::string &password);
void cheevosLogout();
void cheevosInfo(UiRaInfo &out);
void cheevosList(std::vector<UiRaAchievement> &out);

#endif
