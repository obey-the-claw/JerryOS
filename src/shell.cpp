// shell.cpp - runs the show: reads buttons, routes them to the current
// screen, and draws status bar + screen + hint bar ~30 times a second.

#include "shell.h"

#include <Preferences.h>
#include <string.h>

#include "ble_scanner.h"
#include "intro.h"
#include "mic.h"
#include "screens.h"
#include "status_led.h"
#include "theme.h"
#include "ui.h"
#include "wifi_scanner.h"

void Screen::onLeftShort(Shell &shell) { shell.back(); }

Shell::Shell(JerryDisplay &lcd, Screen &home) : lcd_(lcd), frame_(&lcd) {
  stack_[0] = &home;
}

void Shell::begin() {
  power::holdOn();  // first: keep the board powered on battery
  power::begin();
  pinMode(PIN_SPK_AMP_EN, OUTPUT);  // Jerry is silent: keep the speaker amp off
  digitalWrite(PIN_SPK_AMP_EN, LOW);
  left_.begin();
  right_.begin();
  settings_.load();
  {  // devices you've marked "mine" in Followers
    Preferences p;
    if (p.begin("jerryos", true)) {
      uint8_t buf[FollowWatch::MAX_TRUSTED * 6];
      const size_t n = p.getBytes("trust", buf, sizeof buf);
      follow_.loadTrusted(buf, (int)(n / 6));
      uint8_t rigAddr[6];
      if (p.getBytes("rig", rigAddr, 6) == 6) rig_.pair(rigAddr);
      if (p.isKey("fixlat")) {  // where the rig box last had a fix (for sunrise / sunset)
        hasFix_ = true;
        fixLat_ = p.getInt("fixlat", 0);
        fixLon_ = p.getInt("fixlon", 0);
        utcOffsetMin_ = p.getShort("utcoff", 0);
      }
      Badges::Saved b;
      if (p.getBytes("badges", &b, sizeof b) == sizeof b) badges_.load(b);
      static uint8_t trips[TripLog::MAX * sizeof(TripRecap)];
      trips_.load(trips, (int)p.getBytes("trips", trips, sizeof trips));
      p.end();
    }
  }

  lcd_.init();
  lcd_.setRotation(1);
  applyBrightness();
  mic::setCalibration(Settings::soundCalChoice(settings_.soundCalIdx));
  frame_.setColorDepth(16);
  if (!frame_.createSprite(theme::W, theme::H))
    Serial.println(F("!! could not allocate the frame buffer"));

  power_ = gauge_.update(power::read());
  lastInput_ = millis();
  Serial.printf("[shell] JerryOS %s up. brightness %u%%, alert window %u min\n",
                JERRYOS_VERSION, settings_.brightnessPct(), settings_.alertMinutes());
  ble::begin();  // Tracker Watch listens in the background on every screen
  wifi::setEnabled(settings_.wifiScan);
  wifi::begin();  // ...and so does the Wi-Fi scanner/logger
  current().enter(*this);
  logScreen("boot");
  open(introScreen());  // one of the intros (Settings -> Intro); any button closes it
}

// Tiny USB command line. Type a command + Enter in a serial terminal:
//   wifi dump   print the whole Wi-Fi log (tools/pull_wifi_log.py does this)
//   wifi clear  erase the Wi-Fi log
//   trips       print the Expedition log (REC session recaps) as CSV
//   help        list commands
void Shell::pollConsole() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (cmdLen_ < (int)sizeof(cmd_) - 1) cmd_[cmdLen_++] = (char)c;
      continue;
    }
    cmd_[cmdLen_] = 0;
    cmdLen_ = 0;
    if (!strcmp(cmd_, "wifi dump")) wifi::dumpLog();
    else if (!strcmp(cmd_, "wifi clear")) wifi::clearLog();
    else if (!strcmp(cmd_, "trips")) {  // the Expedition log as CSV (newest first)
      Serial.println(F("# trip,day_of_year,start_min,seconds,networks,meters,top_kmh,peak_bt,trackers,"
                       "followers,flock,knocks"));
      for (int i = 0; i < trips_.count(); i++) {
        const TripRecap &r = trips_.at(i);
        Serial.printf("%u,%u,%d,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u\n", r.number, r.doy,
                      r.startMin == 0xFFFF ? -1 : r.startMin, (unsigned long)r.seconds,
                      (unsigned long)r.nets, (unsigned long)r.meters, r.topKmh, r.peakBt, r.trackers,
                      r.followers, r.flock, r.knocks);
      }
      Serial.println(F("# end"));
    } else if (cmd_[0]) Serial.println(F("[console] commands: wifi dump | wifi clear | trips | help"));
  }
}

