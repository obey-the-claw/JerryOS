// dashes.cpp - see dashes.h.

#include "dashes.h"

#include <Arduino.h>
#include <math.h>

#include "ble_scanner.h"
#include "mic.h"
#include "shell.h"
#include "sun_clock.h"
#include "theme.h"
#include "ui.h"
#include "wifi_scanner.h"

using namespace theme;
using ui::centered;
using ui::text;

namespace {

constexpr float PI_F = 3.14159265f;

const char *const kNames[DASH_COUNT] = {"Jerry", "Radar", "Trail Dash", "All Clear", "Trends", "Night Watch"};
const char *const kShort[DASH_COUNT] = {"jerry", "radar", "trail", "clear", "trends", "night"};

uint32_t addrHash(const uint8_t a[6]) {  // FNV-1a: a fixed spot on the radar per device
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) h = (h ^ a[i]) * 16777619u;
  return h;
}

uint16_t mix(uint16_t a, uint16_t b, float k) {  // blend two 565 colours, k = 0 a .. 1 b
  k = k < 0 ? 0 : k > 1 ? 1 : k;
  const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((int)(ar + (br - ar) * k) << 11) | ((int)(ag + (bg - ag) * k) << 5) | (int)(ab + (bb - ab) * k));
}

// ---------------------------------------------------------------------------
// Radar
// ---------------------------------------------------------------------------
const uint16_t R_BG = lgfx::color565(0x03, 0x16, 0x0a);
const uint16_t R_RING = lgfx::color565(0x0c, 0x44, 0x1e);
const uint16_t R_SWEEP = lgfx::color565(0x10, 0x5a, 0x28);
const uint16_t R_BLIP = lgfx::color565(0x6c, 0xff, 0x8c);
const uint16_t R_DIM = lgfx::color565(0x1a, 0x6a, 0x30);
constexpr float SWEEP_MS = 2500;

void blipPos(const uint8_t addr[6], int rssi, int cx, int cy, int R, int &x, int &y, float &deg) {
  deg = (addrHash(addr) % 360);
  const float k = (float)(-rssi - 35) / 65.0f;  // -35 dBm = middle, -100 = edge
  const float r = 5 + (k < 0 ? 0 : k > 1 ? 1 : k) * (R - 7);
  x = cx + (int)(cosf(deg * PI_F / 180) * r);
  y = cy + (int)(sinf(deg * PI_F / 180) * r);
}

// How bright a blip is: full just after the sweep passes, fading for a turn.
float glow(float blipDeg, float sweepDeg) {
  float d = fmodf(sweepDeg - blipDeg + 720, 360);
  return 1 - d / 360;
}

