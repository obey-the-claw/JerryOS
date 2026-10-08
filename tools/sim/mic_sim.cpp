// Fake microphone for the PC simulator: a room with someone talking.
#include <Arduino.h>
#include <math.h>

#include "mic.h"

namespace mic {
namespace {
LevelMeter levels;
uint32_t lastBlock = 0;
}  // namespace
void use() {
  const uint32_t now = millis();
  while (now - lastBlock >= 100) {  // one block per 100 ms of sim time
    lastBlock += 100;
    const float t = lastBlock / 1000.0f;
    levels.add(-58 + 14 * fabsf(sinf(t * 1.7f)) + (((lastBlock / 100) % 7 == 0) ? 9 : 0));
  }
}
bool running() { return true; }
bool chipFound() { return true; }
const LevelMeter &meter() { return levels; }
void resetPeak() { levels.resetPeak(); }
void setCalibration(int db) { levels.setCalibration(db); }
}  // namespace mic
