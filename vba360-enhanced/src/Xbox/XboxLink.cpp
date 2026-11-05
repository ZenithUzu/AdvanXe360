// Link cable between two consoles on the same network.
//
// A TCP connection carries fixed 12-byte messages. It is set up on a thread so
// the game never waits for it; once connected everything runs on the emulator
// thread with a non-blocking socket.
//
// Game Boy / Color: the side whose clock starts a transfer sends its byte and
// waits for the answer; the other side answers when its game is ready (SC bit 7
// set, external clock).
//
// GBA: the host is the parent. A transfer waits for the child's answer, and both
// then finish it after the same number of cycles. An emulator runs a frame in a
// burst and then idles, so the child could be idle just when the parent needs
// it: the host hands out cycles (MSG_GRANT) and the child stops at the limit, so
// the two run together. If they stop coming (the host's menu is open) the child
// carries on alone.
//
// With the option off the GBA core behaves exactly as it did without any of this.
#include "XboxLink.h"

#ifdef _XBOX
#include <xtl.h>
#include <winsockx.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>

#include "XboxCovers.h"   // xboxNetStart

// --- GBA core ------------------------------------------------------------------------
extern unsigned char *ioMem;
extern unsigned short IF;

bool gba_link_enabled = false;   // GBA.cpp: the serial registers are ours