void Shell::saveRig() {
  Preferences p;
  if (!p.begin("jerryos", false)) return;
  if (rig_.paired()) p.putBytes("rig", rig_.pairedAddr(), 6);
  else p.remove("rig");
  p.end();
}

void Shell::saveBadges() {
  Preferences p;
  if (!p.begin("jerryos", false)) return;
  const Badges::Saved b = badges_.save();
  p.putBytes("badges", &b, sizeof b);
  p.end();
}

void Shell::saveTrips() {
  Preferences p;
  if (!p.begin("jerryos", false)) return;
  p.putBytes("trips", trips_.data(), trips_.bytes());
  p.end();
}

void Shell::clearTrips() {
  trips_.clear();
  saveTrips();
}

bool Shell::award(Badge b) {
  if (!badges_.earn(b)) return false;
  announce(b);
  return true;
}

void Shell::announce(Badge b) {
  saveBadges();
  char msg[24];
  snprintf(msg, sizeof msg, "Badge: %s!", badgeName(b));
  toast(msg, 5000);
  jerry_.flash(Mood::Approved, 2500);
  Serial.printf("[badge] %s\n", badgeName(b));
}

bool Shell::sun(int &rise, int &set) const {
  if (!clock_.valid() || !hasFix_) return false;
  return sunTimes(fixLat_ / 1e7, fixLon_ / 1e7, clock_.dayOfYear(millis()), clock_.utcOffsetMin(), rise, set);
}

void Shell::saveFix() {
  Preferences p;
  if (!p.begin("jerryos", false)) return;
  p.putInt("fixlat", fixLat_);
  p.putInt("fixlon", fixLon_);
  p.putShort("utcoff", (int16_t)utcOffsetMin_);
  p.end();
}

void Shell::setClock(uint16_t minuteOfDay, uint16_t dayOfYear) {
  clock_.set(minuteOfDay, dayOfYear, utcOffsetMin_, millis());
  Serial.printf("[clock] set by hand: %02u:%02u, day %u\n", minuteOfDay / 60, minuteOfDay % 60, dayOfYear);
}

void Shell::powerOff() {
  using namespace theme;
  if (power_.usb) {  // USB keeps the board powered whatever we do
    toast("Unplug USB first", 3000);
    return;
  }
  Serial.println(F("[power] switching off"));
  wifi::setRecording(false);  // close the log cleanly (and the trip recap is kept)
  frame_.fillScreen(BARK);
  jerry_.setMood(Mood::Sleepy);
  jerry_.draw(frame_, W / 2, H / 2 - 8, 0.6f);
  frame_.setFont(&fonts::Font0);
  frame_.setTextColor(CREAM);
  frame_.setCursor((W - frame_.textWidth("Bye! Stay wild.")) / 2, H - 22);
  frame_.print("Bye! Stay wild.");
  frame_.pushSprite(0, 0);
  led::show(Rgb{0, 0, 0});
  delay(1500);
  lcd_.setBrightness(0);
  digitalWrite(PIN_POWER_HOLD, LOW);  // the power latch lets go
  delay(2000);
  // Still here? Something kept the power on: carry on as normal.
  applyBrightness();
  toast("Couldn't switch off", 3000);
}

