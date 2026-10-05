// Fake speaker for the PC simulator: sounds are just printed (with SIMLOG=1).
#include <Arduino.h>

#include "audio.h"

namespace audio {
namespace {
uint8_t vol = 2;
const char *name(Sound s) {
  static const char *const n[] = {"boot", "happy", "rec-on", "rec-off", "alert", "low-batt",
                                  "tick", "erase"};
  return n[(int)s];
}
}  // namespace
bool begin() { return true; }
bool ready() { return true; }
void setVolume(uint8_t level) { vol = level; }
void play(Sound s) {
  if (vol) Serial.printf("[audio] %s\n", name(s));
}
}  // namespace audio
