// Menu music (see XboxMusic.h). Streams the file in small buffers from the
// menu loop, so no thread is needed; it shares SDL's XAudio2 engine.
#include "XboxMusic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long le32(const unsigned char *p)
{
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

bool musicParseWav(const unsigned char *buf, long len, WavInfo &out)
{
  if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
    return false;
  bool haveFmt = false;
  long pos = 12;
  while (pos + 8 <= len) {
    const unsigned char *ck = buf + pos;
    unsigned long size = le32(ck + 4);
    if (memcmp(ck, "fmt ", 4) == 0 && pos + 8 + 16 <= len) {
      unsigned int format = le16(ck + 8);
      out.channels = (int)le16(ck + 10);
      out.rate = (int)le32(ck + 12);
      out.bits = (int)le16(ck + 22);
      // 1 = PCM; 0xFFFE = extensible, which is PCM here when 16-bit.
      if ((format != 1 && format != 0xFFFE) || out.bits != 16 || out.channels < 1 || out.channels > 2 ||
          out.rate < 8000 || out.rate > 96000)
        return false;
      haveFmt = true;
    } else if (memcmp(ck, "data", 4) == 0) {
      if (!haveFmt)
        return false;
      out.dataStart = pos + 8;
      out.dataBytes = (long)size;
      return true;
    }
    pos += 8 + (long)((size + 1) & ~1UL);   // chunks are padded to even sizes
  }
  return false;
}

#ifdef _XBOX

#include <xtl.h>
#include <xaudio2.h>
#include <math.h>

#include "../ui/UiApp.h"

UiHost &xboxHost();               // XboxHost.cpp (its header pulls in the whole core)
std::string xboxEmulatorDir();

extern "C" IXAudio2 *XboxSDL_AudioEngine(void);   // libSDLx360 SDL_xboxaudio.c

namespace {

enum { BUFFERS = 4, BUFFER_BYTES = 64 * 1024 };

IXAudio2SourceVoice *g_voice = 0;
FILE *g_file = 0;
long g_left = 0;                       // bytes of the song not read yet
int g_rate = 0, g_channels = 0;
unsigned char g_buf[BUFFERS][BUFFER_BYTES];
int g_next = 0;                        // buffer to fill next
std::vector<std::string> g_songs;
int g_song = -1;
int g_lastMode = -1;
int g_failures = 0;                    // songs in a row that wouldn't play
int g_volume = -1;                     // volume last given to the voice
std::string g_wantSong;                // chosen by hand ("" = all)

void closeSong()
{
  if (g_voice) {
    g_voice->Stop(0);
    g_voice->FlushSourceBuffers();
    g_voice->DestroyVoice();
    g_voice = 0;
  }
  g_volume = -1;
  if (g_file) {
    fclose(g_file);
    g_file = 0;
  }
  g_left = 0;
}

void listSongs()
{
  srand((unsigned int)GetTickCount());
  g_songs.clear();
  std::vector<UiDirEntry> e;
  xboxHost().listDir(uiJoinPath(xboxEmulatorDir(), "music"), e);
  for (size_t i = 0; i < e.size(); i++)
    if (!e[i].isDir && uiExtension(e[i].name) == ".wav")
      g_songs.push_back(e[i].path);
}

// Opens the next song; false if there is none that plays.
bool openSong(int mode)
{
  closeSong();
  if (g_songs.empty() || g_failures >= (int)g_songs.size())
    return false;
  int chosen = -1;
  for (size_t i = 0; i < g_songs.size() && !g_wantSong.empty(); i++)
    if (uiFileName(g_songs[i]) == g_wantSong)
      chosen = (int)i;
  if (chosen >= 0) {
    g_song = chosen;   // the chosen song, over and over
  } else if (mode == MUSIC_SHUFFLE && g_songs.size() > 1) {
    int n;
    do n = rand() % (int)g_songs.size(); while (n == g_song);
    g_song = n;
  } else {
    g_song = (g_song + 1) % (int)g_songs.size();
  }
  g_file = fopen(g_songs[g_song].c_str(), "rb");
  if (!g_file)
    return false;
  unsigned char head[512];
  long got = (long)fread(head, 1, sizeof(head), g_file);
  WavInfo w;
  IXAudio2 *engine = XboxSDL_AudioEngine();
  if (!engine || !musicParseWav(head, got, w) || fseek(g_file, w.dataStart, SEEK_SET) != 0) {
    closeSong();
    return false;
  }
  WAVEFORMATEX f;
  memset(&f, 0, sizeof(f));
  f.wFormatTag = WAVE_FORMAT_PCM;
  f.nChannels = (WORD)w.channels;
  f.nSamplesPerSec = (DWORD)w.rate;
  f.wBitsPerSample = 16;
  f.nBlockAlign = (WORD)(w.channels * 2);
  f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
  if (FAILED(engine->CreateSourceVoice(&g_voice, &f))) {
    g_voice = 0;
    closeSong();
    return false;
  }
  g_left = w.dataBytes;
  g_next = 0;
  g_voice->Start(0);
  return true;
}

// Tops the voice's queue up; false once the song has fully played.
bool feed()
{
  XAUDIO2_VOICE_STATE st;
  g_voice->GetState(&st);
  while (st.BuffersQueued < BUFFERS && g_left > 0 && g_file) {
    long want = g_left < BUFFER_BYTES ? g_left : BUFFER_BYTES;
    want &= ~3L;
    long got = want > 0 ? (long)fread(g_buf[g_next], 1, want, g_file) & ~1L : 0;
    if (got <= 0) {
      g_left = 0;
      break;
    }
    g_left -= got;
    unsigned char *p = g_buf[g_next];
    for (long i = 0; i + 1 < got; i += 2) {   // .wav is little-endian, the console big-endian
      unsigned char t = p[i];
      p[i] = p[i + 1];
      p[i + 1] = t;
    }
    XAUDIO2_BUFFER b;
    memset(&b, 0, sizeof(b));
    b.AudioBytes = (UINT32)got;
    b.pAudioData = p;
    if (FAILED(g_voice->SubmitSourceBuffer(&b)))
      return false;
    g_next = (g_next + 1) % BUFFERS;
    st.BuffersQueued++;
  }
  return g_left > 0 || st.BuffersQueued > 0;
}

} // namespace

void xboxMusicUpdate(int mode, int volumePercent, const std::string &song)
{
  if (song != g_wantSong) {   // another song chosen: start it now
    g_wantSong = song;
    closeSong();
    g_failures = 0;
  }
  if (mode == MUSIC_OFF) {
    if (g_voice || g_file)
      closeSong();
    g_lastMode = mode;
    return;
  }
  if (g_lastMode == MUSIC_OFF || g_lastMode < 0) {   // turned on, or back from a game: look again
    listSongs();
    g_failures = 0;
  }
  g_lastMode = mode;
  if (!g_voice) {
    if (g_failures >= (int)g_songs.size())
      return;   // nothing that plays: don't keep trying every frame
    if (!openSong(mode)) {
      g_failures++;
      return;
    }
    g_failures = 0;
  }
  if (volumePercent != g_volume) {   // only when it changes, not every frame
    g_voice->SetVolume(volumePercent / 100.0f);
    g_volume = volumePercent;
  }
  if (!feed())
    closeSong();   // the next frame starts the next song
}

void xboxMusicStop()
{
  closeSong();
  g_lastMode = -1;
}

// --- Short sounds (achievement unlocked) ------------------------------------

namespace {
IXAudio2SourceVoice *g_sfxVoice = 0;
std::vector<unsigned char> g_sfx;   // kept until the next sound: the voice reads it while playing
int g_sfxRate = 0, g_sfxChannels = 0;

// A little two-note chime (mono, 44.1 kHz), for when there's no sound file.
void makeChime(std::vector<unsigned char> &out)
{
  const int rate = 44100, n = rate * 45 / 100;
  out.resize(n * 2);
  for (int i = 0; i < n; i++) {
    float t = (float)i / rate;
    float f = i < n / 3 ? 1046.5f : 1568.0f;   // C6, then G6
    float local = i < n / 3 ? t : t - (float)(n / 3) / rate;
    float env = expf(-local * 9.0f) * (local < 0.004f ? local / 0.004f : 1.0f);
    float v = sinf(6.2831853f * f * t) * 0.55f + sinf(6.2831853f * f * 2 * t) * 0.15f;
    int s = (int)(v * env * 20000.0f);
    out[i * 2] = (unsigned char)((s >> 8) & 0xFF);   // big-endian, as the console plays it
    out[i * 2 + 1] = (unsigned char)(s & 0xFF);
  }
}
}

void xboxPlaySound(const std::string &path)
{
  IXAudio2 *engine = XboxSDL_AudioEngine();
  if (!engine)
    return;
  // The same sound again (every achievement): just replay it, without reading
  // the file or making a voice (both stall the game for a moment).
  static std::string loaded;
  if (g_sfxVoice && !g_sfx.empty() && path == loaded) {
    g_sfxVoice->Stop(0);
    g_sfxVoice->FlushSourceBuffers();
    XAUDIO2_BUFFER again;
    memset(&again, 0, sizeof(again));
    again.AudioBytes = (UINT32)g_sfx.size();
    again.pAudioData = &g_sfx[0];
    again.Flags = XAUDIO2_END_OF_STREAM;
    g_sfxVoice->SubmitSourceBuffer(&again);
    g_sfxVoice->Start(0);
    return;
  }
  loaded = path;
  if (g_sfxVoice) {
    g_sfxVoice->Stop(0);
    g_sfxVoice->FlushSourceBuffers();
    g_sfxVoice->DestroyVoice();
    g_sfxVoice = 0;
  }
  g_sfx.clear();
  g_sfxRate = 44100;
  g_sfxChannels = 1;
  FILE *f = path.empty() ? 0 : fopen(path.c_str(), "rb");
  if (f) {
    std::vector<unsigned char> file;
    unsigned char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0 && file.size() < 4 * 1024 * 1024)
      file.insert(file.end(), buf, buf + got);
    fclose(f);
    WavInfo w;
    if (!file.empty() && musicParseWav(&file[0], (long)file.size(), w) &&
        w.dataStart + w.dataBytes <= (long)file.size()) {
      g_sfx.assign(file.begin() + w.dataStart, file.begin() + w.dataStart + (w.dataBytes & ~1L));
      for (size_t i = 0; i + 1 < g_sfx.size(); i += 2) {   // .wav is little-endian
        unsigned char t = g_sfx[i];
        g_sfx[i] = g_sfx[i + 1];
        g_sfx[i + 1] = t;
      }
      g_sfxRate = w.rate;
      g_sfxChannels = w.channels;
    }
  }
  if (g_sfx.empty())
    makeChime(g_sfx);
  WAVEFORMATEX fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.wFormatTag = WAVE_FORMAT_PCM;
  fmt.nChannels = (WORD)g_sfxChannels;
  fmt.nSamplesPerSec = (DWORD)g_sfxRate;
  fmt.wBitsPerSample = 16;
  fmt.nBlockAlign = (WORD)(g_sfxChannels * 2);
  fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
  if (FAILED(engine->CreateSourceVoice(&g_sfxVoice, &fmt))) {
    g_sfxVoice = 0;
    return;
  }
  XAUDIO2_BUFFER b;
  memset(&b, 0, sizeof(b));
  b.AudioBytes = (UINT32)g_sfx.size();
  b.pAudioData = &g_sfx[0];
  b.Flags = XAUDIO2_END_OF_STREAM;
  g_sfxVoice->SubmitSourceBuffer(&b);
  g_sfxVoice->Start(0);
}

#endif // _XBOX