namespace {

const unsigned short LINK_PORT = 0x4158;   // 16728
const unsigned char PROTOCOL = 1;
const int MSG_SIZE = 12;   // type, flag, seq (2), value (4), time (4)

enum { MSG_HELLO = 1, MSG_HEARTBEAT, MSG_GRANT, MSG_XFER, MSG_REPLY, MSG_GB_SEND, MSG_GB_REPLY, MSG_GB_CANCEL };
enum { SYSTEM_GBA = 1, SYSTEM_GB = 2 };
enum { STATE_OFF, STATE_CONNECTING, STATE_HANDSHAKE, STATE_ONLINE };

enum {
  REG_SIODATA32_L = 0x120, REG_SIODATA32_H = 0x122, REG_SIOMULTI2 = 0x124, REG_SIOMULTI3 = 0x126,
  REG_SIOCNT = 0x128, REG_SIODATA8 = 0x12A, REG_RCNT = 0x134, REG_IF = 0x202
};
enum { SIO_NORMAL8, SIO_NORMAL32, SIO_MULTI, SIO_OTHER };

const int GBA_FRAME_TICKS = 280896;
const int GRANT_TICKS = 16384;      // the child is never further ahead than this
const int POLL_TICKS = 2048;        // how often a running GBA looks at the socket
const DWORD STALL_MS = 250;         // nothing heard for this long: its menu is open
const DWORD GBA_REPLY_MS = 100;     // a transfer gives up after this
const DWORD GRANT_WAIT_MS = 100;    // the child stops waiting and runs alone
const DWORD GB_REPLY_MS = 40;       // two frames: the other game may be busy
const DWORD GB_SHORT_REPLY_MS = 2;  // the other game isn't listening: don't crawl
const DWORD GB_SHORT_FOR_MS = 500;
const DWORD HANDSHAKE_MS = 5000;

int g_state = STATE_OFF;
int g_mode = LINK_MODE_OFF;
std::string g_host;
bool g_isGba = false;
int g_event = LINK_EVENT_NONE;
bool g_badIp = false;
volatile bool g_noNet = false;

SOCKET g_sock = INVALID_SOCKET;
unsigned char g_rx[MSG_SIZE * 32];
int g_rxLen = 0;
DWORD g_lastRx = 0, g_handshakeStart = 0;

volatile LONG g_threadGen = 0;
volatile LONG g_readySock = (LONG)INVALID_SOCKET;

unsigned short g_seq = 0;
// The transfer this side is waiting on.
bool g_waiting = false, g_gotReply = false;
unsigned short g_waitSeq = 0;
int g_waitMode = 0;
unsigned g_replyValue = 0;

// GBA
bool g_transfer = false;
int g_xMode = SIO_OTHER, g_transferLeft = 0, g_pollLeft = 0, g_heartbeatLeft = 0;
unsigned g_xIn = 0, g_xOut = 0;
unsigned g_clock = 0, g_nextGrant = 0, g_limit = 0;
bool g_limitValid = false;
bool g_resync = true;   // the next grant also sets the child's clock

// GBA: a transfer the other side started at a moment this side hasn't reached yet.
bool g_deferred = false;
int g_dMode = 0;
unsigned short g_dSeq = 0;
unsigned g_dValue = 0, g_dTime = 0;

// Game Boy: the byte the other side is sending (the newest one).
bool g_gbPending = false;
unsigned char g_gbValue = 0;
unsigned short g_gbSeq = 0;
DWORD g_gbShortUntil = 0;

bool isClient() { return g_mode == LINK_MODE_JOIN; }
bool online() { return g_state == STATE_ONLINE; }
bool stalled() { return GetTickCount() - g_lastRx > STALL_MS; }

unsigned short rd16(int a) { return (unsigned short)(ioMem[a] | (ioMem[a + 1] << 8)); }
void wr16(int a, unsigned short v)
{
  ioMem[a] = (unsigned char)v;
  ioMem[a + 1] = (unsigned char)(v >> 8);
}

// --- Connecting (on its own thread) --------------------------------------------------

struct ConnectJob
{
  LONG gen;
  int mode;
  unsigned long ip;
  DWORD delayMs;
};

void closeSock(SOCKET s)
{
  if (s != INVALID_SOCKET) {
#ifdef _XBOX
    shutdown(s, SD_BOTH);
#endif
    closesocket(s);
  }
}

bool setNonBlocking(SOCKET s)
{
  u_long on = 1;
  return ioctlsocket(s, FIONBIO, &on) == 0;
}

// 0 nothing yet, >0 ready, <0 failed
int waitSock(SOCKET s, bool forWrite, int ms)
{
  fd_set main, err;
  FD_ZERO(&main);
  FD_ZERO(&err);
  FD_SET(s, &main);
  FD_SET(s, &err);
  timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  int r = select((int)s + 1, forWrite ? 0 : &main, forWrite ? &main : 0, &err, &tv);
  if (r < 0 || (r > 0 && FD_ISSET(s, &err)))
    return -1;
  return r;
}

void sleepUnlessStale(LONG gen, DWORD ms)
{
  for (DWORD t = 0; t < ms && gen == g_threadGen; t += 50)
    Sleep(50);
}

SOCKET hostAccept(LONG gen)
{
  while (gen == g_threadGen) {
    SOCKET l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (l == INVALID_SOCKET) {
      sleepUnlessStale(gen, 1000);
      continue;
    }
    BOOL yes = TRUE;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(LINK_PORT);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(l, (sockaddr *)&a, sizeof(a)) != 0 || listen(l, 1) != 0 || !setNonBlocking(l)) {
      closeSock(l);
      sleepUnlessStale(gen, 1000);
      continue;
    }
    while (gen == g_threadGen) {
      int r = waitSock(l, false, 250);
      if (r < 0)
        break;
      if (r == 0)
        continue;
      SOCKET s = accept(l, 0, 0);
      if (s != INVALID_SOCKET) {
        closeSock(l);
        return s;
      }
    }
    closeSock(l);
  }
  return INVALID_SOCKET;
}

SOCKET joinConnect(LONG gen, unsigned long ip)
{
  while (gen == g_threadGen) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s != INVALID_SOCKET && setNonBlocking(s)) {
      sockaddr_in a;
      memset(&a, 0, sizeof(a));
      a.sin_family = AF_INET;
      a.sin_port = htons(LINK_PORT);
      a.sin_addr.s_addr = ip;
      int r = connect(s, (sockaddr *)&a, sizeof(a));
      if (r != 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        r = -1;
        for (int i = 0; i < 8 && gen == g_threadGen; i++) {
          int w = waitSock(s, true, 250);
          if (w != 0) {
            r = w > 0 ? 0 : -1;
            break;
          }
        }
      }
      if (r == 0)
        return s;
    }
    closeSock(s);
    sleepUnlessStale(gen, 1000);
  }
  return INVALID_SOCKET;
}

