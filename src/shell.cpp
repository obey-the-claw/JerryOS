// shell.cpp - runs the show: reads buttons, routes them to the current
// screen, and draws status bar + screen + hint bar ~30 times a second.

#include "shell.h"

#include "ble_scanner.h"
#include "screens.h"
#include "theme.h"

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
                settings_.sound ? "on" : "off");
  ble::begin();  // Tracker Watch listens in the background on every screen
  current().enter(*this);
  logScreen("boot");
}

void Shell::applyBrightness() { lcd_.setBrightness(settings_.brightnessPct() * 255 / 100); }

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
  if (l != ButtonEvent::None || r != ButtonEvent::None) lastInput_ = millis();

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

  if (now - lastPowerRead_ >= 1000) {
    lastPowerRead_ = now;
    const bool wasUsb = power_.usb;
    power_ = gauge_.update(power::read());
    if (power_.usb && !wasUsb) jerry_.flash(Mood::Approved, 1500);  // "thanks for the juice"
  }
  if (now - lastLog_ >= 10000) {
    lastLog_ = now;
    Serial.printf("[status] %s | batt %.2fV %s | usb %d chg %d | mood %s | ble %d nearby, "
                  "%lu adverts, %lu tracker | up %lus\n",
                  current().tag(), power_.battV,
                  power_.batteryPresent ? (String(power_.percent) + "%").c_str() : "none?",
                  power_.usb, power_.charging, moodName(jerry_.mood()), ble::nearbyDevices(),
                  (unsigned long)ble::advertsHeard(), (unsigned long)ble::trackerAdverts(), now / 1000);
  }

  // Tracker Watch: take in what the radio heard, check for new alerts.
  ble::drain(tracker_);
  if (tracker_.tick(now, settings_.alertMinutes())) {
    Serial.println(F("[tracker] ALERT: a tracker has been following for the whole alert window"));
    lastInput_ = now;  // wake Jerry up
    if (&current() != &trackerScreen()) open(trackerScreen());
  }

  jerry_.update(now);
  current().update(*this, now);

  if (now - lastFrame_ < 33) return;  // ~30 frames per second
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
