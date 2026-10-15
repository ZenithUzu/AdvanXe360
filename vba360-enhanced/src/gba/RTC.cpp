#include "../System.h"
#include "GBA.h"
#include "Globals.h"
#include "../common/Port.h"
#include "../Util.h"
#include "../NLS.h"
#if (0)
#include "Xbox/Storage.h"
#endif
#include <time.h>
#include <memory.h>

enum RTCSTATE { IDLE, COMMAND, DATA, READDATA };

#if (0)
extern GameStorage xStorage;
#endif

// Cartridge GPIO port (0x80000C4 data, 0x80000C6 direction, 0x80000C8
// enable): real-time clock, Boktai's solar sensor, WarioWare: Twisted!'s
// gyroscope and rumble. The sensor and rumble handling is adapted from
// current VisualBoyAdvance-M (src/core/gba/gbaRtc.cpp, GPL v2+).
typedef struct {
  u8 byte0;
  u8 select;    // pin direction: 0 = read, 1 = write (for each of 4 bits)
  u8 enable;    // bit 0 = the game can read the port
  u8 command;
  int dataLen;
  int bits;
  RTCSTATE state;
  u8 data[12];
  // reserved[10] = last solar sensor clock, reserved[11] = sensor counter
  u8 reserved[12];
  bool reserved2;
  u32 reserved3;
} RTCCLOCKDATA;

static RTCCLOCKDATA rtcClockData;
static bool rtcEnabled = false;
static bool rtcSolarEnabled = false;
static bool rtcGyroEnabled = false;
static bool rtcRumbleEnabled = false;

void rtcEnable(bool e)
{
  rtcEnabled = e;
}

bool rtcIsEnabled()
{
  return rtcEnabled;
}

void rtcEnableSolar(bool e)
{
  rtcSolarEnabled = e;
}

void rtcEnableGyro(bool e)
{
  rtcGyroEnabled = e;
}

void rtcEnableRumble(bool e)
{
  rtcRumbleEnabled = e;
}

// Does this cartridge have anything on its GPIO port?
static bool rtcHasGpio()
{
  return rtcEnabled || rtcSolarEnabled || rtcGyroEnabled || rtcRumbleEnabled;
}

u16 rtcRead(u32 address)
{
  if(rtcHasGpio()) {
    u16 res = 0;
    switch(address){
    case 0x80000c8:
      return rtcClockData.enable;
    case 0x80000c6:
      return rtcClockData.select;
    case 0x80000c4:
      if(!(rtcClockData.enable & 1))
        return 0;
      // Boktai solar sensor: the bit goes high once the counter the game
      // clocks reaches the light level (lower darkness = more sun).
      if(rtcSolarEnabled && rtcClockData.select == 0x07) {
        if(rtcClockData.reserved[11] >= systemGetSensorDarkness())
          res |= 8;
      }
      // WarioWare: Twisted! rotation sensor, read one bit at a time.
      if(rtcGyroEnabled && rtcClockData.select == 0x0b) {
        u16 v = (u16)(0x6C0 + systemGetSensorZ());
        res |= ((v >> rtcClockData.reserved[11]) & 1) << 2;
      }
      // Real-time clock
      if(rtcEnabled && (rtcClockData.select & 0x04))
        res |= rtcClockData.byte0;
      return res;
    }
  }

  return READ16LE((&rom[address & 0x1FFFFFE]));
}

static u8 toBCD(u8 value)
{
  value = value % 100;
  int l = value % 10;
  int h = value / 10;
  return h * 16 + l;
}

