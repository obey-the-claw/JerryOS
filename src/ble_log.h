// ble_log.h - the Bluetooth side of REC.
//
// While REC is on, every Bluetooth device heard gets one line in /ble.csv
// per recording (like the Wi-Fi log): what it is, its signal, and where you
// were if the rig box has a GPS fix. Trackers, followers and cameras are
// marked so they stand out. Pure logic here (unit-tested on a PC); the file
// itself lives next to the Wi-Fi log (wifi_scanner.cpp).
#pragma once
#include <stddef.h>
#include <stdint.h>

extern const char BLE_LOG_HEADER[];  // "session,uptime_s,address,label,kind,rssi,lat,lon"

// What a logged device is (the "kind" column).
enum class BleKind : uint8_t { Device, Tracker, LostTracker, Follower, Camera };
const char *bleKindName(BleKind k);  // "device", "tracker", "lost tracker", ...

// One CSV line, no newline. addr is stored backwards (as NimBLE gives it)
// and printed the usual way round. Returns the length.
int formatBleLogLine(char *out, size_t outSize, uint32_t session, uint32_t uptimeSec, const uint8_t addr[6],
                     const char *label, BleKind kind, int rssi, bool hasLocation, int32_t latE7, int32_t lonE7);

// Which devices have already been logged in this recording.
class SeenSet {
 public:
  static constexpr int CAP = 1024;  // more than that in one recording: the rest aren't logged
  bool insert(const uint8_t addr[6]);  // true = new (and now remembered)
  void clear();
  int size() const { return n_; }

 private:
  uint8_t addr_[CAP][6] = {};
  bool used_[CAP] = {};
  int n_ = 0;
};
