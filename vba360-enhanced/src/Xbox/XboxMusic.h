// Menu music: plays the .wav files of the emulator's music\ folder while the
// main menu is open (never during a game or the in-game menu).
// Interface -> Menu music: off, in order or shuffled; and its volume.
#ifndef XBOX_MUSIC_H
#define XBOX_MUSIC_H

// What a .wav file holds (only 16-bit PCM is played).
struct WavInfo
{
  int channels, rate, bits;
  long dataStart, dataBytes;   // where the samples are in the file
};

// Reads a RIFF/WAVE header from the first bytes of a file. False if it is
// not 16-bit PCM with 1 or 2 channels, or the "data" part isn't in buf.
bool musicParseWav(const unsigned char *buf, long len, WavInfo &out);

// Call every main menu frame: starts, feeds and changes songs.
// mode: UiMusicMode; volumePercent: 0..100.
void xboxMusicUpdate(int mode, int volumePercent);
// Stops the music and frees its voice (before a game starts).
void xboxMusicStop();

#endif