DWORD WINAPI connectThread(void *param)
{
  ConnectJob job = *(ConnectJob *)param;
  delete (ConnectJob *)param;
  sleepUnlessStale(job.gen, job.delayMs);

  std::string err;
  while (job.gen == g_threadGen && !xboxNetStart(err)) {
    g_noNet = true;
    sleepUnlessStale(job.gen, 1000);
  }
  g_noNet = false;

  SOCKET s = job.mode == LINK_MODE_HOST ? hostAccept(job.gen) : joinConnect(job.gen, job.ip);
  if (s == INVALID_SOCKET)
    return 0;
  BOOL yes = TRUE;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
  setNonBlocking(s);
  if (job.gen == g_threadGen)
    InterlockedExchange(&g_readySock, (LONG)s);
  else
    closeSock(s);
  return 0;
}

void startThread(DWORD delayMs)
{
  InterlockedIncrement(&g_threadGen);   // an older thread sees this and gives up
  g_state = STATE_CONNECTING;
  g_badIp = false;
  unsigned long ip = 0;
  if (g_mode == LINK_MODE_JOIN) {
    ip = inet_addr(g_host.c_str());
    if (ip == (unsigned long)INADDR_NONE || ip == 0) {
      g_badIp = true;
      return;
    }
  }
  ConnectJob *job = new ConnectJob;
  job->gen = g_threadGen;
  job->mode = g_mode;
  job->ip = ip;
  job->delayMs = delayMs;
  HANDLE h = CreateThread(0, 64 * 1024, connectThread, job, 0, 0);
  if (h)
    CloseHandle(h);
  else
    delete job;
}

// --- Messages ------------------------------------------------------------------------

void dropConnection(int event, DWORD retryDelayMs);
void dispatch(const unsigned char *m);

void sendMsg(int type, int flag, unsigned short seq, unsigned value, unsigned time = 0)
{
  if (g_sock == INVALID_SOCKET)
    return;
  unsigned char m[MSG_SIZE] = { (unsigned char)type, (unsigned char)flag, (unsigned char)(seq >> 8), (unsigned char)seq,
                                (unsigned char)(value >> 24), (unsigned char)(value >> 16),
                                (unsigned char)(value >> 8), (unsigned char)value,
                                (unsigned char)(time >> 24), (unsigned char)(time >> 16),
                                (unsigned char)(time >> 8), (unsigned char)time };
  int sent = 0;
  DWORD start = GetTickCount();
  while (sent < MSG_SIZE) {
    int n = send(g_sock, (const char *)m + sent, MSG_SIZE - sent, 0);
    if (n > 0) {
      sent += n;
    } else if (WSAGetLastError() != WSAEWOULDBLOCK) {
      dropConnection(LINK_EVENT_LOST, 0);
      return;
    } else if (sent == 0 && type != MSG_XFER && type != MSG_REPLY && type != MSG_GB_SEND && type != MSG_GB_REPLY) {
      return;   // full: only something that gets sent again anyway
    } else if (GetTickCount() - start > 50) {
      dropConnection(LINK_EVENT_LOST, 0);   // half a message: the stream can't recover
      return;
    } else {
      waitSock(g_sock, true, 1);
    }
  }
}

void pump()
{
  while (g_sock != INVALID_SOCKET) {
    int n = recv(g_sock, (char *)g_rx + g_rxLen, (int)sizeof(g_rx) - g_rxLen, 0);
    if (n == 0) {
      dropConnection(LINK_EVENT_LOST, 0);
      return;
    }
    if (n < 0) {
      if (WSAGetLastError() != WSAEWOULDBLOCK)
        dropConnection(LINK_EVENT_LOST, 0);
      return;
    }
    g_lastRx = GetTickCount();
    g_rxLen += n;
    int at = 0;
    while (g_rxLen - at >= MSG_SIZE && g_sock != INVALID_SOCKET) {
      unsigned char m[MSG_SIZE];
      memcpy(m, g_rx + at, MSG_SIZE);
      at += MSG_SIZE;
      dispatch(m);
    }
    if (g_sock == INVALID_SOCKET)
      return;
    g_rxLen -= at;
    if (g_rxLen > 0)
      memmove(g_rx, g_rx + at, g_rxLen);
  }
}

// Waits for the answer to the message just sent. False: none came.
bool waitReply(DWORD ms)
{
  DWORD start = GetTickCount();
  for (;;) {
    pump();
    if (g_gotReply || !online())
      break;
    if (GetTickCount() - start >= ms)
      break;
    waitSock(g_sock, false, 1);
  }
  g_waiting = false;
  return g_gotReply;
}

// --- GBA serial port -----------------------------------------------------------------

