// screens.cpp - every JerryOS v0 screen.
//
//   Home ─ L ─> Menu ─┬─> Tracker Watch   (Checkpoint 4)
//                     ├─> Rig Dashboard   (coming soon)
//                     └─> Settings ─┬─> Diagnostics
//                                   └─> About
//
// Buttons everywhere: L tap = menu/back, R tap = next, R hold = select,
// L hold = home (handled by the shell).

#include "screens.h"

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
    shell.jerry().setMood(lowBatt || shell.idleMs() > 120000 ? Mood::Sleepy : Mood::Idle);
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) override {
    shell.jerry().draw(g, W / 2, y0 + h / 2, 0.95f);
  }
};

// ---------------------------------------------------------------------------
// Tracker Watch: placeholder until Checkpoint 4
// ---------------------------------------------------------------------------
class TrackerScreen : public InfoScreen {
 public:
  const char *tag() const override { return "TRACKER"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void update(Shell &shell, uint32_t) override { shell.jerry().setMood(Mood::Scanning); }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    drawJerrySmall(shell, g, y0);
    centered(g, y0 + 60, GOLD, "Tracker Watch");
    centered(g, y0 + 74, CREAM, "arrives at");
    centered(g, y0 + 86, CREAM, "Checkpoint 4");
  }
};

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
    snprintf(buf, sizeof(buf), "psram %u KB", ESP.getPsramSize() / 1024); row(buf);
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
  static constexpr int kLines = 16;
  const char *const kText[kLines] = {
      "*JerryOS v" JERRYOS_VERSION,
      "Jerry the Squatch",
      "Chief Gear Inspector",
      "5quatchrated.com",
      "",
      "*Built with",
      "LovyanGFX (lovyan03)",
      "Espressif ESP32",
      "PlatformIO",
      "",
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

TrackerScreen trackerScreen;
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
    {"Tracker Watch", &trackerScreen},
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
