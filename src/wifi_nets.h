// wifi_nets.h - the Wi-Fi networks Jerry has heard, and how a log line looks.
//
// Pure logic (no radio, no files), so it's unit-tested on a PC. The radio
// side is in wifi_scanner.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct WifiNet {
  uint8_t bssid[6];   // the access point's hardware address
  char ssid[33];      // network name as broadcast (raw bytes, may be empty = hidden)
  uint8_t channel;
  uint8_t auth;       // ESP-IDF wifi_auth_mode_t value
  int8_t rssi;        // last signal (dBm)
  int8_t bestRssi;    // strongest signal this session
  uint32_t firstMs, lastMs;
  uint16_t sightings;
  bool used;
};

// "open", "WEP", "WPA2", "WPA3"...
const char *authName(uint8_t auth);
inline bool isOpen(uint8_t auth) { return auth == 0; }

// Network name made safe for the little screen font: "(hidden)" for no name,
// '?' for anything that isn't plain text (emoji etc.).
void displaySsid(const WifiNet &n, char *out, size_t outSize);

// One CSV log line (no newline):  session,uptime_s,bssid,"ssid",auth,channel,rssi
// Returns the length written. The header row is LOG_HEADER.
extern const char LOG_HEADER[];
int formatLogLine(char *out, size_t outSize, uint32_t session, uint32_t uptimeSec,
                  const WifiNet &n);

class WifiTable {
 public:
  static constexpr int MAX = 96;
  static constexpr uint32_t WINDOW_MS = 90000;  // "around now" = heard in the last 90 s

  // Returns true the first time this network is seen (that's when it's logged).
  bool update(const uint8_t bssid[6], const char *ssid, uint8_t channel, uint8_t auth, int rssi,
              uint32_t now);
  int count(uint32_t now) const;
  uint32_t uniqueSeen() const { return unique_; }
  // Networks heard recently, strongest first. Returns how many.
  int snapshot(WifiNet *out, int max, uint32_t now) const;
  const WifiNet *find(const uint8_t bssid[6]) const;

 private:
  WifiNet net_[MAX] = {};
  uint32_t unique_ = 0;
};
