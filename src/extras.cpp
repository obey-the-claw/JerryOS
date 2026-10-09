// extras.cpp - the v1.4 screens:
//   Knock Detector   listen for wood knocks and howls (knock_detect.h)
//   Expeditions      recaps of your REC sessions, and the card after each one
//   Rate Gear        the 5quatch Rated stamp, for gear reviews on camera
//   Badges           the achievements Jerry hands out (badges.h)

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "expedition.h"
#include "funk.h"
#include "mic.h"
#include "screens.h"
#include "shell.h"
#include "speaker.h"
#include "sun_clock.h"
#include "theme.h"
#include "ui.h"

using namespace theme;
using ui::centered;
using ui::text;

namespace {

// "Oct 9" from the day of the year (ignores leap years: close enough).
void formatDay(int doy, char *out, int size) {
  static const char *const mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int m = 0, d = doy;
  while (m < 11 && d > len[m]) d -= len[m++];
  snprintf(out, size, "%s %d", mon[m], d);
}

// ---------------------------------------------------------------------------
// Knock Detector
// ---------------------------------------------------------------------------
class KnockScreen : public Screen {
 public:
  const char *tag() const override { return "KNOCKS"; }
  Hints hints() const override { return {"back", "clear", nullptr}; }
  void onRightShort(Shell &) override {
    n_ = 0;
    flashUntil_ = 0;
  }
  void update(Shell &shell, uint32_t now) override {
    mic::use();
    shell.wake();  // you're listening: keep the screen on
    if (now - lastLevel_ >= 100) {  // the scrolling level trace
      lastLevel_ = now;
      level_[head_] = (int8_t)(mic::meter().hasData() ? mic::meter().dbfs() : -90);
      mark_[head_] = 0;
      head_ = (head_ + 1) % TRACE;
    }
    SoundEvent e;
    while (mic::takeSoundEvent(e)) {
      memmove(&ev_[1], &ev_[0], sizeof(ev_[0]) * (MAX_EV - 1));
      ev_[0] = e;
      ev_[0].ms = now;
      if (n_ < MAX_EV) n_++;
      mark_[(head_ + TRACE - 1) % TRACE] = e.kind == SoundKind::Knock ? 1 : 2;
      flashUntil_ = now + 900;
      shell.jerry().flash(Mood::Alert, 1500);
    }
    shell.jerry().setMood(Mood::Scanning);
  }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const uint32_t now = millis();
    if (!mic::chipFound()) {
      centered(g, y0 + 40, ALERT, "microphone not found");
      return;
    }
    const bool flash = (int32_t)(now - flashUntil_) < 0;
    if (flash) g.drawRoundRect(0, y0, W, CONTENT_H, 4, (now / 150) % 2 ? GOLD : BARK);
    shell.jerry().draw(g, 16, y0 + 15, 0.24f);
    g.setTextSize(2);
    const char *head = !mic::knockReady() ? "hush..." : flash ? (ev_[0].kind == SoundKind::Knock ? "KNOCK!" : "HOWL!") : "ears on";
    text(g, 36, y0 + 4, flash ? GOLD : CREAM, head);
    g.setTextSize(1);
    char b[32];
    if (!mic::knockReady()) snprintf(b, sizeof b, "learning...");
    else snprintf(b, sizeof b, "%lu knock%s %lu howl%s", (unsigned long)mic::knockCount(),
                  mic::knockCount() == 1 ? "" : "s", (unsigned long)mic::howlCount(), mic::howlCount() == 1 ? "" : "s");
    text(g, 36, y0 + 22, DIM, b);
    // the level trace: the last 12 seconds, knocks marked gold, howls cream
    const int tx = 4, ty = y0 + 34, th = 26, tw = W - 8;
    g.drawRect(tx - 1, ty - 1, tw + 2, th + 2, BAR);
    for (int i = 0; i < TRACE; i++) {
      const int k = (head_ + i) % TRACE;
      const int v = level_[k] < -80 ? 0 : (level_[k] + 80) * th / 70;
      const int x = tx + i * tw / TRACE;
      if (v > 0) g.drawFastVLine(x, ty + th - (v > th ? th : v), v > th ? th : v, mark_[k] ? GOLD : FUR_DARK);
      if (mark_[k]) g.fillTriangle(x - 2, ty - 5, x + 2, ty - 5, x, ty - 1, mark_[k] == 1 ? GOLD : CREAM);
    }
    // what was heard
    if (!n_) {
      centered(g, y0 + 70, DIM, "Knock on a tree and");
      centered(g, y0 + 82, DIM, "listen for a reply...");
      return;
    }
    for (int i = 0; i < n_ && i < 4; i++) {
      const SoundEvent &e = ev_[i];
      const uint32_t ago = (now - e.ms) / 1000;
      char when[12];
      if (ago < 60) snprintf(when, sizeof when, "%lus ago", (unsigned long)ago);
      else snprintf(when, sizeof when, "%lum ago", (unsigned long)(ago / 60));
      if (e.kind == SoundKind::Knock && e.inARow > 1) snprintf(b, sizeof b, "KNOCK x%u +%udB", e.inARow, e.aboveDb);
      else snprintf(b, sizeof b, "%s +%udB", e.kind == SoundKind::Knock ? "KNOCK" : "HOWL", e.aboveDb);
      const int y = y0 + 64 + i * 10;
      text(g, 6, y, i == 0 ? GOLD : CREAM, b);
      text(g, W - 6 - g.textWidth(when), y, DIM, when);
    }
  }

