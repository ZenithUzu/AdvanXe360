// Link cable between two consoles on the same network (TCP).
// One console hosts, the other joins by IP. Game Boy / Color: the serial port.
// GBA: Multiplayer and Normal modes. See XboxLink.cpp.
#ifndef XBOX_LINK_H
#define XBOX_LINK_H

#include <string>

enum { LINK_MODE_OFF = 0, LINK_MODE_HOST, LINK_MODE_JOIN, LINK_MODE_COUNT };

// What xboxLinkStatus() reports.
enum { LINK_STATUS_OFF = 0, LINK_STATUS_NO_GAME, LINK_STATUS_NO_NET, LINK_STATUS_BAD_IP,
       LINK_STATUS_WAITING, LINK_STATUS_ONLINE };

// What xboxLinkFrame() has to tell the player.
enum { LINK_EVENT_NONE = 0, LINK_EVENT_CONNECTED, LINK_EVENT_LOST, LINK_EVENT_MISMATCH };

// A game started (isGba: else Game Boy / Color). Starts looking for the other console.
void xboxLinkStart(int mode, const std::string &host, bool isGba);
// The settings may have changed: reconnects only if they did. No game running: does nothing.
void xboxLinkSettings(int mode, const std::string &host);
void xboxLinkStop();
int xboxLinkFrame();   // once a frame: LINK_EVENT_*

bool xboxLinkConnected();
bool xboxLinkIsClient();   // connected, and the other console sets the pace
int xboxLinkStatus();
// This console's address; "" while the network is still starting.
std::string xboxLinkLocalIp();

// Game Boy / Color: the device on Gambatte's serial port.
unsigned char xboxLinkGbSend(unsigned char data);            // this side's clock
bool xboxLinkGbPoll(unsigned char out, unsigned char *in);   // the other side's clock

#endif
