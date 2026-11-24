// battery_life.h - "about how long is left?" from how fast the battery %
// has been dropping on battery power.
//
// Needs a real trend before it guesses: at least 10 minutes on battery and a
// 3% drop. Plugging in USB starts it over. Pure logic (unit-tested on a PC).
#pragma once
#include <stdint.h>

class BatteryLife {
 public:
  // Feed it about once a second: the shown battery % and whether USB is in.
  void update(uint32_t nowMs, int percent, bool onUsb) {
    if (onUsb || percent < 0) {
      started_ = false;
      return;
    }
    if (!started_) {
      started_ = true;
      t0_ = nowMs;
      p0_ = percent;
    }
    now_ = nowMs;
    pct_ = percent;
  }
  // Minutes left, or -1 while it's still working it out.
  int minutesLeft() const {
    if (!started_) return -1;
    const uint32_t elapsed = now_ - t0_;
    const int drop = p0_ - pct_;
    if (elapsed < 10 * 60000u || drop < 3) return -1;
    const float perPercentMin = elapsed / 60000.0f / drop;
    return (int)(pct_ * perPercentMin + 0.5f);
  }

 private:
  bool started_ = false;
  uint32_t t0_ = 0, now_ = 0;
  int p0_ = 0, pct_ = 0;
};