 private:
  static constexpr int TRACE = 120, MAX_EV = 6;
  int8_t level_[TRACE] = {};
  uint8_t mark_[TRACE] = {};
  int head_ = 0;
  uint32_t lastLevel_ = 0, flashUntil_ = 0;
  SoundEvent ev_[MAX_EV] = {};
  int n_ = 0;
};

// ---------------------------------------------------------------------------
// Expedition recap card (after REC stops, or picked from the list)
// ---------------------------------------------------------------------------
void drawRecap(lgfx::LovyanGFX &g, int y0, const TripRecap &r) {
  char b[32], day[10], at[10], span[12];
  snprintf(b, sizeof b, "EXPEDITION #%u", r.number);
  centered(g, y0 + 3, GOLD, b);
  formatSpan(r.seconds / 60, span, sizeof span);
  if (r.seconds < 60) snprintf(span, sizeof span, "%lus", (unsigned long)r.seconds);
  if (r.doy && r.startMin != 0xFFFF) {
    formatDay(r.doy, day, sizeof day);
    formatClock(r.startMin, at, sizeof at);
    snprintf(b, sizeof b, "%s %s - %s", day, at, span);
  } else {
    snprintf(b, sizeof b, "lasted %s", span);
  }
  centered(g, y0 + 14, CREAM, b);
  g.drawFastHLine(8, y0 + 25, W - 16, BAR);
  struct Cell {
    const char *label;
    char value[10];
    bool hot;
  } cells[8];
  auto set = [](Cell &c, const char *l, unsigned long v, bool hot) {
    c.label = l;
    snprintf(c.value, sizeof c.value, "%lu", v);
    c.hot = hot;
  };
  set(cells[0], "nets", r.nets, false);
  cells[1].label = "miles";
  snprintf(cells[1].value, sizeof cells[1].value, "%.1f", r.meters / 1609.34f);
  cells[1].hot = false;
  set(cells[2], "BT max", r.peakBt, false);
  set(cells[3], "mph max", (unsigned long)(r.topKmh * 0.6214f + 0.5f), false);
  set(cells[4], "tags", r.trackers, r.trackers > 0);
  set(cells[5], "follow", r.followers, r.followers > 0);
  set(cells[6], "Flock", r.flock, r.flock > 0);
  set(cells[7], "knocks", r.knocks, r.knocks > 0);
  for (int i = 0; i < 8; i++) {
    const int x = i % 2 ? W / 2 + 2 : 4, y = y0 + 30 + (i / 2) * 15;
    text(g, x, y, DIM, cells[i].label);
    text(g, x + 60 - 2 - g.textWidth(cells[i].value), y, cells[i].hot ? GOLD : CREAM, cells[i].value);
  }
  const char *verdict = r.followers ? "Something followed you!"
                        : r.flock  ? "Flock cameras spotted."
                        : r.knocks ? "Heard something out there..."
                        : r.seconds < 120 ? "Short and sweet."
                                          : "Clean trip. Nice.";
  centered(g, y0 + 92, r.followers ? ALERT : GOLD, verdict);
}

class TripScreen : public Screen {
 public:
  const char *tag() const override { return "RECAP"; }
  Hints hints() const override { return {"done", nullptr, nullptr}; }
  void show(const TripRecap &r) {
    r_ = r;
    fromList_ = true;
  }
  void enter(Shell &shell) override {
    if (!fromList_) r_ = shell.lastRecap();  // just finished recording
    shell.jerry().flash(Mood::Approved, 1500);
  }
  void onLeftShort(Shell &shell) override {
    fromList_ = false;
    shell.back();
  }
  void draw(Shell &, lgfx::LovyanGFX &g, int y0, int) override { drawRecap(g, y0, r_); }

 private:
  TripRecap r_ = {};
  bool fromList_ = false;
};
TripScreen tripObj;

