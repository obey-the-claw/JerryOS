// Fake Wi-Fi radio for the PC simulator: a street's worth of networks run
// through the real WifiTable, with an in-memory "log file".
#include <Arduino.h>

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

void scan() {
  const uint32_t now = millis();
  int i = 0;
  for (const Fake &f : kScene) {
    uint8_t bssid[6] = {0x3c, 0x52, 0x82, 0x10, 0x00, (uint8_t)i++};
    table.update(bssid, f.ssid, f.ch, f.auth, f.rssi + (int)(now / 20000 % 3), now);
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
void setRecording(bool r) {
  if (r && !rec) { sess++; savedCount = 0; lastScan = 0; }
  rec = r;
}
bool recording() { return rec; }
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
