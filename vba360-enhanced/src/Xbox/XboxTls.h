// HTTPS for the emulator: TLS (BearSSL, MIT) on top of an open TCP socket.
// The console's own network library only speaks plain HTTP to normal
// servers, and today's sites (GitHub, RetroAchievements...) need modern TLS.
#ifndef XBOX_TLS_H
#define XBOX_TLS_H

#include <string>

struct XboxTls;

// Starts TLS on a connected socket and checks the server's certificate for
// host. 0 on failure, with err set ("tls 62": BearSSL's error number).
XboxTls *xboxTlsOpen(unsigned long long sock, const std::string &host, std::string &err);
// Sends all of data (false on failure).
bool xboxTlsWrite(XboxTls *t, const char *data, int len);
// Reads up to len bytes: how many, or 0 when the server is done (or on error).
int xboxTlsRead(XboxTls *t, char *buf, int len);
// BearSSL's last error (0 = none; the server closing the connection is fine).
int xboxTlsError(XboxTls *t);
void xboxTlsClose(XboxTls *t);

#endif
