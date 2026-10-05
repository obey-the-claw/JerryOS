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
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(PIN_BATT_ADC);
  s.pinMv = sum / 32;
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

// Battery voltage wobbles from moment to moment (the screen and radio draw
// power in bursts), so a raw reading makes the % jump around. BatteryGauge
// turns raw readings into a calm number, the way a phone does:
//   1. a slow running average (~15 s) irons out the wobble;
//   2. the shown % only moves when the change is real (a few %);
//   3. on battery the % never ticks up (charge only goes down unplugged);
//   4. plugging / unplugging USB is a real jump, so it resets straight away.
// Call update() about once a second.
class BatteryGauge {
 public:
  PowerStatus update(PowerStatus s) {
    if (!started_ || s.usb != lastUsb_) {
      avgV_ = s.battV;  // start fresh, quickly settle for a few readings
      settle_ = 4;
      started_ = true;
    } else {
      avgV_ += (s.battV - avgV_) * (settle_ ? 0.5f : 0.07f);
    }
    const bool settling = settle_ > 0;
    if (settle_) settle_--;
    const bool justSettled = settling && settle_ == 0;
    lastUsb_ = s.usb;

    s.battV = avgV_;
    s.batteryPresent = !s.usb || (avgV_ > 3.0f && avgV_ < 4.35f);
    const int pct = s.batteryPresent ? voltsToPercent(avgV_) : -1;

    if (pct < 0 || shown_ < 0 || justSettled) {
      shown_ = pct;  // first reading, or the post-plug/unplug average is ready
    } else if (settling) {
      // keep showing the old number for the few seconds the average settles
    } else if (s.usb) {
      if (abs(pct - shown_) >= 5) shown_ = pct;           // charging: either way
    } else {
      if (pct <= shown_ - 2 || pct >= shown_ + 10) shown_ = pct;  // draining: only down
    }
    s.percent = shown_;
    return s;
  }

 private:
  bool started_ = false;
  bool lastUsb_ = false;
  int settle_ = 0;
  float avgV_ = 0.0f;
  int shown_ = -1;
};

}  // namespace power
