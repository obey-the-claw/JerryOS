// badges.h - little achievements Jerry celebrates.
//
// Each one is earned once and kept (Menu -> Badges). The shell checks the
// conditions every few seconds and shows a toast when a new one lands.
// Pure logic (unit-tested on a PC); the shell saves it.
#pragma once
#include <stdint.h>

enum class Badge : uint8_t {
  FlockSpotter,   // passed a Flock camera
  TagSpotter,     // heard a lost tracker
  ShadowSpotted,  // a follower was flagged
  CrowdSurfer,    // 50+ Bluetooth devices at once
  NetCollector,   // 500 Wi-Fi networks saved (all expeditions)
  Explorer,       // 10 expeditions
  RoadTripper,    // 100 miles of expeditions
  KnockKnock,     // heard a possible knock
  GearInspector,  // stamped a 5quatch rating
  FunkSoul,       // saw all 4 intros
  NightOwl,       // 10 minutes on Night Watch
  RiggedUp,       // paired the Pi rig box
  ShieldsUp,      // Shield caught something
  HotelDetective, // found a hidden-camera hint
  COUNT
};
constexpr int BADGE_COUNT = (int)Badge::COUNT;

const char *badgeName(Badge b);   // "Flock Spotter"
const char *badgeHint(Badge b);   // "Pass a Flock camera"

// Everything the conditions look at.
struct BadgeInputs {
  bool flockSeen, lostTracker, follower, rigPaired, shieldCaught, cameraHint, knockHeard, rated;
  int btNow;
};

class Badges {
 public:
  bool has(Badge b) const { return bits_ & (1u << (int)b); }
  int earnedCount() const;
  bool earn(Badge b);  // true = newly earned
  // Check everything; returns the first newly earned badge, or COUNT if none.
  Badge check(const BadgeInputs &in);

  // Running totals (kept across power-offs).
  void addTrip(uint32_t nets, uint32_t meters);
  void introSeen(int which);
  void nightSeconds(uint32_t s) { nightS_ += s; }
  uint32_t netsTotal() const { return nets_; }
  uint32_t metersTotal() const { return meters_; }
  uint16_t trips() const { return trips_; }

  // For saving.
  struct Saved {
    uint32_t bits, nets, meters, nightS;
    uint16_t trips;
    uint8_t intros, pad;
  };
  Saved save() const { return {bits_, nets_, meters_, nightS_, trips_, intros_, 0}; }
  void load(const Saved &s);

 private:
  uint32_t bits_ = 0, nets_ = 0, meters_ = 0, nightS_ = 0;
  uint16_t trips_ = 0;
  uint8_t intros_ = 0;
};