void drawRadar(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) {
  const uint32_t now = millis();
  const int cx = W / 2, cy = y0 + h / 2, R = h / 2 - 4;
  g.fillRect(0, y0, W, h, INK);
  g.fillCircle(cx, cy, R, R_BG);
  const float sweep = fmodf(now / SWEEP_MS * 360, 360);
  // the sweep and its fading trail
  for (int k = 6; k >= 0; k--)
    g.fillArc(cx, cy, 0, R, sweep - 8 * (k + 1), sweep - 8 * k, mix(R_BG, R_SWEEP, 1 - k / 7.0f));
  for (int r = R / 3; r <= R; r += R / 3) g.drawCircle(cx, cy, r, R_RING);
  g.drawFastHLine(cx - R, cy, 2 * R, R_RING);
  g.drawFastVLine(cx, cy - R, 2 * R, R_RING);
  g.drawLine(cx, cy, cx + (int)(cosf(sweep * PI_F / 180) * R), cy + (int)(sinf(sweep * PI_F / 180) * R), R_BLIP);

  static NearbyDevice devs[NearbyTable::MAX];
  const int n = ble::nearbySnapshot(devs, NearbyTable::MAX);
  for (int i = 0; i < n; i++) {
    int x, y;
    float deg;
    blipPos(devs[i].addr, devs[i].rssi, cx, cy, R, x, y, deg);
    g.fillCircle(x, y, 1, mix(R_DIM, R_BLIP, glow(deg, sweep)));
  }
  // Cameras: gold squares.
  const CamSighting *cams[16];
  const int nc = shell.cameras().list(cams, 16);
  int camsNear = 0;
  for (int i = 0; i < nc; i++) {
    if (now - cams[i]->lastMs > 120000) continue;
    camsNear++;
    int x, y;
    float deg;
    blipPos(cams[i]->addr, cams[i]->rssi, cx, cy, R, x, y, deg);
    g.fillRect(x - 2, y - 2, 5, 4, mix(NOSE, GOLD, 0.4f + 0.6f * glow(deg, sweep)));
    g.fillRect(x + 3, y - 1, 2, 2, GOLD);  // lens
  }
  // Trackers (orange; lost ones red) and followers (red, with a ring).
  const TrackedDevice *trk[TrackerWatch::MAX_DEVICES];
  int lost = 0;
  for (int k = 0; k < TRACKER_KINDS; k++) {
    const int c = shell.tracker().listDevices((TrackerKind)k, now, trk, TrackerWatch::MAX_DEVICES);
    for (int i = 0; i < c; i++) {
      if (now - trk[i]->lastSeenMs > TrackerWatch::RECENT_MS) continue;
      int x, y;
      float deg;
      blipPos(trk[i]->addr, trk[i]->rssi, cx, cy, R, x, y, deg);
      if (trk[i]->separated) lost++;
      g.fillCircle(x, y, 2, trk[i]->separated ? ALERT : GOLD);
    }
  }
  const Follower *fol[FollowWatch::MAX];
  const int nf = shell.follow().followers(fol, FollowWatch::MAX);
  for (int i = 0; i < nf; i++) {
    if (!fol[i]->flagged) continue;
    int x, y;
    float deg;
    blipPos(fol[i]->addr, fol[i]->rssi, cx, cy, R, x, y, deg);
    g.fillCircle(x, y, 2, ALERT);
    g.drawCircle(x, y, 4 + (int)((now / 120) % 3), ALERT);
  }
  // Corners: the numbers.
  char b[16];
  snprintf(b, sizeof b, "BT %d", n);
  text(g, 2, y0 + 2, R_BLIP, b);
  const ui::Safety s = ui::safety(shell, now);
  const char *word = s == ui::Safety::Alert ? "ALERT" : s == ui::Safety::Watch ? "WATCH" : "SAFE";
  text(g, W - 2 - g.textWidth(word), y0 + 2, s == ui::Safety::Safe ? GOOD : s == ui::Safety::Watch ? GOLD : ALERT, word);
  if (lost) {
    snprintf(b, sizeof b, "tag %d", lost);
    text(g, 2, y0 + h - 9, ALERT, b);
  }
  if (camsNear) {
    snprintf(b, sizeof b, "cam %d", camsNear);
    text(g, W - 2 - g.textWidth(b), y0 + h - 9, GOLD, b);
  }
}

// ---------------------------------------------------------------------------
// Trail Dash: a Jeep's instrument cluster
// ---------------------------------------------------------------------------
const uint16_t T_BG = lgfx::color565(0x0c, 0x0c, 0x0a);
const uint16_t T_FACE = lgfx::color565(0x1a, 0x1c, 0x16);
const uint16_t T_TICK = lgfx::color565(0xc8, 0xc0, 0xa8);
const uint16_t T_NEEDLE = lgfx::color565(0xff, 0x6a, 0x1a);
const uint16_t T_OLIVE = lgfx::color565(0x5a, 0x6b, 0x2c);

// A round gauge: 270 degrees from bottom-left, clockwise. v in lo..hi.
void gauge(lgfx::LovyanGFX &g, int cx, int cy, int r, float v, float lo, float hi, int ticks, bool numbers) {
  g.fillCircle(cx, cy, r + 2, T_OLIVE);  // bezel
  g.fillCircle(cx, cy, r, T_FACE);
  for (int i = 0; i <= ticks; i++) {
    const float a = (135 + 270.0f * i / ticks) * PI_F / 180;
    const int inner = r - (i % 2 ? 3 : 5);
    g.drawLine(cx + (int)(cosf(a) * inner), cy + (int)(sinf(a) * inner), cx + (int)(cosf(a) * (r - 1)),
               cy + (int)(sinf(a) * (r - 1)), T_TICK);
    if (numbers && i % 2 == 0 && i > 0 && i < ticks) {  // (the ends would sit on the readout)
      char b[6];
      snprintf(b, sizeof b, "%d", (int)(lo + (hi - lo) * i / ticks));
      const int tx = cx + (int)(cosf(a) * (r - 11)) - g.textWidth(b) / 2, ty = cy + (int)(sinf(a) * (r - 11)) - 3;
      text(g, tx, ty, DIM, b);
    }
  }
  const float k = (v - lo) / (hi - lo);
  const float a = (135 + 270 * (k < 0 ? 0 : k > 1 ? 1 : k)) * PI_F / 180;
  g.drawWideLine(cx, cy, cx + (int)(cosf(a) * (r - 4)), cy + (int)(sinf(a) * (r - 4)), r > 20 ? 2.0f : 1.4f, T_NEEDLE);
  g.fillCircle(cx, cy, r > 20 ? 3 : 2, T_TICK);
}

