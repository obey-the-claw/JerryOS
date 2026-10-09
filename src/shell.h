// shell.h - the JerryOS "operating system": screen stack, buttons, bars.
#pragma once
#include "buttons.h"
#include "display.h"
#include "jerry_face.h"
#include "power.h"
#include "screen.h"
#include "settings.h"
#include "attack_watch.h"
#include "badges.h"
#include "expedition.h"
#include "history.h"
#include "sun_clock.h"
#include "camera_watch.h"
#include "follow_watch.h"
#include "rig_link.h"
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
  RigLink &rig() { return rig_; }
  const History &history() const { return history_; }
  const WallClock &clock() const { return clock_; }
  Badges &badges() { return badges_; }
  const TripLog &trips() const { return trips_; }
  const Expedition &trip() const { return trip_; }
  const TripRecap &lastRecap() const { return lastRecap_; }
  // Today's sunrise / sunset where the rig box last had a GPS fix.
  bool sun(int &rise, int &set) const;
  // A new badge: save it, toast it, Jerry grins. false = already had it.
  bool award(Badge b);
  void saveBadges();
  void clearTrips();
  void saveRig();      // after pairing / forgetting the rig box
  void factoryReset(); // wipe settings, "mine" list, pairing and the Wi-Fi log, then restart
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
  bool pocket() const { return pocket_; }  // buttons locked (hold both to toggle)

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
  RigLink rig_;
  History history_;
  WallClock clock_;
  Badges badges_;
  Expedition trip_;
  TripLog trips_;
  TripRecap lastRecap_ = {};
  bool wasRecording_ = false;
  bool hasFix_ = false;
  int32_t fixLat_ = 0, fixLon_ = 0;
  uint32_t lastRigMs_ = 0, lastHistory_ = 0, lastTripSample_ = 0, lastBadgeCheck_ = 0;
  bool nightDim_ = false;
  TripSample tripSample(uint32_t now);
  void updateExtras(uint32_t now);  // clock, trends, expeditions, badges
  void saveTrips();
  void announce(Badge b);  // toast + save a badge just earned
  char toast_[24] = "";
  uint32_t toastUntil_ = 0;
  void drawToast(lgfx::LovyanGFX &g);
  uint32_t lastKickoffRead_ = 0;
  SceneTracker scene_;
  uint32_t lastFollowCheck_ = 0;
  void checkFollowers(uint32_t now);
  void updateLed(uint32_t now);
  uint32_t lastLed_ = 0;
  // Pocket mode: hold both buttons 1 s to lock / unlock.
  static constexpr uint32_t COMBO_MS = 1000;
  bool pocket_ = false;
  bool combo_ = false, comboFired_ = false;
  uint32_t comboStart_ = 0, peekUntil_ = 0;
  void setPocket(bool on);

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
