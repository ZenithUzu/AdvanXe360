// Box art downloader (see XboxCovers.h).
#include "XboxCovers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#ifdef _XBOX
#include <xtl.h>
#include <winsockx.h>
#include "XboxLog.h"
#define COVERS_LOG xboxLog
#else
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
static void COVERS_LOG(const char *fmt, ...) {}
#endif

// ============================================================================
// Name matching
// ============================================================================

namespace {

std::string lowerAscii(const std::string &s)
{
  std::string r = s;
  for (size_t i = 0; i < r.size(); i++) {
    unsigned char c = (unsigned char)r[i];
    if (c >= 'A' && c <= 'Z')
      r[i] = (char)(c - 'A' + 'a');
  }
  return r;
}

// Latin-1 accented letters to plain ones ("Pok\xe9mon" -> pokemon).
char plainLetter(unsigned char c)
{
  if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return (char)c;
  if ((c >= 0xC0 && c <= 0xC5) || (c >= 0xE0 && c <= 0xE5)) return 'a';
  if ((c >= 0xC8 && c <= 0xCB) || (c >= 0xE8 && c <= 0xEB)) return 'e';
  if ((c >= 0xCC && c <= 0xCF) || (c >= 0xEC && c <= 0xEF)) return 'i';
  if ((c >= 0xD2 && c <= 0xD6) || (c >= 0xF2 && c <= 0xF6)) return 'o';
  if ((c >= 0xD9 && c <= 0xDC) || (c >= 0xF9 && c <= 0xFC)) return 'u';
  if (c == 0xD1 || c == 0xF1) return 'n';
  if (c == 0xC7 || c == 0xE7) return 'c';
  return 0;
}

// UTF-8 accented letters (C3 xx) etc. read as one Latin-1 letter.
std::string toLatin1(const std::string &s)
{
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == 0xC3 && i + 1 < s.size()) {
      r += (char)(((unsigned char)s[i + 1] & 0x3F) | 0xC0);
      i++;
    } else {
      r += (char)c;
    }
  }
  return r;
}

bool isStopWord(const std::string &w)
{
  static const char *stop[] = { "the", "a", "an", "of", "and", "version", "edition", "de", "la",
                                "el", "los", "las", "y", 0 };
  for (int i = 0; stop[i]; i++)
    if (w == stop[i])
      return true;
  return false;
}

std::vector<std::string> words(const std::string &s, bool dropStop)
{
  std::vector<std::string> out;
  std::string latin = toLatin1(s), cur;
  for (size_t i = 0; i <= latin.size(); i++) {
    char c = i < latin.size() ? plainLetter((unsigned char)latin[i]) : 0;
    if (c) {
      cur += c;
    } else if (!cur.empty()) {
      if (!dropStop || !isStopWord(cur))
        out.push_back(cur);
      cur.clear();
    }
  }
  return out;
}

// "Title (USA) [b]" -> "Title" and "(usa) [b]".
void splitTitle(const std::string &name, std::string &title, std::string &tags)
{
  size_t p = name.find_first_of("([");
  title = p == std::string::npos ? name : name.substr(0, p);
  tags = p == std::string::npos ? std::string() : lowerAscii(name.substr(p));
}

bool wordMatches(const std::string &a, const std::string &b)
{
  if (a == b)
    return true;
  // "pkmn" no, but "zeld" / "zelda" or "minish" / "minishcap" style prefixes yes.
  size_t n = a.size() < b.size() ? a.size() : b.size();
  return n >= 4 && a.compare(0, n, b, 0, n) == 0;
}

bool hasWord(const std::string &tags, const char *w)
{
  std::vector<std::string> t = words(tags, false);
  for (size_t i = 0; i < t.size(); i++)
    if (t[i] == w)
      return true;
  return false;
}

