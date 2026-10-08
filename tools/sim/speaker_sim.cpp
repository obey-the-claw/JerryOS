// Fake speaker for the PC simulator (the groove itself can be heard with
// tools/sim/render_funk.sh).
#include <Arduino.h>

#include "speaker.h"

namespace speaker {
void playIntro(uint8_t level, int groove) {
  Serial.printf("[speaker] (sim) groove %d, level %u\n", groove, level);
}
void stop() {}
bool busy() { return false; }
}  // namespace speaker
