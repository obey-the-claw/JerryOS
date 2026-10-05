// power.h - keep the device powered, and read battery / USB / charging.
#pragma once
#include <Arduino.h>
#include "board_pins.h"

struct PowerStatus {
  uint32_t pinMv = 0;      // voltage at the ADC pin (after the divider)
  float battV = 0.0f;      // estimated battery voltage
  int percent = -1;        // -1 = unknown / no battery
  bool usb = false;        // USB power present
  bool charging = false;   // charger chip says it's charging
  bool batteryPresent = false;
};

namespace power {

// Must run first thing in setup(): on battery, the board's power latch only
// stays on while GPIO10 is held high.
inline void holdOn() {
  pinMode(PIN_POWER_HOLD, OUTPUT);
  digitalWrite(PIN_POWER_HOLD, HIGH);
}

inline void begin() {
  pinMode(PIN_USB_SENSE, INPUT);
  pinMode(PIN_CHARGING, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_BATT_ADC, ADC_11db);
}

// Single-cell LiPo voltage -> rough percent (piecewise, resting voltage).
inline int voltsToPercent(float v) {
  static const float pts[][2] = {{3.30f, 0},  {3.50f, 10}, {3.70f, 30}, {3.80f, 50},
                                 {3.95f, 70}, {4.10f, 90}, {4.20f, 100}};
  if (v <= pts[0][0]) return 0;
  for (size_t i = 1; i < sizeof(pts) / sizeof(pts[0]); i++) {
    if (v <= pts[i][0]) {
      float t = (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]);
      return (int)(pts[i - 1][1] + t * (pts[i][1] - pts[i - 1][1]) + 0.5f);
    }
  }
  return 100;
}

inline PowerStatus read() {
  PowerStatus s;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_BATT_ADC);
  s.pinMv = sum / 16;
  s.battV = s.pinMv * BATT_DIVIDER / 1000.0f;
  s.usb = digitalRead(PIN_USB_SENSE) == HIGH;
  s.charging = digitalRead(PIN_CHARGING) == LOW;

  // "No battery" guess: a real LiPo sits between ~3.0 V and ~4.3 V. Outside
  // that while on USB, we're probably reading the charger with no cell.
  // (If we're running at all without USB, a battery must be there.)
  s.batteryPresent = !s.usb || (s.battV > 3.0f && s.battV < 4.35f);
  s.percent = s.batteryPresent ? voltsToPercent(s.battV) : -1;
  return s;
}

}  // namespace power
