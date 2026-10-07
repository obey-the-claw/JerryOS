// sound_level.cpp - see sound_level.h.

#include "sound_level.h"

#include <math.h>

float blockDbfs(const int16_t *s, size_t n) {
  if (!n) return -90;
  double mean = 0;
  for (size_t i = 0; i < n; i++) mean += s[i];
  mean /= n;
  double sum = 0;
  for (size_t i = 0; i < n; i++) {
    const double v = s[i] - mean;
    sum += v * v;
  }
  const double rms = sqrt(sum / n) / 32768.0;
  const float db = rms > 0 ? (float)(20 * log10(rms)) : -90;
  return db < -90 ? -90 : db;
}

const char *loudnessName(int db) {
  if (db < 40) return "very quiet";
  if (db < 55) return "quiet";
  if (db < 70) return "talking";
  if (db < 85) return "loud";
  if (db < 100) return "very loud";
  return "painful!";
}

void LevelMeter::add(float dbfs) {
  // Rise fast (catch a bang), fall slowly (readable), like a real meter.
  if (!blocks_++) smooth_ = dbfs;
  else smooth_ += (dbfs > smooth_ ? 0.6f : 0.15f) * (dbfs - smooth_);
  if (dbfs > peak_) peak_ = dbfs;
}