void lamp(lgfx::LovyanGFX &g, int &x, int y, const char *label, bool on, uint16_t col) {
  const int w = g.textWidth(label) + 4;
  if (on) g.fillRoundRect(x, y, w, 9, 2, col);
  else g.drawRoundRect(x, y, w, 9, 2, lgfx::color565(0x30, 0x30, 0x28));
  text(g, x + 2, y + 1, on ? INK : lgfx::color565(0x40, 0x40, 0x38), label);
  x += w + 2;
}

void drawTrail(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) {
  const uint32_t now = millis();
  g.fillRect(0, y0, W, h, T_BG);
  // warning lamps
  int x = 2;
  const ui::Safety s = ui::safety(shell, now);
  const bool rig = shell.rig().connected(now);
  lamp(g, x, y0 + 1, "GPS", rig && shell.rig().status().fix, GOOD);
  lamp(g, x, y0 + 1, "REC", wifi::recording(), ALERT);
  lamp(g, x, y0 + 1, "!", s != ui::Safety::Safe && (s == ui::Safety::Watch || (now / 400) % 2), s == ui::Safety::Alert ? ALERT : GOLD);
  if (shell.clock().valid()) {
    char t[10];
    formatClock(shell.clock().minute(now), t, sizeof t);
    text(g, W - 2 - g.textWidth(t), y0 + 2, T_TICK, t);
  }
  // the big one: speed with the rig, otherwise Bluetooth devices
  const int cx = W / 2, cy = y0 + 46, r = 31;
  char big[8];
  const int mph = rig ? (int)(shell.rig().status().speedKmh * 0.6214f + 0.5f) : 0;
  const int bt = ble::nearbyDevices();
  gauge(g, cx, cy, r, rig ? mph : bt, 0, rig ? 80 : 60, rig ? 8 : 6, true);
  snprintf(big, sizeof big, "%d", rig ? mph : bt);
  g.setTextSize(2);
  text(g, cx - g.textWidth(big) / 2, cy + 9, CREAM, big);
  g.setTextSize(1);
  centered(g, cy + 25, DIM, rig ? "MPH" : "BT");
  // small gauges: Wi-Fi (left), sound (right), label above and reading below
  const int sy = y0 + 72;
  const int nets = wifi::enabled() ? wifi::nearby() : 0;
  const LevelMeter &m = mic::meter();
  gauge(g, 15, sy, 12, nets, 0, 60, 4, false);
  gauge(g, W - 16, sy, 12, m.hasData() ? m.db() : 30, 30, 100, 4, false);
  text(g, 3, sy - 22, DIM, "WiFi");
  text(g, W - 3 - g.textWidth("dB"), sy - 22, DIM, "dB");
  char b[8];
  snprintf(b, sizeof b, "%d", nets);
  text(g, 15 - g.textWidth(b) / 2, sy + 15, T_TICK, b);
  if (m.hasData()) snprintf(b, sizeof b, "%d", m.db());
  else snprintf(b, sizeof b, "--");
  text(g, W - 16 - g.textWidth(b) / 2, sy + 15, T_TICK, b);
  // sunset / sunrise, if we know
  int rise, set;
  if (shell.sun(rise, set)) {
    const int nowMin = shell.clock().minute(now);
    char t[8], line[20];
    const bool day = nowMin >= rise && nowMin < set;
    formatClock(day ? set : rise, t, sizeof t);
    snprintf(line, sizeof line, "%s %s", day ? "sunset" : "sunrise", t);
    centered(g, y0 + h - 21, GOLD, line);
  }
  // fuel gauge = battery
  const PowerStatus &p = shell.power();
  const int fx = 12, fw = W - 24, fy = y0 + h - 8;
  text(g, 3, fy - 1, DIM, "E");
  const int left = shell.batteryMinutesLeft();  // the fuel gauge's "range": time left
  if (left >= 0) {
    char r[8];
    if (left >= 60) snprintf(r, sizeof r, "%dh", (left + 30) / 60);
    else snprintf(r, sizeof r, "%dm", left);
    text(g, W - 2 - g.textWidth(r), fy - 1, GOLD, r);
  } else {
    text(g, W - 8, fy - 1, DIM, "F");
  }
  const int fwEnd = left >= 0 ? W - 22 : fx + fw;
  g.drawRect(fx, fy, fwEnd - fx, 6, T_TICK);
  const int pct = p.batteryPresent ? p.percent : 100;
  g.fillRect(fx + 1, fy + 1, (fwEnd - fx - 2) * pct / 100, 4, pct <= 15 ? ALERT : p.charging ? GOLD : GOOD);
  for (int i = 1; i < 4; i++) g.drawFastVLine(fx + (fwEnd - fx) * i / 4, fy - 2, 2, T_TICK);
}