bool rtcWrite(u32 address, u16 value)
{
  if(!rtcHasGpio())
    return false;

  if(address == 0x80000c8) {
    rtcClockData.enable = (u8)value;
  } else if(address == 0x80000c6) {
    rtcClockData.select = (u8)value;
    // rumble stops when the game stops driving that pin
    if(rtcRumbleEnabled && !(value & 8))
      systemCartridgeRumble(false);
  } else if(address == 0x80000c4) {
    // WarioWare: Twisted! / Drill Dozer rumble
    if(rtcRumbleEnabled && (rtcClockData.select & 0x08))
      systemCartridgeRumble((value & 8) != 0);

    // Boktai solar sensor
    if(rtcSolarEnabled && rtcClockData.select == 0x07) {
      if(value & 2)
        rtcClockData.reserved[11] = 0;   // reset the counter
      if((value & 1) && !(rtcClockData.reserved[10] & 1)) {
        // clock: count up, ready for another read
        if(rtcClockData.reserved[11] < 255)
          rtcClockData.reserved[11]++;
        else
          rtcClockData.reserved[11] = 0;
      }
      rtcClockData.reserved[10] = (u8)(value & rtcClockData.select);
    }

    // WarioWare: Twisted! rotation sensor
    if(rtcGyroEnabled && rtcClockData.select == 0x0b) {
      if(value & 2)
        rtcClockData.reserved[11]--;     // clock high: next bit
      if(value & 1)
        rtcClockData.reserved[11] = 15;  // start a conversion
      rtcClockData.byte0 = (u8)(value & rtcClockData.select);
    }

    if(rtcEnabled && (rtcClockData.select & 4)) {
      if(rtcClockData.state == IDLE && rtcClockData.byte0 == 1 && value == 5) {
          rtcClockData.state = COMMAND;
          rtcClockData.bits = 0;
          rtcClockData.command = 0;
      } else if(!(rtcClockData.byte0 & 1) && (value & 1)) { // bit transfer
        rtcClockData.byte0 = (u8)value;
        switch(rtcClockData.state) {
        case COMMAND:
          rtcClockData.command |= ((value & 2) >> 1) << (7-rtcClockData.bits);
          rtcClockData.bits++;
          if(rtcClockData.bits == 8) {
            rtcClockData.bits = 0;
            switch(rtcClockData.command) {
            case 0x60:
              // not sure what this command does but it doesn't take parameters
              // maybe it is a reset or stop
              rtcClockData.state = IDLE;
              rtcClockData.bits = 0;
              break;
            case 0x62:
              // this sets the control state but not sure what those values are
              rtcClockData.state = READDATA;
              rtcClockData.dataLen = 1;
              break;
            case 0x63:
              rtcClockData.dataLen = 1;
              rtcClockData.data[0] = 0x40;
              rtcClockData.state = DATA;
              break;
            case 0x64:
              break;
            case 0x65:
              {
                struct tm *newtime;
                time_t long_time;

                time( &long_time );                /* Get time as long integer. */
                newtime = localtime( &long_time ); /* Convert to local time. */

                rtcClockData.dataLen = 7;
                rtcClockData.data[0] = toBCD(newtime->tm_year);
                rtcClockData.data[1] = toBCD(newtime->tm_mon+1);
                rtcClockData.data[2] = toBCD(newtime->tm_mday);
                rtcClockData.data[3] = toBCD(newtime->tm_wday);
                rtcClockData.data[4] = toBCD(newtime->tm_hour);
                rtcClockData.data[5] = toBCD(newtime->tm_min);
                rtcClockData.data[6] = toBCD(newtime->tm_sec);
                rtcClockData.state = DATA;
              }
              break;
            case 0x67:
              {
                struct tm *newtime;
                time_t long_time;

                time( &long_time );                /* Get time as long integer. */
                newtime = localtime( &long_time ); /* Convert to local time. */

                rtcClockData.dataLen = 3;
                rtcClockData.data[0] = toBCD(newtime->tm_hour);
                rtcClockData.data[1] = toBCD(newtime->tm_min);
                rtcClockData.data[2] = toBCD(newtime->tm_sec);
                rtcClockData.state = DATA;
              }
              break;
            default:
              systemMessage(0, N_("Unknown RTC command %02x"), rtcClockData.command);
              rtcClockData.state = IDLE;
              break;
            }
          }
          break;
        case DATA:
          if(rtcClockData.select & 2) {
          } else if(rtcClockData.select & 4) {
            rtcClockData.byte0 = (rtcClockData.byte0 & ~2) |
              ((rtcClockData.data[rtcClockData.bits >> 3] >>
                (rtcClockData.bits & 7)) & 1)*2;
            rtcClockData.bits++;
            if(rtcClockData.bits == 8*rtcClockData.dataLen) {
              rtcClockData.bits = 0;
              rtcClockData.state = IDLE;
            }
          }
          break;
        case READDATA:
          if(!(rtcClockData.select & 2)) {
          } else {
            rtcClockData.data[rtcClockData.bits >> 3] =
              (rtcClockData.data[rtcClockData.bits >> 3] >> 1) |
              ((value << 6) & 128);
            rtcClockData.bits++;
            if(rtcClockData.bits == 8*rtcClockData.dataLen) {
              rtcClockData.bits = 0;
              rtcClockData.state = IDLE;
            }
          }
          break;
	default:
          break;
        }
      } else
        rtcClockData.byte0 = (u8)value;
    }
  }
  return true;
}

void rtcReset()
{
  XMemSet(&rtcClockData, 0, sizeof(rtcClockData));

  rtcClockData.byte0 = 0;
  rtcClockData.select = 0;
  rtcClockData.enable = 0;
  rtcClockData.command = 0;
  rtcClockData.dataLen = 0;
  rtcClockData.bits = 0;
  rtcClockData.state = IDLE;
}

#if (0)
void Xbox_rtcSaveGame(HANDLE file)
{
  xStorage.WriteStream(file,(char *)&rtcClockData, sizeof(rtcClockData)); 
}
#endif

void rtcSaveGame(gzFile gzFile)
{
  utilGzWrite(gzFile, &rtcClockData, sizeof(rtcClockData));
}

void rtcReadGame(gzFile gzFile)
{
  utilGzRead(gzFile, &rtcClockData, sizeof(rtcClockData));
}