TripSample Shell::tripSample(uint32_t now) {
  TripSample s;
  s.bt = ble::nearbyDevices();
  s.trackers = ui::lostTrackersNow(*this, now);
  s.flockPassed = cams_.passedToday();
  s.followersFlagged = follow_.flaggedCount();
  s.knocks = mic::knockCount();
  s.fix = rig_.location(now, s.latE7, s.lonE7);
  s.speedKmh = rig_.connected(now) ? rig_.status().speedKmh : -1;
  return s;
}

// The v1.4 extras: the box's clock and GPS, the Trends history, the
// Expedition log (follows REC) and badges.
void Shell::updateExtras(uint32_t now) {
  if (rig_.connected(now) && rig_.lastHeardMs() != lastRigMs_) {
    lastRigMs_ = rig_.lastHeardMs();
    const RigStatus &st = rig_.status();
    if (st.clock) {
      clock_.set(st.clockMin, st.doy, st.utcOff15 * 15, now);
      utcOffsetMin_ = st.utcOff15 * 15;
    }
    if (st.fix) {
      const bool first = !hasFix_ || !lastFixSave_;
      hasFix_ = true;
      fixLat_ = st.latE7;
      fixLon_ = st.lonE7;
      // Remember it (sunrise / sunset keep working without the box): once
      // per power-on, then every 30 minutes.
      if (first || now - lastFixSave_ >= 30 * 60000u) {
        lastFixSave_ = now;
        saveFix();
      }
    }
  }
  if (now - lastHistory_ >= History::STEP_MS) {
    lastHistory_ = now;
    history_.add(ble::nearbyDevices(), wifi::enabled() ? wifi::nearby() : -1,
                 mic::running() && mic::meter().hasData() ? mic::meter().db() : -1);
  }
  const bool rec = wifi::recording();
  if (rec && !wasRecording_) {
    trip_.begin(now, tripSample(now), clock_.valid() ? clock_.minute(now) : 0xFFFF,
                clock_.valid() ? clock_.dayOfYear(now) : 0, (uint8_t)wifi::session());
    lastTripSample_ = now;
  } else if (rec && now - lastTripSample_ >= 5000) {
    lastTripSample_ = now;
    trip_.sample(now, tripSample(now));
  } else if (!rec && wasRecording_) {  // REC stopped: the recap card
    trip_.sample(now, tripSample(now));
    lastRecap_ = trip_.end(now, wifi::savedThisSession());
    trips_.add(lastRecap_);
    saveTrips();
    badges_.addTrip(lastRecap_.nets, lastRecap_.meters);
    saveBadges();
    open(tripScreen());
  }
  wasRecording_ = rec;
  if (now - lastBadgeCheck_ >= 5000) {
    lastBadgeCheck_ = now;
    BadgeInputs in = {};
    in.flockSeen = cams_.passedToday() > 0;
    in.lostTracker = tracker_.anySeparatedNow(now);
    in.follower = follow_.flaggedCount() > 0;
    in.rigPaired = rig_.paired();
    in.shieldCaught = attack_.anyActive();
    in.cameraHint = cams_.hintsSeen() > 0;
    in.knockHeard = mic::knockCount() > 0;
    in.btNow = ble::nearbyDevices();
    const Badge b = badges_.check(in);  // earns it
    if (b != Badge::COUNT) announce(b);
  }
  // Night Watch keeps the backlight low.
  const bool night = atHome() && current().fullScreen();
  if (night != nightDim_) {
    nightDim_ = night;
    if (backlight_ == Backlight::On) applyBrightness();
  }
}

void Shell::factoryReset() {
  Serial.println(F("[shell] FACTORY RESET"));
  wifi::clearLog();
  Preferences p;
  if (p.begin("jerryos", false)) {
    p.clear();
    p.end();
  }
  delay(300);
  ESP.restart();
}