// ---------------------------------------------------------------------------
// All Clear
// ---------------------------------------------------------------------------
uint32_t calmSince = 0;  // when it last went back to SAFE

void countBox(lgfx::LovyanGFX &g, int x, int y, int w, int h, const char *label, int n, uint16_t hot) {
  g.fillRoundRect(x, y, w, h, 4, n ? hot : BAR);
  text(g, x + 4, y + 3, n ? INK : DIM, label);
  char b[8];
  snprintf(b, sizeof b, "%d", n);
  g.setTextSize(2);
  text(g, x + w - 5 - g.textWidth(b), y + h - 16, n ? INK : CREAM, b);
  g.setTextSize(1);
}

void drawAllClear(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) {
  const uint32_t now = millis();
  const ui::Safety s = ui::safety(shell, now);
  if (s != ui::Safety::Safe || !calmSince) calmSince = now;
  uint16_t bg, fg;
  const char *word;
  if (s == ui::Safety::Alert) {
    const bool on = (now / 400) % 2;
    bg = on ? ALERT : lgfx::color565(0x50, 0x0c, 0x0c);
    fg = TFT_WHITE;
    word = "ALERT!";
  } else if (s == ui::Safety::Watch) {
    bg = lgfx::color565(0x4a, 0x32, 0x06);
    fg = GOLD;
    word = "HEADS UP";
  } else {
    bg = lgfx::color565(0x0a, 0x36, 0x18);
    fg = GOOD;
    word = "ALL CLEAR";
  }
  g.fillRect(0, y0, W, h, bg);
  g.setTextSize(2);
  centered(g, y0 + 8, fg, word);
  g.setTextSize(1);
  char line[24];
  if (s == ui::Safety::Safe) {
    char span[10];
    formatSpan((now - calmSince) / 60000, span, sizeof span);
    snprintf(line, sizeof line, "quiet for %s", span);
  } else {
    snprintf(line, sizeof line, "%s", s == ui::Safety::Alert ? "open Menu to look" : "worth a look");
  }
  centered(g, y0 + 28, CREAM, line);
  // the counts
  int attacks = 0;
  for (int t = 0; t < (int)Threat::COUNT; t++) attacks += shell.attacks().state((Threat)t).active;
  const int bw = 58, bh = 30, gx = 4, gy = y0 + 42;
  countBox(g, gx, gy, bw, bh, "trackers", ui::lostTrackersNow(shell, now), GOLD);
  countBox(g, gx + bw + 4, gy, bw, bh, "followers", shell.follow().flaggedCount(), ALERT);
  countBox(g, gx, gy + bh + 4, bw, bh, "Flock", shell.cameras().nearbyFlock(now), GOLD);
  countBox(g, gx + bw + 4, gy + bh + 4, bw, bh, "attacks", attacks, ALERT);
}

// ---------------------------------------------------------------------------
// Trends
// ---------------------------------------------------------------------------
void sparkPanel(lgfx::LovyanGFX &g, const History &hist, History::Series sr, int y, int ph, const char *label,
                uint16_t col, const char *unit) {
  const int gx = 4, gw = W - 8;
  g.fillRoundRect(2, y, W - 4, ph, 3, BAR);
  const int latest = hist.latest(sr);
  char b[20];
  if (latest >= 0) snprintf(b, sizeof b, "%s %d%s", label, latest, unit);
  else snprintf(b, sizeof b, "%s --", label);
  text(g, 6, y + 2, col, b);
  const int top = y + 12, gh = ph - 14;
  int peak = hist.max(sr);
  if (peak < 10) peak = 10;
  int px = -1, py = 0;
  int any = 0;
  for (int i = 0; i < hist.size(); i++) {
    const int v = hist.at(sr, i);
    const int x = gx + gw - (hist.size() - 1 - i) * gw / (History::N - 1);
    if (v < 0) {
      px = -1;
      continue;
    }
    any++;
    const int y2 = top + gh - v * gh / peak;
    g.drawFastVLine(x, y2, top + gh - y2, mix(BAR, col, 0.35f));
    if (px >= 0) g.drawLine(px, py, x, y2, col);
    px = x;
    py = y2;
  }
  snprintf(b, sizeof b, "max %d", hist.max(sr));
  if (any) text(g, W - 6 - g.textWidth(b), y + 2, DIM, b);
  else text(g, W - 6 - g.textWidth(sr == History::SOUND ? "mic off" : "warming up"), y + 2, DIM,
            sr == History::SOUND ? "mic off" : "warming up");
}

