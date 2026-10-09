// history.h - the last 10 minutes of what's around, for the Trends dash.
//
// The shell adds a sample every 10 s: Bluetooth devices, Wi-Fi networks and
// the sound level (-1 = the microphone wasn't on). 60 samples, oldest first.
// Pure logic (unit-tested on a PC).
#pragma once
#include <stdint.h>

class History {
 public:
  static constexpr int N = 60;
  static constexpr uint32_t STEP_MS = 10000;
  enum Series { BT, WIFI, SOUND, SERIES };

  void add(int bt, int wifi, int db);
  int size() const { return n_; }
  int at(Series s, int i) const;  // i = 0 oldest .. size()-1 newest; -1 = no reading
  int latest(Series s) const { return n_ ? at(s, n_ - 1) : -1; }
  int max(Series s) const;        // largest reading (0 if none)

 private:
  int16_t v_[SERIES][N] = {};
  int head_ = 0, n_ = 0;  // next slot, how many filled
};
