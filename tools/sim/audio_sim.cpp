// Fake speaker for the PC simulator: sounds are just printed (with SIMLOG=1).
// To actually hear Jerry's voice on a PC: tools/sim/render_voice.sh.
#include <Arduino.h>

#include "audio.h"

namespace audio {
namespace {
uint8_t vol = 2;
}  // namespace
bool begin() { return true; }
bool ready() { return true; }
void setVolume(uint8_t level) { vol = level; }
void play(Sound s) {
  if (vol) Serial.printf("[audio] %s\n", soundName(s));
}
}  // namespace audio
