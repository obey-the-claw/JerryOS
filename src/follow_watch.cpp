// follow_watch.cpp - see follow_watch.h.

#include "follow_watch.h"

#include <string.h>

namespace {

uint32_t fingerprint(const uint8_t *b) {  // FNV-1a hash of a 6-byte address
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) h = (h ^ b[i]) * 16777619u;
  return h ? h : 1;
}

}  // namespace

void SceneTracker::onScan(const uint8_t (*bssids)[6], int n, uint32_t now) {
  if (n <= 0) return;  // nothing heard (open country): can't tell, keep the old place
  if (n > MAX) n = MAX;
  uint32_t cur[MAX];
  for (int i = 0; i < n; i++) cur[i] = fingerprint(bssids[i]);

  if (anchorN_ == 0) {  // first networks we ever hear: that's place 0
    memcpy(anchor_, cur, n * sizeof cur[0]);
    anchorN_ = n;
    return;
  }
  int shared = 0;
  for (int i = 0; i < n; i++)
    for (int j = 0; j < anchorN_; j++)
      if (cur[i] == anchor_[j]) {
        shared++;
        break;
      }
  const int either = n + anchorN_ - shared;
  const float similar = either ? (float)shared / either : 1.0f;
  // Two different-looking scans in a row = we really moved (one odd scan
  // can just be a busy moment).
  differentScans_ = similar < SAME_PLACE ? differentScans_ + 1 : 0;
  if (differentScans_ >= 2) {
    memcpy(anchor_, cur, n * sizeof cur[0]);
    anchorN_ = n;
    differentScans_ = 0;
    place_++;
    lastMove_ = now;
  }
}

bool FollowWatch::tick(const NearbyDevice *devs, int n, uint32_t place, uint32_t now) {
  bool newAlert = false;
  for (int i = 0; i < n; i++) {
    const NearbyDevice &d = devs[i];
    if (isTrusted(d.addr)) continue;
    if (strstr(d.label, "(own)") || strstr(d.label, "(lost)")) continue;  // Tracker Watch's job

    Follower *slot = nullptr, *freeSlot = nullptr, *oldest = nullptr;
    for (auto &f : dev_) {
      if (!f.used) {
        if (!freeSlot) freeSlot = &f;
      } else if (memcmp(f.addr, d.addr, 6) == 0) {
        slot = &f;
        break;
      } else if (!f.flagged && (!oldest || (int32_t)(oldest->lastMs - f.lastMs) > 0)) {
        oldest = &f;  // never push out a flagged follower
      }
    }
    if (!slot) {
      slot = freeSlot ? freeSlot : oldest;
      if (!slot) continue;
      memset(slot, 0, sizeof *slot);
      memcpy(slot->addr, d.addr, 6);
      slot->firstMs = now;
      slot->lastPlace = place;
      slot->places = 1;
      slot->used = true;
    } else if (now - slot->lastMs > GAP_MS) {  // gone for a while: start counting again
      slot->firstMs = now;
      slot->lastPlace = place;
      slot->places = 1;
    } else if (place != slot->lastPlace) {
      slot->lastPlace = place;
      if (slot->places < 0xFFFF) slot->places++;
    }
    memcpy(slot->label, d.label, sizeof slot->label);
    slot->rssi = d.rssi;
    slot->lastMs = now;

    if (!slot->flagged && slot->places >= FOLLOW_PLACES &&
        now - slot->firstMs >= FOLLOW_MIN * 60000) {
      slot->flagged = slot->alerting = true;
      newAlert = true;
    }
  }
  // A flagged device that's been gone a long time is no longer "following".
  for (auto &f : dev_)
    if (f.used && now - f.lastMs > 6 * GAP_MS) f.used = false;
  return newAlert;
}

int FollowWatch::followers(const Follower **out, int max) const {
  int n = 0;
  for (const auto &f : dev_) {
    if (!f.used || !f.flagged) continue;
    int pos = n < max ? n : max;  // most recently heard first
    while (pos > 0 && (int32_t)(f.lastMs - out[pos - 1]->lastMs) > 0) {
      if (pos < max) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < max) {
      out[pos] = &f;
      if (n < max) n++;
    }
  }
  return n;
}

int FollowWatch::flaggedCount() const {
  int c = 0;
  for (const auto &f : dev_)
    if (f.used && f.flagged) c++;
  return c;
}

int FollowWatch::candidates(uint32_t now) const {
  int c = 0;
  for (const auto &f : dev_)
    if (f.used && !f.flagged && f.places >= 2 && now - f.lastMs < GAP_MS) c++;
  return c;
}

bool FollowWatch::anyAlert() const {
  for (const auto &f : dev_)
    if (f.used && f.alerting) return true;
  return false;
}

void FollowWatch::acknowledgeAll() {
  for (auto &f : dev_) f.alerting = false;
}

void FollowWatch::trust(const uint8_t addr[6]) {
  if (!isTrusted(addr)) {
    if (trustedN_ == MAX_TRUSTED) {  // full: forget the oldest
      memmove(trusted_[0], trusted_[1], sizeof trusted_ - sizeof trusted_[0]);
      trustedN_--;
    }
    memcpy(trusted_[trustedN_++], addr, 6);
  }
  for (auto &f : dev_)
    if (f.used && memcmp(f.addr, addr, 6) == 0) f.used = false;
}

bool FollowWatch::isTrusted(const uint8_t addr[6]) const {
  for (int i = 0; i < trustedN_; i++)
    if (memcmp(trusted_[i], addr, 6) == 0) return true;
  return false;
}

void FollowWatch::loadTrusted(const uint8_t *data, int count) {
  trustedN_ = count < 0 ? 0 : count > MAX_TRUSTED ? MAX_TRUSTED : count;
  memcpy(trusted_, data, trustedN_ * 6);
}
