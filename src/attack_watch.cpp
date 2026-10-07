// attack_watch.cpp - see attack_watch.h.

#include "attack_watch.h"

#include <stdio.h>
#include <string.h>

namespace {

inline uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

uint32_t fingerprint(const uint8_t *b) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) h = (h ^ b[i]) * 16777619u;
  return h ? h : 1;
}

}  // namespace

Popup popupKind(const uint8_t *p, size_t len) {
  for (size_t i = 0; i < len;) {
    const uint8_t fieldLen = p[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) break;
    const uint8_t type = p[i + 1];
    const uint8_t *d = p + i + 2;
    const size_t dn = fieldLen - 1;
    if (type == 0xFF && dn >= 3) {
      const uint16_t company = le16(d);
      // Apple "proximity pairing" (0x07: AirPods-style pop-up) and
      // "nearby action" (0x0F: setup pop-ups).
      if (company == 0x004C && (d[2] == 0x07 || d[2] == 0x0F)) return Popup::Apple;
      // Microsoft Swift Pair beacon: 0x03 0x00 after the company ID.
      if (company == 0x0006 && dn >= 4 && d[2] == 0x03 && d[3] == 0x00) return Popup::Windows;
      // Samsung Galaxy Buds / watch "easy setup" pop-up.
      if (company == 0x0075 && dn >= 4 && d[2] == 0x42 && d[3] == 0x09) return Popup::Samsung;
    } else if (type == 0x16 && dn >= 2 && le16(d) == 0xFE2C) {
      return Popup::Android;  // Google Fast Pair
    }
    i += 1 + fieldLen;
  }
  return Popup::None;
}

const char *popupName(Popup p) {
  switch (p) {
    case Popup::Apple: return "Apple";
    case Popup::Android: return "Android";
    case Popup::Windows: return "Windows";
    case Popup::Samsung: return "Samsung";
    default: return "?";
  }
}

const char *threatName(Threat t) {
  switch (t) {
    case Threat::Kickoff: return "WiFi kickoffs";
    case Threat::Popup: return "BT popup spam";
    case Threat::Fake: return "Fake networks";
    default: return "?";
  }
}

void AttackWatch::raise(Threat t, uint32_t amount, const char *detail, uint32_t now) {
  ThreatState &s = st_[(int)t];
  if (!s.active) {
    s.active = true;
    s.alerting = true;
    s.peak = 0;
    fresh_[(int)t] = true;
  }
  s.lastMs = now;
  if (amount >= s.peak) {
    s.peak = amount;
    snprintf(s.detail, sizeof s.detail, "%s", detail);
  }
}

void AttackWatch::onKickoffs(uint32_t frames, uint32_t now) {
  const uint32_t sec = now / 1000;
  if (sec != kickSec_) {  // new second: clear the seconds we skipped
    const uint32_t gap = sec - kickSec_;
    for (uint32_t i = 1; i <= gap && i <= 10; i++) kick_[(kickSec_ + i) % 10] = 0;
    kickSec_ = sec;
  }
  kick_[sec % 10] += frames;
  uint32_t total = 0;
  for (uint32_t k : kick_) total += k;
  if (total >= KICKOFF_FLOOD) {
    char d[24];
    snprintf(d, sizeof d, "%lu in 10s", (unsigned long)total);
    raise(Threat::Kickoff, total, d, now);
  }
}

void AttackWatch::onPopup(const uint8_t addr[6], Popup kind, uint32_t now) {
  if (kind == Popup::None) return;
  const uint32_t h = fingerprint(addr);
  for (const Seen &s : pop_)
    if (s.hash == h && now - s.ms < WINDOW_MS) return;  // same address again: not new
  pop_[popNext_] = {h, now, kind};
  popNext_ = (popNext_ + 1) % 64;

  int count = 0, perKind[5] = {};
  for (const Seen &s : pop_)
    if (s.hash && now - s.ms < WINDOW_MS) {
      count++;
      perKind[(int)s.kind]++;
    }
  if (count >= (int)POPUP_FLOOD) {
    int top = 1;
    for (int k = 2; k < 5; k++)
      if (perKind[k] > perKind[top]) top = k;
    char d[24];
    snprintf(d, sizeof d, "%s x%d", popupName((Popup)top), count);
    raise(Threat::Popup, count, d, now);
  }
}

void AttackWatch::checkNetworks(const WifiNet *nets, int n, uint32_t now) {
  // Evil twin: the same name both locked and open, right now.
  for (int i = 0; i < n; i++) {
    if (!nets[i].ssid[0] || !isOpen(nets[i].auth)) continue;
    for (int j = 0; j < n; j++) {
      if (j == i || isOpen(nets[j].auth) || strcmp(nets[i].ssid, nets[j].ssid) != 0) continue;
      char d[24], name[11];
      displaySsid(nets[i], name, sizeof name);
      snprintf(d, sizeof d, "'%s' (open)", name);
      raise(Threat::Fake, 2, d, now);
      break;
    }
  }
  // Many names from one box: addresses that differ only in the last byte.
  for (int i = 0; i < n; i++) {
    int names = 1;
    for (int j = 0; j < n; j++) {
      if (j == i || memcmp(nets[i].bssid, nets[j].bssid, 5) != 0) continue;
      bool dup = strcmp(nets[i].ssid, nets[j].ssid) == 0;
      for (int k = 0; k < j && !dup; k++)  // count each name once
        if (k != i && memcmp(nets[i].bssid, nets[k].bssid, 5) == 0 &&
            strcmp(nets[k].ssid, nets[j].ssid) == 0)
          dup = true;
      if (!dup) names++;
    }
    if (names >= MANY_NAMES) {
      char d[24];
      snprintf(d, sizeof d, "1 box, %d names", names);
      raise(Threat::Fake, names, d, now);
      break;
    }
  }
}

bool AttackWatch::tick(uint32_t now) {
  bool started = false;
  for (int t = 0; t < (int)Threat::COUNT; t++) {
    ThreatState &s = st_[t];
    // Fake networks are judged on scans every 20 s, so allow longer to calm down.
    const uint32_t quiet = t == (int)Threat::Fake ? 3 * QUIET_MS : QUIET_MS;
    if (s.active && now - s.lastMs > quiet) s.active = false;
    if (fresh_[t]) {
      fresh_[t] = false;
      started = true;
    }
  }
  return started;
}

bool AttackWatch::anyAlert() const {
  for (const auto &s : st_)
    if (s.alerting) return true;
  return false;
}

bool AttackWatch::anyActive() const {
  for (const auto &s : st_)
    if (s.active) return true;
  return false;
}

void AttackWatch::acknowledgeAll() {
  for (auto &s : st_) s.alerting = false;
}
