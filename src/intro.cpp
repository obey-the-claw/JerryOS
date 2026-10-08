// intro.cpp - the boot intro: Jerry walks out of the woods, hops in his Jeep
// and drives off, and "JerryOS" pops up. About 4 s, then it waits on the
// title for "press any button". Any button skips it at any point.
//
// Drawn in code (like Jerry himself), full screen, at the shell's ~30 fps.
// Everything is a function of the time since the intro started, so it plays
// the same at any frame rate.

#include "intro.h"

#include <Arduino.h>
#include <math.h>

#include "shell.h"
#include "speaker.h"
#include "theme.h"

using namespace theme;

namespace {

constexpr int32_t SCENE_MS = 2600;  // the story, in "scene time" (see SPEED)
constexpr int32_t SPEED = 60;       // percent: plays at 60% speed (~4.3 s real time)
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
  }
}

class IntroScreen : public Screen {
 public:
  const char *tag() const override { return "JerryOS"; }
  Hints hints() const override { return {nullptr, nullptr, nullptr}; }
  bool fullScreen() const override { return true; }
  void enter(Shell &shell) override {
    if (started_) return;  // coming back from an alert: carry on, don't restart
    started_ = true;
    start_ = millis();
    if (shell.settings().tuneVol) speaker::playIntro(shell.settings().tuneVol);  // the funk
  }
  // Any button skips the intro (and cuts the music).
  void onLeftShort(Shell &shell) override { leave(shell); }
  void onRightShort(Shell &shell) override { leave(shell); }
  void onRightLong(Shell &shell) override { leave(shell); }
  void update(Shell &shell, uint32_t now) override {
    // Keep the screen awake while the story plays; once it's waiting on the
    // title, normal dimming applies (Jerry keeps watching in the background).
    if (sceneTime(now) < SCENE_MS) shell.wake();
  }

  void draw(Shell &, lgfx::LovyanGFX &g, int, int) override {
    // Signed on purpose: "t - 1750" must go negative early on, not wrap around.
    const int32_t t = sceneTime(millis());
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
    title(g, (t - 1750) / 450.0f);
    if (t >= SCENE_MS && (millis() / 600) % 2) {  // the end: wait for a button
      const char *msg = "press any button";
      g.fillRoundRect((W - g.textWidth(msg)) / 2 - 4, 54, g.textWidth(msg) + 8, 13, 4, INK);
      g.setTextColor(CREAM);
      g.setCursor((W - g.textWidth(msg)) / 2, 57);
      g.print(msg);
    }
  }

 private:
  // Scene time: real time slowed to SPEED %, so the story plays calmly.
  int32_t sceneTime(uint32_t now) const { return (int32_t)((int64_t)(now - start_) * SPEED / 100); }
  void leave(Shell &shell) {
    speaker::stop();
    shell.back();
  }
  uint32_t start_ = 0;
  bool started_ = false;  // music once per power-on (enter() also runs on "back")
};

IntroScreen introObj;

}  // namespace

Screen &introScreen() { return introObj; }
