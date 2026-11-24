// intro.cpp - the boot intros, each with its own funk groove (funk.cpp):
//   1 Jeep Jam     Jerry walks out of the woods, hops in his Jeep, drives off
//   2 Bluff Creek  grainy 1967-style film: he strides past the trees and
//                  gives the camera THE look back
//   3 Selfie       first person: Jerry leans into his phone from the driver's
//                  seat, the aviators drop, the grin
//   4 Cruisin'     3/4 view from behind the Jeep on a sunset highway
// 4-6 s each, then "JerryOS" and "press any button". Any button skips.
//
// Drawn in code (like Jerry himself), full screen, at the shell's ~30 fps.
// Everything is a function of the time since the intro started, so it plays
// the same at any frame rate.

#include "intro.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "jerry_face.h"
#include "shell.h"
#include "speaker.h"
#include "theme.h"

using namespace theme;

namespace {

constexpr int GROUND = 106;  // where feet and tyres touch

const uint16_t OLIVE = lgfx::color565(0x5a, 0x6b, 0x2c);       // the Jeep
const uint16_t OLIVE_DARK = lgfx::color565(0x3c, 0x48, 0x1c);
const uint16_t PINE = lgfx::color565(0x1d, 0x4a, 0x2a);
const uint16_t PINE_DARK = lgfx::color565(0x12, 0x33, 0x1c);
const uint16_t DIRT = lgfx::color565(0x6b, 0x4a, 0x2e);
const uint16_t ROAD = lgfx::color565(0x4a, 0x36, 0x24);
const uint16_t GLASS = lgfx::color565(0x9c, 0xc8, 0xd8);
const uint16_t GREY = lgfx::color565(0x8a, 0x8a, 0x8a);

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float ease(float v) {  // smooth start and stop
  v = clamp01(v);
  return v * v * (3 - 2 * v);
}

void pine(lgfx::LovyanGFX &g, int x, int base, int h, uint16_t col) {
  g.fillRect(x - 1, base - 4, 3, 4, NOSE);  // trunk
  for (int k = 0; k < 3; k++) {          // three tiers
    const int top = base - h + k * h / 4;
    const int w = 5 + k * 3;
    g.fillTriangle(x, top, x - w, top + h / 3 + 2, x + w, top + h / 3 + 2, col);
  }
}

void scenery(lgfx::LovyanGFX &g) {
  // Morning sky in bands, far hills, then the ground and a dirt road.
  static const uint16_t sky[] = {lgfx::color565(0x2c, 0x3e, 0x6b), lgfx::color565(0x45, 0x5d, 0x8c),
                                 lgfx::color565(0x7a, 0x86, 0xa8), lgfx::color565(0xd9, 0x9a, 0x6c)};
  for (int i = 0; i < 4; i++) g.fillRect(0, i * 18, W, 18, sky[i]);
  g.fillRect(0, 72, W, GROUND - 72, sky[3]);
  g.fillCircle(100, 74, 9, lgfx::color565(0xf6, 0xd0, 0x6a));  // sunrise
  const uint16_t hill = lgfx::color565(0x4b, 0x3f, 0x5c);
  g.fillTriangle(30, 92, 75, 58, 120, 92, hill);
  g.fillTriangle(70, 92, 110, 66, 150, 92, lgfx::color565(0x3c, 0x33, 0x4c));
  g.fillRect(0, 88, W, GROUND - 88, lgfx::color565(0x3a, 0x4a, 0x2a));
  g.fillRect(0, GROUND, W, H - GROUND, DIRT);
  g.fillRect(0, GROUND + 4, W, 9, ROAD);
  for (int x = 4; x < W; x += 16) g.fillRect(x, GROUND + 8, 6, 1, DIRT);  // road stays put
  // The woods on the left (back row).
  pine(g, 6, GROUND - 2, 46, PINE_DARK);
  pine(g, 30, GROUND - 6, 38, PINE_DARK);
  pine(g, 18, GROUND, 40, PINE);
}

void woodsFront(lgfx::LovyanGFX &g) {  // the tree Jerry walks out from behind
  pine(g, 8, GROUND + 2, 34, PINE);
}

// Jerry, full body, walking right. (x, GROUND) = between his feet.
void jerryWalking(lgfx::LovyanGFX &g, int x, int y, float phase) {
  const float s = sinf(phase), bob = fabsf(s) * 1.5f;
  const int by = y - 15 - (int)bob;
  // legs (alternate), arms (swing opposite)
  g.drawWideLine(x - 2, by + 5, x - 2 + (int)(4 * s), y, 2.4f, FUR_DARK);
  g.drawWideLine(x + 3, by + 5, x + 3 - (int)(4 * s), y, 2.4f, FUR_DARK);
  g.drawWideLine(x - 5, by - 3, x - 6 - (int)(4 * s), by + 6, 2.0f, FUR_DARK);
  // body: shaggy
  g.fillEllipse(x, by, 7, 9, FUR);
  for (int k = -2; k <= 2; k++)
    g.fillTriangle(x + k * 3 - 2, by + 5, x + k * 3 + 2, by + 5, x + k * 3, by + 10, FUR_DARK);
  g.drawWideLine(x + 5, by - 3, x + 6 + (int)(4 * s), by + 6, 2.0f, FUR);
  // head: big, with the grin and googly eyes
  const int hx = x + 2, hy = by - 13;
  for (int k = 0; k < 8; k++) {
    const float a = k * 0.785f;
    g.fillTriangle(hx + (int)(cosf(a - 0.3f) * 6), hy + (int)(sinf(a - 0.3f) * 6),
                   hx + (int)(cosf(a + 0.3f) * 6), hy + (int)(sinf(a + 0.3f) * 6),
                   hx + (int)(cosf(a) * 9), hy + (int)(sinf(a) * 9) + 1, FUR_DARK);
  }
  g.fillCircle(hx, hy, 7, FUR);
  g.fillEllipse(hx + 1, hy + 1, 5, 5, FUR_LIGHT);
  g.fillCircle(hx - 1, hy - 1, 2, TFT_WHITE);
  g.fillCircle(hx + 3, hy - 1, 2, TFT_WHITE);
  g.drawPixel(hx, hy - 1, INK);
  g.drawPixel(hx + 4, hy - 1, INK);
  g.fillRect(hx - 1, hy + 3, 5, 2, GRIN);
  g.drawWideLine(hx - 3, hy - 4, hx, hy - 4, 1.0f, INK);  // his bushy brows
  g.drawWideLine(hx + 2, hy - 5, hx + 5, hy - 4, 1.0f, INK);
}

// Jerry sitting in the Jeep: head and shoulders above the door.
void jerrySeated(lgfx::LovyanGFX &g, int x, int y) {
  g.fillEllipse(x, y + 4, 6, 4, FUR_DARK);  // shoulders
  for (int k = 0; k < 8; k++) {
    const float a = k * 0.785f;
    g.fillTriangle(x + (int)(cosf(a - 0.3f) * 5), y - 4 + (int)(sinf(a - 0.3f) * 5),
                   x + (int)(cosf(a + 0.3f) * 5), y - 4 + (int)(sinf(a + 0.3f) * 5),
                   x + (int)(cosf(a) * 8), y - 3 + (int)(sinf(a) * 8), FUR_DARK);
  }
  g.fillCircle(x, y - 4, 6, FUR);
  g.fillCircle(x + 1, y - 5, 2, TFT_WHITE);
  g.fillCircle(x + 4, y - 5, 2, TFT_WHITE);
  g.drawPixel(x + 2, y - 5, INK);
  g.drawPixel(x + 5, y - 5, INK);
  g.fillRect(x, y - 1, 5, 2, GRIN);
  g.drawWideLine(x - 1, y - 8, x + 2, y - 8, 1.0f, INK);  // brows
  g.drawWideLine(x + 3, y - 9, x + 6, y - 8, 1.0f, INK);
}

// Road wheel: a knobby off-road tyre (tread lugs turn with the wheel).
void wheel(lgfx::LovyanGFX &g, int cx, int cy, float angle) {
  for (int k = 0; k < 10; k++) {
    const float a = angle + k * 0.628f;
    g.fillCircle(cx + (int)lroundf(cosf(a) * 7), cy + (int)lroundf(sinf(a) * 7), 1, INK);
  }
  g.fillCircle(cx, cy, 7, INK);
  g.drawCircle(cx, cy, 5, lgfx::color565(0x30, 0x30, 0x30));  // sidewall
  g.fillCircle(cx, cy, 3, GREY);
  for (int k = 0; k < 3; k++) {
    const float a = angle + k * 2.094f;
    g.drawLine(cx, cy, cx + (int)(cosf(a) * 3), cy + (int)(sinf(a) * 3), INK);
  }
}

// A classic open-top Jeep facing right. x = rear bumper.
void jeep(lgfx::LovyanGFX &g, int x, float wheelAngle, bool driver) {
  const int gy = GROUND;
  // Spare tyre on the tailgate, seen side-on: a tall knobby black slab.
  g.fillRect(x + 7, gy - 19, 3, 4, INK);                                // mount
  g.fillRoundRect(x + 2, gy - 27, 6, 20, 3, INK);
  for (int k = 0; k < 6; k++) g.fillRect(x + 1, gy - 25 + k * 3, 1, 2, INK);  // tread lugs
  g.drawFastVLine(x + 6, gy - 25, 16, lgfx::color565(0x30, 0x30, 0x30));      // sidewall
  g.drawWideLine(x + 12, gy - 20, x + 12, gy - 33, 1.6f, INK);   // roll bar
  g.drawWideLine(x + 12, gy - 33, x + 29, gy - 33, 1.6f, INK);
  if (driver) jerrySeated(g, x + 22, gy - 24);
  g.fillRoundRect(x + 9, gy - 22, 40, 13, 2, OLIVE);           // tub
  g.fillRect(x + 31, gy - 25, 17, 4, OLIVE);                    // hood
  g.fillRect(x + 47, gy - 25, 3, 15, OLIVE_DARK);               // grille
  for (int k = 0; k < 3; k++) g.drawFastVLine(x + 48, gy - 23 + k * 4, 2, INK);
  g.fillCircle(x + 48, gy - 21, 2, CREAM);                      // headlight
  g.drawWideLine(x + 30, gy - 25, x + 28, gy - 36, 1.4f, INK);  // windshield frame
  g.drawWideLine(x + 31, gy - 25, x + 29, gy - 35, 1.0f, GLASS);
  g.fillRect(x + 9, gy - 16, 40, 2, OLIVE_DARK);                // body line
  g.fillRect(x + 9, gy - 13, 14, 3, INK);                       // fender flares
  g.fillRect(x + 33, gy - 13, 14, 3, INK);
  wheel(g, x + 16, gy - 7, wheelAngle);
  wheel(g, x + 40, gy - 7, wheelAngle);
}

void title(lgfx::LovyanGFX &g, float k) {  // k: 0..1 pop-in
  if (k <= 0) return;
  g.setTextSize(2);
  const char *name = "JerryOS";
  const int w = g.textWidth(name);
  const int y = 12 + (int)((1 - ease(k)) * -20);  // drops in from the top
  g.setTextColor(INK);
  g.setCursor((W - w) / 2 + 1, y + 1);
  g.print(name);
  g.setTextColor(GOLD);
  g.setCursor((W - w) / 2, y);
  g.print(name);
  g.setTextSize(1);
  if (k > 0.6f) {
    char v[16];
    snprintf(v, sizeof v, "v%s", JERRYOS_VERSION);
    g.setTextColor(CREAM);
    g.setCursor((W - g.textWidth(v)) / 2, y + 19);
    g.print(v);
    // Say hi on Instagram (on a dark tag so it reads over any intro).
    const char *ig = "IG @5quatchRated";
    const int iw = g.textWidth(ig);
    g.fillRoundRect((W - iw) / 2 - 3, y + 29, iw + 6, 11, 3, INK);
    g.setTextColor(GOLD);
    g.setCursor((W - iw) / 2, y + 31);
    g.print(ig);
  }
}

// "press any button", blinking, once the story is over.
void pressAnyButton(lgfx::LovyanGFX &g) {
  if ((millis() / 600) % 2 == 0) return;
  const char *msg = "press any button";
  g.fillRoundRect((W - g.textWidth(msg)) / 2 - 4, 58, g.textWidth(msg) + 8, 13, 4, INK);
  g.setTextColor(CREAM);
  g.setCursor((W - g.textWidth(msg)) / 2, 61);
  g.print(msg);
}

uint32_t hash(uint32_t v) {  // cheap repeatable "random" for film grain etc.
  v ^= v >> 16;
  v *= 0x7feb352du;
  v ^= v >> 15;
  v *= 0x846ca68bu;
  return v ^ (v >> 16);
}

// ===========================================================================
// 1. Jeep Jam (scene time runs at 60%: the story is 2.6 s of scene time)
// ===========================================================================
void drawJeepJam(lgfx::LovyanGFX &g, int32_t t) {
  scenery(g);
  const int jeepX = 70 + (t < 1650 ? 0 : (int)((t - 1650) * (float)(t - 1650) / 9000.0f));  // speeds up
  const float wheelAngle = t < 1650 ? 0 : (jeepX - 70) / 7.0f;
  if (t < 1350) {
    // Walk out of the woods to the Jeep (behind the front tree at first).
    const float k = ease((t - 150) / 1100.0f);
    const int x = -8 + (int)(k * 72);
    jeep(g, jeepX, wheelAngle, false);
    jerryWalking(g, x, GROUND, t / 95.0f);
    woodsFront(g);
  } else if (t < 1650) {
    // Hop in: an arc up into the driver's seat.
    const float k = ease((t - 1350) / 300.0f);
    jeep(g, jeepX, wheelAngle, false);
    if (k < 0.85f) {
      const int x = 64 + (int)(k * 28);
      const int y = GROUND - (int)(sinf(k * 3.1416f) * 14) - (int)(k * 6);
      jerryWalking(g, x, y, 0);
    } else {
      jeep(g, jeepX, wheelAngle, true);
    }
    woodsFront(g);
  } else {
    // Drive off with a puff of dust.
    for (int p = 0; p < 4; p++) {
      const float age = clamp01((t - 1650 - p * 120) / 500.0f);
      if (age <= 0 || age >= 1) continue;
      const int r = 2 + (int)(age * 6);
      const int px = 70 + 2 + p * 6 + (int)((jeepX - 70) * 0.6f * (1 - age)) - (int)(age * 8);
      g.fillCircle(px, GROUND - 4 - (int)(age * 6), r, lgfx::color565(0xb8, 0x9a, 0x78));
    }
    jeep(g, jeepX, wheelAngle, true);
    woodsFront(g);
  }
}

// ===========================================================================
// 2. Bluff Creek: an old, shaky, sepia home movie by a creek. Jerry strides
// past the trees, swinging those long arms, and halfway across he turns and
// looks right at the camera. (A tribute to the most famous 59 seconds of
// film in squatchery, 1967.)
// ===========================================================================
const uint16_t S_HAZE = lgfx::color565(0xd8, 0xc4, 0x9c);
const uint16_t S_SAND = lgfx::color565(0xbe, 0xa0, 0x72);
const uint16_t S_MID = lgfx::color565(0x8c, 0x6e, 0x4a);
const uint16_t S_DARK = lgfx::color565(0x56, 0x40, 0x2a);
const uint16_t S_DEEP = lgfx::color565(0x30, 0x22, 0x16);
const uint16_t S_FIG = lgfx::color565(0x26, 0x18, 0x0e);   // Jerry, on old film
const uint16_t S_FIG_HI = lgfx::color565(0x4e, 0x36, 0x24);

// The walk: big shoulders, long swinging arms, a bent-knee stride.
// (x, y) = between his feet. look: 0 = facing his way, 1 = looking at you.
void squatchStride(lgfx::LovyanGFX &g, int x, int y, float phase, float look) {
  const float s = sinf(phase);
  const int hipY = y - 26, shY = y - 44;
  // One leg / arm: d = +1 near side, -1 far side (they swing opposite).
  auto leg = [&](int d, uint16_t c) {
    const int kx = x + (int)(8 * s * d) + 2, ky = hipY + 13;
    const int fx = x + (int)(12 * s * d) - 2;
    g.drawWideLine(x, hipY, kx, ky, 4.6f, c);
    g.drawWideLine(kx, ky, fx, y - 1, 4.0f, c);
    g.fillRoundRect(fx - 2, y - 3, 8, 3, 1, c);  // big foot
  };
  auto arm = [&](int d, uint16_t c) {
    const int ex = x + 1 - (int)(7 * s * d), ey = shY + 12;
    g.drawWideLine(x + 1, shY + 2, ex, ey, 3.8f, c);
    g.drawWideLine(ex, ey, x + 2 - (int)(13 * s * d), shY + 26, 3.4f, c);  // hands down by the knees
  };
  leg(-1, S_DARK);
  arm(-1, S_DARK);
  g.fillEllipse(x, hipY - 8, 7, 12, S_FIG);    // body
  g.fillEllipse(x + 1, shY + 2, 10, 7, S_FIG);  // those shoulders
  g.drawWideLine(x - 5, shY + 4, x - 4, hipY - 4, 1.0f, S_FIG_HI);  // fur on the back
  leg(1, S_FIG);
  arm(1, S_FIG);
  // head: a pointy crest in profile, or Jerry's face when he looks back
  const int hx = x + 4 - (int)(look * 3), hy = shY - 6;
  if (look < 0.5f) {
    g.fillCircle(hx, hy, 6, S_FIG);
    g.fillTriangle(hx - 5, hy - 2, hx + 2, hy - 4, hx - 2, hy - 11, S_FIG);  // crest
    g.fillRect(hx + 4, hy - 1, 3, 4, S_FIG);                                // brow / face
  } else {
    g.fillCircle(hx, hy, 7, S_FIG);
    g.fillTriangle(hx - 5, hy - 4, hx + 5, hy - 4, hx, hy - 12, S_FIG);
    g.fillEllipse(hx, hy + 1, 5, 5, S_MID);     // face
    g.fillCircle(hx - 2, hy - 1, 1, S_HAZE);    // googly eyes, on you
    g.fillCircle(hx + 2, hy - 1, 1, S_HAZE);
    g.drawPixel(hx - 2, hy - 1, S_FIG);
    g.drawPixel(hx + 2, hy - 1, S_FIG);
    g.drawFastHLine(hx - 2, hy + 3, 5, S_HAZE);  // and a grin
  }
}

void drawBluffCreek(lgfx::LovyanGFX &g, int32_t t) {
  const uint32_t frame = t / 42;  // ~24 frames a second, like the real thing
  if (t < 900) {  // film leader: 2... 1...
    g.fillScreen(S_MID);
    const float a = (t % 450) / 450.0f * 360.0f;
    g.fillArc(64, 64, 0, 44, -90, -90 + a, S_SAND);
    g.drawCircle(64, 64, 44, S_DEEP);
    g.drawCircle(64, 64, 36, S_DEEP);
    g.drawFastHLine(0, 64, W, S_DEEP);
    g.drawFastVLine(64, 0, H, S_DEEP);
    g.setTextSize(4);
    g.setTextColor(S_DEEP);
    g.setCursor(53, 50);
    g.print(t < 450 ? "2" : "1");
    g.setTextSize(1);
  } else {
    // The camera is hand-held and twitchy: everything shifts a pixel or two,
    // more when he turns around.
    const bool lookTime = t > 3300 && t < 4500;
    const int amp = lookTime ? 2 : 1;
    const int ox = (int)(hash(frame) % (2 * amp + 1)) - amp;
    const int oy = (int)(hash(frame + 99) % (2 * amp + 1)) - amp;
    g.fillScreen(S_HAZE);
    // the woods: a faded back row, a nearer row, pale trunks
    const uint16_t back = lgfx::color565(0xa8, 0x8c, 0x64), near = lgfx::color565(0x7c, 0x62, 0x42);
    g.fillRect(0, 40 + oy, W, 24, back);
    for (int k = 0; k < 11; k++) {
      const int x = k * 13 + (int)(hash(k) % 9) + ox - 6, h = 26 + (int)(hash(k + 20) % 20);
      g.fillTriangle(x, 46 + oy - h, x - 7, 46 + oy, x + 7, 46 + oy, back);
    }
    for (int k = 0; k < 7; k++) {
      const int x = k * 20 + (int)(hash(k + 40) % 12) + ox - 4, h = 34 + (int)(hash(k + 60) % 18);
      g.fillTriangle(x, 52 + oy - h, x - 8, 54 + oy, x + 8, 54 + oy, near);
      g.fillRect(x - 8, 52 + oy, 17, 12, near);
      g.drawFastVLine(x + 2, 54 + oy, 10, S_DARK);  // trunk
    }
    // the sandbar, with logs and stones
    g.fillRect(0, 64 + oy, W, H, S_SAND);
    g.fillRect(0, 64 + oy, W, 2, S_MID);
    g.drawWideLine(4 + ox, 80 + oy, 38 + ox, 74 + oy, 2.6f, S_MID);
    g.drawWideLine(86 + ox, 120 + oy, 126 + ox, 112 + oy, 3.2f, S_MID);
    g.fillEllipse(104 + ox, 78 + oy, 4, 2, S_MID);
    g.fillEllipse(20 + ox, 116 + oy, 5, 2, S_MID);
    // Jerry: steady across, feet on the beat (80 BPM), a look back at ~3.9 s
    const int x = -20 + (int)((t - 900) * 0.027f);
    const float look = lookTime ? clamp01((1 - fabsf(t - 3900) / 600.0f) * 2) : 0;
    squatchStride(g, x + ox, 108 + oy, (t - 900) / 238.7f, look);
    // Old film: grain and the odd scratch.
    for (int k = 0; k < 40; k++) {
      const uint32_t r = hash(frame * 64 + k);
      g.drawPixel(r % W, (r >> 8) % H, (r >> 16) & 1 ? S_DEEP : S_HAZE);
    }
    if (hash(frame + 7) % 3 == 0) g.drawFastVLine(hash(frame + 3) % W, 0, H, S_HAZE);
  }
  for (int c = 0; c < 4; c++) {  // the rounded film gate
    const int x = c & 1 ? W - 1 : 0, y = c & 2 ? H - 1 : 0, dx = c & 1 ? -1 : 1, dy = c & 2 ? -1 : 1;
    g.fillTriangle(x, y, x + dx * 14, y, x, y + dy * 14, INK);
  }
  g.fillRect(0, 0, 2, H, INK);
  g.fillRect(W - 2, 0, 2, H, INK);
}

// ===========================================================================
// 3. Selfie: Jerry's phone, at arm's length, from the driver's seat. He
// leans in, looks around, the aviators drop, the grin.
// ===========================================================================
void aviators(lgfx::LovyanGFX &g, int cx, int cy, float sc, float shine) {
  const uint16_t lens = lgfx::color565(0x1c, 0x24, 0x34);
  const int rx = (int)(14 * sc), ry = (int)(11 * sc), off = (int)(16 * sc);
  for (int side = -1; side <= 1; side += 2) {
    const int lx = cx + side * off;
    g.fillEllipse(lx, cy, rx, ry, lens);
    g.fillTriangle(lx - rx + 2, cy + 2, lx + rx - 2, cy + 2, lx - side * 3, cy + ry + 4, lens);  // teardrop
    g.drawEllipse(lx, cy, rx, ry, GOLD);
    g.drawLine(lx - 7, cy - 3, lx - 1, cy - 8, lgfx::color565(0x60, 0x78, 0x98));  // reflections
    g.drawLine(lx - 8, cy + 2, lx + 1, cy - 6, lgfx::color565(0x40, 0x50, 0x68));
  }
  g.drawWideLine(cx - off + rx - 2, cy - ry / 2, cx + off - rx + 2, cy - ry / 2, 1.4f, GOLD);  // bridge
  if (shine > 0) {  // ting!
    const int sx = cx - off - rx / 2, sy = cy - ry / 2, r = (int)(shine * 9);
    g.fillTriangle(sx - r, sy, sx + r, sy, sx, sy - 1, TFT_WHITE);
    g.fillTriangle(sx - r, sy, sx + r, sy, sx, sy + 1, TFT_WHITE);
    g.fillTriangle(sx, sy - r, sx, sy + r, sx - 1, sy, TFT_WHITE);
    g.fillTriangle(sx, sy - r, sx, sy + r, sx + 1, sy, TFT_WHITE);
  }
}

JerryFace selfieFace;  // the real home-screen Jerry, close up

void drawSelfie(lgfx::LovyanGFX &g, int32_t t) {
  // Behind him: sky, the woods whipping past, the roll bar.
  static const uint16_t sky[] = {lgfx::color565(0x4a, 0x7c, 0xc0), lgfx::color565(0x6e, 0x9a, 0xd0),
                                 lgfx::color565(0x9c, 0xc0, 0xe0)};
  for (int i = 0; i < 3; i++) g.fillRect(0, i * 14, W, 14, sky[i]);
  g.fillRect(0, 42, W, H - 42, lgfx::color565(0x3a, 0x5a, 0x2c));
  for (int k = 0; k < 8; k++) {  // pines flying by
    const int x = (int)((k * 41 + 400000 - t * 0.22f)) % 190 - 30;
    const int h = 40 + (k * 13) % 22;
    g.fillTriangle(x, 46 - h + 10, x - 12, 52, x + 12, 52, k & 1 ? PINE : PINE_DARK);
    g.fillRect(x - 12, 50, 24, 8, k & 1 ? PINE : PINE_DARK);
  }
  g.drawWideLine(14, H, 18, 26, 4.0f, INK);  // roll bar
  g.drawWideLine(18, 26, 110, 26, 4.0f, INK);
  g.drawWideLine(110, 26, 114, H, 4.0f, INK);

  // Leans in from below ("is this thing on?"), eyes darting around, then
  // cool: the aviators drop, and the big grin on the horn hit (~4.2 s).
  selfieFace.setMood(t < 700 ? Mood::Alert : t < 2400 ? Mood::Scanning : t < 4200 ? Mood::Idle : Mood::Approved);
  selfieFace.update(millis());
  const float sc = 0.82f;
  const float beat = fabsf(sinf(3.1416f * t / 600.0f));  // head bob, 100 BPM
  const int cx = 64 + (int)(sinf(t / 900.0f) * 2);
  const int cy = 70 + (int)((1 - ease(t / 700.0f)) * 80) + (int)(beat * (t > 3000 ? 3 : 1.5f));
  selfieFace.draw(g, cx, cy, sc, true);
  if (t > 2400) {  // aviators drop in from above, with a little bounce
    const float k = clamp01((t - 2400) / 500.0f);
    const float bounce = t < 2900 ? 0 : sinf(clamp01((t - 2900) / 300.0f) * 3.1416f) * 3;
    const int eyeY = cy - (int)(8 * sc);
    const int gy = (int)(eyeY * ease(k) - 30 * (1 - ease(k)) - bounce);
    aviators(g, cx, gy, sc, t > 4200 && t < 4700 ? sinf((t - 4200) / 500.0f * 3.1416f) : 0);
  }
  g.fillCircle(118, 126, 18, FUR_DARK);  // his arm, holding the phone (that's us)
  g.fillCircle(112, 132, 16, FUR);
}

// ===========================================================================
// 4. Cruisin': from behind the Jeep, a little off to the side, down a sunset
// highway through the pines. Arcade-style 3-D road.
// ===========================================================================
constexpr int HORIZON = 54;
// Where the road's middle is on screen line y (it bends away towards the horizon).
float roadX(int y, float bend) {
  const float p = 1 - (y - HORIZON) / (float)(H - HORIZON);
  return 64 + bend * p * p * 40;
}

void squatchSign(lgfx::LovyanGFX &g, int x, int y, int r) {  // yellow diamond on a post
  g.fillRect(x - 1, y, 2 + r / 6, r * 2, GREY);
  g.fillTriangle(x, y - r, x - r, y, x + r, y, GOLD);
  g.fillTriangle(x, y + r, x - r, y, x + r, y, GOLD);
  if (r < 5) return;
  g.fillCircle(x, y - r / 3, r / 5 + 1, INK);                 // a squatch: head,
  g.fillEllipse(x, y + r / 6, r / 4 + 1, r / 3, INK);         // body,
  g.drawLine(x - r / 6, y + r / 2, x - r / 3, y + r * 2 / 3, INK);  // striding
  g.drawLine(x + r / 6, y + r / 2, x + r / 3, y + r * 2 / 3, INK);
}

void drawCruisin(lgfx::LovyanGFX &g, int32_t t) {
  static const uint16_t sky[] = {lgfx::color565(0x34, 0x1c, 0x58), lgfx::color565(0x6e, 0x26, 0x6a),
                                 lgfx::color565(0xb8, 0x38, 0x5c), lgfx::color565(0xea, 0x6c, 0x3e),
                                 lgfx::color565(0xf8, 0xa8, 0x48)};
  for (int i = 0; i < 5; i++) g.fillRect(0, i * 11, W, 11, sky[i]);
  const float bend = sinf(t / 1500.0f) * 0.9f;
  const int sunX = (int)roadX(HORIZON, bend);
  g.fillCircle(sunX, HORIZON, 20, lgfx::color565(0xfc, 0xd8, 0x5a));
  for (int k = 0; k < 4; k++) g.fillRect(sunX - 22, HORIZON - 16 + k * 4, 44, 1 + k / 2, sky[3 + (k > 1)]);
  const uint16_t ridge = lgfx::color565(0x4a, 0x22, 0x48);
  g.fillTriangle(-10, HORIZON, 26, HORIZON - 14, 60, HORIZON, ridge);
  g.fillTriangle(70, HORIZON, 108, HORIZON - 18, 150, HORIZON, ridge);

  // The road, one screen line at a time: stripes rush towards you.
  const float travel = t * 0.010f;
  const uint16_t grassA = lgfx::color565(0x2e, 0x5a, 0x2a), grassB = lgfx::color565(0x24, 0x4a, 0x22);
  const uint16_t asphaltA = lgfx::color565(0x4a, 0x46, 0x50), asphaltB = lgfx::color565(0x44, 0x40, 0x4a);
  for (int y = HORIZON; y < H; y++) {
    const float p = (y - HORIZON + 1) / (float)(H - HORIZON);  // 0 far .. 1 near
    const bool band = ((int)(1.6f / p + travel)) & 1;
    const int cx = (int)roadX(y, bend), half = 2 + (int)(p * 54), rumble = 1 + (int)(p * 6);
    g.drawFastHLine(0, y, W, band ? grassA : grassB);
    g.drawFastHLine(cx - half - rumble, y, 2 * (half + rumble), band ? ALERT : CREAM);
    g.drawFastHLine(cx - half, y, 2 * half, band ? asphaltA : asphaltB);
    if (band) g.drawFastHLine(cx - 1 - (int)(p * 2), y, 2 + (int)(p * 4), GOLD);  // centre line
  }
  // Pines (and one sign) by the road, far ones first.
  const float SPAN = 6;
  for (int i = 5; i >= 0; i--) {
    const float z = fmodf(i * 1.0f - t * 0.0012f + 600, SPAN) + 0.4f;  // distance
    const float p = 0.42f / z;
    if (p > 1.4f) continue;
    const int y = HORIZON + (int)(p * (H - HORIZON));
    const int edge = (int)roadX(y < H ? y : H - 1, bend) + (i & 1 ? 1 : -1) * (int)(6 + p * 70);
    const int h = (int)(p * 70);
    if (h < 3) continue;
    g.fillTriangle(edge, y - h, edge - h / 3, y, edge + h / 3, y, i & 1 ? PINE : PINE_DARK);
    g.fillTriangle(edge, y - h - h / 4, edge - h / 4, y - h / 2, edge + h / 4, y - h / 2, i & 1 ? PINE : PINE_DARK);
  }
  if (t > 600 && t < 3600) {  // SQUATCH XING
    const float z = 6.4f - (t - 600) * 0.0021f;
    const float p = 0.42f / z;
    const int y = HORIZON + (int)(p * (H - HORIZON));
    squatchSign(g, (int)roadX(y < H ? y : H - 1, bend) + (int)(10 + p * 64), y - (int)(p * 44), (int)(p * 24));
  }

  // The Jeep from behind, 3/4: we see the tailgate and its left side.
  const int jx = 84 + (int)(sinf(t / 700.0f) * 4) - (int)(bend * 5);  // right lane
  const int jy = 116 + (int)((1 - ease(t / 900.0f)) * 50) + ((t / 90) % 4 == 0 ? 1 : 0);  // drives in, rumbles
  // It's angled a touch to the left (steering into the bend), so its left
  // side shows and runs off towards where its nose points.
  const int vx = jx - 80;
  auto toward = [&](int x, int y, float k, int &ox, int &oy) {  // a point k of the way to the horizon
    ox = x + (int)((vx - x) * k);
    oy = y + (int)((HORIZON - y) * k);
  };
  int ax, ay, bx, by;
  g.fillEllipse(jx, jy + 2, 30, 4, lgfx::color565(0x22, 0x20, 0x26));  // shadow
  toward(jx - 24, jy - 2, 0.2f, ax, ay);
  toward(jx - 24, jy - 26, 0.2f, bx, by);
  g.fillTriangle(jx - 24, jy - 2, ax, ay, bx, by, OLIVE_DARK);  // the side
  g.fillTriangle(jx - 24, jy - 2, jx - 24, jy - 26, bx, by, OLIVE_DARK);
  g.drawLine(jx - 24, jy - 14, (ax + bx) / 2, (ay + by) / 2, OLIVE);  // body line
  g.fillEllipse(ax + 1, ay - 3, 4, 6, INK);                    // front tyre
  // Jerry: back of his head, shoulders, arm out over the door (waving at the title)
  g.fillEllipse(jx - 6, jy - 30, 15, 9, FUR_DARK);
  for (int k = 0; k < 10; k++) {
    const float a = 3.1416f + k * 0.35f;
    g.fillTriangle(jx - 6 + (int)(cosf(a - 0.2f) * 8), jy - 44 + (int)(sinf(a - 0.2f) * 8),
                   jx - 6 + (int)(cosf(a + 0.2f) * 8), jy - 44 + (int)(sinf(a + 0.2f) * 8),
                   jx - 6 + (int)(cosf(a) * 13), jy - 44 + (int)(sinf(a) * 13), FUR_DARK);
  }
  g.fillCircle(jx - 6, jy - 42, 10, FUR_DARK);  // back of his head: all hair
  for (int k = -2; k <= 2; k++)
    g.fillTriangle(jx - 6 + k * 4 - 2, jy - 50 + abs(k), jx - 6 + k * 4 + 2, jy - 50 + abs(k),
                   jx - 6 + k * 5, jy - 36, k & 1 ? FUR : FUR_DARK);
  const bool wave = t > 3200 && t < 4400;
  const int handX = wave ? jx - 30 + (int)(sinf(t / 70.0f) * 4) : jx - 32;
  const int handY = wave ? jy - 58 : jy - 18;
  g.drawWideLine(jx - 16, jy - 32, jx - 28, wave ? jy - 44 : jy - 26, 3.6f, FUR);
  g.drawWideLine(jx - 28, wave ? jy - 44 : jy - 26, handX, handY, 3.4f, FUR);
  g.fillCircle(handX, handY, 3, FUR_LIGHT);
  // roll bar, tub, tail lights, bumper, wheels, and the spare (round, from here)
  g.drawWideLine(jx - 18, jy - 24, jx - 16, jy - 50, 2.4f, INK);
  g.drawWideLine(jx - 16, jy - 50, jx + 16, jy - 50, 2.4f, INK);
  g.drawWideLine(jx + 16, jy - 50, jx + 18, jy - 24, 2.4f, INK);
  g.fillRoundRect(jx - 24, jy - 26, 48, 24, 2, OLIVE);
  g.fillRect(jx - 22, jy - 22, 4, 8, ALERT);
  g.fillRect(jx + 18, jy - 22, 4, 8, ALERT);
  g.fillRect(jx - 26, jy - 4, 52, 4, INK);
  g.fillRoundRect(jx - 28, jy - 10, 9, 14, 2, INK);
  g.fillRoundRect(jx + 19, jy - 10, 9, 14, 2, INK);
  for (int k = 0; k < 12; k++) {
    const float a = k * 0.5236f + t / 200.0f;
    g.fillCircle(jx + (int)(cosf(a) * 10), jy - 15 + (int)(sinf(a) * 10), 1, INK);
  }
  g.fillCircle(jx, jy - 15, 10, INK);
  g.fillCircle(jx, jy - 15, 4, GREY);
  // exhaust puffs drifting back at us
  for (int k = 0; k < 3; k++) {
    const float age = ((t + k * 260) % 780) / 780.0f;
    g.fillCircle(jx + 14 + (int)(age * 10), jy + (int)(age * 10), 1 + (int)(age * 4),
                 lgfx::color565(0x9a, 0x92, 0xa0));
  }
}

// ===========================================================================
struct Intro {
  const char *name;
  void (*draw)(lgfx::LovyanGFX &, int32_t);
  int32_t sceneMs;  // the story, in scene time; then "press any button"
  int32_t speed;    // scene time per real time, %
  int32_t titleAt;  // when "JerryOS" drops in (scene time)
};
const Intro kIntros[INTRO_COUNT] = {
    {"Jeep Jam", drawJeepJam, 2600, 60, 1750},
    {"Bluff Creek", drawBluffCreek, 6000, 100, 4800},
    {"Selfie", drawSelfie, 5000, 100, 4300},
    {"Cruisin'", drawCruisin, 4300, 100, 3200},
};

int8_t forced = -1;  // replayIntro(): play this one next

class IntroScreen : public Screen {
 public:
  const char *tag() const override { return "JerryOS"; }
  Hints hints() const override { return {nullptr, nullptr, nullptr}; }
  bool fullScreen() const override { return true; }
  void enter(Shell &shell) override {
    if (started_ && forced < 0) return;  // back from an alert: carry on, don't restart
    started_ = true;
    start_ = millis();
    which_ = pick(shell.settings().introIdx);
    shell.badges().introSeen(which_);  // Funk Soul Bro badge: see all four
    shell.saveBadges();
    if (shell.settings().tuneVol) speaker::playIntro(shell.settings().tuneVol, which_);  // the funk
  }
  // Any button skips the intro (and cuts the music).
  void onLeftShort(Shell &shell) override { leave(shell); }
  void onRightShort(Shell &shell) override { leave(shell); }
  void onRightLong(Shell &shell) override { leave(shell); }
  void update(Shell &shell, uint32_t now) override {
    // Keep the screen awake while the story plays; once it's waiting on the
    // title, normal dimming applies (Jerry keeps watching in the background).
    if (sceneTime(now) < kIntros[which_].sceneMs) shell.wake();
  }