int scoreName(const std::string &rom, const std::string &cand)
{
  std::string lr = lowerAscii(rom), lc = lowerAscii(cand);
  if (lr == lc || lowerAscii(coversLibretroName(rom)) == lc)
    return 1000;

  std::string rTitle, rTags, cTitle, cTags;
  splitTitle(rom, rTitle, rTags);
  splitTitle(cand, cTitle, cTags);
  std::vector<std::string> rw = words(rTitle, true), cw = words(cTitle, true);
  if (rw.empty() || cw.empty())
    return 0;

  // Every word of the ROM's title must be in the cover's title (one may be
  // missing in long titles); the more extra words the cover has, the lower.
  int common = 0;
  std::vector<bool> used(cw.size(), false);
  for (size_t i = 0; i < rw.size(); i++) {
    for (size_t j = 0; j < cw.size(); j++) {
      if (!used[j] && wordMatches(rw[i], cw[j])) {
        used[j] = true;
        common++;
        break;
      }
    }
  }
  int missingWords = (int)rw.size() - common;
  if (missingWords > (rw.size() >= 4 ? 1 : 0))
    return 0;
  // Words in the ROM's brackets that are part of the cover's title count too:
  // "Super Mario Bros (Classic NES Series)" = "Classic NES Series - Super Mario Bros.".
  int romWords = (int)rw.size();
  std::vector<std::string> tw = words(rTags, true);
  for (size_t i = 0; i < tw.size(); i++) {
    for (size_t j = 0; j < cw.size(); j++) {
      if (!used[j] && wordMatches(tw[i], cw[j])) {
        used[j] = true;
        common++;
        romWords++;
        break;
      }
    }
  }
  int unionWords = romWords + (int)cw.size() - common;
  int score = (common == romWords && common == (int)cw.size()) ? 600 : 400 * common / unionWords;
  if (score < 250)
    return 0;   // too loose ("Pokemon" alone, "Golden Sun" vs "Golden Sun - The Lost Age")

  // Prefer the same region as the ROM, then USA / World / Europe.
  static const char *regions[] = { "usa", "world", "europe", "japan", "spain", "france", "germany",
                                   "italy", "brazil", "korea", "australia", 0 };
  bool romRegion = false;
  for (int i = 0; regions[i]; i++) {
    if (hasWord(rTags, regions[i])) {
      romRegion = true;
      if (hasWord(cTags, regions[i]))
        score += 60;
    }
  }
  if (!romRegion) {
    if (hasWord(cTags, "usa")) score += 30;
    else if (hasWord(cTags, "world")) score += 25;
    else if (hasWord(cTags, "europe")) score += 20;
    else if (hasWord(cTags, "japan")) score += 5;
  }
  // Translations, prototypes, demos... only when nothing better exists.
  if (cTags.find('[') != std::string::npos && rTags.find('[') == std::string::npos)
    score -= 50;
  static const char *odd[] = { "beta", "proto", "demo", "sample", "kiosk", "unl", "pirate", 0 };
  for (int i = 0; odd[i]; i++)
    if (hasWord(cTags, odd[i]) && !hasWord(rTags, odd[i]))
      score -= 40;
  if (hasWord(cTags, "rev"))
    score -= 2;
  return score;
}

int hexValue(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string urlDecode(const std::string &s)
{
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size() && hexValue(s[i + 1]) >= 0 && hexValue(s[i + 2]) >= 0) {
      r += (char)(hexValue(s[i + 1]) * 16 + hexValue(s[i + 2]));
      i += 2;
    } else if (s[i] == '+') {
      r += '+';
    } else {
      r += s[i];
    }
  }
  return r;
}

std::string htmlUnescape(const std::string &s)
{
  static const char *ent[][2] = { { "&amp;", "&" }, { "&#39;", "'" }, { "&quot;", "\"" },
                                  { "&lt;", "<" }, { "&gt;", ">" }, { 0, 0 } };
  std::string r = s;
  for (int i = 0; ent[i][0]; i++) {
    size_t p = 0, n = strlen(ent[i][0]);
    while ((p = r.find(ent[i][0], p)) != std::string::npos) {
      r.replace(p, n, ent[i][1]);
      p++;
    }
  }
  return r;
}

} // namespace

