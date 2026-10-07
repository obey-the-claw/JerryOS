// settings.h - user settings, saved to flash so they survive power-off.
//
// Each setting is stored as an index into a fixed list of choices, so a
// corrupted or old value can never become something nonsensical.
#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct Settings {
  static constexpr uint8_t N_BRIGHTNESS = 5;
  static constexpr uint8_t N_ALERT = 6;
  static uint8_t brightnessChoice(uint8_t i) {
    static const uint8_t pct[N_BRIGHTNESS] = {20, 40, 60, 80, 100};
    return pct[i < N_BRIGHTNESS ? i : 0];
  }
  static uint8_t alertChoice(uint8_t i) {
    static const uint8_t minutes[N_ALERT] = {10, 15, 20, 30, 45, 60};
    return minutes[i < N_ALERT ? i : 0];
  }

  uint8_t brightnessIdx = 3;  // 80%
  uint8_t alertIdx = 3;       // 30 minutes (brief default)
  bool wifiScan = true;       // passive Wi-Fi scanner + logger
  uint8_t ledMode = 2;        // colour LED: 0 off, 1 alerts only, 2 all (LedMode)

  uint8_t brightnessPct() const { return brightnessChoice(brightnessIdx); }
  uint8_t alertMinutes() const { return alertChoice(alertIdx); }

  void load() {
    Preferences p;
    if (!p.begin("jerryos", true)) return;  // nothing saved yet: keep defaults
    brightnessIdx = clampIdx(p.getUChar("bright", brightnessIdx), N_BRIGHTNESS);
    alertIdx = clampIdx(p.getUChar("alert", alertIdx), N_ALERT);
    wifiScan = p.getBool("wifi", wifiScan);
    ledMode = clampIdx(p.getUChar("led", ledMode), 3);
    p.end();
  }

  void save() const {
    Preferences p;
    if (!p.begin("jerryos", false)) return;
    p.putUChar("bright", brightnessIdx);
    p.putUChar("alert", alertIdx);
    p.putBool("wifi", wifiScan);
    p.putUChar("led", ledMode);
    p.end();
  }

 private:
  static uint8_t clampIdx(uint8_t v, uint8_t n) { return v < n ? v : 0; }
};
