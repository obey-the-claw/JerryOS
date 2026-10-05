// screens.cpp - every JerryOS v0 screen.
//
//   Home ─ L ─> Menu ─┬─> Tracker Watch ──> tracker list (one kind)
//                     ├─> Rig Dashboard   (coming soon)
//                     └─> Settings ─┬─> Diagnostics
//                                   └─> About
//
// Buttons everywhere: L tap = menu/back, R tap = next, R hold = select,
// L hold = home (handled by the shell).

#include "screens.h"

#include "ble_scanner.h"
#include "shell.h"
#include "theme.h"

using namespace theme;

namespace {

// ---------------------------------------------------------------------------
// Small drawing helpers shared by the screens
// ---------------------------------------------------------------------------
void text(lgfx::LovyanGFX &g, int x, int y, uint16_t col, const char *s) {
  g.setTextColor(col);
  g.setCursor(x, y);
  g.print(s);
}

void centered(lgfx::LovyanGFX &g, int y, uint16_t col, const char *s) {
  text(g, (W - g.textWidth(s)) / 2, y, col, s);
}

// A vertical list with a gold highlight bar on the selected row.
void drawList(lgfx::LovyanGFX &g, int y0, const char *const *labels, const char *const *values,
              int count, int cursor, bool editing) {
  const int rowH = 18;
  for (int i = 0; i < count; i++) {
    const int y = y0 + 4 + i * rowH;
    const bool sel = i == cursor;
    if (sel) g.fillRoundRect(3, y, W - 6, rowH - 3, 3, editing ? CREAM : GOLD);
    const uint16_t fg = sel ? BARK : CREAM;
    text(g, 8, y + 4, fg, labels[i]);
    if (values && values[i]) {
      const char *v = values[i];
      const int vx = W - 8 - g.textWidth(v);
      if (sel && editing) {  // arrows show the value can be changed
        text(g, vx - 8, y + 4, fg, "<");
        text(g, vx, y + 4, fg, v);
      } else {
        text(g, vx, y + 4, sel ? BARK : GOLD, v);
      }
    }
  }
}

// Base for screens that show a little Jerry plus a few lines of text.
class InfoScreen : public Screen {
 protected:
  void drawJerrySmall(Shell &shell, lgfx::LovyanGFX &g, int y0) {
    shell.jerry().draw(g, W / 2, y0 + 27, 0.48f);
  }
};

// ---------------------------------------------------------------------------
// Home: Jerry's face
// ---------------------------------------------------------------------------
class HomeScreen : public Screen {
 public:
  const char *tag() const override { return "JERRY"; }
  Hints hints() const override { return {"menu", "poke", nullptr}; }

  void onLeftShort(Shell &shell) override { shell.open(menuScreen()); }
  void onRightShort(Shell &shell) override {
    shell.jerry().flash(Mood::Approved, 1500);  // a poke makes Jerry grin
  }

  void update(Shell &shell, uint32_t) override {
    // Jerry reacts: sleepy on low battery, or after 2 minutes of no buttons.
    const PowerStatus &p = shell.power();
    const bool lowBatt = p.batteryPresent && !p.usb && p.percent <= 15;
    if (shell.tracker().anyAlert()) shell.jerry().setMood(Mood::Alert);
    else shell.jerry().setMood(lowBatt || shell.idleMs() > 120000 ? Mood::Sleepy : Mood::Idle);
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) override {
    shell.jerry().draw(g, W / 2, y0 + h / 2, 0.95f);
  }
};

// ---------------------------------------------------------------------------
// Tracker Watch
// ---------------------------------------------------------------------------
// One row per tracker kind: how many separated ones are around right now,
// and how long that kind has kept up with you. Hold R on a row to see the
// individual devices; on an alerting row, hold R to acknowledge ("ok").
Screen &trackerList(TrackerKind k);  // the per-kind device list, below

const char *ago(char *buf, size_t n, int minutes) {
  if (minutes <= 0) snprintf(buf, n, "-");
  else if (minutes < 60) snprintf(buf, n, "%dm", minutes);
  else snprintf(buf, n, "%dh%02d", minutes / 60, minutes % 60);
  return buf;
}

class TrackerScreen : public Screen {
 public:
  const char *tag() const override { return "TRACKER"; }
  Hints hints() const override {
    return {"back", "next", rowAlerting_ ? "ok" : "list"};
  }
  void onRightShort(Shell &) override { cursor_ = (cursor_ + 1) % TRACKER_KINDS; }
  void onRightLong(Shell &shell) override {
    const TrackerKind k = static_cast<TrackerKind>(cursor_);
    if (shell.tracker().summary(k, millis()).alerting) {
      shell.tracker().acknowledge(k, millis(), shell.settings().alertMinutes());
      Serial.printf("[tracker] %s alert acknowledged for %u min\n", trackerName(k),
                    shell.settings().alertMinutes());
      return;
    }
    shell.open(trackerList(k));
  }

