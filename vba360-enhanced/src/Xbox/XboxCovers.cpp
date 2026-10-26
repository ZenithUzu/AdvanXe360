// Box art downloader (see XboxCovers.h).
#include "XboxCovers.h"
#include "fex/fex.h"
#include "XboxTls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <map>

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

bool resolveDns(const std::string &host, unsigned long &ip, std::string &err);

bool resolve(const std::string &host, unsigned long &ip, std::string &err)
{
  unsigned long a = inet_addr(host.c_str());
  if (a != (unsigned long)INADDR_NONE && a != 0) {
    ip = a;
    return true;
  }
  // One lookup per host and session: every cover used to ask the DNS again.
  static std::string lastHost;
  static unsigned long lastIp = 0;
  if (host == lastHost && lastIp) {
    ip = lastIp;
    return true;
  }
  bool found = resolveDns(host, ip, err);
  if (found) {
    lastHost = host;
    lastIp = ip;
  }
  return found;
}

bool resolveDns(const std::string &host, unsigned long &ip, std::string &err)
{
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
  int rcv = 256 * 1024;   // a bigger receive window: much faster downloads
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&rcv, sizeof(rcv));
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

// "http[s]://host[:port]/path" -> parts. False for anything else.
bool splitUrl(const std::string &url, std::string &host, int &port, std::string &path, bool &https)
{
  std::string l = lowerAscii(url.substr(0, 8));
  https = l == "https://";
  if (!https && l.substr(0, 7) != "http://")
    return false;
  std::string rest = url.substr(https ? 8 : 7);
  size_t slash = rest.find('/');
  std::string hp = slash == std::string::npos ? rest : rest.substr(0, slash);
  path = slash == std::string::npos ? "/" : rest.substr(slash);
  size_t colon = hp.find(':');
  port = https ? 443 : 80;
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

} // namespace