void Shell::saveTrusted() {
  Preferences p;
  if (!p.begin("jerryos", false)) return;
  p.putBytes("trust", follow_.trustedData(), follow_.trustedCount() * 6);
  p.end();
}

// Followers: every 20 s, "have I moved?" from Wi-Fi, then "what's still
// with me?" from Bluetooth.
void Shell::checkFollowers(uint32_t now) {
  if (now - lastFollowCheck_ < 20000) return;
  lastFollowCheck_ = now;
  static WifiNet nets[SceneTracker::MAX];
  static uint8_t bssids[SceneTracker::MAX][6];
  const int nn = wifi::enabled() ? wifi::snapshot(nets, SceneTracker::MAX) : 0;
  for (int i = 0; i < nn; i++) memcpy(bssids[i], nets[i].bssid, 6);
  scene_.onScan(bssids, nn, now);
  attack_.checkNetworks(nets, nn, now);  // evil twins, many-names boxes
  bool newCam = false;
  for (int i = 0; i < nn; i++) {          // Flock camera hotspots / addresses
    CamMatch m;
    if (matchCameraWifi(nets[i].bssid, nets[i].ssid, m) &&
        cams_.onSighting(nets[i].bssid, m, nets[i].rssi, now) && isFlock(m.kind))
      newCam = true;  // only Flock pops up; camera hints are just listed
  }
  if (newCam) toast("Flock camera nearby!");

  static NearbyDevice devs[NearbyTable::MAX];
  const int nd = ble::nearbySnapshot(devs, NearbyTable::MAX);
  if (follow_.tick(devs, nd, scene_.place(), now)) {
    Serial.println(F("[follow] ALERT: a Bluetooth device has been with you across several places"));
    lastInput_ = now;  // wake the screen
    if (&current() != &followersScreen()) open(followersScreen());
  }
}

// The colour LED: a silent signal you can read with the screen off.
void Shell::updateLed(uint32_t now) {
  if (now - lastLed_ < 20) return;
  lastLed_ = now;
  LedInputs in;
  in.alert = anyAlert();
  in.watching = tracker_.anySeparatedNow(now) || follow_.flaggedCount() > 0 ||
                attack_.anyActive() || cams_.nearbyFlock(now) > 0;
  in.recording = wifi::recording();
  // Night Watch: the LED goes dark too (no glow in the woods at night; the
  // dim red screen still shows alerts). Not in pocket mode, where the screen
  // is off and the LED is the only signal left.
  if (nightDim_ && !pocket_) return led::show(Rgb{0, 0, 0});
  led::show(ledColor(in, (LedMode)settings_.ledMode, now));
}

void Shell::toast(const char *text, uint32_t ms) {
  snprintf(toast_, sizeof toast_, "%s", text);
  toastUntil_ = millis() + ms;
  lastInput_ = millis();  // wake the screen so you can read it
}

void Shell::drawToast(lgfx::LovyanGFX &g) {
  using namespace theme;
  if ((int32_t)(millis() - toastUntil_) >= 0) return;
  g.setFont(&fonts::Font0);
  const int w = g.textWidth(toast_) + 10, x = (W - w) / 2, y = CONTENT_Y + 2;
  g.fillRoundRect(x, y, w, 13, 4, GOLD);
  g.drawRoundRect(x, y, w, 13, 4, BARK);
  g.setTextColor(BARK);
  g.setCursor(x + 5, y + 3);
  g.print(toast_);
}

void Shell::applyBrightness() {
  backlight_ = Backlight::On;
  const int pct = nightDim_ ? min(20, (int)settings_.brightnessPct()) : settings_.brightnessPct();
  lcd_.setBrightness(pct * 255 / 100);
}

