// tracker_watch.h - decides when a tracker is *following* you.
//
// Trackers change their Bluetooth address to protect owner privacy (AirTags
// every 15 min near their owner and every 24 h when separated; SmartTags
// every 15 min). So "the same address for 30 minutes" isn't enough on its
// own. Instead, for each tracker *kind* we keep a strip of the last 64
// minutes: was a separated tracker of this kind seen in that minute?
//
// If a kind has been around (short gaps allowed) for the whole alert window
// (default 30 min, set in Settings), that's an alert: something of that type
// has kept up with you the entire time.
//
// We also keep a per-address list for the "seen for X min" detail view.
//
// Pure logic, no radio, no screen: unit-tested on a PC (tools/sim).
#pragma once
#include <stdint.h>

#include "tracker_detect.h"

struct TrackedDevice {
  uint8_t addr[6];
  TrackerKind kind;
  bool separated;
  int8_t rssi;           // signal strength: closer = higher (less negative)
  uint32_t firstSeenMs;  // start of the current unbroken sighting
  uint32_t lastSeenMs;
  bool used;
};

struct KindSummary {
  int devicesNow;   // separated trackers of this kind seen in the last 2 min
  int withOwner;    // ones travelling with their owner (not counted for alerts)
  int streakMin;    // minutes this kind has been continuously around
  bool alerting;    // streak reached the alert window and not acknowledged
};

class TrackerWatch {
 public:
  static constexpr int MAX_DEVICES = 48;
  static constexpr int GAP_MIN = 2;          // missing minutes allowed in a streak
  static constexpr uint32_t RECENT_MS = 120000;

  // Feed every tracker advert in (from the BLE scanner).
  void onSighting(const uint8_t addr[6], const TrackerAdvert &adv, int rssi, uint32_t nowMs);

  // Call regularly (every loop is fine). Returns true when a NEW alert starts.
  bool tick(uint32_t nowMs, int windowMin);

  KindSummary summary(TrackerKind k, uint32_t nowMs) const;
  bool anyAlert() const;
  bool anySeparatedNow(uint32_t nowMs) const;

  // "OK, I know about it": silence this kind for one alert window.
  void acknowledge(TrackerKind k, uint32_t nowMs, int windowMin);

  // Devices of one kind seen recently, most recent first. Returns the count.
  int listDevices(TrackerKind k, uint32_t nowMs, const TrackedDevice **out, int max) const;

 private:
  void advanceTo(uint32_t minute);
  int streak(int kind) const;

  TrackedDevice devices_[MAX_DEVICES] = {};
  uint64_t presence_[TRACKER_KINDS] = {};  // bit 0 = this minute, bit n = n minutes ago
  uint32_t curMinute_ = 0;
  bool started_ = false;
  bool alerting_[TRACKER_KINDS] = {};
  uint32_t ackUntilMs_[TRACKER_KINDS] = {};
};
