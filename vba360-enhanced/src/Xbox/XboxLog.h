// Diagnostic log: vba360.log next to default.xex.
//
// The Xbox 360 C runtime has no console, and freopen() on stdout did not
// write anything there, so the log is a plain file of its own. Files that
// print to stdout / stderr include this header (after <stdio.h>) and their
// fprintf(stdout, ...) / printf(...) calls land in the log.
#ifndef XBOX_LOG_H
#define XBOX_LOG_H

#include <stdio.h>
#include <stdarg.h>

// Opens (and empties) the log. Safe to call more than once.
void xboxLogOpen(const char *path);
// One line (a '\n' is added when missing); flushed right away.
void xboxLog(const char *format, ...);
void xboxLogV(const char *format, va_list args);

// fprintf / printf replacements: stdout and stderr go to the log, other
// streams to the real fprintf.
int xboxLogFprintf(FILE *stream, const char *format, ...);
int xboxLogPrintf(const char *format, ...);

#if defined(_XBOX) && !defined(XBOX_LOG_NO_REDIRECT)
#define fprintf xboxLogFprintf
#define printf xboxLogPrintf
#endif

#endif
