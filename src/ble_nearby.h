// ble_nearby.h - "what's around me": every BLE device heard recently, with
// the best human label we can work out from its broadcast alone.
//
// Pure logic (no radio), so it's unit-tested on a PC. Passive only: we never
// ask a device for more information, we just read what it already broadcasts.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Best label for one advertisement, e.g. "JBL Flip 5", "AirPods/Beats",
// "Apple device", "Microsoft", "AirTag (lost)". Returns how specific it is:
// 3 = the device's own name, 2 = a known device type, 1 = maker only, 0 = unknown.
int describeAdvert(const uint8_t *payload, size_t len, char *label, size_t labelSize);

struct NearbyDevice {
  uint8_t addr[6];
  int8_t rssi;           // smoothed signal strength (dBm): -40 close, -95 far
  uint8_t labelRank;     // describeAdvert's score for `label`
  uint32_t firstMs, lastMs;
  char label[22];
  bool used;
};

class NearbyTable {
 public:
  static constexpr int MAX = 64;
  static constexpr uint32_t WINDOW_MS = 60000;  // "nearby" = heard in the last minute

  void update(const uint8_t addr[6], int rssi, const uint8_t *payload, size_t len, uint32_t now);
  int count(uint32_t now) const;
  // Devices heard in the last minute, strongest first. Returns how many.
  int snapshot(NearbyDevice *out, int max, uint32_t now) const;

 private:
  NearbyDevice dev_[MAX] = {};
};
