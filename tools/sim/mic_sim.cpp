// Fake microphone for the PC simulator: a room with someone talking, and
// now and then a knock (every ~6.5 s) or a howl (every ~40 s) for the
// Knock Detector.
#include <Arduino.h>
#include <math.h>

#include "mic.h"

namespace mic {
namespace {
LevelMeter levels;
KnockDetector knocks;
uint32_t lastBlock = 0;
}  // namespace
void use() {
  const uint32_t now = millis();
  if (now - lastBlock > 1000) lastBlock = now - 100;  // wasn't listening: start fresh
  while (now - lastBlock >= 100) {  // one block per 100 ms of sim time
    lastBlock += 100;
    const float t = lastBlock / 1000.0f;
    const float db = -58 + 14 * fabsf(sinf(t * 1.7f)) + (((lastBlock / 100) % 7 == 0) ? 9 : 0);
    levels.add(db);
    BlockFeatures f = {-62, -60, 2000};  // quiet woods
    if (lastBlock % 6500 < 100) f = {-42, -22, 3000};          // knock!
    if (lastBlock % 40000 < 1500) f = {-40, -38, 900 + (float)(lastBlock % 300) * 0.1f};  // howl
    knocks.add(f, lastBlock);
  }
}
bool running() { return true; }
bool chipFound() { return true; }
const LevelMeter &meter() { return levels; }
void resetPeak() { levels.resetPeak(); }
void setCalibration(int db) { levels.setCalibration(db); }
bool takeSoundEvent(SoundEvent &e) { return knocks.take(e); }
uint32_t knockCount() { return knocks.knocks(); }
uint32_t howlCount() { return knocks.howls(); }
bool knockReady() { return knocks.ready(); }
}  // namespace mic
