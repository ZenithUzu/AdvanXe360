// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "SoundSDL.h"

#include <string.h>

extern int emulating;
extern bool speedup;

// Set by the Xbox frontend: play silence (sound off / muted fast forward)
// and stop waiting for the audio buffer (fast forward).
bool soundMuted = false;
bool soundNoThrottle = false;
int soundVolumePercent = 100;
int soundBufferMs = 100;

// Hold up to 100 ms of data in the ring buffer
const float SoundSDL::_delay = 0.1f;

SoundSDL::SoundSDL():
	_rbuf(0), _rate(44100), _bufferMs(100)
{

}

void SoundSDL::soundCallback(void *data, u8 *stream, int len)
{
	reinterpret_cast<SoundSDL*>(data)->read(reinterpret_cast<u16 *>(stream), len);
}

void SoundSDL::read(u16 * stream, int length)
{
	if (length <= 0 || !emulating)
		return;

	SDL_mutexP(_mutex);

	_rbuf.read(stream, std::min(static_cast<std::size_t>(length) >> 1, _rbuf.used()));

	SDL_CondSignal(_cond);
	SDL_mutexV(_mutex);
}

void SoundSDL::write(u16 * finalWave, int length)
{
	if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING)
		SDL_PauseAudio(0);

	// Silence keeps the timing (the buffer still throttles the emulation).
	if (soundMuted)
		memset(finalWave, 0, length);
	else if (soundVolumePercent != 100) {
		s16 *p = reinterpret_cast<s16 *>(finalWave);
		int vol = soundVolumePercent * 256 / 100;
		for (int i = 0; i < (length >> 1); i++) {
			int v = (p[i] * vol) >> 8;
			p[i] = (s16)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
		}
	}

	SDL_mutexP(_mutex);

	// A new buffer size from the settings: start the buffer over.
	if (soundBufferMs != _bufferMs && soundBufferMs >= 20 && soundBufferMs <= 500)
	{
		_bufferMs = soundBufferMs;
		_rbuf.reset(_bufferMs * _rate * 2 / 1000);
	}

	unsigned int samples = length >> 2;

	std::size_t avail;
	while ((avail = _rbuf.avail() >> 1) < samples)
	{
		_rbuf.write(finalWave, avail << 1);

		finalWave += avail << 1;
		samples -= avail;

		// If emulating and not in speed up mode, synchronize to audio
		// by waiting till there is enough room in the buffer
		if (emulating && !speedup && !soundNoThrottle)
		{
			SDL_CondWait(_cond, _mutex);
		}
		else
		{
			// Drop the remaining of the audio data
			SDL_mutexV(_mutex);
			return;
		}
	}

	_rbuf.write(finalWave, samples << 1);

	SDL_mutexV(_mutex);
}


bool SoundSDL::init(long sampleRate)
{
	SDL_AudioSpec audio;
	audio.freq = sampleRate;
	audio.format = AUDIO_S16SYS;
	audio.channels = 2;
	audio.samples = 1024;
	audio.callback = soundCallback;
	audio.userdata = this;

	if(SDL_OpenAudio(&audio, NULL))
	{
		fprintf(stderr,"Failed to open audio: %s\n", SDL_GetError());
		return false;
	}

	_rate = sampleRate;
	_bufferMs = (soundBufferMs >= 20 && soundBufferMs <= 500) ? soundBufferMs : 100;
	_rbuf.reset(_bufferMs * sampleRate * 2 / 1000);

	_cond  = SDL_CreateCond();
	_mutex = SDL_CreateMutex();

	return true;
}

SoundSDL::~SoundSDL()
{
	SDL_mutexP(_mutex);
	int iSave = emulating;
	emulating = 0;
	SDL_CondSignal(_cond);
	SDL_mutexV(_mutex);

	// Stop the audio thread first: it may be about to lock _mutex in
	// read(), and waiting on a destroyed mutex could hang the console.
	SDL_CloseAudio();

	SDL_DestroyCond(_cond);
	_cond = NULL;

	SDL_DestroyMutex(_mutex);
	_mutex = NULL;

	emulating = iSave;
}

void SoundSDL::pause()
{
	SDL_PauseAudio(1);
}

void SoundSDL::resume()
{
	SDL_PauseAudio(0);
}

void SoundSDL::reset()
{
}
