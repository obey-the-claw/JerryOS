// Fake speaker for the PC simulator (the groove itself can be heard with
// tools/sim/render_funk.sh).
#include <Arduino.h>

#include "speaker.h"

namespace speaker {
void playIntro() { Serial.println("[speaker] (sim) boot groove"); }
void stop() {}
bool busy() { return false; }
}  // namespace speaker
