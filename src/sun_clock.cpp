// sun_clock.cpp - see sun_clock.h.

#include "sun_clock.h"

#include <math.h>
#include <stdio.h>

void WallClock::set(uint16_t minuteOfDay, uint16_t dayOfYear, int utcOffsetMin, uint32_t nowMs) {
  if (minuteOfDay >= 1440 || dayOfYear < 1 || dayOfYear > 366) return;
  minute_ = minuteOfDay;
  doy_ = dayOfYear;
  offset_ = utcOffsetMin;
  setMs_ = nowMs;
  valid_ = true;
}

int WallClock::minute(uint32_t nowMs) const {
  return (int)((minute_ + (nowMs - setMs_) / 60000) % 1440);
}

int WallClock::dayOfYear(uint32_t nowMs) const {
  const uint32_t days = (minute_ + (nowMs - setMs_) / 60000) / 1440;
  return (int)((doy_ - 1 + days) % 366) + 1;  // near enough over New Year
}

bool sunTimes(double lat, double lon, int doy, int offsetMin, int &rise, int &set) {
  const double PI = 3.14159265358979, RAD = PI / 180;
  const double g = 2 * PI / 365 * (doy - 1);  // fractional year, at noon
  const double eqtime = 229.18 * (0.000075 + 0.001868 * cos(g) - 0.032077 * sin(g) -
                                  0.014615 * cos(2 * g) - 0.040849 * sin(2 * g));
  const double decl = 0.006918 - 0.399912 * cos(g) + 0.070257 * sin(g) - 0.006758 * cos(2 * g) +
                      0.000907 * sin(2 * g) - 0.002697 * cos(3 * g) + 0.00148 * sin(3 * g);
  const double c = cos(90.833 * RAD) / (cos(lat * RAD) * cos(decl)) - tan(lat * RAD) * tan(decl);
  if (c > 1 || c < -1) return false;  // polar night / midnight sun
  const double ha = acos(c) / RAD;
  auto wrap = [](double m) {
    const int v = (int)lround(m) % 1440;
    return v < 0 ? v + 1440 : v;
  };
  rise = wrap(720 - 4 * (lon + ha) - eqtime + offsetMin);
  set = wrap(720 - 4 * (lon - ha) - eqtime + offsetMin);
  return true;
}

void formatClock(int m, char *out, int size) {
  const int h24 = m / 60, h = h24 % 12 == 0 ? 12 : h24 % 12;
  snprintf(out, size, "%d:%02d%c", h, m % 60, h24 < 12 ? 'a' : 'p');
}

void formatSpan(int minutes, char *out, int size) {
  if (minutes >= 60) snprintf(out, size, "%dh %02dm", minutes / 60, minutes % 60);
  else snprintf(out, size, "%dm", minutes);
}
