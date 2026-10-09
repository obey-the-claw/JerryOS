// expedition.cpp - see expedition.h.

#include "expedition.h"

#include <math.h>
#include <string.h>

double distanceM(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
  const double R = 6371000, D = 3.14159265358979 / 180 / 1e7;
  const double p1 = lat1 * D, p2 = lat2 * D, dp = (lat2 - lat1) * D, dl = ((double)lon2 - lon1) * D;
  const double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  return 2 * R * atan2(sqrt(a), sqrt(1 - a));
}

void Expedition::begin(uint32_t now, const TripSample &s, uint16_t startMin, uint16_t doy, uint8_t number) {
  r_ = {};
  r_.startMin = startMin;
  r_.doy = doy;
  r_.number = number;
  base_ = s;
  startMs_ = now;
  havePoint_ = false;
  active_ = true;
  sample(now, s);
}

void Expedition::sample(uint32_t now, const TripSample &s) {
  if (!active_) return;
  r_.seconds = (now - startMs_) / 1000;
  if (s.bt > r_.peakBt) r_.peakBt = (uint16_t)s.bt;
  if (s.trackers > r_.trackers) r_.trackers = (uint16_t)s.trackers;
  r_.flock = (uint16_t)(s.flockPassed - base_.flockPassed);
  r_.followers = (uint16_t)(s.followersFlagged > base_.followersFlagged ? s.followersFlagged - base_.followersFlagged : 0);
  r_.knocks = (uint16_t)(s.knocks - base_.knocks);
  if (s.speedKmh > r_.topKmh) r_.topKmh = (uint8_t)(s.speedKmh > 255 ? 255 : s.speedKmh);
  if (s.fix) {
    if (!havePoint_) {
      lat_ = s.latE7;
      lon_ = s.lonE7;
      havePoint_ = true;
    } else {
      const double d = distanceM(lat_, lon_, s.latE7, s.lonE7);
      if (d >= MIN_STEP_M && d < 5000) {  // a real move (not wobble, not a GPS jump)
        r_.meters += (uint32_t)d;
        lat_ = s.latE7;
        lon_ = s.lonE7;
      }
    }
  }
}

TripRecap Expedition::end(uint32_t now, uint32_t netsSaved) {
  r_.seconds = (now - startMs_) / 1000;
  r_.nets = netsSaved;
  active_ = false;
  return r_;
}

void TripLog::add(const TripRecap &r) {
  memmove(&trips_[1], &trips_[0], sizeof(TripRecap) * (MAX - 1));
  trips_[0] = r;
  if (n_ < MAX) n_++;
}

void TripLog::load(const uint8_t *data, int bytes) {
  n_ = bytes / (int)sizeof(TripRecap);
  if (n_ > MAX) n_ = MAX;
  memcpy(trips_, data, n_ * sizeof(TripRecap));
}