class ExpeditionsScreen : public Screen {
 public:
  const char *tag() const override { return "EXPEDITIONS"; }
  Hints hints() const override { return {"back", "next", "open"}; }
  void enter(Shell &shell) override {
    if (cursor_ >= shell.trips().count()) cursor_ = 0;
  }
  void onRightShort(Shell &shell) override {
    if (shell.trips().count()) cursor_ = (cursor_ + 1) % shell.trips().count();
  }
  void onRightLong(Shell &shell) override {
    if (!shell.trips().count()) return;
    tripObj.show(shell.trips().at(cursor_));
    shell.open(tripObj);
  }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int) override {
    const TripLog &log = shell.trips();
    if (!log.count()) {
      shell.jerry().draw(g, W / 2, y0 + 30, 0.42f);
      centered(g, y0 + 62, CREAM, "No expeditions yet.");
      centered(g, y0 + 76, DIM, "Hold R on the home");
      centered(g, y0 + 88, DIM, "screen to start REC.");
      return;
    }
    static char labels[TripLog::MAX][16], values[TripLog::MAX][10];
    const char *lp[TripLog::MAX], *vp[TripLog::MAX];
    for (int i = 0; i < log.count(); i++) {
      const TripRecap &r = log.at(i);
      char day[10] = "";
      if (r.doy) formatDay(r.doy, day, sizeof day);
      snprintf(labels[i], sizeof labels[i], "#%u %s", r.number, day);
      if (r.meters >= 1609) snprintf(values[i], sizeof values[i], "%.1fmi", r.meters / 1609.34f);
      else formatSpan(r.seconds / 60, values[i], sizeof values[i]);
      lp[i] = labels[i];
      vp[i] = values[i];
    }
    ui::drawList(g, y0, lp, vp, log.count(), cursor_, false);
  }

 private:
  int cursor_ = 0;
};
ExpeditionsScreen expeditionsObj;

// ---------------------------------------------------------------------------
// Rate Gear: the 5quatch Rated stamp
// ---------------------------------------------------------------------------
// A Bigfoot footprint: the sole and five toes. filled = earned.
void footprint(lgfx::LovyanGFX &g, int x, int y, int s, uint16_t col, bool filled) {
  if (filled) {
    g.fillEllipse(x, y + s, s * 2 / 3, s, col);
    g.fillEllipse(x + s / 5, y + s * 2, s / 2, s * 2 / 3, col);
  } else {
    g.drawEllipse(x, y + s, s * 2 / 3, s, col);
    g.drawEllipse(x + s / 5, y + s * 2, s / 2, s * 2 / 3, col);
  }
  for (int t = 0; t < 5; t++) {
    const float a = (-150 + t * 28) * 3.14159f / 180;
    const int tx = x + (int)(cosf(a) * s * 0.85f), ty = y + (int)(sinf(a) * s * 0.8f) + s / 3;
    const int r = t == 0 ? s / 3 : s / 4;
    if (filled) g.fillCircle(tx, ty, r > 0 ? r : 1, col);
    else g.drawCircle(tx, ty, r > 0 ? r : 1, col);
  }
}

const char *verdict(int n) {
  static const char *const v[] = {"", "Leave it in woods", "Meh-squatch", "Squatchable", "Trail Worthy",
                                  "Bigfoot Approved!"};
  return v[n < 1 ? 1 : n > 5 ? 5 : n];
}

