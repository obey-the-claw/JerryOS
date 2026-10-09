// badges.cpp - see badges.h.

#include "badges.h"

namespace {
struct Info {
  const char *name, *hint;
};
const Info kInfo[BADGE_COUNT] = {
    {"Flock Spotter", "Pass a Flock camera"},
    {"Tag Spotter", "Hear a lost tracker"},
    {"Shadow Spotted", "Get a follower flagged"},
    {"Crowd Surfer", "50+ BT devices at once"},
    {"Net Collector", "Save 500 WiFi nets"},
    {"Explorer", "Record 10 expeditions"},
    {"Road Tripper", "100 miles of expeditions"},
    {"Knock Knock", "Hear a possible knock"},
    {"Gear Inspector", "Stamp a 5quatch rating"},
    {"Funk Soul Bro", "See all 4 intros"},
    {"Night Owl", "10 min on Night Watch"},
    {"Rigged Up", "Pair the Pi rig box"},
    {"Shields Up", "Shield catches something"},
    {"Hotel Detective", "Find a hidden-cam hint"},
};
constexpr uint32_t MILES_100_M = 160934;
}  // namespace

const char *badgeName(Badge b) { return (int)b < BADGE_COUNT ? kInfo[(int)b].name : "?"; }
const char *badgeHint(Badge b) { return (int)b < BADGE_COUNT ? kInfo[(int)b].hint : "?"; }

int Badges::earnedCount() const {
  int n = 0;
  for (int i = 0; i < BADGE_COUNT; i++) n += has((Badge)i);
  return n;
}

bool Badges::earn(Badge b) {
  if (has(b)) return false;
  bits_ |= 1u << (int)b;
  return true;
}

Badge Badges::check(const BadgeInputs &in) {
  const bool cond[BADGE_COUNT] = {
      in.flockSeen,          in.lostTracker,         in.follower,  in.btNow >= 50,
      nets_ >= 500,          trips_ >= 10,           meters_ >= MILES_100_M,
      in.knockHeard,         in.rated,               intros_ == 0x0F,
      nightS_ >= 600,        in.rigPaired,           in.shieldCaught, in.cameraHint,
  };
  for (int i = 0; i < BADGE_COUNT; i++)
    if (cond[i] && earn((Badge)i)) return (Badge)i;
  return Badge::COUNT;
}

void Badges::addTrip(uint32_t nets, uint32_t meters) {
  nets_ += nets;
  meters_ += meters;
  if (trips_ < 65535) trips_++;
}

void Badges::introSeen(int which) {
  if (which >= 0 && which < 8) intros_ |= 1u << which;
}

void Badges::load(const Saved &s) {
  bits_ = s.bits & ((1u << BADGE_COUNT) - 1);
  nets_ = s.nets;
  meters_ = s.meters;
  nightS_ = s.nightS;
  trips_ = s.trips;
  intros_ = s.intros;
}
