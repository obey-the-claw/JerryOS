// expedition.h - Expedition log: a recap of each REC session.
//
// REC (hold R on the home screen) already starts saving Wi-Fi networks. While
// it's on, Jerry also keeps score: how long, the most Bluetooth devices at
// once, trackers, followers, Flock cameras passed, knocks heard, and (with
// the rig box's GPS) distance and top speed. When REC stops you get a recap
// card, and the last few are kept (Menu -> Expeditions). Nothing is kept
// unless you pressed REC.
//
// Pure logic (unit-tested on a PC).
#pragma once
#include <stdint.h>

struct TripRecap {
  uint32_t seconds;
  uint32_t nets;        // Wi-Fi networks saved
  uint32_t meters;      // distance (needs the rig's GPS)
  uint16_t peakBt;      // most Bluetooth devices at once
  uint16_t trackers;    // most lost trackers at once
  uint16_t followers;   // newly flagged followers
  uint16_t flock;       // Flock cameras passed
  uint16_t knocks;      // possible knocks heard
  uint16_t startMin;    // local start time (0xFFFF = clock unknown)
  uint16_t doy;         // day of the year (0 = unknown)
  uint8_t topKmh;       // top speed (needs the rig)
  uint8_t number;       // REC session number (wraps)
};

// Great-circle distance between two points, metres.
double distanceM(int32_t lat1E7, int32_t lon1E7, int32_t lat2E7, int32_t lon2E7);

// What's going on right now, fed in every few seconds while recording.
struct TripSample {
  int bt, trackers;
  uint32_t flockPassed, followersFlagged, knocks;  // running totals since power-on
  bool fix;
  int32_t latE7, lonE7;
  int speedKmh;  // -1 = no rig
};

class Expedition {
 public:
  static constexpr double MIN_STEP_M = 15;  // GPS wobble smaller than this isn't travel

  void begin(uint32_t now, const TripSample &s, uint16_t startMin, uint16_t doy, uint8_t number);
  void sample(uint32_t now, const TripSample &s);
  TripRecap end(uint32_t now, uint32_t netsSaved);
  bool active() const { return active_; }
  const TripRecap &sofar() const { return r_; }

 private:
  bool active_ = false;
  uint32_t startMs_ = 0;
  TripSample base_ = {};
  bool havePoint_ = false;
  int32_t lat_ = 0, lon_ = 0;
  TripRecap r_ = {};
};

// The last few recaps, newest first, stored as one small block.
class TripLog {
 public:
  static constexpr int MAX = 8;
  void add(const TripRecap &r);
  int count() const { return n_; }
  const TripRecap &at(int i) const { return trips_[i]; }  // 0 = newest
  // For saving: raw bytes + how many.
  const uint8_t *data() const { return (const uint8_t *)trips_; }
  int bytes() const { return n_ * (int)sizeof(TripRecap); }
  void load(const uint8_t *data, int bytes);
  void clear() { n_ = 0; }

 private:
  TripRecap trips_[MAX] = {};
  int n_ = 0;
};