class RateScreen : public Screen {
 public:
  const char *tag() const override { return "RATE GEAR"; }
  Hints hints() const override {
    return stamped_ ? Hints{"back", "again", nullptr} : Hints{"back", "+1", "stamp"};
  }
  bool fullScreen() const override { return stamped_; }
  void enter(Shell &) override { stamped_ = false; }
  void onLeftShort(Shell &shell) override {
    if (stamped_) stamped_ = false;
    else shell.back();
  }
  void onRightShort(Shell &) override {
    if (stamped_) stamped_ = false;
    else rating_ = rating_ % 5 + 1;
  }
  void onRightLong(Shell &shell) override {
    if (stamped_) return;
    stamped_ = true;
    start_ = millis();
    shell.wake();
    if (shell.settings().tuneVol)
      speaker::playIntro(shell.settings().tuneVol, rating_ >= 3 ? FunkIntro::STING_GOOD : FunkIntro::STING_MEH);
    shell.award(Badge::GearInspector);
  }
  void update(Shell &shell, uint32_t) override {
    shell.jerry().setMood(rating_ >= 3 ? Mood::Approved : Mood::Sleepy);
    if (stamped_) shell.wake();
  }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) override {
    if (stamped_) return drawStamp(shell, g);
    centered(g, y0 + 6, DIM, "How squatchy is it?");
    const int s = 7;
    for (int i = 0; i < 5; i++) footprint(g, 16 + i * 24, y0 + 22, s, i < rating_ ? GOLD : DIM, i < rating_);
    g.setTextSize(2);
    char b[8];
    snprintf(b, sizeof b, "%d/5", rating_);
    centered(g, y0 + 52, CREAM, b);
    g.setTextSize(1);
    centered(g, y0 + 72, GOLD, verdict(rating_));
  }

 private:
  void drawStamp(Shell &shell, lgfx::LovyanGFX &g) {
    const uint32_t t = millis() - start_;
    g.fillScreen(BARK);
    // The card slams down: big and faint, then snaps to size with a thud.
    const float k = t < 260 ? t / 260.0f : 1;
    const int grow = (int)((1 - k) * 40);
    const int shake = t > 260 && t < 420 ? ((t / 30) % 2 ? 2 : -2) : 0;
    const int x = 8 - grow + shake, y = 14 - grow, w = W - 16 + 2 * grow, hh = 92 + 2 * grow;
    g.fillRoundRect(x, y, w, hh, 8, k < 1 ? FUR_DARK : CREAM);
    g.drawRoundRect(x, y, w, hh, 8, GOLD);
    g.drawRoundRect(x + 3, y + 3, w - 6, hh - 6, 6, GOLD);
    if (k < 1) return;
    g.setTextSize(2);
    text(g, (W - g.textWidth("5QUATCH")) / 2 + shake, 22, BARK, "5QUATCH");
    g.setTextSize(1);
    text(g, (W - g.textWidth("R A T E D")) / 2 + shake, 40, GRIN, "R A T E D");
    for (int i = 0; i < 5; i++) {  // footprints stomp in one by one
      const uint32_t at = 420 + i * 170;
      if (t < at) break;
      const bool on = i < rating_;
      const int pop = t - at < 100 ? 2 : 0;
      footprint(g, 20 + i * 22, 54 - pop, 6 + pop / 2, on ? NOSE : FUR_LIGHT, on);
    }
    if (t > 420 + 5 * 170) {
      centered(g, 88, BARK, verdict(rating_));
      shell.jerry().draw(g, W - 14, 110, 0.18f);
    }
    if (t > 2600 && (millis() / 600) % 2) centered(g, H - 10, DIM, "R again  L back");
  }
  int rating_ = 5;
  bool stamped_ = false;
  uint32_t start_ = 0;
};
RateScreen rateObj;

// ---------------------------------------------------------------------------
// Badges
// ---------------------------------------------------------------------------
class BadgesScreen : public Screen {
 public:
  const char *tag() const override { return "BADGES"; }
  Hints hints() const override { return {"back", "next", nullptr}; }
  void onRightShort(Shell &) override { cursor_ = (cursor_ + 1) % BADGE_COUNT; }
  void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) override {
    const Badges &b = shell.badges();
    char head[24];
    snprintf(head, sizeof head, "%d of %d earned", b.earnedCount(), BADGE_COUNT);
    centered(g, y0 + 3, GOLD, head);
    const int rows = 6, rowH = 12, top = cursor_ < rows ? 0 : cursor_ - rows + 1;
    for (int i = top; i < BADGE_COUNT && i < top + rows; i++) {
      const int y = y0 + 15 + (i - top) * rowH;
      const bool got = b.has((Badge)i), sel = i == cursor_;
      if (sel) g.fillRoundRect(2, y - 1, W - 4, rowH, 3, BAR);
      // a medal: gold and shiny if earned, an outline if not
      if (got) {
        g.fillTriangle(9, y + 5, 7, y - 1, 11, y - 1, ALERT);
        g.fillCircle(9, y + 5, 4, GOLD);
        g.drawPixel(8, y + 4, CREAM);
      } else {
        g.drawCircle(9, y + 5, 4, DIM);
      }
      text(g, 18, y + 1, got ? CREAM : DIM, badgeName((Badge)i));
    }
    g.drawFastHLine(4, y0 + h - 18, W - 8, BAR);
    centered(g, y0 + h - 13, b.has((Badge)cursor_) ? GOOD : DIM, badgeHint((Badge)cursor_));
  }

 private:
  int cursor_ = 0;
};
BadgesScreen badgesObj;
KnockScreen knockObj;

}  // namespace

Screen &tripScreen() { return tripObj; }
Screen &knockScreen() { return knockObj; }
Screen &expeditionsScreen() { return expeditionsObj; }
Screen &rateScreen() { return rateObj; }
Screen &badgesScreen() { return badgesObj; }
