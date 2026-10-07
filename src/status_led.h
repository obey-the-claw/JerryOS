// status_led.h - the little colour LED as a silent signal.
//
// Readable at a glance, even with the screen off:
//   red, fast blink      alert: look at Jerry (tracker, follower or attack)
//   amber, slow pulse    watching: something's around but not alarming yet
//   blue blip every 3 s  recording the Wi-Fi log
//   green, soft breath   all clear (only in "all" mode)
//   off                  nothing to say (or LED switched off)
//
// The pattern logic is pure (tested on a PC); led::show() drives the LED.
#pragma once
#include <stdint.h>

enum class LedMode : uint8_t { Off, Alerts, All };  // Settings -> LED
const char *ledModeName(LedMode m);                  // "off", "alerts", "all"

struct LedInputs {
  bool alert;      // an unseen alert (tracker / follower / attack)
  bool watching;   // something around: lost tracker, attack still going, follower listed
  bool recording;  // Wi-Fi log REC on
};

struct Rgb {
  uint8_t r, g, b;
  bool operator==(const Rgb &o) const { return r == o.r && g == o.g && b == o.b; }
};

// What colour the LED should be at time `now` (ms).
Rgb ledColor(const LedInputs &in, LedMode mode, uint32_t now);

namespace led {
void show(Rgb c);  // send to the LED (only when it changes)
}
