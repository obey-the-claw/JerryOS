// wifi_nets.cpp - see wifi_nets.h.

#include "wifi_nets.h"

#include <stdio.h>
#include <string.h>

const char LOG_HEADER[] = "session,uptime_s,bssid,ssid,auth,channel,rssi";

const char *authName(uint8_t auth) {
  switch (auth) {
    case 0: return "open";
    case 1: return "WEP";
    case 2: return "WPA";
    case 3: return "WPA2";
    case 4: return "WPA/WPA2";
    case 5: return "WPA2-Ent";
    case 6: return "WPA3";
    case 7: return "WPA2/WPA3";
    case 8: return "WAPI";
    default: return "other";
  }
}

void displaySsid(const WifiNet &n, char *out, size_t outSize) {
  if (!outSize) return;
  if (!n.ssid[0]) {
    snprintf(out, outSize, "(hidden)");
    return;
  }
  size_t k = 0;
  for (size_t i = 0; n.ssid[i] && k + 1 < outSize; i++) {
    const uint8_t c = (uint8_t)n.ssid[i];
    if (c >= 0x20 && c < 0x7F) out[k++] = (char)c;
    else if (c >= 0xC0 || c < 0x20 || c == 0x7F) out[k++] = '?';  // one '?' per character
    // 0x80-0xBF: middle of a multi-byte character, skip
  }
  out[k] = 0;
}

int formatLogLine(char *out, size_t outSize, uint32_t session, uint32_t uptimeSec,
                  const WifiNet &n) {
  // The name goes in quotes with any quote doubled, so commas in names are safe.
  // Control characters (newlines!) would break the file, so they become '?'.
  char name[70];
  size_t k = 0;
  for (size_t i = 0; n.ssid[i] && k + 2 < sizeof name; i++) {
    const uint8_t c = (uint8_t)n.ssid[i];
    if (c == '"') {
      name[k++] = '"';
      name[k++] = '"';
    } else {
      name[k++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
    }
  }
  name[k] = 0;
  const uint8_t *b = n.bssid;
  const int len = snprintf(out, outSize, "%lu,%lu,%02x:%02x:%02x:%02x:%02x:%02x,\"%s\",%s,%u,%d",
                           (unsigned long)session, (unsigned long)uptimeSec, b[0], b[1], b[2], b[3],
                           b[4], b[5], name, authName(n.auth), n.channel, n.rssi);
  return (len < 0 || (size_t)len >= outSize) ? (int)(outSize ? outSize - 1 : 0) : len;
}

bool WifiTable::update(const uint8_t bssid[6], const char *ssid, uint8_t channel, uint8_t auth,
                       int rssi, uint32_t now) {
  WifiNet *slot = nullptr, *freeSlot = nullptr, *oldest = nullptr;
  for (auto &n : net_) {
    if (!n.used) {
      if (!freeSlot) freeSlot = &n;
    } else if (memcmp(n.bssid, bssid, 6) == 0) {
      slot = &n;
      break;
    } else if (!oldest || (int32_t)(oldest->lastMs - n.lastMs) > 0) {
      oldest = &n;
    }
  }
  const bool isNew = slot == nullptr;
  if (isNew) {
    slot = freeSlot ? freeSlot : oldest;  // full: forget the longest-silent one
    memset(slot, 0, sizeof *slot);
    memcpy(slot->bssid, bssid, 6);
    slot->firstMs = now;
    slot->bestRssi = (int8_t)rssi;
    slot->used = true;
    unique_++;
  }
  snprintf(slot->ssid, sizeof slot->ssid, "%s", ssid ? ssid : "");
  slot->channel = channel;
  slot->auth = auth;
  slot->rssi = (int8_t)rssi;
  if (rssi > slot->bestRssi) slot->bestRssi = (int8_t)rssi;
  slot->lastMs = now;
  if (slot->sightings < 0xFFFF) slot->sightings++;
  return isNew;
}

int WifiTable::count(uint32_t now) const {
  int c = 0;
  for (const auto &n : net_)
    if (n.used && now - n.lastMs < WINDOW_MS) c++;
  return c;
}

int WifiTable::snapshot(WifiNet *out, int max, uint32_t now) const {
  int n = 0;
  for (const auto &w : net_) {
    if (!w.used || now - w.lastMs >= WINDOW_MS) continue;
    int pos = n < max ? n : max;  // insertion sort, strongest first
    while (pos > 0 && out[pos - 1].rssi < w.rssi) {
      if (pos < max) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < max) {
      out[pos] = w;
      if (n < max) n++;
    }
  }
  return n;
}

const WifiNet *WifiTable::find(const uint8_t bssid[6]) const {
  for (const auto &n : net_)
    if (n.used && memcmp(n.bssid, bssid, 6) == 0) return &n;
  return nullptr;
}