int sioMode(unsigned short cnt, unsigned short rcnt)
{
  if (rcnt & 0x8000)
    return SIO_OTHER;
  switch (cnt & 0x3000) {
  case 0x0000: return SIO_NORMAL8;
  case 0x1000: return SIO_NORMAL32;
  case 0x2000: return SIO_MULTI;
  default: return SIO_OTHER;
  }
}

int transferTicks(int mode, unsigned short cnt)
{
  static const int multi[4] = { 72527, 18132, 12088, 6044 };   // 9600...115200 bauds, two players
  if (mode == SIO_MULTI)
    return multi[cnt & 3];
  int bit = (cnt & 2) ? 8 : 64;   // 2 MHz or 256 KHz
  return bit * (mode == SIO_NORMAL32 ? 32 : 8);
}

unsigned idleValue(int mode)
{
  return mode == SIO_MULTI ? 0xFFFFu : mode == SIO_NORMAL8 ? 0xFFu : 0xFFFFFFFFu;
}

unsigned dataOut(int mode)
{
  if (mode == SIO_MULTI)
    return rd16(REG_SIODATA8);
  if (mode == SIO_NORMAL8)
    return rd16(REG_SIODATA8) & 0xFF;
  return rd16(REG_SIODATA32_L) | ((unsigned)rd16(REG_SIODATA32_H) << 16);
}

// SIOCNT's read-only bits in Multiplayer: SI (we are a child) and SD (cable there).
unsigned short multiLineBits()
{
  return (unsigned short)((isClient() ? 4 : 0) | (online() ? 8 : 0));
}

void finishTransfer()
{
  g_transfer = false;
  unsigned short cnt = rd16(REG_SIOCNT);
  if (g_xMode == SIO_MULTI) {
    wr16(REG_SIODATA32_L, (unsigned short)(isClient() ? g_xIn : g_xOut));
    wr16(REG_SIODATA32_H, (unsigned short)(isClient() ? g_xOut : g_xIn));
    wr16(REG_SIOMULTI2, 0xFFFF);
    wr16(REG_SIOMULTI3, 0xFFFF);
    cnt = (unsigned short)((cnt & ~0x30) | (isClient() ? 0x10 : 0));
  } else if (g_xMode == SIO_NORMAL8) {
    wr16(REG_SIODATA8, (unsigned short)(g_xIn & 0xFF));
  } else {
    wr16(REG_SIODATA32_L, (unsigned short)g_xIn);
    wr16(REG_SIODATA32_H, (unsigned short)(g_xIn >> 16));
  }
  cnt &= ~0x80;
  wr16(REG_SIOCNT, cnt);
  if (cnt & 0x4000) {
    IF |= 0x80;
    wr16(REG_IF, IF);
  }
}

void beginTransfer(int mode, unsigned short cnt, unsigned out, unsigned in)
{
  g_xMode = mode;
  g_xOut = out;
  g_xIn = in;
  g_transfer = true;
  g_transferLeft = transferTicks(mode, cnt);
}

// This side's clock starts a transfer.
void startAsMaster(int mode, unsigned short cnt)
{
  unsigned out = dataOut(mode), in = idleValue(mode);
  if (online() && !stalled()) {
    g_waiting = true;
    g_gotReply = false;
    g_waitSeq = ++g_seq;
    g_waitMode = mode;
    sendMsg(MSG_XFER, mode, g_waitSeq, out, g_clock);
    if (waitReply(GBA_REPLY_MS))
      in = g_replyValue;
  }
  beginTransfer(mode, cnt, out, in);
}

// The other side's clock started one, and this side has reached that moment.
void runXfer(int mode, unsigned short seq, unsigned value)
{
  unsigned short cnt = rd16(REG_SIOCNT);
  int mine = sioMode(cnt, rd16(REG_RCNT));
  if (mine != mode) {
    sendMsg(MSG_REPLY, 0, seq, idleValue(mode));
    return;
  }
  sendMsg(MSG_REPLY, 0, seq, dataOut(mode));
  if (mode == SIO_MULTI) {
    if (g_transfer)
      finishTransfer();   // the next one is here: the last one is over
    cnt = rd16(REG_SIOCNT);
    wr16(REG_SIOCNT, (unsigned short)(cnt | 0x80));
    beginTransfer(mode, cnt, dataOut(mode), value);
  } else if ((cnt & 0x81) == 0x80 && !g_transfer) {   // waiting, on the external clock
    beginTransfer(mode, cnt, dataOut(mode), value);
  }
}

