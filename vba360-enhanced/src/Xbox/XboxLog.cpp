#define XBOX_LOG_NO_REDIRECT
#include "XboxLog.h"
#include <string.h>
#include <time.h>
#include <string>

namespace {
FILE *g_log = 0;

void writeV(const char *format, va_list args, bool newline)
{
  if (!g_log)
    return;
  char buf[2048];
  // _vsnprintf doesn't end a cut-off line with a 0.
  buf[sizeof(buf) - 2] = 0;
  int n = _vsnprintf(buf, sizeof(buf) - 2, format, args);
  if (n < 0 || n > (int)sizeof(buf) - 2) {
    buf[sizeof(buf) - 2] = 0;
    n = (int)strlen(buf);
  }
  buf[n] = 0;
  if (newline && (n == 0 || buf[n - 1] != '\n')) {
    buf[n++] = '\n';
    buf[n] = 0;
  }
  fwrite(buf, 1, n, g_log);
  fflush(g_log);
}
}

void xboxLogOpen(const char *path)
{
  if (g_log)
    return;
  // Keep the previous session's log (it may hold the steps before a
  // freeze that needed a forced power-off).
  std::string prev = std::string(path);
  size_t dot = prev.rfind('.');
  prev = (dot == std::string::npos ? prev : prev.substr(0, dot)) + "-anterior.log";
  remove(prev.c_str());
  rename(path, prev.c_str());
  // "c": every fflush goes to the disk, not only to the cache, so the
  // lines are there even after a hard freeze.
  g_log = fopen(path, "wc");
  if (!g_log)
    g_log = fopen(path, "w");
  if (g_log)
    xboxLog("VBA360 enhanced-ai log (built " __DATE__ " " __TIME__ ")");
}

void xboxLogV(const char *format, va_list args)
{
  writeV(format, args, true);
}

void xboxLog(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  writeV(format, args, true);
  va_end(args);
}

int xboxLogFprintf(FILE *stream, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int r = 0;
  if (stream == stdout || stream == stderr)
    writeV(format, args, false);
  else
    r = vfprintf(stream, format, args);
  va_end(args);
  return r;
}

int xboxLogPrintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  writeV(format, args, false);
  va_end(args);
  return 0;
}
