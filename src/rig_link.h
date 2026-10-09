// rig_link.h - the Rig Dashboard link: Jerry <- Pi Zero "Scanner" box.
//
// The box broadcasts a small Bluetooth "status beacon" every ~2 s
// (tools/pi/jerry_beacon.py). Jerry only LISTENS for it, so he still never
// transmits anything. The beacon carries the box's GPS fix and its own
// counts. While the fix is fresh, Jerry's Wi-Fi log lines get a location.
//
// Beacon = one manufacturer-data record, company ID 0xFFFF (reserved by the
// Bluetooth SIG for testing / private use), then 22 bytes, little-endian:
//
//   off  size  field
//    0    2    magic "JR"
//    2    1    version (1)
//    3    1    flags: bit0 = GPS fix
//    4    2    sequence number (goes up every update)
//    6    4    latitude  x 1e7 (signed)
//   10    4    longitude x 1e7 (signed)
//   14    1    satellites in use
//   15    1    speed, km/h (255 = 255+)
//   16    2    Wi-Fi networks the box sees
//   18    2    Bluetooth devices the box sees
//   20    1    alerts on the box (0 = none)
//   21    1    box battery % (255 = on mains / unknown)
//
// Version 2 (JerryOS 1.4) adds the box's clock, so Jerry knows the time and
// can work out sunrise / sunset. 24 bytes, still one ordinary broadcast:
//    4    2    local time: minute of the day 0-1439 (0xFFFF = clock not set)
//              (was a sequence number in v1; nothing used it)
//   22    2    bits 0-8: day of the year 1-366,
//              bits 9-15: UTC offset in 15-minute steps (signed)
// Jerry reads both versions.
//
// Pairing: anyone could broadcast this, so Jerry only trusts the one box
// you pair (hold R on the Rig screen). Pure logic, unit-tested on a PC.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct RigStatus {
  bool fix;
  int32_t latE7, lonE7;
  uint8_t sats, speedKmh;
  uint16_t wifi, ble;
  uint8_t alerts, battery;
  uint16_t seq;      // v1 only
  // v2: the box's clock
  bool clock;        // valid local time + date
  uint16_t clockMin; // local minute of the day, 0-1439
  uint16_t doy;      // day of the year, 1-366
  int8_t utcOff15;   // local time - UTC, in 15-minute steps (e.g. -28 = UTC-7)
};

// Is this Bluetooth broadcast a rig beacon? Fills `out` if so.
bool parseRigBeacon(const uint8_t *payload, size_t len, RigStatus &out);
// Build one (used by the tests and the simulator; the Pi script does the same).
size_t buildRigBeacon(const RigStatus &s, uint8_t *out, size_t outSize);

class RigLink {
 public:
  static constexpr uint32_t FRESH_MS = 10000;  // older than this = "lost the box"

  // A beacon was heard from `addr`.
  void onBeacon(const uint8_t addr[6], const RigStatus &s, int rssi, uint32_t now);

  bool paired() const { return paired_; }
  void pair(const uint8_t addr[6]);
  void unpair() { paired_ = false; }
  const uint8_t *pairedAddr() const { return pairedAddr_; }

  // The paired box, heard recently?
  bool connected(uint32_t now) const { return paired_ && heard_ && now - lastMs_ < FRESH_MS; }
  const RigStatus &status() const { return status_; }
  uint32_t lastHeardMs() const { return lastMs_; }
  int rssi() const { return rssi_; }
  // A fresh GPS fix from the paired box (for tagging the Wi-Fi log).
  bool location(uint32_t now, int32_t &latE7, int32_t &lonE7) const;

  // An unpaired box heard recently (offered for pairing on the Rig screen).
  bool candidate(uint32_t now, uint8_t addr[6]) const;

 private:
  bool paired_ = false, heard_ = false;
  uint8_t pairedAddr_[6] = {};
  RigStatus status_ = {};
  uint32_t lastMs_ = 0;
  int rssi_ = 0;
  uint8_t candAddr_[6] = {};
  uint32_t candMs_ = 0;
  bool cand_ = false;
};