  void draw(Shell &, lgfx::LovyanGFX &g, int, int) override {
    // Signed on purpose: "t - titleAt" must go negative early on, not wrap around.
    const Intro &in = kIntros[which_];
    const int32_t t = sceneTime(millis());
    in.draw(g, t);
    title(g, (t - in.titleAt) / 450.0f);
    if (t >= in.sceneMs) pressAnyButton(g);
  }

 private:
  // Which intro: the forced one, the one picked in Settings, or (random) any
  // but the one played last time.
  static int pick(uint8_t setting) {
    int i;
    if (forced >= 0) {
      i = forced;
      forced = -1;
      return i;
    }
    if (setting > 0 && setting <= INTRO_COUNT) return setting - 1;
    Preferences p;
    const int last = p.begin("jerryos", true) ? p.getUChar("ilast", 255) : 255;
    p.end();
    i = random(last < INTRO_COUNT ? INTRO_COUNT - 1 : INTRO_COUNT);
    if (last < INTRO_COUNT && i >= last) i++;
    if (p.begin("jerryos", false)) {
      p.putUChar("ilast", i);
      p.end();
    }
    return i;
  }
  // Scene time: real time scaled by the intro's speed.
  int32_t sceneTime(uint32_t now) const { return (int32_t)((int64_t)(now - start_) * kIntros[which_].speed / 100); }
  void leave(Shell &shell) {
    speaker::stop();
    shell.back();
  }
  uint32_t start_ = 0;
  int which_ = 0;
  bool started_ = false;  // music once per power-on (enter() also runs on "back")
};

IntroScreen introObj;

}  // namespace

Screen &introScreen() { return introObj; }
const char *introName(int i) { return i >= 0 && i < INTRO_COUNT ? kIntros[i].name : "?"; }
void replayIntro(int i) { forced = i >= 0 && i < INTRO_COUNT ? i : 0; }
