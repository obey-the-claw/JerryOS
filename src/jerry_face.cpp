// jerry_face.cpp - placeholder Jerry drawn with circles, triangles and lines.
//
// Jerry's signature look (from the brief): big round googly eyes looking
// down-left, mismatched bushy black brows, small brown triangle nose, huge
// orange grin with white zigzag teeth, shaggy brown fur.
//
// All coordinates below are in "face units": the head is a circle of radius
// 46 centred on (0, 0). draw() scales them to pixels.

#include "jerry_face.h"

#include <Arduino.h>
#include <math.h>

#include "theme.h"

const char *moodName(Mood m) {
  switch (m) {
    case Mood::Idle: return "idle";
    case Mood::Scanning: return "scanning";
    case Mood::Alert: return "alert";
    case Mood::Approved: return "approved";
    case Mood::Sleepy: return "sleepy";
  }
  return "?";
}

void JerryFace::setMood(Mood m) { mood_ = m; }

void JerryFace::flash(Mood m, uint32_t ms) {
  flashMood_ = m;
  flashUntil_ = millis() + ms;
  if (flashUntil_ == 0) flashUntil_ = 1;  // 0 means "no flash"
}

void JerryFace::pickNextGlance(uint32_t now) {
  // Jerry's resting look is down-left. Every few seconds he glances
  // somewhere else, then comes back.
  static const float spots[][2] = {
      {-0.8f, 0.0f}, {0.8f, 0.0f}, {0.7f, 0.5f}, {0.0f, -0.7f}, {0.0f, 0.0f}, {-0.6f, -0.5f}};
  const bool atRest = fabsf(targetX_ + 0.6f) < 0.01f && fabsf(targetY_ - 0.5f) < 0.01f;
  if (atRest) {
    const auto &s = spots[random(sizeof(spots) / sizeof(spots[0]))];
    targetX_ = s[0];
    targetY_ = s[1];
    nextGlance_ = now + random(600, 1600);  // short look elsewhere
  } else {
    targetX_ = -0.6f;
    targetY_ = 0.5f;
    nextGlance_ = now + random(2000, 5000);  // long rest
  }
}

void JerryFace::update(uint32_t now) {
  now_ = now;
  if (flashUntil_ && (int32_t)(now - flashUntil_) >= 0) flashUntil_ = 0;
  const Mood m = mood();

  // Blinking: every 2-6 s, sometimes a quick double blink. Sleepy blinks slowly.
  if ((int32_t)(now - nextBlink_) >= 0) {
    blinkEnd_ = now + (m == Mood::Sleepy ? 350 : 120);
    nextBlink_ = blinkEnd_ + (random(4) == 0 ? 160 : random(2000, 6000));
  }

  // Where the eyes want to point.
  switch (m) {
    case Mood::Idle:
      if ((int32_t)(now - nextGlance_) >= 0) pickNextGlance(now);
      break;
    case Mood::Scanning:
      targetX_ = sinf(now / 380.0f) * 0.9f;  // sweep side to side
      targetY_ = 0.1f;
      lookX_ = targetX_;
      lookY_ = targetY_;
      return;
    case Mood::Alert:
      targetX_ = 0.0f;  // staring straight at you
      targetY_ = 0.0f;
      break;
    case Mood::Approved:
      break;
    case Mood::Sleepy:
      targetX_ = -0.2f;
      targetY_ = 0.8f;
      break;
  }
  // Ease toward the target so the eyes glide instead of teleporting.
  lookX_ += (targetX_ - lookX_) * 0.3f;
  lookY_ += (targetY_ - lookY_) * 0.3f;
}

