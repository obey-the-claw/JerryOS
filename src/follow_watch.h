// follow_watch.h - "is something following me?" for ANY Bluetooth device.
//
// Tracker Watch knows what AirTags look like. This catches everything else
// that stays with you while you move: a car's Bluetooth, a watch, a dashcam,
// a gadget someone slipped in your bag.
//
// Without GPS, "you moved" comes from Wi-Fi: when most of the networks
// around you are different from before, you're somewhere new (a "place").
// A Bluetooth device seen at several places in a row, for a long time, is
// following you. At home nothing changes, so nothing gets flagged.
//
// Phones, AirPods and watches change their Bluetooth address every ~15
// minutes for privacy, so FOLLOW_MIN is longer than that: a passenger's
// phone won't trip it. Your own gear that doesn't change address (a Garmin,
// your car) gets marked "mine" once and is ignored from then on.
//
// Pure logic, unit-tested on a PC (tools/sim/test_trackers.cpp).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ble_nearby.h"

// --- "Have I moved?" from the Wi-Fi networks around ---
class SceneTracker {
 public:
  static constexpr int MAX = 48;
  static constexpr float SAME_PLACE = 0.25f;  // share less than this = somewhere new

  // Call after each Wi-Fi scan with the networks heard recently (any order).
  void onScan(const uint8_t (*bssids)[6], int n, uint32_t now);
  uint32_t place() const { return place_; }    // goes up by 1 each time you arrive somewhere new
  uint32_t lastMoveMs() const { return lastMove_; }
  bool hasWifi() const { return anchorN_ > 0; }

 private:
  uint32_t anchor_[MAX] = {};  // fingerprints (hashes) of the networks where we "are"
  int anchorN_ = 0;
  int differentScans_ = 0;
  uint32_t place_ = 0, lastMove_ = 0;
};

// One device that's been with us.
struct Follower {
  uint8_t addr[6];
  char label[22];
  int8_t rssi;
  uint32_t firstMs, lastMs;
  uint32_t lastPlace;
  uint16_t places;  // different places it was seen at, without a long gap
  bool alerting;    // flagged, not looked at yet
  bool flagged;     // has met the "following" rule
  bool used;
};

class FollowWatch {
 public:
  static constexpr int MAX = 64;
  static constexpr int MAX_TRUSTED = 16;
  static constexpr uint32_t FOLLOW_MIN = 20;      // minutes with you...
  static constexpr uint16_t FOLLOW_PLACES = 4;    // ...across this many places
  static constexpr uint32_t GAP_MS = 5 * 60000;   // unseen this long = start over

  // Call every ~20 s with what Bluetooth hears now and the current place.
  // Returns true when a device newly qualifies as following you.
  bool tick(const NearbyDevice *devs, int n, uint32_t place, uint32_t now);

  // Followers (flagged), most recent first. Returns how many.
  int followers(const Follower **out, int max) const;
  int flaggedCount() const;            // how many are listed as followers
  int candidates(uint32_t now) const;  // with you at 2+ places, not flagged yet
  bool anyAlert() const;
  void acknowledgeAll();               // you've seen the list

  // "It's mine": never flag this address again.
  void trust(const uint8_t addr[6]);
  bool isTrusted(const uint8_t addr[6]) const;
  int trustedCount() const { return trustedN_; }
  void clearTrusted() { trustedN_ = 0; }
  // For saving the trusted list (6 bytes each).
  const uint8_t *trustedData() const { return &trusted_[0][0]; }
  void loadTrusted(const uint8_t *data, int count);

 private:
  Follower dev_[MAX] = {};
  uint8_t trusted_[MAX_TRUSTED][6] = {};
  int trustedN_ = 0;
};
