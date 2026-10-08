// speaker.h - plays Jerry's boot grooves (funk.cpp) through the speaker.
//
// The only sound JerryOS makes. It runs in the background while the intro
// plays, then switches the audio chip and amplifier off again (no hiss, no
// battery drain). Settings -> Intro tune sets
// the volume (low / med / high) or turns it off.
#pragma once
#include <stdint.h>

namespace speaker {

// Start an intro's groove (funk.h): level 1 low, 2 med, 3 high. Returns at once.
void playIntro(uint8_t level, int groove);
void stop();       // cut it short (e.g. the intro was skipped)
bool busy();       // playing (the microphone waits until it's done)

}  // namespace speaker
