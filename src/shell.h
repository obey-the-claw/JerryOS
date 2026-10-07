// shell.h - the JerryOS "operating system": screen stack, buttons, bars.
#pragma once
#include "buttons.h"
#include "display.h"
#include "jerry_face.h"
#include "power.h"
#include "screen.h"
#include "settings.h"
#include "attack_watch.h"
#include "camera_watch.h"
#include "follow_watch.h"
#include "tracker_watch.h"

class Shell {
 public:
  Shell(JerryDisplay &lcd, Screen &home);

  void begin();
  void loop();

  // Navigation.
  void open(Screen &s);  // go deeper (menu -> mode)
  void back();           // one level up; at home this does nothing
  void goHome();
  Screen &current() { return *stack_[depth_ - 1]; }
  bool atHome() const { return depth_ == 1; }

  // Shared services for screens.
  JerryFace &jerry() { return jerry_; }
  Settings &settings() { return settings_; }
  TrackerWatch &tracker() { return tracker_; }
  FollowWatch &follow() { return follow_; }
  AttackWatch &attacks() { return attack_; }
  CameraWatch &cameras() { return cams_; }
  // A short message over any screen, e.g. "Flock camera nearby".
  void toast(const char *text, uint32_t ms = 4000);
  bool toastShowing() const { return (int32_t)(millis() - toastUntil_) < 0; }
  const SceneTracker &scene() const { return scene_; }
  void saveTrusted();                     // after "it's mine" / clearing the list
  bool anyAlert() const { return tracker_.anyAlert() || follow_.anyAlert() || attack_.anyAlert(); }
  const PowerStatus &power() const { return power_; }
  void applyBrightness();
  uint32_t idleMs() const { return millis() - lastInput_; }
  void wake() { lastInput_ = millis(); }

 private:
  void handleButtons();
  void drawStatusBar(lgfx::LovyanGFX &g);
  void drawHintBar(lgfx::LovyanGFX &g, const Hints &h);
  void logScreen(const char *why);
  void pollConsole();  // commands typed over USB (used by tools/pull_wifi_log.py)
  void updateBacklight(uint32_t now);

  JerryDisplay &lcd_;
  LGFX_Sprite frame_;
  Button left_{PIN_BTN_LEFT};
  Button right_{PIN_BTN_RIGHT};
  JerryFace jerry_;
  Settings settings_;
  PowerStatus power_;
  power::BatteryGauge gauge_;
  TrackerWatch tracker_;
  FollowWatch follow_;
  AttackWatch attack_;
  CameraWatch cams_;
  char toast_[24] = "";
  uint32_t toastUntil_ = 0;
  void drawToast(lgfx::LovyanGFX &g);
  uint32_t lastKickoffRead_ = 0;
  SceneTracker scene_;
  uint32_t lastFollowCheck_ = 0;
  void checkFollowers(uint32_t now);
  void updateLed(uint32_t now);
  uint32_t lastLed_ = 0;

  static constexpr int MAX_DEPTH = 6;
  Screen *stack_[MAX_DEPTH];
  int depth_ = 1;

  uint32_t lastInput_ = 0;
  uint32_t lastPowerRead_ = 0;
  uint32_t lastFrame_ = 0;
  uint32_t lastLog_ = 0;
  // Screen saver on battery: dim after a minute, off after five.
  enum class Backlight : uint8_t { On, Dim, Off } backlight_ = Backlight::On;
  char cmd_[32] = {};
  int cmdLen_ = 0;
};