void onXfer(int mode, unsigned short seq, unsigned value, unsigned time)
{
  if (g_waiting && mode == g_waitMode) {
    g_replyValue = value;   // both started at once (Normal mode): each one's data is the answer
    g_gotReply = true;
    return;
  }
  if (g_deferred)           // the one before had to wait: it can't any more
    runXfer(g_dMode, g_dSeq, g_dValue);
  g_deferred = false;
  // The host is stopped at that moment, waiting: the child may run up to it
  // even if the last grant said less (or they would wait for each other).
  if (isClient() && g_limitValid && (int)(time - g_limit) > 0)
    g_limit = time;
  if ((int)(g_clock - time) < 0 && g_limitValid) {
    // This side is behind: answer when its game gets there, as on a real cable
    // (its interrupt has to run before the next transfer). The other side waits.
    g_deferred = true;
    g_dMode = mode;
    g_dSeq = seq;
    g_dValue = value;
    g_dTime = time;
    return;
  }
  runXfer(mode, seq, value);
}

void refreshLines()
{
  if (!g_isGba || !ioMem)
    return;
  unsigned short cnt = rd16(REG_SIOCNT);
  if (sioMode(cnt, rd16(REG_RCNT)) == SIO_MULTI)
    wr16(REG_SIOCNT, (unsigned short)((cnt & ~0x0C) | multiLineBits()));
}

// Not the receive buffer: this runs while pump() is reading it (MSG_HELLO).
void resetSession()
{
  g_waiting = g_gotReply = false;
  g_limitValid = false;
  g_resync = true;
  g_deferred = false;
  g_gbPending = false;
  g_gbShortUntil = 0;
  g_pollLeft = g_heartbeatLeft = 0;
  g_nextGrant = g_clock;
}

void dropConnection(int event, DWORD retryDelayMs)
{
  closeSock(g_sock);
  g_sock = INVALID_SOCKET;
  g_rxLen = 0;
  if (g_state == STATE_ONLINE || event == LINK_EVENT_MISMATCH)
    g_event = event;
  resetSession();
  startThread(retryDelayMs);
  refreshLines();
}

void dispatch(const unsigned char *m)
{
  int type = m[0], flag = m[1];
  unsigned short seq = (unsigned short)((m[2] << 8) | m[3]);
  unsigned value = ((unsigned)m[4] << 24) | ((unsigned)m[5] << 16) | ((unsigned)m[6] << 8) | m[7];
  unsigned time = ((unsigned)m[8] << 24) | ((unsigned)m[9] << 16) | ((unsigned)m[10] << 8) | m[11];
  switch (type) {
  case MSG_HELLO:
    if (value != (unsigned)(g_isGba ? SYSTEM_GBA : SYSTEM_GB) || flag != PROTOCOL) {
      dropConnection(LINK_EVENT_MISMATCH, 3000);
      return;
    }
    if (g_state == STATE_HANDSHAKE) {
      g_state = STATE_ONLINE;
      g_event = LINK_EVENT_CONNECTED;
      resetSession();
      refreshLines();
    }
    break;
  case MSG_GRANT:
    g_limit = value;
    g_limitValid = true;
    if (g_resync) {   // first one, or back from a pause: start from the host's time
      g_resync = false;
      g_clock = value - GRANT_TICKS;
    }
    break;
  case MSG_XFER:
    onXfer(flag, seq, value, time);
    break;
  case MSG_REPLY:
  case MSG_GB_REPLY:
    if (g_waiting && seq == g_waitSeq) {
      g_replyValue = value;
      g_gotReply = true;
    }
    break;
  case MSG_GB_SEND:
    if (g_waiting) {           // both started at once: each one's byte is the answer
      g_replyValue = value;
      g_gotReply = true;
    } else {
      g_gbPending = true;      // an older one nobody took is lost, as on a real cable
      g_gbValue = (unsigned char)value;
      g_gbSeq = seq;
    }
    break;
  case MSG_GB_CANCEL:
    if (g_gbPending && g_gbSeq == seq)
      g_gbPending = false;
    break;
  default:
    break;   // heartbeat: receiving it was the point
  }
}

} // namespace

