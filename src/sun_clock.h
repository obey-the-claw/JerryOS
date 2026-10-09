// sun_clock.h - what time it is (from the rig box's clock) and when the sun
// rises and sets (from its GPS fix).
//
// Jerry has no clock of his own. The Pi box sends its local time + date in
// its beacon (rig_link.h, v2); WallClock remembers it and keeps counting with
// millis(), so the time stays right after the box goes out of range (until
// Jerry restarts).
//
// Pure logic (unit-tested on a PC).
#pragma once
#include <stdint.h>

class WallClock {
 public:
  void set(uint16_t minuteOfDay, uint16_t dayOfYear, int utcOffsetMin, uint32_t nowMs);
  bool valid() const { return valid_; }
  int minute(uint32_t nowMs) const;     // local minute of the day, 0-1439
  int dayOfYear(uint32_t nowMs) const;  // 1-366
  int utcOffsetMin() const { return offset_; }

 private:
  bool valid_ = false;
  uint16_t minute_ = 0, doy_ = 1;
  int offset_ = 0;
  uint32_t setMs_ = 0;
};

// Sunrise and sunset, local minutes of the day (NOAA's formulas, good to a
// minute or two). false = the sun doesn't rise or set that day (polar).
bool sunTimes(double latDeg, double lonDeg, int dayOfYear, int utcOffsetMin, int &rise, int &set);

// "6:24a", "12:05p" (12-hour, like Steve's phone).
void formatClock(int minuteOfDay, char *out, int size);
// "1h 20m", "45m"
void formatSpan(int minutes, char *out, int size);
