// Rewind: hold a button and the game goes backwards, like RetroArch's.
//
// Every few frames the whole emulator state (uncompressed) is saved, but only
// as what changed: its XOR with the next state, with the runs of zeros (the
// parts that didn't change, most of it) left out. Going back undoes one of
// those at a time. The oldest go when the buffer is full.
#ifndef XBOX_REWIND_H
#define XBOX_REWIND_H

#include <stddef.h>

// Starts over (a new game, a state loaded): forgets every saved step.
// stateSize: room for one state; budget: bytes for the steps.
void rewindReset(size_t stateSize, size_t budget);
// The newest state: the whole stateSize buffer (bytes past the state included).
void rewindPush(const unsigned char *state);
// Steps back once: state gets the previous one (stateSize bytes). False when
// there is nothing older (state then holds the oldest one left, if any).
bool rewindPop(unsigned char *state);
// How many steps back are kept.
int rewindSteps();
// False until the first state was pushed (nothing to go back to yet).
bool rewindHasState();

#endif
