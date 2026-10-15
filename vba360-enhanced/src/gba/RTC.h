#ifndef RTC_H
#define RTC_H

#include <xtl.h>

u16 rtcRead(u32 address);
bool rtcWrite(u32 address, u16 value);
void rtcEnable(bool);
bool rtcIsEnabled();
// Extra hardware on the cartridge GPIO port (set per game by the frontend).
void rtcEnableSolar(bool);    // Boktai solar sensor
void rtcEnableGyro(bool);     // WarioWare: Twisted! rotation sensor
void rtcEnableRumble(bool);   // WarioWare: Twisted!, Drill Dozer
// Custom game time: seconds added to the console clock (0 = real time).
void rtcSetTimeOffset(int seconds);
void rtcReset();

void rtcReadGame(gzFile gzFile);
void rtcSaveGame(gzFile gzFile);

void Xbox_rtcSaveGame(HANDLE file);

#endif // RTC_H