// GET (post empty) or POST a URL into body; http:// or https:// (TLS by
// BearSSL, see XboxTls.h). err: "" or a short reason ("dns", "connect",
// "tls 62", "http 404"...). Follows redirects.
bool httpRequestStatus(const std::string &url0, const std::string &post, std::string &body, int &status,
                       std::string &err, const std::string &userAgent)
{
  status = 0;
  std::string url = url0;
  for (int hop = 0; hop < 4; hop++) {
    std::string host, path;
    int port;
    bool https;
    if (!splitUrl(url, host, port, path, https)) {
      err = "url";
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
    XboxTls *tls = 0;
    if (https && !(tls = xboxTlsOpen((unsigned long long)s, host, err))) {
      closeSock(s);
      return false;
    }
    std::string req = (post.empty() ? "GET " : "POST ") + path + " HTTP/1.0\r\nHost: " + host +
                      "\r\nUser-Agent: " + (userAgent.empty() ? std::string("AdvanXe360") : userAgent) +
                      "\r\nConnection: close\r\n";
    if (!post.empty())
      req += "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: " + intStr((int)post.size()) + "\r\n";
    req += "\r\n" + post;
    bool sent = tls ? xboxTlsWrite(tls, req.c_str(), (int)req.size())
                    : send(s, req.c_str(), (int)req.size(), 0) == (int)req.size();
    if (!sent) {
      err = tls ? "tls " + intStr(xboxTlsError(tls)) : "send";
      xboxTlsClose(tls);
      closeSock(s);
      return false;
    }
    std::string data;
    std::vector<char> chunk(64 * 1024);
    char *buf = &chunk[0];
    for (;;) {
      if (g_cancel) {
        xboxTlsClose(tls);
        closeSock(s);
        err = "cancel";
        return false;
      }
      int n = tls ? xboxTlsRead(tls, buf, (int)chunk.size()) : recv(s, buf, (int)chunk.size(), 0);
      if (n <= 0)
        break;
      data.append(buf, n);
      if (data.size() > 48 * 1024 * 1024)   // the cheat database is the biggest file
        break;
    }
    int tlsErr = tls ? xboxTlsError(tls) : 0;
    xboxTlsClose(tls);
    closeSock(s);
    if (data.empty() && tlsErr) {
      err = "tls " + intStr(tlsErr);   // the handshake or the certificate check failed
      return false;
    }
    size_t he = data.find("\r\n\r\n");
    if (data.compare(0, 5, "HTTP/") != 0 || he == std::string::npos) {
      err = data.empty() ? "empty" : "reply";
      return false;
    }
    std::string head = data.substr(0, he + 2);
    size_t sp = head.find(' ');
    status = sp == std::string::npos ? 0 : atoi(head.c_str() + sp + 1);
    if (status >= 300 && status < 400) {
      std::string loc = headerValue(head, "location");
      if (loc.empty()) {
        err = "http " + intStr(status);
        return false;
      }
      if (loc[0] == '/')
        loc = (https ? "https://" : "http://") + host + (port != (https ? 443 : 80) ? ":" + intStr(port) : std::string()) + loc;
      url = loc;
      continue;
    }
    // Any other answer is the server's: its text matters too (an API saying
    // "wrong password" with a 401...).
    body = data.substr(he + 4);
    if (lowerAscii(headerValue(head, "transfer-encoding")).find("chunked") != std::string::npos)
      body = unchunk(body);
    if (status != 200)
      err = "http " + intStr(status);
    return true;
  }
  err = "redirects";
  return false;
}

bool httpRequest(const std::string &url, const std::string &post, std::string &body, std::string &err,
                 const std::string &userAgent)
{
  int status;
  if (!httpRequestStatus(url, post, body, status, err, userAgent))
    return false;
  if (status == 200)
    return true;
  body.clear();
  return false;
}

bool xboxNetStart(std::string &err) { return netStart(err); }

bool httpGet(const std::string &url, std::string &body, std::string &err)
{
  return httpRequest(url, "", body, err, "");
}

namespace {

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

void readLines(const std::string &path, std::vector<std::string> &out)
{
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return;
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    std::string l = line;
    while (!l.empty() && (l[l.size() - 1] == '\n' || l[l.size() - 1] == '\r'))
      l.erase(l.size() - 1);
    if (!l.empty())
      out.push_back(l);
  }
  fclose(f);
}

void writeLines(const std::string &path, const std::vector<std::string> &lines)
{
  std::string data;
  for (size_t i = 0; i < lines.size(); i++)
    data += lines[i] + "\n";
  writeFile(path, data);
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
        // The list of names is kept in covers\ (it's the slowest part, several
        // MB); delete the .txt to fetch it again.
        std::string cache = it.coverPath.substr(0, it.coverPath.find_last_of("\\/") + 1) +
                            "_lista-" + kSystemFolder[sys] + ".txt";
        readLines(cache, index[sys]);
        std::string html;
        std::string url = server + "/" + coversUrlEncode(kSystemFolder[sys]) + "/Named_Boxarts/";
        if (!index[sys].empty()) {
          COVERS_LOG("Covers: %s list from cache: %d names", kSystemFolder[sys], (int)index[sys].size());
        } else if (httpGet(url, html, err)) {
          index[sys] = coversParseIndex(html);
          writeLines(cache, index[sys]);
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

// "Pokemon - Fire Red Version (USA)" -> "pokemonfireredversion".
static std::string compactTitle(const std::string &name)
{
  std::string t, out;
  splitTitle(name, t, out);
  out.clear();
  for (size_t i = 0; i < t.size(); i++)
    if (isalnum((unsigned char)t[i]))
      out += (char)tolower((unsigned char)t[i]);
  return out;
}

bool cheatsFetch(const std::string &romName, const std::string &gameCode, const char *systemFolder,
                 const std::string &zipPath, const std::string &outPath, std::string &err)
{
  g_cancel = false;
  FILE *have = fopen(zipPath.c_str(), "rb");
  if (have) {
    fclose(have);
  } else {
    if (!netStart(err))
      return false;
    std::string zip;
    if (!httpGet("http://buildbot.libretro.com/assets/frontend/cheats.zip", zip, err))
      return false;
    if (zip.size() < 4 || zip.compare(0, 2, "PK") != 0) {
      err = "zip";
      return false;
    }
    if (!writeFile(zipPath, zip)) {
      err = "write";
      return false;
    }
  }
  // Names of that console's .cht files, and where each one is in the zip.
  fex_t *fe = 0;
  if (fex_open(&fe, zipPath.c_str()) || !fe) {
    err = "zip";
    return false;
  }
  std::string folder = std::string(systemFolder) + "/";
  std::vector<std::string> names, full;
  std::vector<fex_pos_t> where;
  fex_pos_t index = -1;   // _gamecodes.txt: GBA game code -> file
  while (!fex_done(fe)) {
    std::string n = fex_name(fe);
    size_t at = n.find(folder);
    if (at != std::string::npos && n.size() > 4 && lowerAscii(n.substr(n.size() - 4)) == ".cht") {
      std::string base = n.substr(at + folder.size());
      names.push_back(base.substr(0, base.size() - 4));
      full.push_back(n.substr(n.find(systemFolder)));
      where.push_back(fex_tell_arc(fe));
    } else if (n.find("_gamecodes.txt") != std::string::npos) {
      index = fex_tell_arc(fe);
    }
    if (fex_next(fe))
      break;
  }
  int m = -1;
  // 1. The GBA game code in the ROM's header (any language of the game).
  const void *data = 0;
  if (gameCode.size() == 4 && index >= 0 && !fex_seek_arc(fe, index) && !fex_data(fe, &data) && data) {
    std::string idx((const char *)data, fex_size(fe));
    size_t at = idx.find(gameCode + "=");
    if (at != std::string::npos && (at == 0 || idx[at - 1] == '\n')) {
      size_t eol = idx.find_first_of("\r\n", at);
      std::string file = idx.substr(at + 5, eol == std::string::npos ? std::string::npos : eol - at - 5);
      for (size_t i = 0; i < full.size() && m < 0; i++)
        if (full[i] == file)
          m = (int)i;
    }
  }
  // 2. The name; 3. the name without spaces or symbols ("Fire Red" = "FireRed").
  if (m < 0)
    m = coversBestMatch(romName, names);
  if (m < 0) {
    std::string want = compactTitle(romName);
    for (size_t i = 0; i < names.size() && m < 0 && !want.empty(); i++)
      if (compactTitle(names[i]) == want)
        m = (int)i;
  }
  bool ok = false;
  if (m < 0) {
    err = "nomatch";
  } else if (!fex_seek_arc(fe, where[m])) {
    data = 0;
    if (!fex_data(fe, &data) && data)
      ok = writeFile(outPath, std::string((const char *)data, fex_size(fe)));
    if (!ok)
      err = "zip";
  }
  fex_close(fe);
  return ok;
}

// --- Cheats straight from GitHub (HTTPS) ----------------------------------------
//
// The newest codes, one game at a time: GitHub's API lists the console's
// files (names only, a few KB), the matching one is downloaded. libretro's
// database by name; SuperFW's (GBA) by the game code in the ROM's header.

// The values of "key":"..." in a JSON text, in order (GitHub's listings).
std::vector<std::string> jsonStrings(const std::string &json, const char *key)
{
  std::vector<std::string> out;
  std::string pat = std::string("\"") + key + "\"";
  size_t p = 0;
  while ((p = json.find(pat, p)) != std::string::npos) {
    p += pat.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == ':'))
      p++;
    if (p >= json.size() || json[p] != '"')
      continue;
    std::string v;
    for (p++; p < json.size() && json[p] != '"'; p++) {
      char c = json[p];
      if (c == '\\' && p + 1 < json.size()) {
        char e = json[++p];
        if (e == 'n') v += '\n';
        else if (e == 't') v += '\t';
        else if (e == 'u' && p + 4 < json.size()) {
          unsigned long u = strtoul(json.substr(p + 1, 4).c_str(), 0, 16);
          v += u < 0x100 ? (char)u : '?';
          p += 4;
        } else v += e;   // \" \\ \/
      } else {
        v += c;
      }
    }
    out.push_back(v);
  }
  return out;
}

namespace {

std::string urlPath(const std::string &s)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
      o += (char)c;
    else {
      o += '%';
      o += hex[c >> 4];
      o += hex[c & 15];
    }
  }
  return o;
}

