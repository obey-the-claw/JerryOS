// shell.cpp - runs the show: reads buttons, routes them to the current
// screen, and draws status bar + screen + hint bar ~30 times a second.

#include "shell.h"

#include "audio.h"
#include "ble_scanner.h"
#include "screens.h"
#include "theme.h"
#include "wifi_scanner.h"

void Screen::onLeftShort(Shell &shell) { shell.back(); }

Shell::Shell(JerryDisplay &lcd, Screen &home) : lcd_(lcd), frame_(&lcd) {
  stack_[0] = &home;
}

void Shell::begin() {
  power::holdOn();  // first: keep the board powered on battery
  power::begin();
  left_.begin();
  right_.begin();
  settings_.load();

  lcd_.init();
  lcd_.setRotation(1);
  applyBrightness();
  frame_.setColorDepth(16);
  if (!frame_.createSprite(theme::W, theme::H))
    Serial.println(F("!! could not allocate the frame buffer"));

  power_ = gauge_.update(power::read());
  lastInput_ = millis();
  Serial.printf("[shell] JerryOS %s up. brightness %u%%, alert window %u min, sound %s\n",
                JERRYOS_VERSION, settings_.brightnessPct(), settings_.alertMinutes(),
                Settings::volumeName(settings_.volumeIdx));
  audio::setVolume(settings_.volumeIdx);
  if (audio::begin()) audio::play(audio::Sound::Boot);
  ble::begin();  // Tracker Watch listens in the background on every screen
  wifi::setEnabled(settings_.wifiScan);
  wifi::begin();  // ...and so does the Wi-Fi scanner/logger
  current().enter(*this);
  logScreen("boot");
}

// Tiny USB command line. Type a command + Enter in a serial terminal:
//   wifi dump   print the whole Wi-Fi log (tools/pull_wifi_log.py does this)
//   wifi clear  erase the Wi-Fi log
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
    else if (cmd_[0]) Serial.println(F("[console] commands: wifi dump | wifi clear | help"));
  }
}

void Shell::applyBrightness() {
  backlight_ = Backlight::On;
  lcd_.setBrightness(settings_.brightnessPct() * 255 / 100);
}

// On battery, save power when nobody's looking: dim after 1 min without a
// button press, backlight off after 5 min. On USB the screen stays on.
// A tracker alert wakes the screen (it resets the idle timer).
void Shell::updateBacklight(uint32_t now) {
  const uint32_t idle = now - lastInput_;
  Backlight want = Backlight::On;
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

// Beeps that aren't tied to a button: tracker alerts, low battery.
void Shell::soundEvents(uint32_t now) {
  if (tracker_.anyAlert() && now - lastAlertSound_ >= 60000) {  // repeat each minute until "ok"
    lastAlertSound_ = now;
    audio::play(audio::Sound::Alert);
  }
  if (!tracker_.anyAlert()) lastAlertSound_ = now - 60000;  // next alert beeps straight away
  const bool low = power_.batteryPresent && !power_.usb && power_.percent <= 15;
  if (low && !lowBattWarned_) {
    lowBattWarned_ = true;
    audio::play(audio::Sound::LowBatt);
  }
  if (power_.usb) lowBattWarned_ = false;
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

void Shell::handleButtons() {
  const ButtonEvent l = left_.update();
  const ButtonEvent r = right_.update();
  if (l == ButtonEvent::None && r == ButtonEvent::None) return;
  lastInput_ = millis();
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
  ble::drain(tracker_);
  if (tracker_.tick(now, settings_.alertMinutes())) {
    Serial.println(F("[tracker] ALERT: a tracker has been following for the whole alert window"));
    lastInput_ = now;  // wake Jerry (and the screen) up
    if (&current() != &trackerScreen()) open(trackerScreen());
  }
  soundEvents(now);
  updateBacklight(now);

  jerry_.update(now);
  current().update(*this, now);

  if (now - lastFrame_ < 33) return;  // ~30 frames per second
  if (backlight_ == Backlight::Off) return;  // nobody can see it: don't draw
  lastFrame_ = now;
  frame_.fillScreen(theme::BARK);
  current().draw(*this, frame_, theme::CONTENT_Y, theme::CONTENT_H);
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
  if (tracker_.anyAlert() && (millis() / 500) % 2) {  // blinking "!" = tracker alert
    const int x = 3 + g.textWidth(current().tag()) + 3;
    g.fillRoundRect(x, 1, 9, 10, 2, ALERT);
    g.setTextColor(TFT_WHITE);
    g.setCursor(x + 2, 2);
    g.print('!');
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
  int x = 2;
  if (h.tapL) x = drawChip(g, x, 'L', false, h.tapL);
  if (h.tapR) x = drawChip(g, x, 'R', false, h.tapR);
  if (h.holdR) drawChip(g, x, 'R', true, h.holdR);
}
