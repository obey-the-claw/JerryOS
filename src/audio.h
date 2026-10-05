// audio.h - Jerry's voice: short beeps and chirps through the speaker.
//
// Hardware: ES8311 audio chip (set up over I2C), fed sound over I2S, then a
// small amplifier (enable pin GPIO 9) into the speaker. The amp is only
// switched on while a sound plays, which saves battery and avoids hiss.
//
// play() never waits: sounds go into a queue and a background task plays
// them, so the screen keeps running smoothly.
#pragma once
#include <stdint.h>

namespace audio {

enum class Sound : uint8_t {
  Boot,      // rising three-note "hello"
  Happy,     // poke / saved: two-note chirp up
  RecOn,     // logging started
  RecOff,    // logging stopped
  Alert,     // tracker following you: urgent hi-lo x3
  LowBatt,   // sad falling three notes
  Tick,      // volume preview
  Erase,     // log erased: two low notes
};

bool begin();                  // set up the audio chip; false = chip didn't answer
bool ready();                  // chip found and running
void setVolume(uint8_t level); // 0 = off, 1 low, 2 medium, 3 high
void play(Sound s);            // queue a sound (ignored when volume is off)

}  // namespace audio