std::vector<std::string> coversParseIndex(const std::string &html)
{
  std::vector<std::string> out;
  size_t p = 0;
  while ((p = html.find("href=\"", p)) != std::string::npos) {
    p += 6;
    size_t e = html.find('"', p);
    if (e == std::string::npos)
      break;
    std::string link = htmlUnescape(html.substr(p, e - p));
    p = e;
    if (link.find('/') != std::string::npos || link.find('?') != std::string::npos)
      continue;   // parent folder, sorting links
    std::string name = urlDecode(link);
    if (name.size() > 4 && lowerAscii(name.substr(name.size() - 4)) == ".png")
      out.push_back(name.substr(0, name.size() - 4));
  }
  return out;
}

std::string coversUrlEncode(const std::string &s)
{
  static const char *hex = "0123456789ABCDEF";
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      r += (char)c;
    } else {
      r += '%';
      r += hex[c >> 4];
      r += hex[c & 15];
    }
  }
  return r;
}

std::string coversLibretroName(const std::string &name)
{
  std::string r = name;
  for (size_t i = 0; i < r.size(); i++)
    if (strchr("&*/:`<>?\\|", r[i]))
      r[i] = '_';
  return r;
}

int coversBestMatch(const std::string &romName, const std::vector<std::string> &names)
{
  int best = -1, bestScore = 199;
  for (size_t i = 0; i < names.size(); i++) {
    int s = scoreName(romName, names[i]);
    if (s > bestScore) {
      bestScore = s;
      best = (int)i;
    }
  }
  return best;
}

// ============================================================================
// Network: plain HTTP GET
// ============================================================================

namespace {

#ifdef _XBOX
typedef SOCKET Sock;
const Sock kBadSock = INVALID_SOCKET;
void closeSock(Sock s) { closesocket(s); }
#else
typedef int Sock;
const Sock kBadSock = -1;
void closeSock(Sock s) { close(s); }
#endif

volatile bool g_cancel = false;

std::string intStr(int v)
{
  char b[16];
  sprintf(b, "%d", v);
  return b;
}

bool netStart(std::string &err)
{
#ifdef _XBOX
  static bool started = false;
  if (started)
    return true;
  // Homebrew talks to normal (non Xbox Live) servers: no network security.
  XNetStartupParams xnsp;
  memset(&xnsp, 0, sizeof(xnsp));
  xnsp.cfgSizeOfStruct = sizeof(xnsp);
  xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
  int r = XNetStartup(&xnsp);
  if (r != 0)
    COVERS_LOG("Covers: XNetStartup returned %d", r);
  WSADATA wsa;
  r = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (r != 0) {
    err = "WSAStartup " + intStr(r);
    return false;
  }
  if (!(XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE)) {
    err = "link";
    return false;
  }
  // Wait for the address (DHCP) for up to 10 seconds.
  XNADDR addr;
  DWORD st = XNET_GET_XNADDR_PENDING;
  for (int i = 0; i < 200; i++) {
    st = XNetGetTitleXnAddr(&addr);
    if (st != XNET_GET_XNADDR_PENDING)
      break;
    Sleep(50);
  }
  if (st == XNET_GET_XNADDR_PENDING || st == XNET_GET_XNADDR_NONE) {
    err = "ip";
    return false;
  }
  started = true;
  return true;
#else
  return true;
#endif
}

bool resolve(const std::string &host, unsigned long &ip, std::string &err)
{
  unsigned long a = inet_addr(host.c_str());
  if (a != (unsigned long)INADDR_NONE && a != 0) {
    ip = a;
    return true;
  }
#ifdef _XBOX
  XNDNS *dns = NULL;
  if (XNetDnsLookup(host.c_str(), NULL, &dns) != 0 || !dns) {
    err = "dns";
    return false;
  }
  for (int i = 0; i < 500 && dns->iStatus == WSAEINPROGRESS && !g_cancel; i++)
    Sleep(20);
  bool ok = dns->iStatus == 0 && dns->cina > 0;
  if (ok)
    ip = dns->aina[0].s_addr;
  XNetDnsRelease(dns);
  if (!ok)
    err = "dns";
  return ok;
#else
  struct addrinfo hints, *res = 0;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host.c_str(), 0, &hints, &res) != 0 || !res) {
    err = "dns";
    return false;
  }
  ip = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
  freeaddrinfo(res);
  return true;
