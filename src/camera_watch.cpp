// camera_watch.cpp - see camera_watch.h.

#include "camera_watch.h"

#include <ctype.h>
#include <string.h>

namespace {

inline uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

// Flock accessory service UUID e8ccbb38-9532-46a8-9fe5-1814df172e6f, in the
// byte order it appears in a broadcast (least significant byte first).
const uint8_t kFlockService[16] = {0x6f, 0x2e, 0x17, 0xdf, 0x14, 0x18, 0xe5, 0x9f,
                                   0xa8, 0x46, 0x32, 0x95, 0x38, 0xbb, 0xcc, 0xe8};

bool startsWithNoCase(const char *s, size_t n, const char *prefix) {
  const size_t p = strlen(prefix);
  if (n < p) return false;
  for (size_t i = 0; i < p; i++)
    if (tolower((unsigned char)s[i]) != prefix[i]) return false;
  return true;
}

bool isPenguinName(const char *s, size_t n) {  // "Penguin-" + exactly 10 digits
  if (n != 18 || !startsWithNoCase(s, n, "penguin-")) return false;
  for (size_t i = 8; i < 18; i++)
    if (!isdigit((unsigned char)s[i])) return false;
  return true;
}

}  // namespace

const char *camKindName(CamKind k) {
  switch (k) {
    case CamKind::FlockCamera: return "Flock camera";
    case CamKind::FlockBattery: return "Flock battery";
    case CamKind::Raven: return "Raven sensor";
  }
  return "?";
}

bool isFlockWifiAddr(const uint8_t mac[6]) {
  return mac[0] == 0xb4 && mac[1] == 0x1e && mac[2] == 0x52;  // Flock Safety (IEEE)
}

bool matchCameraWifi(const uint8_t bssid[6], const char *ssid, CamMatch &out) {
  if (isFlockWifiAddr(bssid)) {
    out = {CamKind::FlockCamera, true, "Flock WiFi"};
    return true;
  }
  if (ssid && startsWithNoCase(ssid, strlen(ssid), "flock-")) {
    out = {CamKind::FlockCamera, true, "Flock hotspot"};
    return true;
  }
  return false;
}

bool matchCameraBle(const uint8_t *p, size_t len, CamMatch &out) {
  bool maybe = false;
  CamMatch maybeMatch{};
  for (size_t i = 0; i < len;) {
    const uint8_t fieldLen = p[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) break;
    const uint8_t type = p[i + 1];
    const uint8_t *d = p + i + 2;
    const size_t dn = fieldLen - 1;

    if (type == 0x09 || type == 0x08) {  // name
      const char *name = (const char *)d;
      if (isPenguinName(name, dn)) {
        out = {CamKind::FlockBattery, true, "Penguin name"};
        return true;
      }
      if (dn == 14 && startsWithNoCase(name, dn, "fs ext battery")) {
        out = {CamKind::FlockBattery, true, "FS Ext Battery"};
        return true;
      }
    } else if ((type == 0x06 || type == 0x07) && dn >= 16) {  // 128-bit services
      for (size_t k = 0; k + 16 <= dn; k += 16)
        if (memcmp(d + k, kFlockService, 16) == 0) {
          out = {CamKind::FlockCamera, true, "Flock service"};
          return true;
        }
    } else if ((type == 0x02 || type == 0x03) && dn >= 2) {  // 16-bit services
      for (size_t k = 0; k + 2 <= dn; k += 2) {
        const uint16_t u = le16(d + k);
        if (u >= 0x3100 && u <= 0x3500 && !maybe) {
          maybe = true;
          maybeMatch = {CamKind::Raven, false, "Raven service"};
        }
      }
    } else if (type == 0xFF && dn >= 2 && le16(d) == 0x09C8 && !maybe) {
      maybe = true;
      maybeMatch = {CamKind::FlockBattery, false, "XUNTONG maker"};
    }
    i += 1 + fieldLen;
  }
  if (maybe) out = maybeMatch;
  return maybe;
}

bool CameraWatch::onSighting(const uint8_t addr[6], const CamMatch &m, int rssi, uint32_t now) {
  CamSighting *slot = nullptr, *freeSlot = nullptr, *oldest = nullptr;
  for (auto &c : cam_) {
    if (!c.used) {
      if (!freeSlot) freeSlot = &c;
    } else if (memcmp(c.addr, addr, 6) == 0) {
      slot = &c;
      break;
    } else if (!oldest || (int32_t)(oldest->lastMs - c.lastMs) > 0) {
      oldest = &c;
    }
  }
  bool news;
  if (!slot) {
    slot = freeSlot ? freeSlot : oldest;
    memset(slot, 0, sizeof *slot);
    memcpy(slot->addr, addr, 6);
    slot->firstMs = now;
    slot->used = true;
    passed_++;
    news = true;
  } else {
    news = now - slot->lastMs > 10 * 60000;  // back after a long while: tell again
    if (news) slot->firstMs = now;
  }
  if (m.sure || !slot->sure) {  // keep the strongest reason
    slot->kind = m.kind;
    slot->sure = m.sure;
    slot->why = m.why;
  }
  slot->rssi = (int8_t)rssi;
  slot->lastMs = now;
  return news;
}

int CameraWatch::nearby(uint32_t now) const {
  int n = 0;
  for (const auto &c : cam_)
    if (c.used && now - c.lastMs < NEAR_MS) n++;
  return n;
}

int CameraWatch::list(const CamSighting **out, int max) const {
  int n = 0;
  for (const auto &c : cam_) {
    if (!c.used) continue;
    int pos = n < max ? n : max;
    while (pos > 0 && (int32_t)(c.lastMs - out[pos - 1]->lastMs) > 0) {
      if (pos < max) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < max) {
      out[pos] = &c;
      if (n < max) n++;
    }
  }
  return n;
}
