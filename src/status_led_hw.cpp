// status_led_hw.cpp - drives the WS2812 colour LED on GPIO 46 (device only;
// the PC simulator uses tools/sim/status_led_sim.cpp).

#include <Arduino.h>

#include "board_pins.h"
#include "status_led.h"

namespace led {

void show(Rgb c) {
  static bool first = true;
  static Rgb last{0, 0, 0};
  if (!first && c == last) return;  // only talk to the LED when the colour changes
  first = false;
  last = c;
  neopixelWrite(PIN_RGB_LED, c.r, c.g, c.b);
}

}  // namespace led
