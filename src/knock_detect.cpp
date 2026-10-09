// knock_detect.cpp - see knock_detect.h.

#include "knock_detect.h"

#include <math.h>

namespace {
float toDb(double meanSquare) {
  const double v = meanSquare / (32768.0 * 32768.0);
  return v < 1e-9 ? -90.0f : (float)(10 * log10(v));
}
}  // namespace

BlockFeatures analyzeBlock(const int16_t *s, size_t n, int rate) {
  BlockFeatures f = {-90, -90, 0};
  if (!n) return f;
  double mean = 0;
  for (size_t i = 0; i < n; i++) mean += s[i];
  mean /= n;
  const size_t sub = rate / 100 > 0 ? rate / 100 : 1;  // 10 ms
  double all = 0, part = 0;
  size_t inPart = 0, crossings = 0;
  bool wasPos = s[0] - mean >= 0;
  for (size_t i = 0; i < n; i++) {
    const double v = s[i] - mean;
    all += v * v;
    part += v * v;
    const bool pos = v >= 0;
    if (pos != wasPos) crossings++;
    wasPos = pos;
    if (++inPart == sub || i == n - 1) {
      const float d = toDb(part / inPart);
      if (d > f.peakDb) f.peakDb = d;
      part = 0;
      inPart = 0;
    }
  }
  f.dbfs = toDb(all / n);
  f.zcr = crossings * (float)rate / n;
  return f;
}

void KnockDetector::push(const SoundEvent &e) {
  const uint8_t h = head_.load();
  if ((uint8_t)(h - tail_.load()) >= 8) return;  // full: the screen isn't reading
  ev_[h % 8] = e;
  head_.store((uint8_t)(h + 1));
}

bool KnockDetector::take(SoundEvent &e) {
  const uint8_t t = tail_.load();
  if (t == head_.load()) return false;
  e = ev_[t % 8];
  tail_.store((uint8_t)(t + 1));
  return true;
}

void KnockDetector::add(const BlockFeatures &f, uint32_t now) {
  if (blocks_ < WARMUP) {  // learn the background first
    bg_ = blocks_ == 0 ? f.dbfs : bg_ + (f.dbfs - bg_) / (blocks_ + 1);
    blocks_++;
    return;
  }
  // Knock: short, sharp and well above the background.
  const bool knock = f.peakDb >= bg_ + KNOCK_DB && f.peakDb >= f.dbfs + SHARP_DB &&
                     (!anyKnock_ || now - lastKnockMs_ >= 250);
  if (knock) {
    row_ = anyKnock_ && now - lastKnockMs_ <= ROW_MS && row_ < 255 ? row_ + 1 : 1;
    anyKnock_ = true;
    lastKnockMs_ = now;
    knocks_++;
    const float above = f.peakDb - bg_;
    push({SoundKind::Knock, (uint8_t)(above > 99 ? 99 : above), row_, now});
  }
  // Howl: over a second of steady, louder sound with a steady pitch.
  if (f.dbfs >= bg_ + HOWL_DB) {
    howlRun_++;
    zSum_ += f.zcr;
    zSq_ += f.zcr * f.zcr;
    if (howlRun_ >= HOWL_BLOCKS && !howlFired_) {
      const float mean = zSum_ / howlRun_;
      const float var = zSq_ / howlRun_ - mean * mean;
      const float cv = mean > 0 ? sqrtf(var > 0 ? var : 0) / mean : 1;
      if (mean >= 150 && mean <= 3000 && cv < 0.3f) {
        howlFired_ = true;
        howls_++;
        const float above = f.dbfs - bg_;
        push({SoundKind::Howl, (uint8_t)(above > 99 ? 99 : above), 1, now});
      }
    }
  } else {
    howlRun_ = 0;
    howlFired_ = false;
    zSum_ = zSq_ = 0;
  }
  // The background follows quiet blocks quickly and loud ones very slowly
  // (so a generator starting up becomes "background" after a while).
  if (!knock && howlRun_ == 0) bg_ += (f.dbfs - bg_) * (f.dbfs < bg_ + 6 ? 0.05f : 0.01f);
  else if (howlRun_ > 50) bg_ += (f.dbfs - bg_) * 0.01f;
}
