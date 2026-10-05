// tracker_detect.cpp - see tracker_detect.h and docs/TRACKERS.md.
//
// A BLE advertisement is a list of small records ("AD structures"):
//   [length][type][data ...]   where length counts type + data.
// We look at three record types:
//   0xFF  manufacturer data   [company id, 2 bytes LE][data]
//   0x16  service data        [16-bit service UUID, LE][data]
//   0x02 / 0x03  list of 16-bit service UUIDs

#include "tracker_detect.h"

const char *trackerName(TrackerKind k) {
  switch (k) {
    case TrackerKind::AirTag: return "AirTag";
    case TrackerKind::FindMy: return "Find My";
    case TrackerKind::Tile: return "Tile";
    case TrackerKind::SmartTag: return "SmartTag";
    case TrackerKind::Google: return "Google";
    default: return "?";
  }
}

namespace {

constexpr uint16_t APPLE = 0x004C;
constexpr uint16_t UUID_TILE = 0xFEED;
constexpr uint16_t UUID_TILE_NEW = 0xFD84;
constexpr uint16_t UUID_SAMSUNG_TAG = 0xFD5A;
constexpr uint16_t UUID_GOOGLE_FHN = 0xFEAA;

inline uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

// Apple "Find My" (offline finding) broadcast, manufacturer data:
//   [0] 0x12      Find My payload type
//   [1] length    0x19 (25) = full key: separated from owner for a while
//                 anything else (e.g. 0x02) = recently with its owner
//   [2] status    bits 4-5: 0 = Apple phone/Mac, 1 = AirTag,
//                           2 = Find My accessory, 3 = AirPods
bool apple(const uint8_t *d, size_t n, TrackerAdvert &out) {
  if (n < 3 || d[0] != 0x12) return false;
  const int type = (d[2] & 0x30) >> 4;
  if (type == 0) return false;  // a lost iPhone/Mac, not an item tracker
  out.kind = (type == 1) ? TrackerKind::AirTag : TrackerKind::FindMy;
  out.separated = d[1] == 0x19;
  return true;
}

// Samsung SmartTag, service data 0xFD5A. Byte 0: top bits 0b00010xxx mark a
// SmartTag; the low 3 bits are the connection state:
//   0b011 overmature offline (away from owner a long time)
//   0b010 offline            (away from owner)
//   0b001 premature offline / 0b101 connected (with owner)
bool samsung(const uint8_t *d, size_t n, TrackerAdvert &out) {
  if (n < 1 || (d[0] & 0xF8) != 0x10) return false;
  const int state = d[0] & 0x07;
  out.kind = TrackerKind::SmartTag;
  out.separated = state == 0x03 || state == 0x02;
  return true;
}

// Google Find Hub Network tracker, service data 0xFEAA (Eddystone UUID).
// Frame type 0x40 = normal, 0x41 = "unwanted tracking protection" mode,
// which turns on when the tag has been away from its owner.
// (Other Eddystone frames 0x00-0x30 are ordinary beacons: ignored.)
bool google(const uint8_t *d, size_t n, TrackerAdvert &out) {
  if (n < 1 || (d[0] != 0x40 && d[0] != 0x41)) return false;
  out.kind = TrackerKind::Google;
  out.separated = d[0] == 0x41;
  return true;
}

void tile(TrackerAdvert &out) {
  // Tile has no "away from owner" flag, and older Tiles never change their
  // address, so we count every Tile and let persistence decide.
  out.kind = TrackerKind::Tile;
  out.separated = true;
}

}  // namespace

bool classifyAdvert(const uint8_t *p, size_t len, TrackerAdvert &out) {
  bool sawTileUuid = false;
  size_t i = 0;
  while (i < len) {
    const uint8_t fieldLen = p[i];
    if (fieldLen == 0 || i + 1 + fieldLen > len) break;  // end or malformed
    const uint8_t type = p[i + 1];
    const uint8_t *data = p + i + 2;
    const size_t n = fieldLen - 1;

    if (type == 0xFF && n >= 2 && le16(data) == APPLE) {
      if (apple(data + 2, n - 2, out)) return true;
    } else if (type == 0x16 && n >= 2) {
      const uint16_t uuid = le16(data);
      if (uuid == UUID_TILE || uuid == UUID_TILE_NEW) {
        tile(out);
        return true;
      }
      if (uuid == UUID_SAMSUNG_TAG && samsung(data + 2, n - 2, out)) return true;
      if (uuid == UUID_GOOGLE_FHN && google(data + 2, n - 2, out)) return true;
    } else if (type == 0x02 || type == 0x03) {
      for (size_t k = 0; k + 1 < n; k += 2) {
        const uint16_t uuid = le16(data + k);
        if (uuid == UUID_TILE || uuid == UUID_TILE_NEW) sawTileUuid = true;
      }
    }
    i += 1 + fieldLen;
  }
  if (sawTileUuid) {
    tile(out);
    return true;
  }
  return false;
}
