// Box art downloader: finds each ROM's cover in the libretro thumbnail
// server (thumbnails.libretro.com, the one RetroArch uses) and saves it as
// covers\<ROM name>.png, where the menu already looks for covers.
//
// Plain HTTP only (the Xbox 360 has no TLS for homebrew). The server address
// can be changed in vba360.ini (cover_server=...), e.g. to a PC on the local
// network that forwards the requests.
//
// Portable C++03: the network and thread parts use the Xbox 360 XNet /
// Winsock API on the console and POSIX sockets elsewhere (tests).
#ifndef XBOX_COVERS_H
#define XBOX_COVERS_H

#include <string>
#include <vector>

#include "../ui/UiHost.h"

// --- Name matching (pure functions, tested on PC) ---------------------------

// Names of the .png files in an HTML directory listing, without ".png",
// URL-decoded ("Pokemon - Emerald Version (USA, Europe)").
std::vector<std::string> coversParseIndex(const std::string &html);
// Percent-encodes a path piece for a URL (spaces and symbols).
std::string coversUrlEncode(const std::string &s);
// libretro's file name rule: &*/:`<>?\| become '_'.
std::string coversLibretroName(const std::string &name);
// Index of the box art in names that best fits a ROM file name (without
// extension), or -1 when nothing fits well enough.
int coversBestMatch(const std::string &romName, const std::vector<std::string> &names);

// --- Download job -------------------------------------------------------------

// Starts downloading in a background thread. Returns false if a job is
// already running or the list is empty.
bool coversStart(const std::vector<UiCoverItem> &items, const std::string &server);
void coversStatus(UiCoverStatus &out);
void coversCancel();

// Runs a whole job in the calling thread (used by the thread and by tests).
void coversRun(const std::vector<UiCoverItem> &items, const std::string &server);

// Cheats from a cheat pack in RetroArch's layout (zipPath: the one shipped in
// cheats\, else libretro's, downloaded once). Looks up the GBA game code
// (gameCode, 4 letters, or "") in its _gamecodes.txt, then the name, then the
// name without spaces; the .cht found is written to outPath.
// err: "net" / "dns" / ... / "nomatch" / "zip".
bool cheatsFetch(const std::string &romName, const std::string &gameCode, const char *systemFolder,
                 const std::string &zipPath, const std::string &outPath, std::string &err);

// Network for the rest of the emulator (RetroAchievements...).
bool xboxNetStart(std::string &err);
// GET (post empty) or POST; http:// or https://. userAgent "" = AdvanXe360.
bool httpRequest(const std::string &url, const std::string &post, std::string &body, std::string &err,
                 const std::string &userAgent);
// The same, for APIs: true whenever a server answered, with its status code
// and text (err also says "http 401"...); false only when nothing came back.
bool httpRequestStatus(const std::string &url, const std::string &post, std::string &body, int &status,
                       std::string &err, const std::string &userAgent);

#endif
