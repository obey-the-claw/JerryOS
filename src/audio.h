// audio.h - Jerry's voice through the speaker (sounds: squatch_voice.h).
//
// Hardware: ES8311 audio chip (set up over I2C), fed sound over I2S, then a
// small amplifier (enable pin GPIO 9) into the speaker. The amp is only
// switched on while a sound plays, which saves battery and avoids hiss.
//
// play() never waits: sounds go into a queue and a background task plays
// them, so the screen keeps running smoothly.
#pragma once
#include <stdint.h>

#include "squatch_voice.h"  // the Sound list + Jerry's voice

namespace audio {


bool begin();                  // set up the audio chip; false = chip didn't answer
bool ready();                  // chip found and running
void setVolume(uint8_t level); // 0 = off, 1 low, 2 medium, 3 high
void play(Sound s);            // queue a sound (ignored when volume is off)

}  // namespace audio