// --- Called by the GBA core ----------------------------------------------------------

void StartLink(unsigned short value)
{
  if (!gba_link_enabled)
    return;
  int mode = sioMode(value, rd16(REG_RCNT));
  if (mode == SIO_MULTI) {
    unsigned short reg = (unsigned short)((value & 0x7003) | (rd16(REG_SIOCNT) & 0x30) | multiLineBits() | (g_transfer ? 0x80 : 0));
    wr16(REG_SIOCNT, reg);
    if ((value & 0x80) && !isClient() && !g_transfer) {
      wr16(REG_SIOCNT, (unsigned short)(reg | 0x80));
      startAsMaster(mode, reg);
    }
  } else if (mode == SIO_OTHER) {
    wr16(REG_SIOCNT, value);
  } else {
    unsigned short reg = (unsigned short)((value & ~4) | (g_transfer ? 0x80 : 0));
    wr16(REG_SIOCNT, reg);
    if ((value & 0x81) == 0x81 && !g_transfer)
      startAsMaster(mode, reg);
  }
}

void StartGPLink(unsigned short value)
{
  if (gba_link_enabled)
    wr16(REG_RCNT, value);
}

void LinkUpdate(int ticks)
{
  if (!gba_link_enabled)
    return;
  g_clock += ticks;
  if (g_transfer) {
    g_transferLeft -= ticks;
    if (g_transferLeft <= 0)
      finishTransfer();
  }
  if (g_deferred && ((int)(g_clock - g_dTime) >= 0 || !online() || !g_limitValid)) {
    g_deferred = false;
    if (online())
      runXfer(g_dMode, g_dSeq, g_dValue);
  }
  if (g_state < STATE_HANDSHAKE)
    return;
  g_pollLeft -= ticks;
  if (g_pollLeft <= 0) {
    g_pollLeft = POLL_TICKS;
    pump();
  }
  if (!online())
    return;
  g_heartbeatLeft -= ticks;
  if (g_heartbeatLeft <= 0) {
    g_heartbeatLeft = GBA_FRAME_TICKS / 2;
    sendMsg(MSG_HEARTBEAT, 0, 0, 0);
  }
  if (!isClient()) {
    if ((int)(g_clock - g_nextGrant) >= 0) {
      g_nextGrant = g_clock + GRANT_TICKS;
      if (!stalled())
        sendMsg(MSG_GRANT, 0, 0, g_nextGrant);
    }
  } else if (g_limitValid && (int)(g_clock - g_limit) >= 0) {
    DWORD start = GetTickCount();
    while (online() && g_limitValid && (int)(g_clock - g_limit) >= 0) {
      if (GetTickCount() - start >= GRANT_WAIT_MS) {
        g_limitValid = false;   // the host stopped: run alone until it is back
        g_resync = true;
        break;
      }
      waitSock(g_sock, false, 1);
      pump();
    }
  }
}

// --- Game Boy / Color ----------------------------------------------------------------

unsigned char xboxLinkGbSend(unsigned char data)
{
  if (!online())
    return 0xFF;
  pump();
  if (!online() || stalled())
    return 0xFF;
  g_waiting = true;
  g_gotReply = false;
  g_waitSeq = ++g_seq;
  g_waitMode = -1;
  // What arrived before starting also started before the other side heard us.
  bool crossed = g_gbPending;
  g_gbPending = false;
  sendMsg(MSG_GB_SEND, 0, g_waitSeq, data);
  if (crossed) {
    g_waiting = false;
    return g_gbValue;
  }
  DWORD now = GetTickCount();
  bool shortWait = g_gbShortUntil != 0 && (int)(g_gbShortUntil - now) > 0;
  if (waitReply(shortWait ? GB_SHORT_REPLY_MS : GB_REPLY_MS)) {
    g_gbShortUntil = 0;
    return (unsigned char)g_replyValue;
  }
  if (online()) {
    sendMsg(MSG_GB_CANCEL, 0, g_waitSeq, 0);
    g_gbShortUntil = GetTickCount() + GB_SHORT_FOR_MS;
  }
  return 0xFF;
}