#endif
}

Sock connectTo(unsigned long ip, int port)
{
  Sock s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == kBadSock)
    return kBadSock;
#ifdef _XBOX
  BOOL yes = TRUE;
  setsockopt(s, SOL_SOCKET, 0x5801, (const char *)&yes, sizeof(yes));   // SO_MARKINSECURE
  DWORD ms = 15000;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof(ms));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, sizeof(ms));
#else
  struct timeval tv;
  tv.tv_sec = 15;
  tv.tv_usec = 0;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons((unsigned short)port);
  sa.sin_addr.s_addr = ip;
  if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
    closeSock(s);
    return kBadSock;
  }
  return s;
}

// "http://host[:port]/path" -> parts. False for anything else (https...).
bool splitUrl(const std::string &url, std::string &host, int &port, std::string &path)
{
  const std::string pre = "http://";
  if (lowerAscii(url.substr(0, pre.size())) != pre)
    return false;
  std::string rest = url.substr(pre.size());
  size_t slash = rest.find('/');
  std::string hp = slash == std::string::npos ? rest : rest.substr(0, slash);
  path = slash == std::string::npos ? "/" : rest.substr(slash);
  size_t colon = hp.find(':');
  port = 80;
  if (colon != std::string::npos) {
    port = atoi(hp.c_str() + colon + 1);
    hp = hp.substr(0, colon);
  }
  host = hp;
  return !host.empty() && port > 0;
}

std::string headerValue(const std::string &head, const char *name)
{
  std::string l = lowerAscii(head), key = std::string("\r\n") + name + ":";
  size_t p = l.find(key);
  if (p == std::string::npos)
    return "";
  p += key.size();
  size_t e = head.find("\r\n", p);
  std::string v = head.substr(p, e == std::string::npos ? std::string::npos : e - p);
  while (!v.empty() && (v[0] == ' ' || v[0] == '\t'))
    v.erase(0, 1);
  return v;
}

std::string unchunk(const std::string &body)
{
  std::string out;
  size_t p = 0;
  while (p < body.size()) {
    size_t e = body.find("\r\n", p);
    if (e == std::string::npos)
      break;
    unsigned long n = strtoul(body.substr(p, e - p).c_str(), 0, 16);
    p = e + 2;
    if (n == 0 || p + n > body.size())
      break;
    out.append(body, p, n);
    p += n + 2;
  }
  return out;
}

// GET a URL into body. err: "" or a short reason ("https", "dns", "connect",
// "http 404"...). Follows plain-HTTP redirects.
bool httpGet(const std::string &url0, std::string &body, std::string &err)
{
  std::string url = url0;
  for (int hop = 0; hop < 4; hop++) {
    std::string host, path;
    int port;
    if (!splitUrl(url, host, port, path)) {
      err = lowerAscii(url.substr(0, 8)) == "https://" ? "https" : "url";
      return false;
    }
    unsigned long ip;
    if (!resolve(host, ip, err))
      return false;
    Sock s = connectTo(ip, port);
    if (s == kBadSock) {
      err = "connect";
      return false;
    }
    std::string req = "GET " + path + " HTTP/1.0\r\nHost: " + host +
                      "\r\nUser-Agent: VBA360\r\nConnection: close\r\n\r\n";
    if (send(s, req.c_str(), (int)req.size(), 0) != (int)req.size()) {
      closeSock(s);
      err = "send";
      return false;
    }
    std::string data;
    char buf[8192];
    for (;;) {
      if (g_cancel) {
        closeSock(s);
        err = "cancel";
        return false;
      }
      int n = recv(s, buf, sizeof(buf), 0);
      if (n <= 0)
        break;
      data.append(buf, n);
      if (data.size() > 8 * 1024 * 1024)
        break;
    }
    closeSock(s);
    size_t he = data.find("\r\n\r\n");
    if (data.compare(0, 5, "HTTP/") != 0 || he == std::string::npos) {
      err = data.empty() ? "empty" : "reply";
      return false;
    }
    std::string head = data.substr(0, he + 2);
    size_t sp = head.find(' ');
    int status = sp == std::string::npos ? 0 : atoi(head.c_str() + sp + 1);
    if (status >= 300 && status < 400) {
      std::string loc = headerValue(head, "location");
      if (loc.empty()) {
        err = "http " + intStr(status);
        return false;
      }
      if (loc[0] == '/')
        loc = "http://" + host + (port != 80 ? ":" + intStr(port) : std::string()) + loc;
      url = loc;
      continue;
    }
    if (status != 200) {
      err = "http " + intStr(status);
      return false;
    }
    body = data.substr(he + 4);
    if (lowerAscii(headerValue(head, "transfer-encoding")).find("chunked") != std::string::npos)
      body = unchunk(body);
    return true;
  }
  err = "redirects";
  return false;
}

