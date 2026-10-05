// tracker_detect.h - recognise item trackers from a raw BLE advertisement.
//
// Pure logic (no radio code), so it can be unit-tested on a PC.
//
// Signatures come from AirGuard, the open-source anti-stalking app by
// TU Darmstadt's SEEMOO lab (github.com/seemoo-lab/AirGuard), cross-checked
// with Google's Find Hub Network accessory spec and the Bluetooth SIG UUID
// list. See docs/TRACKERS.md for the details and sources.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum class TrackerKind : uint8_t { AirTag, FindMy, Tile, SmartTag, Google, COUNT };
constexpr int TRACKER_KINDS = static_cast<int>(TrackerKind::COUNT);

const char *trackerName(TrackerKind k);

struct TrackerAdvert {
  TrackerKind kind;
  // true  = separated from its owner (the case that matters for stalking)
  // false = travelling with its owner's phone, e.g. your passenger's keys
  bool separated;
};

// Returns true if the advertisement payload (the raw AD structures) is a
// known tracker, and fills in `out`.
bool classifyAdvert(const uint8_t *payload, size_t len, TrackerAdvert &out);