bool xboxLinkGbPoll(unsigned char out, unsigned char *in)
{
  if (g_state < STATE_HANDSHAKE)
    return false;
  pump();
  if (!g_gbPending || !online())
    return false;
  g_gbPending = false;
  sendMsg(MSG_GB_REPLY, 0, g_gbSeq, out);
  *in = g_gbValue;
  return true;
}

// --- Session -------------------------------------------------------------------------

void xboxLinkStop()
{
  InterlockedIncrement(&g_threadGen);
  closeSock(g_sock);
  g_sock = INVALID_SOCKET;
  g_rxLen = 0;
  closeSock((SOCKET)InterlockedExchange(&g_readySock, (LONG)INVALID_SOCKET));
  g_state = STATE_OFF;
  g_event = LINK_EVENT_NONE;
  g_badIp = false;
  g_transfer = false;
  gba_link_enabled = false;
  resetSession();
}

void xboxLinkStart(int mode, const std::string &host, bool isGba)
{
  xboxLinkStop();
  g_mode = mode;
  g_host = host;
  g_isGba = isGba;
  if (mode == LINK_MODE_OFF)
    return;
  gba_link_enabled = isGba;
  startThread(0);
}

void xboxLinkSettings(int mode, const std::string &host)
{
  if (mode == g_mode && (mode != LINK_MODE_JOIN || host == g_host))
    return;
  bool running = g_state != STATE_OFF;
  g_mode = mode;
  g_host = host;
  // Nothing is running yet (or the option was off, with a game open: it can't
  // know whether that's a GBA game; it starts with the next game).
  if (running) {
    bool isGba = g_isGba;
    xboxLinkStart(mode, host, isGba);
    refreshLines();
  }
}

int xboxLinkFrame()
{
  if (g_state == STATE_OFF)
    return LINK_EVENT_NONE;
  if (g_state == STATE_CONNECTING) {
    SOCKET s = (SOCKET)InterlockedExchange(&g_readySock, (LONG)INVALID_SOCKET);
    if (s != INVALID_SOCKET) {
      g_sock = s;
      g_rxLen = 0;
      g_state = STATE_HANDSHAKE;
      g_handshakeStart = g_lastRx = GetTickCount();
      resetSession();
      sendMsg(MSG_HELLO, PROTOCOL, 0, g_isGba ? SYSTEM_GBA : SYSTEM_GB);
    }
  }
  if (g_state >= STATE_HANDSHAKE) {
    pump();
    if (g_state == STATE_HANDSHAKE && GetTickCount() - g_handshakeStart > HANDSHAKE_MS)
      dropConnection(LINK_EVENT_NONE, 0);
    else if (online() && !g_isGba)
      sendMsg(MSG_HEARTBEAT, 0, 0, 0);
  }
  int e = g_event;
  g_event = LINK_EVENT_NONE;
  return e;
}

bool xboxLinkConnected() { return online(); }
bool xboxLinkIsClient() { return online() && isClient(); }

int xboxLinkStatus()
{
  if (g_mode == LINK_MODE_OFF)
    return LINK_STATUS_OFF;
  if (g_state == STATE_OFF)
    return LINK_STATUS_NO_GAME;
  if (g_badIp)
    return LINK_STATUS_BAD_IP;
  if (online())
    return LINK_STATUS_ONLINE;
  return g_noNet ? LINK_STATUS_NO_NET : LINK_STATUS_WAITING;
}

namespace {
volatile bool g_localNetReady = false;
LONG g_localNetAsked = 0;

DWORD WINAPI localNetThread(void *)
{
  std::string err;
  while (!xboxNetStart(err))
    Sleep(2000);
  g_localNetReady = true;
  return 0;
}
}

std::string xboxLinkLocalIp()
{
  if (InterlockedExchange(&g_localNetAsked, 1) == 0) {
    HANDLE h = CreateThread(0, 64 * 1024, localNetThread, 0, 0, 0);
    if (h)
      CloseHandle(h);
  }
  if (!g_localNetReady)
    return "";
#ifdef _XBOX
  XNADDR addr;
  DWORD st = XNetGetTitleXnAddr(&addr);
  if (st == XNET_GET_XNADDR_PENDING || st == XNET_GET_XNADDR_NONE)
    return "";
  const unsigned char *b = (const unsigned char *)&addr.ina.s_addr;   // network order
  char ip[16];
  sprintf(ip, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  return ip;
#else
  return "127.0.0.1";
#endif
}
