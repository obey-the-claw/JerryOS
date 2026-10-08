// Fake speaker for the PC simulator (the groove itself can be heard with
// tools/sim/render_funk.sh).
#include <Arduino.h>

#include "speaker.h"

namespace speaker {
void playIntro(uint8_t level) { Serial.printf("[speaker] (sim) boot groove, level %u\n", level); }
void stop() {}
bool busy() { return false; }
}  // namespace speaker
