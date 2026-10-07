// camera_watch.cpp - see camera_watch.h.

#include "camera_watch.h"

#include <ctype.h>
#include <string.h>

#include "camera_ouis.h"

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
    case CamKind::Hint: return "Camera";
  }
  return "?";
}

bool isFlockWifiAddr(const uint8_t mac[6]) {
  return mac[0] == 0xb4 && mac[1] == 0x1e && mac[2] == 0x52;  // Flock Safety (IEEE)
}

const char *cameraMaker(const uint8_t mac[6]) {
  int lo = 0, hi = kCamOuiCount - 1;  // binary search: the table is sorted
  const uint32_t key = (uint32_t)mac[0] << 16 | mac[1] << 8 | mac[2];
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const CamOui &o = kCamOuis[mid];
    const uint32_t v = (uint32_t)o.b0 << 16 | o.b1 << 8 | o.b2;
    if (v == key) return kCamBrands[o.brand];
    if (v < key) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

const char *cameraHotspotName(const char *ssid) {
  if (!ssid || !*ssid) return nullptr;
  const size_t n = strlen(ssid);
  // Setup / direct-connect hotspots of cheap spy and IP cameras.
  static const char *const kPrefixes[][2] = {
      {"hdwificam", "HDWiFiCam"}, {"ipcam", "IP camera"},   {"ipc-", "IP camera"},
      {"ipc_", "IP camera"},      {"v380", "V380 cam"},     {"gw_ap", "Yoosee cam"},
      {"cloudcam", "CloudCam"},   {"lookcam", "Lookcam"},   {"hicam", "HiCam"},
      {"minicam", "mini cam"},    {"mini cam", "mini cam"}, {"wificam", "WiFi cam"},
      {"wifi-cam", "WiFi cam"},   {"spycam", "spy cam"},    {"ezviz_", "EZVIZ cam"},
      {"wansview", "Wansview"},   {"tapo_cam", "Tapo cam"}, {"reolink", "Reolink"},
      {"wyze", "Wyze"},           {"ring setup", "Ring"},
  };
  for (const auto &p : kPrefixes)
    if (startsWithNoCase(ssid, n, p[0])) return p[1];
  // V380 cameras call their hotspot "MV" + the device number.
  if (n >= 10 && (ssid[0] == 'M' || ssid[0] == 'm') && (ssid[1] == 'V' || ssid[1] == 'v')) {
    bool digits = true;
    for (size_t i = 2; i < n; i++) digits &= isdigit((unsigned char)ssid[i]) != 0;
    if (digits) return "V380 cam";
  }
  // "camera" anywhere in the name (but not e.g. "Campus").
  for (size_t i = 0; i + 6 <= n; i++)
    if (startsWithNoCase(ssid + i, n - i, "camera")) return "camera WiFi";
  return nullptr;
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
  if (const char *label = cameraHotspotName(ssid)) {
    out = {CamKind::Hint, false, label};
    return true;
  }
  if (const char *maker = cameraMaker(bssid)) {
    out = {CamKind::Hint, false, maker};
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
    (isFlock(m.kind) ? passed_ : hints_)++;
    news = true;
  } else {
    news = now - slot->lastMs > 10 * 60000;  // back after a long while: tell again
    if (news) slot->firstMs = now;
  }
  // Keep the strongest reason: Flock beats a camera hint, "sure" beats "maybe".
  const int have = isFlock(slot->kind) ? (slot->sure ? 3 : 2) : 1;
  const int got = isFlock(m.kind) ? (m.sure ? 3 : 2) : 1;
  if (news || got >= have) {
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

int CameraWatch::nearbyFlock(uint32_t now) const {
  int n = 0;
  for (const auto &c : cam_)
    if (c.used && isFlock(c.kind) && now - c.lastMs < NEAR_MS) n++;
  return n;
}

int CameraWatch::nearbyHints(uint32_t now) const {
  int n = 0;
  for (const auto &c : cam_)
    if (c.used && !isFlock(c.kind) && now - c.lastMs < NEAR_MS) n++;
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