  void update(Shell &shell, uint32_t now) override {
    TrackerWatch &w = shell.tracker();
    // Jerry's face says it all: alert, scanning (separated trackers around
    // but not following yet), or approved (all clear).
    shell.jerry().setMood(w.anyAlert() ? Mood::Alert
                          : w.anySeparatedNow(now) ? Mood::Scanning
                                                   : Mood::Approved);
    // Jump the cursor to an alerting row so "hold R = ok" is one press away.
    for (int k = 0; k < TRACKER_KINDS; k++)
      if (w.summary(static_cast<TrackerKind>(k), now).alerting && !jumped_) {
        cursor_ = k;
        jumped_ = true;
      }
    if (!w.anyAlert()) jumped_ = false;
    rowAlerting_ = w.summary(static_cast<TrackerKind>(cursor_), now).alerting;
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    TrackerWatch &w = shell.tracker();
    shell.jerry().draw(g, 22, y0 + 20, 0.36f);

    // Headline
    const bool alert = w.anyAlert();
    const bool around = w.anySeparatedNow(now);
    text(g, 46, y0 + 3, alert ? ALERT : around ? GOLD : GOOD,
         alert ? "FOLLOWED?" : around ? "WATCHING" : "ALL CLEAR");
    char buf[24];
    snprintf(buf, sizeof buf, "%d BLE nearby", ble::nearbyDevices());
    text(g, 46, y0 + 15, CREAM, buf);
    snprintf(buf, sizeof buf, "alert at %um", shell.settings().alertMinutes());
    text(g, 46, y0 + 26, DIM, buf);

    // One row per kind
    for (int k = 0; k < TRACKER_KINDS; k++) {
      const TrackerKind kind = static_cast<TrackerKind>(k);
      const KindSummary sm = w.summary(kind, now);
      const int y = y0 + 40 + k * 12;
      const bool sel = k == cursor_;
      uint16_t bg = sm.alerting ? ALERT : sel ? GOLD : BARK;
      if (sm.alerting || sel) g.fillRoundRect(2, y - 1, W - 4, 11, 2, bg);
      if (sm.alerting && sel) g.drawRoundRect(2, y - 1, W - 4, 11, 2, GOLD);
      const uint16_t fg = (sm.alerting || sel) ? (sm.alerting ? TFT_WHITE : BARK) : CREAM;
      text(g, 6, y + 1, fg, trackerName(kind));
      snprintf(buf, sizeof buf, "%d", sm.devicesNow);
      text(g, 68, y + 1, sm.devicesNow ? fg : (sel ? BARK : DIM), buf);
      ago(buf, sizeof buf, sm.streakMin);
      text(g, W - 6 - g.textWidth(buf), y + 1, fg, buf);
    }
  }

 private:
  int cursor_ = 0;
  bool jumped_ = false;
  bool rowAlerting_ = false;
};

// Individual devices of one kind: short address, signal, how long seen.
class TrackerListScreen : public Screen {
 public:
  void setKind(TrackerKind k) { kind_ = k; }
  const char *tag() const override { return trackerName(kind_); }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    const TrackedDevice *list[8];
    const int n = shell.tracker().listDevices(kind_, now, list, 8);
    text(g, 4, y0 + 3, DIM, "addr   sig  seen");
    if (n == 0) centered(g, y0 + 44, CREAM, "none seen (10 min)");
    for (int i = 0; i < n; i++) {
      const TrackedDevice &d = *list[i];
      const int y = y0 + 15 + i * 11;
      const bool stale = now - d.lastSeenMs > TrackerWatch::RECENT_MS;
      char buf[28], seen[10];
      // Bluetooth addresses arrive least-significant byte first: show the
      // last two bytes as they'd be printed, e.g. "c4:3a".
      snprintf(buf, sizeof buf, "%02x:%02x %4d %s", d.addr[1], d.addr[0], d.rssi,
               ago(seen, sizeof seen, (int)((d.lastSeenMs - d.firstSeenMs) / 60000) + 1));
      text(g, 4, y, stale ? DIM : (d.separated ? CREAM : DIM), buf);
      if (!d.separated) text(g, W - 22, y, DIM, "own");
    }
  }

