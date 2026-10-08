// Fake Wi-Fi radio for the PC simulator: a street's worth of networks run
// through the real WifiTable, with an in-memory "log file". From minute 8
// you "drive": the networks around change every 2 minutes. Minute 12:
// someone floods "disconnect" frames. From minute 15: an open copy of
// "5quatch HQ" shows up next to the real one (evil twin).
#include <Arduino.h>

#include <string.h>
#include <string>

#include "wifi_scanner.h"

namespace wifi {
namespace {
WifiTable table;
bool on = true, started = false, rec = false;
uint32_t sess = 0, savedCount = 0;
uint32_t lastScan = 0;
std::string logText = std::string(LOG_HEADER) + "\n";

struct Fake { const char *ssid; uint8_t auth, ch; int rssi; };
const Fake kScene[] = {
    {"5quatch HQ", 3, 6, -42},       {"NETGEAR47", 3, 1, -63},
    {"xfinitywifi", 0, 11, -67},     {"DIRECT-7B-HP OfficeJet", 3, 6, -70},
    {"", 3, 11, -72},                {"Ring Setup 4C", 0, 1, -75},
    {"TP-Link_2.4G_9A1C", 7, 3, -78},{"Bob's \"Fast\" WiFi", 6, 9, -81},
    {"ATT8kZ2", 3, 1, -84},          {"Tesla Model Y", 3, 6, -88},
    {"Starbucks WiFi", 0, 11, -90},  {"FBI Van #3", 3, 4, -93},
};

// One network heard: into the table, and into the log while recording.
void add(const uint8_t *bssid, const char *ssid, uint8_t auth, uint8_t ch, int rssi, uint32_t now) {
  table.update(bssid, ssid, ch, auth, rssi, now);
  WifiNet *n = table.find(bssid);
  if (rec && n->loggedIn != sess) {
    n->loggedIn = sess;
    savedCount++;
    char line[160];
    formatLogLine(line, sizeof line, sess, now / 1000, *n);
    logText += line;
    logText += '\n';
  }
}

void scan() {
  const uint32_t now = millis();
  const uint32_t minute = now / 60000;
  const uint8_t place = minute < 8 ? 0 : (uint8_t)(1 + (minute - 8) / 2);
  int i = 0;
  for (const Fake &f : kScene) {
    // 0x02 = a "locally administered" address: never a real company prefix
    const uint8_t bssid[6] = {0x02, (uint8_t)(0x50 + i * 7), 0x82, (uint8_t)(0x10 + i), place, (uint8_t)i};
    i++;
    add(bssid, f.ssid, f.auth, f.ch, f.rssi + (int)(now / 20000 % 3), now);
  }
  if (minute < 8) {  // a cheap spy cam's hotspot in the house
    const uint8_t spy[6] = {0x0a, 0x9b, 0x8c, 0x7d, 0x6e, 0x5f};
    add(spy, "HDWiFiCam-9F21", 0, 6, -71, now);
  }
  if (minute >= 17 && minute < 19) {  // a Flock camera's setup hotspot
    const uint8_t flock[6] = {0xb4, 0x1e, 0x52, 0x0a, 0x1b, 0x2c};
    add(flock, "Flock-0A1B2C", 3, 11, -80, now);
  }
  if (minute >= 15) {  // evil twin: an open "5quatch HQ"
    const uint8_t twin[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    add(twin, "5quatch HQ", 0, 6, -58, now);
  }
}
}  // namespace

void begin() { started = true; }
void setEnabled(bool v) { on = v; }
bool enabled() { return on; }
bool scanning() { return started && on; }
int nearby() {
  if (on && started && (lastScan == 0 || millis() - lastScan >= 20000)) {
    lastScan = millis();
    scan();
  }
  return table.count(millis());
}
uint32_t seenThisBoot() { return table.uniqueSeen(); }
uint32_t session() { return sess; }
uint32_t takeKickoffs() { return millis() / 60000 == 12 ? 5 : 0; }  // flood in minute 12
bool sniffing() { return false; }
bool takeCameraFrame(uint8_t *mac, int8_t &rssi, bool &flock) {
  // At home a Wyze camera streams to the house Wi-Fi (one report per 20 s).
  static uint32_t last = 0;
  if (millis() / 60000 >= 8 || millis() - last < 20000) return false;
  last = millis();
  const uint8_t wyze[6] = {0x2c, 0xaa, 0x8e, 0x12, 0x34, 0x56};
  memcpy(mac, wyze, 6);
  rssi = -58;
  flock = false;
  return true;
}
void setRecording(bool r) {
  if (r && !rec) { sess++; savedCount = 0; lastScan = 0; }
  rec = r;
}
bool recording() { return rec; }
void setLocation(bool, int32_t, int32_t) {}
uint32_t savedThisSession() {
  nearby();  // the sim "scans" whenever someone looks
  return savedCount;
}
int snapshot(WifiNet *out, int max) {
  nearby();  // the sim "scans" whenever someone looks
  return table.snapshot(out, max, millis());
}
bool logReady() { return true; }
bool logFull() { return false; }
uint32_t logBytes() { return (uint32_t)logText.size(); }
void clearLog() { logText = std::string(LOG_HEADER) + "\n"; }
void dumpLog() { printf("%s", logText.c_str()); }

}  // namespace wifi