// On battery, save power when nobody's looking: dim after 1 min without a
// button press, backlight off after 5 min. On USB the screen stays on.
// A tracker alert wakes the screen (it resets the idle timer).
void Shell::updateBacklight(uint32_t now) {
  const uint32_t idle = now - lastInput_;
  Backlight want = Backlight::On;
  if (pocket_ && settings_.pocketDark && (int32_t)(now - peekUntil_) >= 0) {
    // Pocket mode, dark: screen stays off (even for alerts; the LED tells you).
    if (backlight_ != Backlight::Off) {
      backlight_ = Backlight::Off;
      lcd_.setBrightness(0);
    }
    return;
  }
  if (!power_.usb) {
    if (idle > 5 * 60000) want = Backlight::Off;
    else if (idle > 60000) want = Backlight::Dim;
  }
  if (want == backlight_) return;
  if (want == Backlight::On) {
    applyBrightness();
  } else {
    backlight_ = want;
    lcd_.setBrightness(want == Backlight::Off ? 0 : max(12, settings_.brightnessPct() * 255 / 400));
  }
}

void Shell::open(Screen &s) {
  if (depth_ >= MAX_DEPTH) return;
  stack_[depth_++] = &s;
  s.enter(*this);
  logScreen("open");
}

void Shell::back() {
  if (depth_ <= 1) return;
  depth_--;
  current().enter(*this);
  logScreen("back");
}

void Shell::goHome() {
  if (depth_ == 1) return;
  depth_ = 1;
  current().enter(*this);
  logScreen("home");
}

void Shell::logScreen(const char *why) {
  Serial.printf("[shell] %s -> %s (depth %d)\n", why, current().tag(), depth_);
}

void Shell::setPocket(bool on) {
  pocket_ = on;
  peekUntil_ = millis() + 2500;  // show the message, then (if dark) go black
  lastInput_ = millis();
  applyBrightness();
  toast(on ? "Pocket mode: locked" : "Unlocked", 2500);
  jerry_.flash(on ? Mood::Sleepy : Mood::Approved, 1500);
  Serial.printf("[pocket] %s\n", on ? "locked" : "unlocked");
}

void Shell::handleButtons() {
  const ButtonEvent l = left_.update();
  const ButtonEvent r = right_.update();

  // Both buttons together = pocket-mode toggle. While both are (or were)
  // down, single-button events are swallowed so nothing else happens.
  const uint32_t now = millis();
  const bool both = left_.isPressed() && right_.isPressed();
  if (both && !combo_) {
    combo_ = true;
    comboFired_ = false;
    comboStart_ = now;
  }
  // Hold L for 5 s (on its own, not in pocket mode) = switch off. Long
  // before that, at 0.7 s, it has already gone home as usual.
  if (left_.isPressed() && !right_.isPressed() && !pocket_) {
    if (!leftSince_) leftSince_ = now;
    const uint32_t held = now - leftSince_;
    if (held >= 2000 && !offWarned_) {
      offWarned_ = true;
      toast("Keep holding: power off", 3000);
    }
    if (held >= 5000 && !offTried_) {
      offTried_ = true;
      powerOff();
    }
  } else {
    leftSince_ = 0;
    offWarned_ = offTried_ = false;
  }

  if (combo_) {
    if (both && !comboFired_ && now - comboStart_ >= COMBO_MS) {
      comboFired_ = true;
      setPocket(!pocket_);
    }
    if (!left_.isPressed() && !right_.isPressed()) combo_ = false;
    return;
  }

  if (l == ButtonEvent::None && r == ButtonEvent::None) return;
  lastInput_ = now;
  if (pocket_) {  // locked: a press only peeks at the screen
    peekUntil_ = now + 4000;
    applyBrightness();
    toast("Locked: hold both", 2000);
    return;
  }
  if (backlight_ != Backlight::On) {  // screen was dimmed/off: this press just wakes it
    applyBrightness();
    return;
  }

  if (l == ButtonEvent::Long) {
    Serial.println(F("[button] LEFT long -> home"));
    goHome();
  } else if (l == ButtonEvent::Short) {
    Serial.println(F("[button] LEFT short"));
    current().onLeftShort(*this);
  }
  if (r == ButtonEvent::Long) {
    Serial.println(F("[button] RIGHT long"));
    current().onRightLong(*this);
  } else if (r == ButtonEvent::Short) {
    Serial.println(F("[button] RIGHT short"));
    current().onRightShort(*this);
  }
}