 private:
  TrackerKind kind_ = TrackerKind::AirTag;
};

TrackerListScreen trackerListObj;
Screen &trackerList(TrackerKind k) {
  trackerListObj.setKind(k);
  return trackerListObj;
}

// ---------------------------------------------------------------------------
// Rig Dashboard: "coming soon" until the Scanner v0 link exists
// ---------------------------------------------------------------------------
class RigScreen : public InfoScreen {
 public:
  const char *tag() const override { return "RIG"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void update(Shell &shell, uint32_t) override { shell.jerry().setMood(Mood::Scanning); }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    drawJerrySmall(shell, g, y0);
    centered(g, y0 + 58, GOLD, "Rig Dashboard");
    centered(g, y0 + 70, CREAM, "coming soon:");
    centered(g, y0 + 80, DIM, "GPS, sats, networks");
    centered(g, y0 + 90, DIM, "from Scanner v0");
  }
};

// ---------------------------------------------------------------------------
// Diagnostics: raw hardware readings (handy for the kit and for debugging)
// ---------------------------------------------------------------------------
class DiagScreen : public Screen {
 public:
  const char *tag() const override { return "DIAG"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const PowerStatus &p = shell.power();
    char buf[28];
    int y = y0 + 4;
    auto row = [&](const char *s) { text(g, 4, y, CREAM, s); y += 10; };
    snprintf(buf, sizeof(buf), "batt pin %lu mV", (unsigned long)p.pinMv); row(buf);
    snprintf(buf, sizeof(buf), "batt est %.2f V", p.battV); row(buf);
    snprintf(buf, sizeof(buf), "percent  %d", p.percent); row(buf);
    snprintf(buf, sizeof(buf), "usb %s  chg %s", p.usb ? "yes" : "no", p.charging ? "yes" : "no");
    row(buf);
    snprintf(buf, sizeof(buf), "heap %u KB", ESP.getFreeHeap() / 1024); row(buf);
    snprintf(buf, sizeof(buf), "ble %s %d near", ble::listening() ? "on" : "OFF", ble::nearbyDevices()); row(buf);
    snprintf(buf, sizeof(buf), "up %lu s", millis() / 1000); row(buf);
    snprintf(buf, sizeof(buf), "jerry %s", moodName(shell.jerry().mood())); row(buf);
    snprintf(buf, sizeof(buf), "JerryOS %s", JERRYOS_VERSION); row(buf);
  }
};

// ---------------------------------------------------------------------------
// About: version and credits (R scrolls)
// ---------------------------------------------------------------------------
class AboutScreen : public Screen {
 public:
  const char *tag() const override { return "ABOUT"; }
  Hints hints() const override { return {"back", "more", nullptr}; }
  void enter(Shell &) override { scroll_ = 0; }
  void onRightShort(Shell &) override { scroll_ = (scroll_ + 3) % kLines; }
  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int h) override {
    const int visible = (h - 4) / 10;
    for (int i = 0; i < visible && scroll_ + i < kLines; i++) {
      const char *s = kText[scroll_ + i];
      const uint16_t col = (s[0] == '*') ? GOLD : CREAM;
      centered(g, y0 + 4 + i * 10, col, s[0] == '*' ? s + 1 : s);
    }
  }

 private:
  static constexpr int kLines = 18;
  const char *const kText[kLines] = {
      "*JerryOS v" JERRYOS_VERSION,
      "Jerry the Squatch",
      "Chief Gear Inspector",
      "5quatchrated.com",
      "",
      "*Built with",
      "LovyanGFX (lovyan03)",
      "Espressif ESP32",
      "PlatformIO, NimBLE",
      "",
      "*Tracker research",
      "AirGuard (SEEMOO lab)",
      "*AIPI Lite research",
      "delacerda-mrd",
      "DaveMath (@GGDM)",
      "sticks918",
      "bcarroll",
      "Robert Lipe",
  };
  int scroll_ = 0;
};

