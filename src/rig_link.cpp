// rig_link.cpp - see rig_link.h.

#include "rig_link.h"

#include <string.h>

namespace {

constexpr uint16_t COMPANY = 0xFFFF;  // Bluetooth SIG: "for testing / internal use"
constexpr uint8_t VERSION = 1;
constexpr size_t BODY = 22;

uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

}  // namespace

bool parseRigBeacon(const uint8_t *p, size_t len, RigStatus &out) {
  for (size_t i = 0; i < len;) {
    const uint8_t fieldLen = p[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) break;
    const uint8_t *d = p + i + 2;
    const size_t dn = fieldLen - 1;
    if (p[i + 1] == 0xFF && dn >= 2 + BODY && get16(d) == COMPANY) {
      const uint8_t *b = d + 2;
      if (b[0] == 'J' && b[1] == 'R' && b[2] == VERSION) {
        out.fix = b[3] & 1;
        out.seq = get16(b + 4);
        out.latE7 = (int32_t)get32(b + 6);
        out.lonE7 = (int32_t)get32(b + 10);
        out.sats = b[14];
        out.speedKmh = b[15];
        out.wifi = get16(b + 16);
        out.ble = get16(b + 18);
        out.alerts = b[20];
        out.battery = b[21];
        // A fix outside the planet isn't a fix.
        if (out.latE7 < -900000000 || out.latE7 > 900000000 || out.lonE7 < -1800000000 ||
            out.lonE7 > 1800000000)
          out.fix = false;
        return true;
      }
    }
    i += 1 + fieldLen;
  }
  return false;
}

size_t buildRigBeacon(const RigStatus &s, uint8_t *o, size_t n) {
  const size_t need = 3 + 2 + 2 + BODY;  // flags record + manufacturer record
  if (n < need) return 0;
  o[0] = 2; o[1] = 0x01; o[2] = 0x06;                 // flags: general discoverable, BLE only
  o[3] = (uint8_t)(1 + 2 + BODY); o[4] = 0xFF;          // manufacturer data
  put16(o + 5, COMPANY);
  uint8_t *b = o + 7;
  b[0] = 'J'; b[1] = 'R'; b[2] = VERSION; b[3] = s.fix ? 1 : 0;
  put16(b + 4, s.seq);
  put32(b + 6, (uint32_t)s.latE7);
  put32(b + 10, (uint32_t)s.lonE7);
  b[14] = s.sats; b[15] = s.speedKmh;
  put16(b + 16, s.wifi);
  put16(b + 18, s.ble);
  b[20] = s.alerts; b[21] = s.battery;
  return need;
}

void RigLink::onBeacon(const uint8_t addr[6], const RigStatus &s, int rssi, uint32_t now) {
  if (paired_ && memcmp(addr, pairedAddr_, 6) == 0) {
    status_ = s;
    lastMs_ = now;
    rssi_ = rssi;
    heard_ = true;
    return;
  }
  memcpy(candAddr_, addr, 6);  // someone else's (or not yet paired): offer it
  candMs_ = now;
  cand_ = true;
}

void RigLink::pair(const uint8_t addr[6]) {
  memcpy(pairedAddr_, addr, 6);
  paired_ = true;
  heard_ = false;
  cand_ = false;
}

bool RigLink::location(uint32_t now, int32_t &latE7, int32_t &lonE7) const {
  if (!connected(now) || !status_.fix) return false;
  latE7 = status_.latE7;
  lonE7 = status_.lonE7;
  return true;
}

bool RigLink::candidate(uint32_t now, uint8_t addr[6]) const {
  if (!cand_ || now - candMs_ > FRESH_MS) return false;
  memcpy(addr, candAddr_, 6);
  return true;
}