void Shell::loop() {
  const uint32_t now = millis();
  handleButtons();
  pollConsole();

  if (now - lastPowerRead_ >= 1000) {
    lastPowerRead_ = now;
    const bool wasUsb = power_.usb;
    power_ = gauge_.update(power::read());
    life_.update(now, power_.batteryPresent ? power_.percent : -1, power_.usb);
    if (power_.usb && !wasUsb) jerry_.flash(Mood::Approved, 1500);  // "thanks for the juice"
  }
  if (now - lastLog_ >= 10000) {
    lastLog_ = now;
    Serial.printf("[status] %s | batt %.2fV %s | usb %d chg %d | mood %s | ble %d nearby, "
                  "%lu adverts, %lu tracker | wifi %s %d near, %lu seen, log %lu B | up %lus\n",
                  current().tag(), power_.battV,
                  power_.batteryPresent ? (String(power_.percent) + "%").c_str() : "none?",
                  power_.usb, power_.charging, moodName(jerry_.mood()), ble::nearbyDevices(),
                  (unsigned long)ble::advertsHeard(), (unsigned long)ble::trackerAdverts(),
                  wifi::scanning() ? "on" : "off", wifi::nearby(), (unsigned long)wifi::seenThisBoot(),
                  (unsigned long)wifi::logBytes(), now / 1000);
  }

  // Tracker Watch: take in what the radio heard, check for new alerts.
  ble::drain(tracker_, attack_, cams_, rig_);
  {  // The rig box's GPS tags Wi-Fi log lines while its fix is fresh.
    int32_t lat, lon;
    const bool loc = rig_.location(now, lat, lon);
    wifi::setLocation(loc, lat, lon);
  }
  {  // Cameras heard over Wi-Fi traffic (between scans) and Bluetooth
    uint8_t mac[6];
    int8_t rssi;
    bool flock;
    int news = ble::takeNewCameras();  // Bluetooth matches are all Flock gear
    while (wifi::takeCameraFrame(mac, rssi, flock)) {
      const CamMatch m = flock ? CamMatch{CamKind::FlockCamera, true, "Flock WiFi"}
                               : CamMatch{CamKind::Hint, false, cameraMaker(mac)};
      if (cams_.onSighting(mac, m, rssi, now) && flock) news++;  // hints stay quiet
    }
    if (news) {
      Serial.println(F("[cams] Flock camera gear nearby"));
      toast("Flock camera nearby!");
      jerry_.flash(Mood::Scanning, 3000);
    }
  }
  if (tracker_.tick(now, settings_.alertMinutes())) {
    Serial.println(F("[tracker] ALERT: a tracker has been following for the whole alert window"));
    lastInput_ = now;  // wake Jerry (and the screen) up
    if (&current() != &trackerScreen()) open(trackerScreen());
  }
  checkFollowers(now);
  updateLed(now);
  // Attacks: Wi-Fi kick-off frames heard (once a second), then any new threat?
  if (now - lastKickoffRead_ >= 1000) {
    lastKickoffRead_ = now;
    attack_.onKickoffs(wifi::takeKickoffs(), now);
  }
  if (attack_.tick(now)) {
    Serial.println(F("[shield] ALERT: radio attack nearby"));
    lastInput_ = now;  // wake the screen
    if (&current() != &shieldScreen()) open(shieldScreen());
  }
  updateExtras(now);
  updateBacklight(now);

  jerry_.update(now);
  current().update(*this, now);

  if (now - lastFrame_ < 33) return;  // ~30 frames per second
  if (backlight_ == Backlight::Off) return;  // nobody can see it: don't draw
  lastFrame_ = now;
  frame_.fillScreen(theme::BARK);
  if (current().fullScreen()) {  // e.g. the boot intro: no bars
    current().draw(*this, frame_, 0, theme::H);
    frame_.pushSprite(0, 0);
    return;
  }
  current().draw(*this, frame_, theme::CONTENT_Y, theme::CONTENT_H);
  drawToast(frame_);
  drawStatusBar(frame_);
  drawHintBar(frame_, current().hints());
  frame_.pushSprite(0, 0);
}