void JerryFace::draw(lgfx::LovyanGFX &g, int cx, int cy, float s, bool shoulders) const {
  using namespace theme;
  const Mood m = mood();
  const bool blinking = (int32_t)(now_ - blinkEnd_) < 0 && m != Mood::Approved;

  auto X = [&](float x) { return cx + (int)lroundf(x * s); };
  auto Y = [&](float y) { return cy + (int)lroundf(y * s); };
  auto L = [&](float v) { return max(1, (int)lroundf(v * s)); };
  auto line = [&](float x0, float y0, float x1, float y1, float r, uint16_t c) {
    g.drawWideLine(X(x0), Y(y0), X(x1), Y(y1), max(0.6f, r * s), c);
  };

  // --- Neck and shoulders (Bigfoot style) ---
  // Hardly any neck: the head sits low between big, hunched, shaggy
  // shoulders. Fur slopes from the sides of the head down onto them, and
  // the bottom runs off the screen like a portrait.
  if (shoulders) {
    g.fillEllipse(X(0), Y(78), L(66), L(40), FUR_DARK);  // shoulder mass
    for (int side = -1; side <= 1; side += 2) {          // slope head -> shoulders
      g.fillTriangle(X(side * 30), Y(10), X(side * 62), Y(52), X(side * 8), Y(60), FUR_DARK);
    }
    // Shaggy edge along the shoulders: clumps hanging down and outward.
    for (int i = 0; i < 14; i++) {
      const float t = i / 13.0f;                          // 0 = left, 1 = right
      const float x = -60 + t * 120;
      const float y = 44 + fabsf(x) * 0.18f + ((i * 7) % 3);  // edge drops toward the arms
      const float len = 9 + (i * 5) % 6;
      const float lean = x * 0.12f;                       // hair falls outward
      g.fillTriangle(X(x - 6), Y(y - 4), X(x + 6), Y(y - 4), X(x + lean), Y(y + len),
                     (i % 3) ? FUR_DARK : BAR);
      g.fillTriangle(X(x - 4), Y(y - 7), X(x + 4), Y(y - 7), X(x + lean * 0.6f), Y(y + len - 4),
                     (i % 2) ? FUR : FUR_DARK);
    }
    // Lighter chest fur peeking out under the chin.
    g.fillEllipse(X(0), Y(64), L(26), L(16), FUR);
    for (int i = -3; i <= 3; i++)
      g.fillTriangle(X(i * 8 - 5), Y(62), X(i * 8 + 5), Y(62), X(i * 8 + (i % 2 ? 2 : -2)),
                     Y(74 + (i * i) % 4), FUR);
  }

  // --- Shaggy fur ---
  // Uneven clumps all round the head that droop downward (gravity), with a
  // darker layer underneath for depth, then the head on top.
  // Pseudo-random but fixed, so the fur doesn't "boil" between frames.
  auto clump = [&](int i, float baseR, float lenMul, uint16_t col) {
    const int n = 26;
    const float a = i * 2.0f * PI / n + ((i * 53) % 11 - 5) * 0.015f;
    const float len = (6.0f + ((i * 29) % 7)) * lenMul;  // 6..12, uneven
    const float half = 0.12f + ((i * 17) % 4) * 0.02f;   // clump width
    const float bx = cosf(a), by = sinf(a);
    const float tipX = bx * (baseR + len) + (i % 2 ? 1.5f : -1.5f);
    const float tipY = by * (baseR + len) + len * 0.45f;  // droop
    g.fillTriangle(X(cosf(a - half) * baseR), Y(sinf(a - half) * baseR * 1.05f),
                   X(cosf(a + half) * baseR), Y(sinf(a + half) * baseR * 1.05f), X(tipX),
                   Y(tipY), col);
  };
  for (int i = 0; i < 26; i++) clump(i, 40, 1.1f, FUR_DARK);              // under-layer
  for (int i = 0; i < 26; i++) clump(i + 13, 41, 0.8f, (i % 4) ? FUR : FUR_DARK);
  if (shoulders) {
    // Thick ruff under the chin: long clumps that merge into the chest.
    for (int i = -4; i <= 4; i++) {
      const float x = i * 8.0f + ((i * 3) % 2);
      const float len = 14 + ((i * 11 + 44) % 5) * 2;
      g.fillTriangle(X(x - 7), Y(34 - fabsf(x) * 0.25f), X(x + 7), Y(34 - fabsf(x) * 0.25f),
                     X(x + (i % 2 ? 2.5f : -2.5f)), Y(40 + len - fabsf(x) * 0.2f),
                     (i % 2) ? FUR_DARK : FUR);
    }
  }
  g.fillEllipse(cx, cy, L(43), L(45), FUR);
  // Messy bangs over the forehead.
  for (int i = -3; i <= 3; i++) {
    const float x = i * 8.0f + ((i * 5) % 3);
    const float drop = 9.0f + ((i * 7 + 21) % 4) * 2.0f;
    g.fillTriangle(X(x - 6), Y(-44), X(x + 6), Y(-44), X(x + (i % 2 ? 2 : -2)), Y(-44 + drop),
                   (i % 2) ? FUR_DARK : FUR);
  }

  // --- Lighter face patch ---
  g.fillEllipse(X(0), Y(6), L(36), L(34), FUR_LIGHT);

  // --- Mouth (drawn before the eyes; its cover-up rectangle overlaps them) ---
  if (m == Mood::Alert) {
    g.fillEllipse(X(0), Y(21), L(9), L(11), INK);       // "O" of surprise
    g.fillEllipse(X(0), Y(26), L(6), L(4), GRIN);
  } else if (m == Mood::Sleepy) {
    line(-9, 19, -3, 21, 1.4f, INK);                    // small wobbly mouth
    line(-3, 21, 3, 19, 1.4f, INK);
    line(3, 19, 9, 21, 1.4f, INK);
  } else {
    const float rx = (m == Mood::Approved) ? 30 : 27;
    const float ry = (m == Mood::Approved) ? 20 : 17;
    const float top = 13;
    g.fillEllipse(X(0), Y(top), L(rx), L(ry), GRIN);
    g.fillRect(X(-rx - 2), Y(top - ry - 2), L(2 * rx + 4) + 1, Y(top) - Y(top - ry - 2),
               FUR_LIGHT);                               // keep only the lower half: a grin
    // White zigzag teeth along the top edge...
    for (float x = -rx + 4; x + 7 <= rx - 3; x += 7)
      g.fillTriangle(X(x), Y(top), X(x + 7), Y(top), X(x + 3.5f), Y(top + 6), TFT_WHITE);
    // ...and a shorter row along the bottom.
    const float bot = top + ry - 4;
    for (float x = -rx / 2; x + 7 <= rx / 2; x += 7)
      g.fillTriangle(X(x), Y(bot), X(x + 7), Y(bot), X(x + 3.5f), Y(bot - 5), TFT_WHITE);
    line(-rx + 1, top, rx - 1, top, 1.0f, INK);
  }

  // --- Eyes: big round googly eyes ---
  const float er = (m == Mood::Alert) ? 14 : 12;
  const float pr = (m == Mood::Alert) ? 3.5f : 5.0f;
  for (int side = -1; side <= 1; side += 2) {
    const float ex = side * 16.0f, ey = -8.0f;
    if (m == Mood::Approved) {
      // Happy squint: upside-down V.
      line(ex - 9, ey + 3, ex, ey - 5, 2.2f, INK);
      line(ex, ey - 5, ex + 9, ey + 3, 2.2f, INK);
      continue;
    }
    if (blinking) {
      line(ex - er + 1, ey + 1, ex + er - 1, ey + 1, 1.6f, INK);
      continue;
    }
    g.fillCircle(X(ex), Y(ey), L(er), TFT_WHITE);
    g.drawCircle(X(ex), Y(ey), L(er), INK);
    const float px = ex + lookX_ * (er - pr - 1.5f);
    const float py = ey + lookY_ * (er - pr - 1.5f);
    g.fillCircle(X(px), Y(py), L(pr), INK);
    g.fillCircle(X(px - pr * 0.35f), Y(py - pr * 0.35f), L(1.3f), TFT_WHITE);  // shine
    if (m == Mood::Sleepy) {
      // Droopy lids cover the top two-thirds of each eye.
      const int lidBottom = Y(ey - er + er * 1.3f);
      g.fillRect(X(ex - er - 1), Y(ey - er - 1), L(2 * er + 2) + 1, lidBottom - Y(ey - er - 1),
                 FUR_LIGHT);
      g.drawWideLine(X(ex - er + 1), lidBottom, X(ex + er - 1), lidBottom, max(0.6f, 1.2f * s),
                     INK);
    }
  }

  // --- Nose: small brown triangle ---
  g.fillTriangle(X(-5), Y(2), X(5), Y(2), X(0), Y(9), NOSE);

  // --- Brows: bushy, black, and deliberately mismatched ---
  struct Brow { float x0, y0, x1, y1; };
  Brow left, right;
  switch (m) {
    case Mood::Alert:     left = {-29, -27, -6, -34}; right = {6, -34, 29, -27}; break;
    case Mood::Approved:  left = {-28, -29, -6, -31}; right = {6, -32, 28, -33}; break;
    case Mood::Sleepy:    left = {-28, -22, -6, -21}; right = {6, -22, 28, -24}; break;
    case Mood::Scanning:  left = {-29, -25, -6, -23}; right = {6, -31, 29, -37}; break;
    default:              left = {-29, -25, -6, -23}; right = {6, -29, 29, -32}; break;
  }
  line(left.x0, left.y0, left.x1, left.y1, 3.0f, INK);
  line(right.x0, right.y0, right.x1, right.y1, 3.6f, INK);  // the bushier one

  // --- Sleepy: floating z's drift up and fade out ---
  if (m == Mood::Sleepy) {
    g.setFont(&fonts::Font0);
    g.setTextSize(max(1, (int)lroundf(s * 1.5f)));
    for (int i = 0; i < 3; i++) {
      const float t = fmodf(now_ / 1400.0f + i / 3.0f, 1.0f);  // 0..1, staggered
      g.setTextColor(t < 0.8f ? CREAM : DIM);
      g.setCursor(X(32 + t * 10 + i * 3), Y(-30 - t * 22));
      g.print('z');
    }
    g.setTextSize(1);
  }
}