const char *kSystemFolder[UI_COVER_SYSTEMS] = {
  "Nintendo - Game Boy Advance", "Nintendo - Game Boy", "Nintendo - Game Boy Color" };

// --- Job state shared with the menu -------------------------------------------

#ifdef _XBOX
CRITICAL_SECTION g_lock;
bool g_lockInit = false;
void lockInit() { if (!g_lockInit) { InitializeCriticalSection(&g_lock); g_lockInit = true; } }
void lock() { lockInit(); EnterCriticalSection(&g_lock); }
void unlock() { LeaveCriticalSection(&g_lock); }
#else
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
void lock() { pthread_mutex_lock(&g_lock); }
void unlock() { pthread_mutex_unlock(&g_lock); }
#endif

UiCoverStatus g_status;
std::vector<UiCoverItem> g_items;
std::string g_server;

void setStatus(const UiCoverStatus &s)
{
  lock();
  g_status = s;
  unlock();
}

void makeParentDir(const std::string &file)
{
  size_t p = file.find_last_of("\\/");
  if (p == std::string::npos)
    return;
  std::string dir = file.substr(0, p);
#ifdef _XBOX
  CreateDirectory(dir.c_str(), NULL);
#else
  mkdir(dir.c_str(), 0755);
#endif
}

bool writeFile(const std::string &path, const std::string &data)
{
  makeParentDir(path);
  std::string tmp = path + ".part";
  FILE *f = fopen(tmp.c_str(), "wb");
  if (!f)
    return false;
  bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
  ok = (fclose(f) == 0) && ok;
  if (ok) {
    remove(path.c_str());
    ok = rename(tmp.c_str(), path.c_str()) == 0;
  }
  if (!ok)
    remove(tmp.c_str());
  return ok;
}

std::string trimSlash(const std::string &s)
{
  std::string r = s;
  while (!r.empty() && r[r.size() - 1] == '/')
    r.erase(r.size() - 1);
  return r;
}

} // namespace

