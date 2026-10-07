// Fake LED for the PC simulator: remembers the last colour (sim_main draws it).
#include "status_led.h"

Rgb g_simLed{0, 0, 0};

namespace led {
void show(Rgb c) { g_simLed = c; }
}  // namespace led
