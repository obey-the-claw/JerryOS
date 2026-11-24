// ble_log.cpp - see ble_log.h.

#include "ble_log.h"

#include <stdio.h>
#include <string.h>

const char BLE_LOG_HEADER[] = "session,uptime_s,address,label,kind,rssi,lat,lon";

const char *bleKindName(BleKind k) {
  switch (k) {
    case BleKind::Tracker: return "tracker";
    case BleKind::LostTracker: return "lost tracker";
    case BleKind::Follower: return "follower";
    case BleKind::Camera: return "camera";
    default: return "device";
  }
}

int formatBleLogLine(char *out, size_t outSize, uint32_t session, uint32_t uptimeSec, const uint8_t a[6],
                     const char *label, BleKind kind, int rssi, bool hasLocation, int32_t latE7, int32_t lonE7) {
  char name[50];  // quoted, quotes doubled, control characters -> '?'
  size_t k = 0;
  for (size_t i = 0; label && label[i] && k + 2 < sizeof name; i++) {
    const uint8_t c = (uint8_t)label[i];
    if (c == '"') {
      name[k++] = '"';
      name[k++] = '"';
    } else {
      name[k++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
    }
  }
  name[k] = 0;
  char where[32] = ",";
  if (hasLocation) {
    auto deg = [](char *p, size_t n, int32_t e7) {
      const uint32_t v = e7 < 0 ? (uint32_t)(-(int64_t)e7) : (uint32_t)e7;
      return snprintf(p, n, "%s%lu.%07lu", e7 < 0 ? "-" : "", (unsigned long)(v / 10000000),
                      (unsigned long)(v % 10000000));
    };
    int w = deg(where, sizeof where, latE7);
    where[w++] = ',';
    deg(where + w, sizeof where - w, lonE7);
  }
  return snprintf(out, outSize, "%lu,%lu,%02X:%02X:%02X:%02X:%02X:%02X,\"%s\",%s,%d,%s", (unsigned long)session,
                  (unsigned long)uptimeSec, a[5], a[4], a[3], a[2], a[1], a[0], name, bleKindName(kind), rssi, where);
}

bool SeenSet::insert(const uint8_t addr[6]) {
  uint32_t h = 2166136261u;  // FNV-1a picks the starting slot
  for (int i = 0; i < 6; i++) h = (h ^ addr[i]) * 16777619u;
  for (int probe = 0; probe < CAP; probe++) {
    const int s = (int)((h + probe) % CAP);
    if (!used_[s]) {
      if (n_ >= CAP * 3 / 4) return false;  // nearly full: stop logging new ones
      used_[s] = true;
      memcpy(addr_[s], addr, 6);
      n_++;
      return true;
    }
    if (memcmp(addr_[s], addr, 6) == 0) return false;  // already logged
  }
  return false;
}

void SeenSet::clear() {
  memset(used_, 0, sizeof used_);
  n_ = 0;
}
