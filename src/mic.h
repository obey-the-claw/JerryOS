// mic.h - the AIPI Lite's built-in microphone, for the sound meter.
//
// The microphone goes through the ES8311 audio chip (set up over I2C, read
// over I2S). It only runs while something keeps asking for levels: stop
// calling mic::use() and it switches itself off after a few seconds. The
// speaker is never used.
#pragma once
#include "sound_level.h"

namespace mic {

void use();               // "I'm reading levels" - call every loop while needed
bool running();
bool chipFound();         // false = the audio chip didn't answer
const LevelMeter &meter();
void resetPeak();

}  // namespace mic