DiagScreen diagScreen;
AboutScreen aboutScreen;

// ---------------------------------------------------------------------------
// Settings: brightness, alert window, sound, diagnostics, about
// ---------------------------------------------------------------------------
class SettingsScreen : public Screen {
 public:
  const char *tag() const override { return "SETTINGS"; }
  Hints hints() const override {
    if (editing_) return {"undo", "+", "save"};
    return {"back", "next", isValue(cursor_) ? "edit" : "open"};
  }

  void enter(Shell &) override { editing_ = false; }

  void onLeftShort(Shell &shell) override {
    if (!editing_) return shell.back();
    shell.settings() = before_;  // undo
    shell.applyBrightness();
    editing_ = false;
  }

  void onRightShort(Shell &shell) override {
    if (!editing_) {
      cursor_ = (cursor_ + 1) % kItems;
      return;
    }
    Settings &s = shell.settings();
    switch (cursor_) {
      case 0:
        s.brightnessIdx = (s.brightnessIdx + 1) % Settings::N_BRIGHTNESS;
        shell.applyBrightness();  // preview live
        break;
      case 1: s.alertIdx = (s.alertIdx + 1) % Settings::N_ALERT; break;
      case 2: s.sound = !s.sound; break;
    }
  }

  void onRightLong(Shell &shell) override {
    if (editing_) {
      shell.settings().save();
      editing_ = false;
      shell.jerry().flash(Mood::Approved, 1200);
      Serial.println(F("[settings] saved"));
      return;
    }
    if (isValue(cursor_)) {
      before_ = shell.settings();
      editing_ = true;
    } else if (cursor_ == 3) {
      shell.open(diagScreen);
    } else {
      shell.open(aboutScreen);
    }
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const Settings &s = shell.settings();
    char bright[8], alert[8];
    snprintf(bright, sizeof(bright), "%u%%", s.brightnessPct());
    snprintf(alert, sizeof(alert), "%um", s.alertMinutes());
    static const char *const labels[kItems] = {"Brightness", "Alert after", "Sound",
                                               "Diagnostics", "About"};
    const char *values[kItems] = {bright, alert, s.sound ? "on" : "off", nullptr, nullptr};
    drawList(g, y0, labels, values, kItems, cursor_, editing_);
  }

 private:
  static constexpr int kItems = 5;
  static bool isValue(int i) { return i <= 2; }
  int cursor_ = 0;
  bool editing_ = false;
  Settings before_;
};

TrackerScreen trackerScreenObj;
RigScreen rigScreen;
SettingsScreen settingsScreen;

// ---------------------------------------------------------------------------
// Menu: the list of modes. Add a mode here and it shows up.
// ---------------------------------------------------------------------------
struct ModeEntry {
  const char *name;
  Screen *screen;
};

const ModeEntry kModes[] = {
    {"Tracker Watch", &trackerScreenObj},
    {"Rig Dashboard", &rigScreen},
    {"Settings", &settingsScreen},
};
constexpr int kModeCount = sizeof(kModes) / sizeof(kModes[0]);

class MenuScreen : public Screen {
 public:
  const char *tag() const override { return "MENU"; }
  Hints hints() const override { return {"back", "next", "open"}; }
  void onRightShort(Shell &) override { cursor_ = (cursor_ + 1) % kModeCount; }
  void onRightLong(Shell &shell) override { shell.open(*kModes[cursor_].screen); }
  void update(Shell &shell, uint32_t) override { shell.jerry().setMood(Mood::Idle); }
  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override {
    const char *labels[kModeCount];
    for (int i = 0; i < kModeCount; i++) labels[i] = kModes[i].name;
    drawList(g, y0, labels, nullptr, kModeCount, cursor_, false);
  }

 private:
  int cursor_ = 0;
};

HomeScreen homeScreenObj;
MenuScreen menuScreenObj;

}  // namespace

Screen &homeScreen() { return homeScreenObj; }
Screen &menuScreen() { return menuScreenObj; }
Screen &trackerScreen() { return trackerScreenObj; }
