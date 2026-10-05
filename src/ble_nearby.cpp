// ble_nearby.cpp - see ble_nearby.h.

#include "ble_nearby.h"

#include <stdio.h>
#include <string.h>

#include "tracker_detect.h"

namespace {

inline uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

// Bluetooth SIG company identifiers for makers you're likely to meet.
const char *companyName(uint16_t id) {
  switch (id) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x0087: return "Garmin";
    case 0x0171: return "Amazon";
    case 0x038F: return "Xiaomi";
    case 0x009E: return "Bose";
    case 0x012D: return "Sony";
    case 0x0002: return "Intel";
    case 0x000F: return "Broadcom";
    case 0x000D: return "Texas Instr.";
    case 0x0059: return "Nordic";
    case 0x02E5: return "Espressif";
    default: return nullptr;
  }
}

void put(char *label, size_t n, const char *s) { snprintf(label, n, "%s", s); }

}  // namespace

int describeAdvert(const uint8_t *p, size_t len, char *label, size_t n) {
  // 1. Trackers get a clear label (and whether they're with their owner).
  TrackerAdvert t;
  if (classifyAdvert(p, len, t)) {
    snprintf(label, n, "%s (%s)", trackerName(t.kind), t.separated ? "lost" : "own");
    return 2;
  }

  // 2. Walk the AD records once, remembering what we find.
  char name[21] = "";
  const char *maker = nullptr;
  const char *kind = nullptr;
  for (size_t i = 0; i < len;) {
    const uint8_t fieldLen = p[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) break;
    const uint8_t type = p[i + 1];
    const uint8_t *d = p + i + 2;
    const size_t dn = fieldLen - 1;

    if ((type == 0x09 || type == 0x08) && dn > 0 && !name[0]) {  // device name
      size_t k = 0;
      for (size_t j = 0; j < dn && k < sizeof(name) - 1; j++)
        name[k++] = (d[j] >= 0x20 && d[j] < 0x7F) ? (char)d[j] : '?';  // printable only
      name[k] = 0;
      while (k > 0 && name[k - 1] == ' ') name[--k] = 0;
    } else if (type == 0xFF && dn >= 2) {  // manufacturer data
      const uint16_t company = le16(d);
      if (!maker) maker = companyName(company);
      if (company == 0x004C && dn >= 3) {  // Apple: the first byte says what it is
        switch (d[2]) {
          case 0x07: kind = "AirPods/Beats"; break;
          case 0x09: kind = "AirPlay device"; break;
          case 0x10: case 0x0C: case 0x0F: kind = "Apple device"; break;
        }
      } else if (company == 0x0006) {
        kind = "Windows PC";  // Microsoft's "Swift Pair"/CDP beacons come from PCs
      }
    } else if ((type == 0x16 || type == 0x03 || type == 0x02) && dn >= 2) {
      const uint16_t uuid = le16(d);
      if (uuid == 0xFE2C) kind = "Fast Pair device";  // Google Fast Pair accessory
      else if (uuid == 0xFD69) kind = "Samsung phone";
      else if (uuid == 0xFD5A) kind = "Samsung device";
      else if (uuid == 0xFE9F) kind = "Google device";
    }
    i += 1 + fieldLen;
  }

  if (name[0]) { put(label, n, name); return 3; }
  if (kind) { put(label, n, kind); return 2; }
  if (maker) { put(label, n, maker); return 1; }
  put(label, n, "unknown");
  return 0;
}

void NearbyTable::update(const uint8_t addr[6], int rssi, const uint8_t *payload, size_t len,
                         uint32_t now) {
  NearbyDevice *slot = nullptr, *freeSlot = nullptr, *oldest = nullptr;
  for (auto &d : dev_) {
    if (!d.used) {
      if (!freeSlot) freeSlot = &d;
    } else if (memcmp(d.addr, addr, 6) == 0) {
      slot = &d;
      break;
    } else if (!oldest || (int32_t)(oldest->lastMs - d.lastMs) > 0) {
      oldest = &d;
    }
  }
  const bool known = slot != nullptr;
  if (!slot) slot = freeSlot ? freeSlot : oldest;

  char label[sizeof(slot->label)];
  const int rank = describeAdvert(payload, len, label, sizeof label);
  if (!known) {
    memcpy(slot->addr, addr, 6);
    slot->firstMs = now;
    slot->rssi = (int8_t)rssi;
    slot->labelRank = 0;
    slot->label[0] = 0;
    slot->used = true;
  } else {
    slot->rssi = (int8_t)((slot->rssi * 3 + rssi) / 4);  // smooth: signal wobbles a lot
  }
  // Devices send several kinds of broadcast; keep the most specific label seen.
  if (rank >= slot->labelRank || !slot->label[0]) {
    memcpy(slot->label, label, sizeof label);
    slot->labelRank = (uint8_t)rank;
  }
  slot->lastMs = now;
}

int NearbyTable::count(uint32_t now) const {
  int n = 0;
  for (const auto &d : dev_)
    if (d.used && now - d.lastMs < WINDOW_MS) n++;
  return n;
}

int NearbyTable::snapshot(NearbyDevice *out, int max, uint32_t now) const {
  int n = 0;
  for (const auto &d : dev_) {
    if (!d.used || now - d.lastMs >= WINDOW_MS) continue;
    // insert, keeping strongest signal first
    int pos = n < max ? n : max;
    while (pos > 0 && out[pos - 1].rssi < d.rssi) {
      if (pos < max) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < max) {
      out[pos] = d;
      if (n < max) n++;
    }
  }
  return n;
}
