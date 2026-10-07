// camera_watch.h - Flock licence-plate (ALPR) cameras and their gear.
//
// Flock Safety cameras (and their "Penguin" battery packs and Raven
// gunshot sensors) leave radio fingerprints. Signatures come from the
// flock-you project (colonelpanichacks, MIT), flock-you-go (DaveMath) and
// @NitekryDPaul's field research, taken from a Flock camera firmware dump.
// Only the high-confidence ones are used:
//
//   sure   Wi-Fi from Flock's own registered address prefix b4:1e:52
//          Wi-Fi network named "Flock-XXXXXX" (the camera's setup hotspot)
//          Bluetooth "Penguin-" + 10 digits, or "FS Ext Battery" (batteries)
//          Bluetooth Flock accessory service e8ccbb38-9532-46a8-9fe5-...
//   maybe  Bluetooth maker 0x09C8 (XUNTONG, the battery's chip maker)
//          Bluetooth service 0x3100-0x3500 (Raven sensors)
//
// The project's wider "community" prefix list is left out on purpose: it
// includes generic chip makers (e.g. Espressif) used by thousands of
// ordinary gadgets, and Jerry himself.
//
// Listen-only, like everything else. Pure logic, unit-tested on a PC.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum class CamKind : uint8_t { FlockCamera, FlockBattery, Raven };
const char *camKindName(CamKind k);  // "Flock camera", "Flock battery", "Raven sensor"

struct CamMatch {
  CamKind kind;
  bool sure;
  const char *why;  // short reason: "Flock WiFi", "Penguin name"...
};

// Bluetooth broadcast -> Flock gear?
bool matchCameraBle(const uint8_t *payload, size_t len, CamMatch &out);
// Wi-Fi network (from a scan) -> Flock gear?
bool matchCameraWifi(const uint8_t bssid[6], const char *ssid, CamMatch &out);
// Any Wi-Fi frame's sender address -> Flock gear? (prefix check only)
bool isFlockWifiAddr(const uint8_t mac[6]);

struct CamSighting {
  uint8_t addr[6];
  CamKind kind;
  bool sure;
  const char *why;
  int8_t rssi;
  uint32_t firstMs, lastMs;
  bool used;
};

class CameraWatch {
 public:
  static constexpr int MAX = 24;
  static constexpr uint32_t NEAR_MS = 120000;  // "nearby now" = heard in the last 2 min

  // Returns true when this is a camera we haven't heard in the last 10 min
  // (worth telling you about).
  bool onSighting(const uint8_t addr[6], const CamMatch &m, int rssi, uint32_t now);

  int nearby(uint32_t now) const;          // heard in the last 2 min
  uint32_t passedToday() const { return passed_; }  // different ones since power-on
  // Most recent first. Returns how many.
  int list(const CamSighting **out, int max) const;

 private:
  CamSighting cam_[MAX] = {};
  uint32_t passed_ = 0;
};