std::string quote(const std::string &s)
{
  std::string o;
  for (size_t i = 0; i < s.size(); i++)
    o += s[i] == '"' ? '\'' : s[i];
  return o;
}

// Names (without .cht) of libretro's cheat files for a console; kept for the
// session (GitHub allows 60 listings an hour).
bool libretroNames(const std::string &system, std::vector<std::string> &names, std::string &err)
{
  static std::map<std::string, std::vector<std::string> > cache;
  std::map<std::string, std::vector<std::string> >::iterator it = cache.find(system);
  if (it != cache.end()) {
    names = it->second;
    return true;
  }
  std::string body;
  if (!httpRequest("https://api.github.com/repos/libretro/libretro-database/contents/cht", "", body, err, ""))
    return false;
  // The folder's entry: its "name", then its "sha" (the tree to list).
  std::string key = "\"name\":\"" + system + "\"";
  size_t at = body.find(key);
  if (at == std::string::npos) {
    err = "nomatch";
    return false;
  }
  std::vector<std::string> sha = jsonStrings(body.substr(at, 600), "sha");
  if (sha.empty()) {
    err = "reply";
    return false;
  }
  if (!httpRequest("https://api.github.com/repos/libretro/libretro-database/git/trees/" + sha[0], "", body, err, ""))
    return false;
  std::vector<std::string> paths = jsonStrings(body, "path");
  names.clear();
  for (size_t i = 0; i < paths.size(); i++)
    if (paths[i].size() > 4 && lowerAscii(paths[i].substr(paths[i].size() - 4)) == ".cht")
      names.push_back(paths[i].substr(0, paths[i].size() - 4));
  cache[system] = names;
  return true;
}

} // namespace

