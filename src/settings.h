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
  static constexpr uint8_t N_SOUNDCAL = 21;
  static constexpr uint8_t N_TUNEVOL = 4;
  static constexpr uint8_t N_INTRO = 5;  // random + the 4 intros
  static constexpr uint8_t N_DASH = 6;   // see dashes.h
  static const char *tuneVolName(uint8_t i) {
    static const char *const n[N_TUNEVOL] = {"off", "low", "med", "high"};
    return n[i < N_TUNEVOL ? i : 0];
  }
  static int soundCalChoice(uint8_t i) { return ((i < N_SOUNDCAL ? i : 10) - 10) * 3; }  // -30..+30 dB
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
  bool pocketDark = true;     // pocket mode keeps the screen dark (LED only)
  uint8_t soundCalIdx = 10;   // Sound cal: index into soundCalChoice (10 = 0 dB)
  uint8_t tuneVol = 2;        // boot groove volume: 0 off, 1 low, 2 med, 3 high
  uint8_t introIdx = 0;       // boot intro: 0 random, else 1 + its number (intro.h)
  uint8_t dashIdx = 0;        // home dash (dashes.h): tap R on home flips, this remembers
  bool driveMode = true;      // switch to the Trail Dash while the rig says we're driving

  uint8_t brightnessPct() const { return brightnessChoice(brightnessIdx); }
  uint8_t alertMinutes() const { return alertChoice(alertIdx); }

  void load() {
    Preferences p;
    if (!p.begin("jerryos", true)) return;  // nothing saved yet: keep defaults
    brightnessIdx = clampIdx(p.getUChar("bright", brightnessIdx), N_BRIGHTNESS);
    alertIdx = clampIdx(p.getUChar("alert", alertIdx), N_ALERT);
    wifiScan = p.getBool("wifi", wifiScan);
    ledMode = clampIdx(p.getUChar("led", ledMode), 3);
    pocketDark = p.getBool("pdark", pocketDark);
    soundCalIdx = clampIdx(p.getUChar("scal", soundCalIdx), N_SOUNDCAL);
    introIdx = clampIdx(p.getUChar("intro", introIdx), N_INTRO);
    dashIdx = clampIdx(p.getUChar("dash", dashIdx), N_DASH);
    driveMode = p.getBool("drive", driveMode);
    if (p.isKey("tvol")) tuneVol = clampIdx(p.getUChar("tvol", tuneVol), N_TUNEVOL);
    else if (!p.getBool("tune", true)) tuneVol = 0;  // v1.2 had just on/off: keep "off"
    p.end();
  }

  void save() const {
    Preferences p;
    if (!p.begin("jerryos", false)) return;
    p.putUChar("bright", brightnessIdx);
    p.putUChar("alert", alertIdx);
    p.putBool("wifi", wifiScan);
    p.putUChar("led", ledMode);
    p.putBool("pdark", pocketDark);
    p.putUChar("scal", soundCalIdx);
    p.putUChar("tvol", tuneVol);
    p.putUChar("intro", introIdx);
    p.putUChar("dash", dashIdx);
    p.putBool("drive", driveMode);
    p.end();
  }

 private:
  static uint8_t clampIdx(uint8_t v, uint8_t n) { return v < n ? v : 0; }
};