void coversRun(const std::vector<UiCoverItem> &items, const std::string &server0)
{
  UiCoverStatus st;
  st.running = true;
  st.total = (int)items.size();
  setStatus(st);
  std::string server = trimSlash(server0.empty() ? std::string("http://thumbnails.libretro.com") : server0);
  COVERS_LOG("Covers: %d games from %s", st.total, server.c_str());

  std::string err;
  if (!netStart(err)) {
    st.error = err == "link" ? "nolink" : err == "ip" ? "noip" : err;
    st.running = false;
    setStatus(st);
    COVERS_LOG("Covers: no network (%s)", err.c_str());
    return;
  }

  std::vector<std::string> index[UI_COVER_SYSTEMS];
  bool indexTried[UI_COVER_SYSTEMS] = { false, false, false };
  int indexFailures = 0;
  for (size_t i = 0; i < items.size() && !g_cancel; i++) {
    const UiCoverItem &it = items[i];
    st.current = it.romName;
    setStatus(st);
    bool saved = false, looked = false;
    for (int k = 0; k < UI_COVER_SYSTEMS && it.systems[k] >= 0 && !saved && !g_cancel; k++) {
      int sys = it.systems[k];
      if (sys >= UI_COVER_SYSTEMS)
        continue;
      if (!indexTried[sys]) {
        indexTried[sys] = true;
        std::string html;
        std::string url = server + "/" + coversUrlEncode(kSystemFolder[sys]) + "/Named_Boxarts/";
        if (httpGet(url, html, err)) {
          index[sys] = coversParseIndex(html);
          COVERS_LOG("Covers: %s list: %d names", kSystemFolder[sys], (int)index[sys].size());
        } else {
          indexFailures++;
          st.error = err;
          COVERS_LOG("Covers: %s list failed (%s)", kSystemFolder[sys], err.c_str());
        }
      }
      if (index[sys].empty())
        continue;
      looked = true;
      int m = coversBestMatch(it.romName, index[sys]);
      if (m < 0)
        continue;
      std::string png;
      std::string url = server + "/" + coversUrlEncode(kSystemFolder[sys]) + "/Named_Boxarts/" +
                        coversUrlEncode(index[sys][m]) + ".png";
      if (!httpGet(url, png, err)) {
        COVERS_LOG("Covers: %s failed (%s)", index[sys][m].c_str(), err.c_str());
        continue;
      }
      if (png.size() < 8 || memcmp(png.data(), "\x89PNG", 4) != 0)
        continue;
      if (writeFile(it.coverPath, png)) {
        saved = true;
        COVERS_LOG("Covers: %s -> %s", it.romName.c_str(), index[sys][m].c_str());
      }
    }
    // Every list failed: no point going on (no internet, server down, HTTPS only...).
    if (!looked && indexFailures > 0) {
      bool anyIndex = false;
      for (int s = 0; s < UI_COVER_SYSTEMS; s++)
        anyIndex = anyIndex || !index[s].empty();
      bool allTried = true;
      for (int k = 0; k < UI_COVER_SYSTEMS && it.systems[k] >= 0; k++)
        allTried = allTried && indexTried[it.systems[k]];
      if (!anyIndex && allTried)
        break;
    }
    st.done++;
    if (saved) st.found++;
    else st.missing++;
    if (saved || looked)
      st.error.clear();
    setStatus(st);
  }
  if (g_cancel)
    st.error = "cancel";
  st.running = false;
  st.current.clear();
  setStatus(st);
  COVERS_LOG("Covers: done, %d saved, %d without cover (%s)", st.found, st.missing, st.error.c_str());
}

namespace {
#ifdef _XBOX
DWORD WINAPI coversThread(LPVOID)
{
  coversRun(g_items, g_server);
  return 0;
}
#else
void *coversThread(void *)
{
  coversRun(g_items, g_server);
  return 0;
}
#endif
}

bool coversStart(const std::vector<UiCoverItem> &items, const std::string &server)
{
  if (items.empty())
    return false;
  lock();
  bool busy = g_status.running;
  if (!busy) {
    g_status = UiCoverStatus();
    g_status.running = true;
    g_status.total = (int)items.size();
  }
  unlock();
  if (busy)
    return false;
  g_items = items;
  g_server = server;
  g_cancel = false;
#ifdef _XBOX
  HANDLE h = CreateThread(NULL, 256 * 1024, coversThread, NULL, CREATE_SUSPENDED, NULL);
  if (!h) {
    lock();
    g_status.running = false;
    g_status.error = "thread";
    unlock();
    return false;
  }
  XSetThreadProcessor(h, 4);   // away from the emulator and the menu
  SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
  ResumeThread(h);
  CloseHandle(h);
#else
  pthread_t t;
  if (pthread_create(&t, 0, coversThread, 0) != 0) {
    lock();
    g_status.running = false;
    unlock();
    return false;
  }
  pthread_detach(t);
#endif
  return true;
}

void coversStatus(UiCoverStatus &out)
{
  lock();
  out = g_status;
  unlock();
}

void coversCancel()
{
  g_cancel = true;
}