void drawTrends(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) {
  const History &hist = shell.history();
  const int ph = (h - 12) / 3;
  sparkPanel(g, hist, History::BT, y0 + 1, ph - 1, "BT", GOLD, "");
  sparkPanel(g, hist, History::WIFI, y0 + ph + 1, ph - 1, "WiFi", lgfx::color565(0x7c, 0xc4, 0xe8), "");
  sparkPanel(g, hist, History::SOUND, y0 + 2 * ph + 1, ph - 1, "Sound", GRIN, " dB");
  centered(g, y0 + h - 10, DIM, "last 10 minutes");
}

// ---------------------------------------------------------------------------
// Night Watch: full screen, all red, dim
// ---------------------------------------------------------------------------
// Bright reds on a dim backlight (the shell turns it down to 20%): readable,
// but kind to eyes that have adjusted to the dark.
const uint16_t N_RED = lgfx::color565(0xff, 0x20, 0x10);
const uint16_t N_MID = lgfx::color565(0xc0, 0x10, 0x08);
const uint16_t N_DIM = lgfx::color565(0x80, 0x08, 0x04);

void drawNight(Shell &shell, lgfx::LovyanGFX &g) {
  const uint32_t now = millis();
  g.fillScreen(TFT_BLACK);
  centered(g, 4, N_DIM, "NIGHT WATCH");
  const ui::Safety s = ui::safety(shell, now);
  char b[24];
  if (shell.clock().valid()) {  // the time, big
    formatClock(shell.clock().minute(now), b, sizeof b);
    g.setTextSize(3);
    centered(g, 26, N_RED, b);
    g.setTextSize(1);
  } else {
    g.setTextSize(3);
    snprintf(b, sizeof b, "%d", ble::nearbyDevices());
    centered(g, 26, N_RED, b);
    g.setTextSize(1);
    centered(g, 52, N_MID, "bluetooth nearby");
  }
  const char *word = s == ui::Safety::Alert ? "ALERT" : s == ui::Safety::Watch ? "heads up" : "all quiet";
  if (s != ui::Safety::Alert || (now / 500) % 2) {
    g.setTextSize(2);
    centered(g, 66, s == ui::Safety::Safe ? N_MID : N_RED, word);
    g.setTextSize(1);
  }
  snprintf(b, sizeof b, "BT %d   WiFi %d", ble::nearbyDevices(), wifi::enabled() ? wifi::nearby() : 0);
  centered(g, 90, N_MID, b);
  int rise, set;
  if (shell.sun(rise, set)) {
    const int m = shell.clock().minute(now);
    const bool day = m >= rise && m < set;
    char t[8], span[10];
    const int until = ((day ? set : rise) - m + 1440) % 1440;
    formatClock(day ? set : rise, t, sizeof t);
    formatSpan(until, span, sizeof span);
    snprintf(b, sizeof b, "%s %s, %s", day ? "dark" : "rise", t, span);
    centered(g, 104, N_DIM, b);
  }
  // a slow heartbeat: still watching
  const float beat = (sinf(now / 900.0f) + 1) / 2;
  g.fillCircle(W - 8, H - 8, 2, mix(N_DIM, N_RED, beat));
  if (wifi::recording()) g.fillCircle(8, H - 8, 2, N_RED);
}

}  // namespace

const char *dashName(Dash d) { return kNames[(int)d < DASH_COUNT ? (int)d : 0]; }
const char *dashShort(Dash d) { return kShort[(int)d < DASH_COUNT ? (int)d : 0]; }
bool dashWantsMic(Dash d) { return d == Dash::Trail || d == Dash::Trends; }

void drawDash(Dash d, Shell &shell, lgfx::LovyanGFX &g, int y0, int h) {
  switch (d) {
    case Dash::Radar: drawRadar(shell, g, y0, h); break;
    case Dash::Trail: drawTrail(shell, g, y0, h); break;
    case Dash::AllClear: drawAllClear(shell, g, y0, h); break;
    case Dash::Trends: drawTrends(shell, g, y0, h); break;
    case Dash::Night: drawNight(shell, g); break;
    default: break;
  }
}
