// HTTPS: see XboxTls.h. BearSSL (Thomas Pornin, MIT license) does the TLS;
// this file gives it the socket, random numbers, the date and the root
// certificates.
#include "XboxTls.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _XBOX
#include <xtl.h>
#include <winsockx.h>
#else
#include <sys/socket.h>
#include <stdio.h>
#endif

#include "bearssl.h"
#include "XboxTlsRoots.inc"

struct XboxTls
{
  br_ssl_client_context sc;
  br_x509_minimal_context xc;
  br_sslio_context io;
  unsigned char buf[BR_SSL_BUFSIZE_BIDI];
  unsigned long long sock;
};

namespace {

int sockRead(void *ctx, unsigned char *buf, size_t len)
{
  unsigned long long s = *(unsigned long long *)ctx;
  int n = recv((int)s, (char *)buf, (int)len, 0);
  return n > 0 ? n : -1;
}

int sockWrite(void *ctx, const unsigned char *buf, size_t len)
{
  unsigned long long s = *(unsigned long long *)ctx;
  int n = send((int)s, (const char *)buf, (int)len, 0);
  return n > 0 ? n : -1;
}

// Days from 0000-01-01 to y-m-d (proleptic Gregorian), as BearSSL counts them.
unsigned long daysFromYear0(int y, int m, int d)
{
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (unsigned long)(era * 146097 + doe + 60);   // + days from 0000-01-01 to 0000-03-01
}

// Today for the certificate check. A console whose clock was never set says
// 2005 or so, and every certificate would look "not valid yet": never earlier
// than the day this was written.
void certTime(unsigned long &days, unsigned long &secs)
{
  int y, mo, d, h, mi, s;
#ifdef _XBOX
  SYSTEMTIME st;
  GetSystemTime(&st);
  y = st.wYear; mo = st.wMonth; d = st.wDay; h = st.wHour; mi = st.wMinute; s = st.wSecond;
#else
  time_t now = time(0);
  struct tm *t = gmtime(&now);
  y = t->tm_year + 1900; mo = t->tm_mon + 1; d = t->tm_mday; h = t->tm_hour; mi = t->tm_min; s = t->tm_sec;
#endif
  days = daysFromYear0(y, mo, d);
  secs = (unsigned long)(h * 3600 + mi * 60 + s);
  unsigned long floor = daysFromYear0(2026, 10, 22);
  if (days < floor) {
    days = floor;
    secs = 0;
  }
}

void randomSeed(unsigned char *seed, int len)
{
#ifdef _XBOX
  XNetRandom(seed, (UINT)len);   // the console's hardware random numbers
  LARGE_INTEGER pc;
  QueryPerformanceCounter(&pc);
  for (int i = 0; i < (int)sizeof(pc) && i < len; i++)
    seed[i] ^= ((unsigned char *)&pc)[i];
#else
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f || fread(seed, 1, len, f) != (size_t)len)
    for (int i = 0; i < len; i++)
      seed[i] = (unsigned char)rand();
  if (f)
    fclose(f);
#endif
}

} // namespace

XboxTls *xboxTlsOpen(unsigned long long sock, const std::string &host, std::string &err)
{
  XboxTls *t = new XboxTls;
  t->sock = sock;
  br_ssl_client_init_full(&t->sc, &t->xc, TAs, TAs_NUM);
  unsigned long days, secs;
  certTime(days, secs);
  br_x509_minimal_set_time(&t->xc, days, secs);
  br_ssl_engine_set_buffer(&t->sc.eng, t->buf, sizeof(t->buf), 1);
  unsigned char seed[32];
  randomSeed(seed, sizeof(seed));
  br_ssl_engine_inject_entropy(&t->sc.eng, seed, sizeof(seed));
  if (!br_ssl_client_reset(&t->sc, host.c_str(), 0)) {
    err = "tls reset";
    delete t;
    return 0;
  }
  br_sslio_init(&t->io, &t->sc.eng, sockRead, &t->sock, sockWrite, &t->sock);
  return t;
}

bool xboxTlsWrite(XboxTls *t, const char *data, int len)
{
  // The handshake happens here, on the first write.
  return br_sslio_write_all(&t->io, data, (size_t)len) == 0 && br_sslio_flush(&t->io) == 0;
}

int xboxTlsRead(XboxTls *t, char *buf, int len)
{
  int n = br_sslio_read(&t->io, buf, (size_t)len);
  return n > 0 ? n : 0;
}

int xboxTlsError(XboxTls *t)
{
  return br_ssl_engine_last_error(&t->sc.eng);
}

void xboxTlsClose(XboxTls *t)
{
  if (t) {
    br_sslio_close(&t->io);
    delete t;
  }
}
