//
//   MBC7 cartridges (Kirby Tilt 'n' Tumble, Command Master): two-axis
//   accelerometer and a 93LC56 serial EEPROM (256 bytes) instead of RAM.
//   VBA360 addition, adapted from SameBoy (Core/memory.c, MIT license,
//   Copyright (c) Lior Halphon).
//

#ifndef GAMBATTE_MBC7_H
#define GAMBATTE_MBC7_H

namespace gambatte {

struct SaveState;

// Accelerometer as the cartridge reports it: 0x81D0 at rest, 0x70 per g.
// The frontend sets these before each frame (gmbBridge::setAccelerometer).
extern unsigned mbc7AccelX, mbc7AccelY;
// True while an MBC7 cartridge is loaded.
extern bool mbc7Present;

// VBA360: Game Boy Camera / Pocket Camera (cartridge type 0xFC). Its
// registers replace RAM at A000-BFFF while RAM bank bit 4 is set; a capture
// asks the frontend for a 128x112 grayscale picture (0 black .. 255 white)
// and turns it into tiles in RAM, as the camera's sensor chip does.
// Algorithm adapted from SameBoy (Core/camera.c, MIT, (c) Lior Halphon).
extern bool cameraPresent;
extern void (*cameraCaptureHook)(unsigned char *gray128x112);
void cameraReset(unsigned char *ram);
unsigned char cameraRead(unsigned p);
void cameraWrite(unsigned p, unsigned data);

class Mbc7Chip {
public:
	Mbc7Chip() : enabled_(false), eeprom_(0) { reset(); }
	// eeprom: 256 bytes (the cartridge's save data), 128 little-endian words.
	void set(bool enabled, unsigned char *eeprom) { enabled_ = enabled; eeprom_ = eeprom; reset(); }
	bool isMbc7() const { return enabled_; }
	void setRamEnable(bool on) { ramEnable_ = on; }
	void setSecondaryEnable(bool on) { secondaryEnable_ = on; }
	bool ramEnable() const { return ramEnable_; }
	unsigned char read(unsigned p) const;
	void write(unsigned p, unsigned data);
	void saveState(SaveState &state) const;
	void loadState(SaveState const &state);

private:
	bool enabled_;
	unsigned char *eeprom_;
	bool ramEnable_, secondaryEnable_;
	bool latchReady_;
	unsigned short xLatch_, yLatch_;
	bool cs_, clk_, di_, do_;
	bool writeEnabled_;
	unsigned short readBits_;
	unsigned short command_;
	unsigned char argumentBitsLeft_;

	void reset();
	unsigned word(unsigned i) const { return eeprom_[i * 2] | eeprom_[i * 2 + 1] << 8; }
	void setWord(unsigned i, unsigned v) { eeprom_[i * 2] = v & 0xFF; eeprom_[i * 2 + 1] = v >> 8 & 0xFF; }
};

}

#endif