// Top bar: mode indicator on the left, battery on the right.
void Shell::drawStatusBar(lgfx::LovyanGFX &g) {
  using namespace theme;
  g.fillRect(0, 0, W, STATUS_H, BAR);
  g.setFont(&fonts::Font0);
  g.setTextSize(1);
  g.setTextColor(GOLD);
  g.setCursor(3, 2);
  g.print(current().tag());
  if (anyAlert() && (millis() / 500) % 2) {  // blinking "!" = tracker / follower alert
    const int x = 3 + g.textWidth(current().tag()) + 3;
    g.fillRoundRect(x, 1, 9, 10, 2, ALERT);
    g.setTextColor(TFT_WHITE);
    g.setCursor(x + 2, 2);
    g.print('!');
  }

  // Padlock = pocket mode.
  if (pocket_) {
    const int x = W - 62;
    g.drawRoundRect(x + 1, 1, 5, 6, 2, CREAM);
    g.fillRect(x, 4, 7, 6, CREAM);
  }

  // Red dot = Wi-Fi log recording.
  if (wifi::recording() && (millis() / 700) % 3) g.fillCircle(W - 50, 6, 3, ALERT);

  // Battery: icon + percent, or "USB" when there's no battery.
  const int bx = W - 18, by = 2;
  if (!power_.batteryPresent) {
    g.setTextColor(CREAM);
    g.setCursor(W - 21, 2);
    g.print("USB");
    return;
  }
  const int pct = power_.percent;
  const uint16_t fill = power_.charging ? GOLD : (pct <= 15 ? ALERT : GOOD);
  g.drawRect(bx, by, 14, 8, CREAM);
  g.fillRect(bx + 14, by + 2, 2, 4, CREAM);
  g.fillRect(bx + 2, by + 2, max(1, 10 * pct / 100), 4, fill);
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  g.setTextColor(CREAM);
  g.setCursor(bx - 3 - g.textWidth(buf), 2);
  g.print(buf);
}

// Bottom bar: what the buttons do on this screen.
//   [L] = tap left    [R] = tap right    filled R = hold right
static int drawChip(lgfx::LovyanGFX &g, int x, char letter, bool hold, const char *label) {
  using namespace theme;
  const int y = theme::H - theme::HINT_H + 2;
  if (hold) {
    g.fillRoundRect(x, y, 9, 9, 2, CREAM);
    g.setTextColor(BAR);
  } else {
    g.drawRoundRect(x, y, 9, 9, 2, CREAM);
    g.setTextColor(CREAM);
  }
  g.setCursor(x + 2, y + 1);
  g.print(letter);
  g.setTextColor(CREAM);
  g.setCursor(x + 11, y + 1);
  g.print(label);
  return x + 11 + g.textWidth(label) + 5;
}

void Shell::drawHintBar(lgfx::LovyanGFX &g, const Hints &h) {
  using namespace theme;
  g.fillRect(0, H - HINT_H, W, HINT_H, BAR);
  g.setFont(&fonts::Font0);
  g.setTextSize(1);
  if (pocket_) {  // buttons are locked: say how to unlock instead
    g.setTextColor(CREAM);
    g.setCursor(3, H - HINT_H + 3);
    g.print("locked: hold L+R");
    return;
  }
  int x = 2;
  if (h.tapL) x = drawChip(g, x, 'L', false, h.tapL);
  if (h.tapR) x = drawChip(g, x, 'R', false, h.tapR);
  if (h.holdR) drawChip(g, x, 'R', true, h.holdR);
}
