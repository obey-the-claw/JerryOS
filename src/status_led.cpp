// status_led.cpp - see status_led.h. The patterns are pure logic; the PC
// simulator provides its own led::show (tools/sim/status_led_sim.cpp).

#include "status_led.h"

const char *ledModeName(LedMode m) {
  switch (m) {
    case LedMode::Off: return "off";
    case LedMode::Alerts: return "alerts";
    case LedMode::All: return "all";
  }
  return "?";
}

namespace {

// A smooth 0..255..0 triangle over `period` ms.
uint8_t wave(uint32_t now, uint32_t period) {
  const uint32_t t = now % period;
  const uint32_t half = period / 2;
  return (uint8_t)(t < half ? t * 255 / half : (period - t) * 255 / half);
}

// The LED is very bright up close: keep everything gentle.
Rgb scaled(uint8_t r, uint8_t g, uint8_t b, uint8_t level) {
  return {(uint8_t)(r * level / 255), (uint8_t)(g * level / 255), (uint8_t)(b * level / 255)};
}

}  // namespace

Rgb ledColor(const LedInputs &in, LedMode mode, uint32_t now) {
  if (mode == LedMode::Off) return {0, 0, 0};
  if (in.alert)  // 4 blinks a second, red
    return (now / 125) % 2 ? Rgb{60, 0, 0} : Rgb{0, 0, 0};
  if (in.recording && now % 3000 < 120)  // short blue blip every 3 s
    return {0, 0, 40};
  if (in.watching)  // slow amber pulse
    return scaled(40, 16, 0, (uint8_t)(40 + wave(now, 2400) * 215 / 255));
  if (mode == LedMode::All)  // soft green breath, never fully off
    return scaled(0, 14, 2, (uint8_t)(30 + wave(now, 4000) * 225 / 255));
  return {0, 0, 0};
}
