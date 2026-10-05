// tracker_watch.cpp - see tracker_watch.h.

#include "tracker_watch.h"

#include <string.h>

namespace {
constexpr uint32_t MINUTE_MS = 60000;
constexpr uint32_t FORGET_MS = 70 * MINUTE_MS;  // drop devices unseen this long
constexpr uint32_t RESTART_MS = 5 * MINUTE_MS;  // a gap this long restarts "seen for"

inline bool after(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }
}  // namespace

void TrackerWatch::advanceTo(uint32_t minute) {
  if (!started_) {
    curMinute_ = minute;
    started_ = true;
    return;
  }
  if (minute == curMinute_) return;
  const uint32_t steps = minute - curMinute_;
  for (int k = 0; k < TRACKER_KINDS; k++) presence_[k] = steps >= 64 ? 0 : presence_[k] << steps;
  curMinute_ = minute;
}

void TrackerWatch::onSighting(const uint8_t addr[6], const TrackerAdvert &adv, int rssi,
                              uint32_t nowMs) {
  advanceTo(nowMs / MINUTE_MS);
  if (adv.separated) presence_[static_cast<int>(adv.kind)] |= 1;

  // Find this address; otherwise take a free slot, or reuse the one unseen longest.
  TrackedDevice *slot = nullptr, *freeSlot = nullptr, *oldest = nullptr;
  for (auto &d : devices_) {
    if (!d.used) {
      if (!freeSlot) freeSlot = &d;
    } else if (memcmp(d.addr, addr, 6) == 0) {
      slot = &d;
      break;
    } else if (!oldest || after(oldest->lastSeenMs, d.lastSeenMs)) {
      oldest = &d;
    }
  }
  const bool known = slot != nullptr;
  if (!slot) slot = freeSlot ? freeSlot : oldest;
  if (!known || nowMs - slot->lastSeenMs > RESTART_MS) {
    memcpy(slot->addr, addr, 6);
    slot->firstSeenMs = nowMs;
    slot->used = true;
  }
  slot->kind = adv.kind;
  slot->separated = adv.separated;
  slot->rssi = (int8_t)(rssi < -127 ? -127 : rssi > 0 ? 0 : rssi);
  slot->lastSeenMs = nowMs;
}

// Minutes this kind has been continuously around, allowing short gaps.
// 0 if it hasn't been seen in the last GAP_MIN+1 minutes.
int TrackerWatch::streak(int k) const {
  const uint64_t bits = presence_[k];
  int lastOne = -1, zeros = 0;
  for (int i = 0; i < 64; i++) {
    if (bits & (1ULL << i)) {
      lastOne = i;
      zeros = 0;
    } else if (++zeros > GAP_MIN) {
      break;
    }
  }
  return lastOne < 0 ? 0 : lastOne + 1;
}

bool TrackerWatch::tick(uint32_t nowMs, int windowMin) {
  advanceTo(nowMs / MINUTE_MS);
  for (auto &d : devices_)
    if (d.used && nowMs - d.lastSeenMs > FORGET_MS) d.used = false;

  bool newAlert = false;
  for (int k = 0; k < TRACKER_KINDS; k++) {
    const int s = streak(k);
    if (s == 0) ackUntilMs_[k] = 0;  // gone: a future return alerts again
    const bool acked = ackUntilMs_[k] && after(ackUntilMs_[k], nowMs);
    const bool should = s >= windowMin && !acked;
    if (should && !alerting_[k]) newAlert = true;
    alerting_[k] = should;
  }
  return newAlert;
}

KindSummary TrackerWatch::summary(TrackerKind kind, uint32_t nowMs) const {
  KindSummary s = {};
  const int k = static_cast<int>(kind);
  for (const auto &d : devices_) {
    if (!d.used || d.kind != kind || nowMs - d.lastSeenMs > RECENT_MS) continue;
    if (d.separated) s.devicesNow++;
    else s.withOwner++;
  }
  s.streakMin = streak(k);
  s.alerting = alerting_[k];
  return s;
}

bool TrackerWatch::anyAlert() const {
  for (bool a : alerting_) if (a) return true;
  return false;
}

bool TrackerWatch::anySeparatedNow(uint32_t nowMs) const {
  for (const auto &d : devices_)
    if (d.used && d.separated && nowMs - d.lastSeenMs <= RECENT_MS) return true;
  return false;
}

void TrackerWatch::acknowledge(TrackerKind kind, uint32_t nowMs, int windowMin) {
  const int k = static_cast<int>(kind);
  ackUntilMs_[k] = nowMs + (uint32_t)windowMin * MINUTE_MS;
  if (ackUntilMs_[k] == 0) ackUntilMs_[k] = 1;
  alerting_[k] = false;
}

int TrackerWatch::listDevices(TrackerKind kind, uint32_t nowMs, const TrackedDevice **out,
                              int max) const {
  const TrackedDevice *found[MAX_DEVICES];
  int n = 0;
  for (const auto &d : devices_)
    if (d.used && d.kind == kind && nowMs - d.lastSeenMs <= 10 * MINUTE_MS) found[n++] = &d;
  // Most recently seen first (simple insertion sort; n is small).
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && after(found[j]->lastSeenMs, found[j - 1]->lastSeenMs); j--) {
      const TrackedDevice *t = found[j];
      found[j] = found[j - 1];
      found[j - 1] = t;
    }
  if (n > max) n = max;
  for (int i = 0; i < n; i++) out[i] = found[i];
  return n;
}
