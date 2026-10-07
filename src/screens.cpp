// screens.cpp - every JerryOS v0 screen.
//
//   Home ─ L ─> Menu ─┬─> Tracker Watch ──> tracker list (one kind)
//                     ├─> Followers ──────> one follower ("mine")
//                     ├─> Shield           (Wi-Fi kick-offs, BT spam, fake networks)
//                     ├─> Cameras          (Flock licence-plate cameras + hidden-cam hints)
//                     ├─> BLE Nearby ─────> one device's details
//                     ├─> WiFi Scan ──────> one network's details
//                     ├─> Rig Dashboard   (coming soon)
//                     └─> Settings ─┬─> Diagnostics
//                                   └─> About
//
// Buttons everywhere: L tap = menu/back, R tap = next, R hold = select,
// L hold = home (handled by the shell).

#include "screens.h"

#include <ctype.h>
#include <string.h>

#include "ble_scanner.h"
#include "shell.h"
#include "status_led.h"
#include "theme.h"
#include "wifi_scanner.h"

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

// A vertical list with a gold highlight bar on the selected row. Shows 5
// rows at a time and scrolls to keep the selected one on screen.
void drawList(lgfx::LovyanGFX &g, int y0, const char *const *labels, const char *const *values,
              int count, int cursor, bool editing) {
  const int rowH = 18, visible = (CONTENT_H - 4) / rowH;
  const int top = cursor < visible ? 0 : cursor - visible + 1;
  if (top + visible < count) g.fillTriangle(W / 2 - 3, y0 + CONTENT_H - 3, W / 2 + 3,
                                            y0 + CONTENT_H - 3, W / 2, y0 + CONTENT_H, DIM);
  for (int i = top; i < count && i < top + visible; i++) {
    const int y = y0 + 4 + (i - top) * rowH;
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
// Jerry, plus everything at a glance along the bottom:
//   [TAG ok] [BT 18] [WiFi 12]     (or [REC 34] while saving the Wi-Fi log)
// Tap R = poke (Jerry says something), hold R = start/stop recording.
class HomeScreen : public Screen {
 public:
  const char *tag() const override { return "JERRY"; }
  Hints hints() const override { return {"menu", "poke", wifi::recording() ? "stop" : "rec"}; }

  void onLeftShort(Shell &shell) override { shell.open(menuScreen()); }

  void onRightShort(Shell &shell) override {
    shell.jerry().flash(Mood::Approved, 1500);  // a poke makes Jerry grin...
    quip(shell);                                 // ...and talk
  }

  void onRightLong(Shell &shell) override {
    if (wifi::recording()) {
      const unsigned long n = wifi::savedThisSession();
      wifi::setRecording(false);
      say(n == 1 ? "Saved 1 network" : "Saved %lu networks", n);
    } else if (!wifi::enabled()) {
      say("WiFi scan is off");
    } else if (wifi::logFull()) {
      say("Log full! Pull it.");
    } else {
      wifi::setRecording(true);
      shell.jerry().flash(Mood::Approved, 1200);
      say("Recording WiFi...");
    }
  }

  void update(Shell &shell, uint32_t) override {
    // Jerry reacts: shocked when followed, sleepy on low battery or after
    // 2 minutes of no buttons, eyes sweeping while he records.
    const PowerStatus &p = shell.power();
    const bool lowBatt = p.batteryPresent && !p.usb && p.percent <= 15;
    if (shell.anyAlert()) shell.jerry().setMood(Mood::Alert);
    else if (lowBatt || shell.idleMs() > 120000) shell.jerry().setMood(Mood::Sleepy);
    else if (wifi::recording()) shell.jerry().setMood(Mood::Scanning);
    else shell.jerry().setMood(Mood::Idle);
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) override {
    shell.jerry().draw(g, W / 2, y0 + h / 2 - 10, 0.76f, /*shoulders=*/true);
    drawPills(shell, g, y0 + h - 13);
    if ((int32_t)(millis() - bubbleUntil_) < 0) drawBubble(g, y0 + 1);
  }

 private:
  // Small rounded labels along the bottom.
  struct Pill {
    char text[12];
    uint16_t fg, bg;
    void set(uint16_t f, uint16_t b, const char *fmt, unsigned long n = 0) {
      fg = f;
      bg = b;
      snprintf(text, sizeof text, fmt, n);
    }
  };

  void drawPills(Shell &shell, lgfx::LovyanGFX &g, int y) {
    const uint32_t now = millis();
    Pill pills[3];
    TrackerWatch &w = shell.tracker();
    // Safety at a glance: trackers + followers.
    if (shell.anyAlert()) pills[0].set(TFT_WHITE, ALERT, "ALERT");
    else if (w.anySeparatedNow(now) || shell.follow().flaggedCount() || shell.attacks().anyActive() ||
             shell.cameras().nearbyFlock(now) ||
             shell.follow().candidates(now) >= 3)
      pills[0].set(BARK, GOLD, "WATCH");
    else pills[0].set(GOOD, INK, "SAFE");
    pills[1].set(CREAM, INK, "BT %lu", (unsigned long)ble::nearbyDevices());
    if (wifi::recording()) pills[2].set(TFT_WHITE, ALERT, "REC %lu", (unsigned long)wifi::savedThisSession());
    else if (!wifi::enabled()) pills[2].set(DIM, INK, "WiFi off");
    else pills[2].set(CREAM, INK, "WiFi %lu", (unsigned long)wifi::nearby());
    int total = 0;
    for (const Pill &p : pills) total += g.textWidth(p.text) + 6;
    if (total + 6 > W) {  // big numbers: squeeze the labels
      snprintf(pills[1].text, sizeof pills[1].text, "B%d", ble::nearbyDevices());
      if (!wifi::recording() && wifi::enabled())
        snprintf(pills[2].text, sizeof pills[2].text, "W%d", wifi::nearby());
      total = 0;
      for (const Pill &p : pills) total += g.textWidth(p.text) + 6;
    }
    const int gap = 3;
    int x = (W - total - 2 * gap) / 2;
    for (const Pill &p : pills) {
      const int pw = g.textWidth(p.text) + 6;
      g.fillRoundRect(x, y, pw, 11, 3, p.bg);
      text(g, x + 3, y + 2, p.fg, p.text);
      x += pw + gap;
    }
  }

  // Jerry's speech bubble across the top.
  void say(const char *fmt, unsigned long n = 0) {
    snprintf(bubble_, sizeof bubble_, fmt, n);
    bubbleUntil_ = millis() + 2500;
  }

  void drawBubble(lgfx::LovyanGFX &g, int y) {
    const int tw = g.textWidth(bubble_);
    const int bw = tw + 8, bx = (W - bw) / 2;
    g.fillRoundRect(bx, y, bw, 12, 4, CREAM);
    g.fillTriangle(W / 2 + 6, y + 11, W / 2 + 14, y + 11, W / 2 + 8, y + 16, CREAM);  // tail
    text(g, bx + 4, y + 2, BARK, bubble_);
  }

  void quip(Shell &shell) {
    const uint32_t now = millis();
    if (shell.anyAlert()) return say("Being followed?!");
    static const char *const kLines[] = {
        "Gear approved!",   "%lu gadgets nearby", "Inspector on duty", "%lu WiFi nets here",
        "Squatch mode: ON", "No trackers. Nice.", "Leave no trace!",   "I'm not blurry.",
        "Stay wild.",       "Believe.",
    };
    constexpr int n = sizeof kLines / sizeof kLines[0];
    for (int tries = 0; tries < n; tries++) {
      const char *line = kLines[next_++ % n];
      if (line == kLines[5] && shell.tracker().anySeparatedNow(now)) continue;  // not true now
      if (line == kLines[1]) return say(line, (unsigned long)ble::nearbyDevices());
      if (line == kLines[3]) {
        if (!wifi::enabled()) continue;
        return say(line, (unsigned long)wifi::nearby());
      }
      return say(line);
    }
  }

  char bubble_[24] = "";
  uint32_t bubbleUntil_ = 0;
  unsigned next_ = 0;
};

// ---------------------------------------------------------------------------
// Tracker Watch
// ---------------------------------------------------------------------------
// One row per tracker kind: how many separated ones are around right now
// (plus a dim "+N" for ones still with their owner, which never alert), and
// how long that kind has kept up with you. Hold R on a row to see the
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
      if (sm.withOwner) {  // with their owner: shown, but they never alert
        snprintf(buf, sizeof buf, "+%d", sm.withOwner);
        text(g, 78, y + 1, (sm.alerting || sel) ? fg : DIM, buf);
      }
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
// BLE Nearby: every Bluetooth LE device heard in the last minute
// ---------------------------------------------------------------------------
// Strongest signal (closest) first. Labels come only from what each device
// broadcasts on its own: its name if it shares one, else its type or maker.
// The list re-sorts every 2 s (not every frame) so it doesn't jitter.

// Four little signal bars, like a phone: -60 dBm or better = 4 bars.
void signalBars(lgfx::LovyanGFX &g, int x, int y, int rssi, uint16_t on, uint16_t off) {
  const int bars = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -80 ? 2 : 1;
  for (int b = 0; b < 4; b++) {
    const int h = 2 + b * 2;
    g.fillRect(x + b * 3, y + 8 - h, 2, h, b < bars ? on : off);
  }
}

// Fit a label in `width` pixels, cutting it short with a '.' if needed.
void fitText(lgfx::LovyanGFX &g, int x, int y, int width, uint16_t col, const char *s) {
  char buf[24];
  snprintf(buf, sizeof buf, "%s", s);
  size_t n = strlen(buf);
  while (n > 1 && g.textWidth(buf) > width) {
    buf[--n] = 0;
    buf[n - 1] = '.';
  }
  text(g, x, y, col, buf);
}

Screen &nearbyDetail(const NearbyDevice &d);  // one device, below

class NearbyScreen : public Screen {
 public:
  const char *tag() const override { return "NEARBY"; }
  Hints hints() const override { return {"back", n_ ? "next" : nullptr, n_ ? "info" : nullptr}; }
  void enter(Shell &) override { refresh(); }
  void onRightShort(Shell &) override {
    if (n_) cursor_ = (cursor_ + 1) % n_;
    if (n_) memcpy(selAddr_, list_[cursor_].addr, 6);
  }
  void onRightLong(Shell &shell) override {
    if (n_) shell.open(nearbyDetail(list_[cursor_]));
  }

  void update(Shell &shell, uint32_t now) override {
    shell.jerry().setMood(Mood::Scanning);
    if (now - lastRefresh_ >= 2000) refresh();
  }

  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override {
    char buf[28];
    snprintf(buf, sizeof buf, "%d heard, by signal", n_);
    text(g, 4, y0 + 3, DIM, buf);
    if (!n_) {
      centered(g, y0 + 44, CREAM, "listening...");
      return;
    }
    // Scroll so the cursor stays on screen.
    const int top = cursor_ < kRows ? 0 : cursor_ - kRows + 1;
    for (int r = 0; r < kRows && top + r < n_; r++) {
      const int i = top + r;
      const NearbyDevice &d = list_[i];
      const int y = y0 + 14 + r * 11;
      const bool sel = i == cursor_;
      if (sel) g.fillRoundRect(2, y - 1, W - 4, 11, 2, GOLD);
      signalBars(g, 5, y, d.rssi, sel ? BARK : GOOD, sel ? FUR_DARK : BAR);
      const uint16_t col = sel ? BARK : d.labelRank >= 2 ? CREAM : DIM;  // names/types bright
      fitText(g, 20, y + 1, W - 24, col, d.label);
    }
  }

 private:
  static constexpr int kRows = 8;
  static constexpr int kMax = NearbyTable::MAX;

  void refresh() {
    lastRefresh_ = millis();
    n_ = ble::nearbySnapshot(list_, kMax);
    // Keep the highlight on the same device after re-sorting.
    cursor_ = 0;
    for (int i = 0; i < n_; i++)
      if (memcmp(list_[i].addr, selAddr_, 6) == 0) cursor_ = i;
    if (n_) memcpy(selAddr_, list_[cursor_].addr, 6);
  }

  NearbyDevice list_[kMax];
  int n_ = 0, cursor_ = 0;
  uint8_t selAddr_[6] = {};
  uint32_t lastRefresh_ = 0;
};

// One device in detail: label, full address, signal, how long it's been around.
class NearbyDetailScreen : public Screen {
 public:
  void set(const NearbyDevice &d) { dev_ = d; }
  const char *tag() const override { return "DEVICE"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void update(Shell &shell, uint32_t now) override {
    shell.jerry().setMood(Mood::Scanning);
    if (now - lastRefresh_ < 1000) return;  // follow the live signal once a second
    lastRefresh_ = now;
    static NearbyDevice all[NearbyTable::MAX];
    const int n = ble::nearbySnapshot(all, NearbyTable::MAX);
    gone_ = true;
    for (int i = 0; i < n; i++)
      if (memcmp(all[i].addr, dev_.addr, 6) == 0) {
        dev_ = all[i];
        gone_ = false;
      }
  }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    shell.jerry().draw(g, 18, y0 + 16, 0.3f);
    fitText(g, 36, y0 + 5, W - 40, GOLD, dev_.label);
    static const char *const kWhat[] = {"no clues sent", "maker only", "device type",
                                        "its own name"};
    text(g, 36, y0 + 17, DIM, kWhat[dev_.labelRank > 3 ? 0 : dev_.labelRank]);

    char buf[28], seen[10];
    const uint8_t *a = dev_.addr;  // stored backwards; print the usual way round
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
    text(g, 4, y0 + 38, CREAM, buf);
    snprintf(buf, sizeof buf, "signal %d dBm", dev_.rssi);
    text(g, 4, y0 + 52, CREAM, buf);
    signalBars(g, W - 18, y0 + 51, dev_.rssi, GOOD, BAR);
    snprintf(buf, sizeof buf, "around %s",
             ago(seen, sizeof seen, (int)((dev_.lastMs - dev_.firstMs) / 60000) + 1));
    text(g, 4, y0 + 66, CREAM, buf);
    if (gone_) text(g, 4, y0 + 80, ALERT, "not heard for 1 min");
    else text(g, 4, y0 + 80, DIM, "addr can rotate (ok)");
  }

 private:
  NearbyDevice dev_ = {};
  uint32_t lastRefresh_ = 0;
  bool gone_ = false;
};

NearbyDetailScreen nearbyDetailObj;
Screen &nearbyDetail(const NearbyDevice &d) {
  nearbyDetailObj.set(d);
  return nearbyDetailObj;
}

// ---------------------------------------------------------------------------
// WiFi Scan: networks heard (listen-only), strongest first, + the logger
// ---------------------------------------------------------------------------
void logSize(char *buf, size_t n) {
  const uint32_t b = wifi::logBytes();
  if (b < 1024 * 1024) snprintf(buf, n, "%luK", (unsigned long)((b + 1023) / 1024));
  else snprintf(buf, n, "%lu.%luM", (unsigned long)(b >> 20), (unsigned long)((b % (1 << 20)) * 10 >> 20));
}

Screen &wifiDetail(const WifiNet &n);  // one network, below

class WifiScreen : public Screen {
 public:
  const char *tag() const override { return "WIFI"; }
  Hints hints() const override { return {"back", n_ ? "next" : nullptr, n_ ? "info" : nullptr}; }
  void enter(Shell &) override { refresh(); }
  void onRightShort(Shell &) override {
    if (!n_) return;
    cursor_ = (cursor_ + 1) % n_;
    memcpy(sel_, list_[cursor_].bssid, 6);
  }
  void onRightLong(Shell &shell) override {
    if (n_) shell.open(wifiDetail(list_[cursor_]));
  }
  void update(Shell &shell, uint32_t now) override {
    shell.jerry().setMood(Mood::Scanning);
    if (now - lastRefresh_ >= 2000) refresh();
  }

  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override {
    char buf[28], size[10];
    logSize(size, sizeof size);
    const bool rec = wifi::recording();
    if (!wifi::enabled()) snprintf(buf, sizeof buf, "scan OFF (Settings)");
    else if (wifi::logFull()) snprintf(buf, sizeof buf, "%d nets  log FULL", n_);
    else if (rec) snprintf(buf, sizeof buf, "%d nets  REC %lu", n_, (unsigned long)wifi::savedThisSession());
    else snprintf(buf, sizeof buf, "%d nets  log %s", n_, size);
    if (rec) g.fillCircle(W - 7, y0 + 6, 3, ALERT);
    text(g, 4, y0 + 3, (wifi::logFull() || rec) ? ALERT : DIM, buf);
    if (!n_) {
      centered(g, y0 + 44, CREAM, wifi::enabled() ? "listening..." : "Wi-Fi scan is off");
      return;
    }
    const int top = cursor_ < kRows ? 0 : cursor_ - kRows + 1;
    for (int r = 0; r < kRows && top + r < n_; r++) {
      const int i = top + r;
      const WifiNet &w = list_[i];
      const int y = y0 + 14 + r * 11;
      const bool sel = i == cursor_;
      if (sel) g.fillRoundRect(2, y - 1, W - 4, 11, 2, GOLD);
      signalBars(g, 5, y, w.rssi, sel ? BARK : GOOD, sel ? FUR_DARK : BAR);
      char name[34];
      displaySsid(w, name, sizeof name);
      const bool open = isOpen(w.auth);
      const int right = open ? W - 33 : W - 6;  // leave room for the "open" tag
      fitText(g, 20, y + 1, right - 20, sel ? BARK : w.ssid[0] ? CREAM : DIM, name);
      if (open) text(g, W - 28, y + 1, sel ? BARK : GOOD, "open");
    }
  }

 private:
  static constexpr int kRows = 8;
  static constexpr int kMax = 48;
  void refresh() {
    lastRefresh_ = millis();
    n_ = wifi::snapshot(list_, kMax);
    cursor_ = 0;
    for (int i = 0; i < n_; i++)
      if (memcmp(list_[i].bssid, sel_, 6) == 0) cursor_ = i;
    if (n_) memcpy(sel_, list_[cursor_].bssid, 6);
  }
  WifiNet list_[kMax];
  int n_ = 0, cursor_ = 0;
  uint8_t sel_[6] = {};
  uint32_t lastRefresh_ = 0;
};

// One network: name, security, channel, hardware address, signal now and best.
class WifiDetailScreen : public Screen {
 public:
  void set(const WifiNet &n) { net_ = n; }
  const char *tag() const override { return "NETWORK"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void update(Shell &shell, uint32_t now) override {
    shell.jerry().setMood(Mood::Scanning);
    if (now - lastRefresh_ < 1000) return;
    lastRefresh_ = now;
    static WifiNet all[WifiTable::MAX];
    const int n = wifi::snapshot(all, WifiTable::MAX);
    gone_ = true;
    for (int i = 0; i < n; i++)
      if (memcmp(all[i].bssid, net_.bssid, 6) == 0) {
        net_ = all[i];
        gone_ = false;
      }
  }
  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override {
    char buf[34], seen[10];
    displaySsid(net_, buf, sizeof buf);
    fitText(g, 4, y0 + 4, W - 8, GOLD, buf);
    snprintf(buf, sizeof buf, "%s  ch %u", authName(net_.auth), net_.channel);
    text(g, 4, y0 + 17, isOpen(net_.auth) ? GOOD : CREAM, buf);
    const uint8_t *b = net_.bssid;
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
    text(g, 4, y0 + 30, CREAM, buf);
    snprintf(buf, sizeof buf, "signal %d dBm", net_.rssi);
    text(g, 4, y0 + 46, CREAM, buf);
    signalBars(g, W - 18, y0 + 45, net_.rssi, GOOD, BAR);
    snprintf(buf, sizeof buf, "best %d dBm", net_.bestRssi);
    text(g, 4, y0 + 58, DIM, buf);
    snprintf(buf, sizeof buf, "around %s",
             ago(seen, sizeof seen, (int)((net_.lastMs - net_.firstMs) / 60000) + 1));
    text(g, 4, y0 + 72, CREAM, buf);
    if (gone_) text(g, 4, y0 + 86, ALERT, "out of range");
    else if (net_.loggedIn && net_.loggedIn == wifi::session() && wifi::recording())
      text(g, 4, y0 + 86, ALERT, "saved (REC)");
    else text(g, 4, y0 + 86, DIM, "not saved");
  }

 private:
  WifiNet net_ = {};
  uint32_t lastRefresh_ = 0;
  bool gone_ = false;
};

WifiDetailScreen wifiDetailObj;
Screen &wifiDetail(const WifiNet &n) {
  wifiDetailObj.set(n);
  return wifiDetailObj;
}

// ---------------------------------------------------------------------------
// Followers: any Bluetooth device that stays with you while you move
// ---------------------------------------------------------------------------
Screen &followerDetail(const Follower &f);  // one device, below

class FollowersScreen : public Screen {
 public:
  const char *tag() const override { return "FOLLOW"; }
  Hints hints() const override { return {"back", n_ ? "next" : nullptr, n_ ? "info" : nullptr}; }
  void enter(Shell &shell) override {
    shell.follow().acknowledgeAll();  // you're looking at it now
    cursor_ = 0;
  }
  void onRightShort(Shell &) override {
    if (n_) cursor_ = (cursor_ + 1) % n_;
  }
  void onRightLong(Shell &shell) override {
    if (n_) shell.open(followerDetail(*list_[cursor_]));
  }
  void update(Shell &shell, uint32_t) override {
    n_ = shell.follow().followers(list_, kMax);
    if (cursor_ >= n_) cursor_ = 0;
    shell.follow().acknowledgeAll();
    shell.jerry().setMood(n_ ? Mood::Alert : Mood::Approved);
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    char buf[28];
    if (!wifi::enabled()) {
      text(g, 4, y0 + 3, ALERT, "needs WiFi scan on");
    } else {
      const SceneTracker &sc = shell.scene();
      if (!sc.hasWifi()) snprintf(buf, sizeof buf, "no WiFi heard yet");
      else snprintf(buf, sizeof buf, "moved %lu times", (unsigned long)sc.place());
      text(g, 4, y0 + 3, DIM, buf);
    }
    if (!n_) {
      shell.jerry().draw(g, W / 2, y0 + 40, 0.42f);
      centered(g, y0 + 66, GOOD, "nobody following");
      const int c = shell.follow().candidates(now);
      snprintf(buf, sizeof buf, c ? "%d tagging along..." : "watching as you move", c);
      centered(g, y0 + 80, DIM, buf);
      return;
    }
    const int top = cursor_ < kRows ? 0 : cursor_ - kRows + 1;
    for (int r = 0; r < kRows && top + r < n_; r++) {
      const int i = top + r;
      const Follower &f = *list_[i];
      const int y = y0 + 14 + r * 11;
      const bool sel = i == cursor_;
      const bool here = now - f.lastMs < 60000;
      if (sel) g.fillRoundRect(2, y - 1, W - 4, 11, 2, GOLD);
      signalBars(g, 5, y, f.rssi, sel ? BARK : here ? ALERT : DIM, sel ? FUR_DARK : BAR);
      char mins[10];
      ago(mins, sizeof mins, (int)((f.lastMs - f.firstMs) / 60000));
      const int mw = g.textWidth(mins);
      fitText(g, 20, y + 1, W - 26 - mw, sel ? BARK : here ? CREAM : DIM, f.label);
      text(g, W - 5 - mw, y + 1, sel ? BARK : here ? ALERT : DIM, mins);
    }
  }

 private:
  static constexpr int kRows = 8;
  static constexpr int kMax = 16;
  const Follower *list_[kMax] = {};
  int n_ = 0, cursor_ = 0;
};

// One follower: what it is, how long, how many places. Hold R = "it's mine".
class FollowerDetailScreen : public Screen {
 public:
  void set(const Follower &f) { f_ = f; }
  const char *tag() const override { return "FOLLOWER"; }
  Hints hints() const override { return {"back", nullptr, "mine"}; }
  void onRightLong(Shell &shell) override {
    shell.follow().trust(f_.addr);  // never flag it again
    shell.saveTrusted();
    Serial.println(F("[follow] marked as mine"));
    shell.jerry().flash(Mood::Approved, 1200);
    shell.back();
  }
  void update(Shell &shell, uint32_t) override { shell.jerry().setMood(Mood::Alert); }
  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    char buf[28], t[10];
    fitText(g, 4, y0 + 4, W - 8, GOLD, f_.label);
    const uint8_t *a = f_.addr;
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
    text(g, 4, y0 + 18, CREAM, buf);
    snprintf(buf, sizeof buf, "with you %s", ago(t, sizeof t, (int)((f_.lastMs - f_.firstMs) / 60000)));
    text(g, 4, y0 + 32, CREAM, buf);
    snprintf(buf, sizeof buf, "at %u places", f_.places);
    text(g, 4, y0 + 44, CREAM, buf);
    snprintf(buf, sizeof buf, "signal %d dBm", f_.rssi);
    text(g, 4, y0 + 56, CREAM, buf);
    signalBars(g, W - 18, y0 + 55, f_.rssi, ALERT, BAR);
    text(g, 4, y0 + 70, now - f_.lastMs < 60000 ? ALERT : DIM,
         now - f_.lastMs < 60000 ? "still with you" : "not heard lately");
    text(g, 4, y0 + 84, DIM, "yours? hold R = mine");
  }

 private:
  Follower f_ = {};
};

FollowerDetailScreen followerDetailObj;
Screen &followerDetail(const Follower &f) {
  followerDetailObj.set(f);
  return followerDetailObj;
}

// ---------------------------------------------------------------------------
// Shield: Wi-Fi kick-offs, Bluetooth pop-up spam, fake networks
// ---------------------------------------------------------------------------
class ShieldScreen : public Screen {
 public:
  const char *tag() const override { return "SHIELD"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void enter(Shell &shell) override { shell.attacks().acknowledgeAll(); }
  void update(Shell &shell, uint32_t) override {
    shell.attacks().acknowledgeAll();  // you're looking at it
    shell.jerry().setMood(shell.attacks().anyActive() ? Mood::Alert : Mood::Approved);
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    const AttackWatch &a = shell.attacks();
    const bool active = a.anyActive();
    shell.jerry().draw(g, 16, y0 + 12, 0.26f);
    text(g, 32, y0 + 3, active ? ALERT : GOOD, active ? "ATTACK NEARBY" : "all quiet");
    text(g, 32, y0 + 13, DIM, wifi::enabled() ? "listening..." : "WiFi scan off!");
    static const char *const kWhat[] = {"deauth floods", "Flipper-style spam", "evil twins"};
    for (int t = 0; t < (int)Threat::COUNT; t++) {
      const ThreatState &s = a.state((Threat)t);
      const int y = y0 + 28 + t * 25;
      char st[12];
      uint16_t col;
      if (s.active) {
        snprintf(st, sizeof st, "NOW");
        col = ALERT;
      } else if (s.lastMs) {
        ago(st, sizeof st, (int)((now - s.lastMs) / 60000) + 1);  // how long ago
        col = GOLD;
      } else {
        snprintf(st, sizeof st, "ok");
        col = GOOD;
      }
      if (s.active) g.fillRoundRect(2, y - 2, W - 4, 23, 3, lgfx::color565(0x50, 0x10, 0x10));
      text(g, 5, y, CREAM, threatName((Threat)t));
      text(g, W - 5 - g.textWidth(st), y, col, st);
      const bool wifiThreat = t != (int)Threat::Popup;
      const char *line2 = s.lastMs ? s.detail : (wifiThreat && !wifi::enabled()) ? "(needs WiFi scan)" : kWhat[t];
      fitText(g, 9, y + 10, W - 12, s.lastMs ? (s.active ? CREAM : GOLD) : DIM, line2);
    }
  }
};

// ---------------------------------------------------------------------------
// Cameras: Flock licence-plate cameras and their gear
// ---------------------------------------------------------------------------
// Does a label already say "cam" (any case)?
bool mentionsCam(const char *s) {
  for (; s && s[0] && s[1] && s[2]; s++)
    if (tolower((unsigned char)s[0]) == 'c' && tolower((unsigned char)s[1]) == 'a' &&
        tolower((unsigned char)s[2]) == 'm')
      return true;
  return false;
}

class CamerasScreen : public Screen {
 public:
  const char *tag() const override { return "CAMERAS"; }
  Hints hints() const override { return {"back", nullptr, nullptr}; }
  void update(Shell &shell, uint32_t now) override {
    shell.jerry().setMood(shell.cameras().nearby(now) ? Mood::Scanning : Mood::Approved);
  }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    const CameraWatch &c = shell.cameras();
    char buf[28];
    const int flock = c.nearbyFlock(now), hints = c.nearbyHints(now);
    snprintf(buf, sizeof buf, "Flock %d   cams? %d", flock, hints);
    text(g, 4, y0 + 3, flock ? GOLD : hints ? CREAM : DIM, buf);
    const CamSighting *list[8];
    const int n = c.list(list, 8);
    if (!n) {
      shell.jerry().draw(g, W / 2, y0 + 40, 0.42f);
      centered(g, y0 + 66, GOOD, "no cameras heard");
      centered(g, y0 + 80, DIM, wifi::enabled() ? "listening..." : "(needs WiFi scan)");
      return;
    }
    for (int i = 0; i < n && i < 8; i++) {
      const CamSighting &s = *list[i];
      const int y = y0 + 14 + i * 11;
      const bool here = now - s.lastMs < CameraWatch::NEAR_MS;
      const bool fl = isFlock(s.kind);
      signalBars(g, 5, y, s.rssi, here ? (fl ? GOLD : CREAM) : DIM, BAR);
      char when[10];
      if (here) snprintf(when, sizeof when, "now");
      else ago(when, sizeof when, (int)((now - s.lastMs) / 60000));
      if (fl) snprintf(buf, sizeof buf, "%s%s", camKindName(s.kind), s.sure ? "" : "?");
      else snprintf(buf, sizeof buf, "%s%s?", s.why ? s.why : "camera",
                    mentionsCam(s.why) ? "" : " cam");  // "Wyze cam?", "HDWiFiCam?"
      const int ww = g.textWidth(when);
      fitText(g, 20, y + 1, W - 26 - ww, here ? CREAM : DIM, buf);
      text(g, W - 5 - ww, y + 1, here ? (fl ? GOLD : CREAM) : DIM, when);
    }
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
    snprintf(buf, sizeof(buf), "ble %d  wifi %d%s", ble::listening() ? ble::nearbyDevices() : -1,
             wifi::nearby(), wifi::recording() ? " REC" : "");
    row(buf);
    snprintf(buf, sizeof(buf), "moved %lu  follow %d", (unsigned long)shell.scene().place(),
             shell.follow().flaggedCount());
    row(buf);
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
  static constexpr int kLines = 25;
  const char *const kText[kLines] = {
      "*JerryOS v" JERRYOS_VERSION,
      "Jerry the Squatch",
      "Chief Gear Inspector",
      "5quatchrated.com",
      "",
      "*Personal monitor",
      "Trackers, BLE, WiFi",
      "Listen-only. Saves",
      "only when you REC.",
      "",
      "*Built with",
      "LovyanGFX (lovyan03)",
      "Espressif ESP32",
      "PlatformIO, NimBLE",
      "*Tracker research",
      "AirGuard (SEEMOO lab)",
      "*Flock signatures",
      "flock-you, flock-you-go",
      "@NitekryDPaul",
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
// Settings: brightness, alert window, LED, Wi-Fi scan, Wi-Fi log, my devices, diag, about
// ---------------------------------------------------------------------------
class SettingsScreen : public Screen {
 public:
  const char *tag() const override { return "SETTINGS"; }
  Hints hints() const override {
    if (editing_) return {"undo", "+", "save"};
    if (erasing_) return {"cancel", nullptr, "erase"};
    return {"back", "next",
            isValue(cursor_) ? "edit" : (cursor_ == kWifiLog || cursor_ == kMine) ? "erase" : "open"};
  }

  void enter(Shell &) override { editing_ = erasing_ = false; }

  void onLeftShort(Shell &shell) override {
    if (erasing_) {
      erasing_ = false;
      return;
    }
    if (!editing_) return shell.back();
    shell.settings() = before_;  // undo
    shell.applyBrightness();
    editing_ = false;
  }

  void onRightShort(Shell &shell) override {
    if (erasing_) return;
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
      case 2: s.ledMode = (s.ledMode + 1) % 3; break;  // the LED shows it live
      case 3: s.wifiScan = !s.wifiScan; break;
    }
  }

  void onRightLong(Shell &shell) override {
    if (erasing_) {  // second hold: really erase
      if (cursor_ == kWifiLog) {
        wifi::clearLog();
      } else {
        shell.follow().clearTrusted();
        shell.saveTrusted();
      }
      erasing_ = false;
      shell.jerry().flash(Mood::Approved, 1200);
      return;
    }
    if (editing_) {
      shell.settings().save();
      wifi::setEnabled(shell.settings().wifiScan);
      if (!shell.settings().wifiScan) wifi::setRecording(false);
      editing_ = false;
      shell.jerry().flash(Mood::Approved, 1200);
      Serial.println(F("[settings] saved"));
      return;
    }
    if (isValue(cursor_)) {
      before_ = shell.settings();
      editing_ = true;
    } else if (cursor_ == kWifiLog || cursor_ == kMine) {
      erasing_ = true;  // ask first: "hold R again to erase"
    } else if (cursor_ == 6) {
      shell.open(diagScreen);
    } else {
      shell.open(aboutScreen);
    }
  }

  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const Settings &s = shell.settings();
    char bright[8], alert[8], log[10];
    snprintf(bright, sizeof(bright), "%u%%", s.brightnessPct());
    snprintf(alert, sizeof(alert), "%um", s.alertMinutes());
    logSize(log, sizeof log);
    static const char *const labels[kItems] = {"Brightness", "Alert after", "LED", "WiFi scan",
                                               "WiFi log", "My devices", "Diagnostics", "About"};
    char mine[8];
    snprintf(mine, sizeof mine, "%d", shell.follow().trustedCount());
    const char *values[kItems] = {bright, alert, ledModeName((LedMode)s.ledMode),
                                  s.wifiScan ? "on" : "off",
                                  erasing_ && cursor_ == kWifiLog ? "erase?" : log,
                                  erasing_ && cursor_ == kMine ? "clear?" : mine, nullptr, nullptr};
    drawList(g, y0, labels, values, kItems, cursor_, editing_ || erasing_);
  }

 private:
  static constexpr int kItems = 8;
  static constexpr int kWifiLog = 4;
  static constexpr int kMine = 5;  // devices marked "mine" in Followers
  static bool isValue(int i) { return i <= 3; }
  int cursor_ = 0;
  bool editing_ = false;
  bool erasing_ = false;
  Settings before_;
};

TrackerScreen trackerScreenObj;
NearbyScreen nearbyScreen;
FollowersScreen followersScreenObj;
ShieldScreen shieldScreenObj;
CamerasScreen camerasScreenObj;
WifiScreen wifiScreen;
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
    {"Followers", &followersScreenObj},
    {"Shield", &shieldScreenObj},
    {"Cameras", &camerasScreenObj},
    {"BLE Nearby", &nearbyScreen},
    {"WiFi Scan", &wifiScreen},
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
Screen &followersScreen() { return followersScreenObj; }
Screen &shieldScreen() { return shieldScreenObj; }
Screen &camerasScreen() { return camerasScreenObj; }