// SuperFW's JSON ({"codes":[{"title":..,"code":"8200055C+0000 ..."}]}) as a
// RetroArch .cht.
std::string superfwToCht(const std::string &json)
{
  size_t c = json.find("\"codes\"");
  if (c == std::string::npos)
    return "";
  std::string rest = json.substr(c);
  std::vector<std::string> titles = jsonStrings(rest, "title"), codes = jsonStrings(rest, "code");
  size_t n = titles.size() < codes.size() ? titles.size() : codes.size();
  std::string out = "cheats = " + intStr((int)n) + "\n\n";
  for (size_t i = 0; i < n; i++) {
    std::string code;
    for (size_t k = 0; k < codes[i].size(); k++)
      code += codes[i][k] == ' ' ? '+' : codes[i][k];
    std::string idx = intStr((int)i);
    out += "cheat" + idx + "_desc = \"" + quote(titles[i]) + "\"\ncheat" + idx + "_code = \"" + code +
           "\"\ncheat" + idx + "_enable = false\n\n";
  }
  return n ? out : "";
}

// "A8B-A9B - Medabots.json" is for game codes starting A8B or A9B; "ADIE - ..." for ADIE only.
bool superfwFileFor(const std::string &file, const std::string &code)
{
  size_t dash = file.find(" - ");
  if (dash == std::string::npos || code.size() < 3)
    return false;
  std::string prefix = file.substr(0, dash);
  size_t p = 0;
  while (p <= prefix.size()) {
    size_t e = prefix.find('-', p);
    std::string tok = prefix.substr(p, e == std::string::npos ? std::string::npos : e - p);
    if (tok.size() >= 3 && tok.size() <= code.size() && code.compare(0, tok.size(), tok) == 0)
      return true;
    if (e == std::string::npos)
      break;
    p = e + 1;
  }
  return false;
}

bool cheatsFetchOnline(const std::string &romName, const std::string &gameCode, const char *systemFolder,
                       std::vector<std::string> &chtTexts, std::string &err)
{
  chtTexts.clear();
  g_cancel = false;
  if (!netStart(err))
    return false;
  bool reached = false;
  // libretro: by name.
  std::vector<std::string> names;
  std::string e1;
  if (libretroNames(systemFolder, names, e1)) {
    reached = true;
    int m = coversBestMatch(romName, names);
    if (m < 0) {
      std::string want = compactTitle(romName);
      for (size_t i = 0; i < names.size() && m < 0 && !want.empty(); i++)
        if (compactTitle(names[i]) == want)
          m = (int)i;
    }
    std::string body;
    if (m >= 0 && httpRequest("https://raw.githubusercontent.com/libretro/libretro-database/master/cht/" +
                                  urlPath(std::string(systemFolder) + "/" + names[m] + ".cht"), "", body, e1, ""))
      chtTexts.push_back(body);
  }
  // SuperFW (GBA): by the game code.
  if (gameCode.size() == 4) {
    static std::vector<std::string> files;
    std::string body, e2;
    if (files.empty() &&
        httpRequest("https://api.github.com/repos/davidgfnet/superfw-cheats/contents/cheats", "", body, e2, ""))
      files = jsonStrings(body, "name");
    reached = reached || !files.empty();
    for (size_t i = 0; i < files.size(); i++)
      if (superfwFileFor(files[i], gameCode) &&
          httpRequest("https://raw.githubusercontent.com/davidgfnet/superfw-cheats/master/cheats/" + urlPath(files[i]),
                      "", body, e2, "")) {
        std::string cht = superfwToCht(body);
        if (!cht.empty())
          chtTexts.push_back(cht);
        break;
      }
  }
  if (!chtTexts.empty())
    return true;
  err = reached ? "nomatch" : (e1.empty() ? "net" : e1);
  return false;
}
