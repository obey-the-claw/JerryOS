// wifi_scanner.cpp - see wifi_scanner.h (device only; the PC simulator uses
// tools/sim/wifi_scanner_sim.cpp instead).
//
// All radio and file work happens in one background task, so a scan
// (~1.5 s) or a slow flash write never freezes the screen. The network table
// is shared with the screens behind a lock, like the BLE table.

#include "wifi_scanner.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>

#include <atomic>

namespace wifi {
namespace {

constexpr const char *LOG_PATH = "/wifi.csv";
constexpr uint32_t SCAN_EVERY_MS = 20000;      // one sweep every 20 s
constexpr uint32_t MS_PER_CHANNEL = 120;       // beacons come every ~102 ms
constexpr uint32_t MAX_LOG_BYTES = 3u << 20;   // 3 MB, about 40,000 networks

SemaphoreHandle_t tableLock = nullptr;  // guards `table`
SemaphoreHandle_t fileLock = nullptr;   // guards the log file
WifiTable table;
std::atomic<bool> on{true}, running{false}, mounted{false}, full{false};
std::atomic<bool> rec{false}, scanNow{false};
std::atomic<uint32_t> bytes{0}, saved{0}, sessionNo{0};

struct Lock {  // holds a lock until the end of the { } block
  explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m_); }
  SemaphoreHandle_t m_;
};

void mountLog() {
  // First boot after JerryOS is flashed, the storage area holds leftovers
  // from the stock firmware, so it gets formatted once (a few seconds).
  if (!LittleFS.begin(false) && !LittleFS.begin(true)) {
    Serial.println(F("[wifi] storage could not be mounted - not logging"));
    return;
  }
  Lock l(fileLock);
  if (!LittleFS.exists(LOG_PATH)) {
    File f = LittleFS.open(LOG_PATH, FILE_WRITE);
    if (f) {
      f.println(LOG_HEADER);
      f.close();
    }
  }
  File f = LittleFS.open(LOG_PATH, FILE_READ);
  bytes = f ? f.size() : 0;
  if (f) f.close();
  full = bytes >= MAX_LOG_BYTES;
  mounted = true;
  Serial.printf("[wifi] log ready: %lu bytes\n", (unsigned long)bytes);
}

void appendLines(const String &lines) {
  if (!mounted || full || !lines.length()) return;
  Lock l(fileLock);
  File f = LittleFS.open(LOG_PATH, FILE_APPEND);
  if (!f) return;
  f.print(lines);
  bytes = f.size();
  f.close();
  if (bytes >= MAX_LOG_BYTES) full = true;
}

void scanOnce() {
  // passive=true: listen only, no probe requests.
  const int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true, /*passive=*/true,
                                  MS_PER_CHANNEL);
  if (n < 0) return;
  const uint32_t now = millis();
  String newLines;
  {
    Lock l(tableLock);
    for (int i = 0; i < n; i++) {
      String ssid;
      uint8_t auth;
      int32_t rssi, channel;
      uint8_t *bssid;
      if (!WiFi.getNetworkInfo(i, ssid, auth, rssi, bssid, channel) || !bssid) continue;
      table.update(bssid, ssid.c_str(), (uint8_t)channel, auth, rssi, now);
      WifiNet *net = table.find(bssid);
      const uint32_t sess = sessionNo;
      if (rec && !full && net && net->loggedIn != sess) {  // first time this recording
        net->loggedIn = sess;
        char line[160];
        formatLogLine(line, sizeof line, sess, now / 1000, *net);
        newLines += line;
        newLines += '\n';
        saved++;
      }
    }
  }
  WiFi.scanDelete();
  appendLines(newLines);
}

// --- Kick-off (deauth) listening ---
// After each scan, Jerry listens to raw Wi-Fi management frames for a few
// seconds, hopping across channels 1-13. Still receive-only. Each
// "deauthentication" or "disassociation" frame counts; a flood of them is
// someone knocking devices off their Wi-Fi.
constexpr uint32_t SNIFF_MS = 6500;
constexpr uint32_t HOP_MS = 500;
std::atomic<uint32_t> kickoffs{0};
std::atomic<bool> sniffingNow{false};

void onFrame(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const auto *pkt = (const wifi_promiscuous_pkt_t *)buf;
  if (pkt->rx_ctrl.sig_len < 24) return;
  const uint8_t frameType = pkt->payload[0];
  if (frameType == 0xC0 || frameType == 0xA0) kickoffs++;  // deauth / disassoc
}

void sniff() {
  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(onFrame);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) return;
  sniffingNow = true;
  static uint8_t ch = 1;
  for (uint32_t t = 0; t < SNIFF_MS && on; t += HOP_MS) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    ch = ch % 13 + 1;
    vTaskDelay(pdMS_TO_TICKS(HOP_MS));
  }
  esp_wifi_set_promiscuous(false);
  sniffingNow = false;
}

void task(void *) {
  mountLog();
  WiFi.persistent(false);  // don't save anything Wi-Fi related to flash
  running = true;
  bool radioUp = false;
  uint32_t lastScan = 0;
  for (;;) {
    if (on && !radioUp) {      // station mode, but we never connect to anything
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(false);
      radioUp = true;
      lastScan = millis() - SCAN_EVERY_MS;  // scan straight away
    } else if (!on && radioUp) {  // switched off in Settings: Wi-Fi radio off, saves battery
      WiFi.mode(WIFI_OFF);
      radioUp = false;
    }
    if (radioUp && (scanNow.exchange(false) || millis() - lastScan >= SCAN_EVERY_MS)) {
      lastScan = millis();
      scanOnce();
      sniff();
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

}  // namespace

void begin() {
  if (tableLock) return;
  tableLock = xSemaphoreCreateMutex();
  fileLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "wifi", 6144, nullptr, 1, nullptr, 0);
  Serial.println(F("[wifi] passive scan task started"));
}

void setEnabled(bool v) { on = v; }
bool enabled() { return on; }
uint32_t takeKickoffs() { return kickoffs.exchange(0); }
bool sniffing() { return sniffingNow; }

bool scanning() { return running && on; }  // (radio follows `on` within 250 ms)

int nearby() {
  if (!tableLock) return 0;
  Lock l(tableLock);
  return table.count(millis());
}

uint32_t seenThisBoot() {
  if (!tableLock) return 0;
  Lock l(tableLock);
  return table.uniqueSeen();
}

uint32_t session() { return sessionNo; }

void setRecording(bool r) {
  if (r == rec) return;
  if (r) {
    Preferences p;  // number each recording so trips can be told apart in the log
    if (p.begin("jerryos", false)) {
      sessionNo = p.getUInt("wsess", 0) + 1;
      p.putUInt("wsess", sessionNo);
      p.end();
    } else {
      sessionNo = sessionNo + 1;
    }
    saved = 0;
    scanNow = true;  // save what's around right away
  }
  rec = r;
  Serial.printf("[wifi] recording %s (session %lu)\n", r ? "ON" : "off", (unsigned long)sessionNo);
}

bool recording() { return rec; }
uint32_t savedThisSession() { return saved; }

int snapshot(WifiNet *out, int max) {
  if (!tableLock) return 0;
  Lock l(tableLock);
  return table.snapshot(out, max, millis());
}

bool logReady() { return mounted; }
bool logFull() { return full; }
uint32_t logBytes() { return bytes; }

void clearLog() {
  if (!mounted) return;
  Lock l(fileLock);
  File f = LittleFS.open(LOG_PATH, FILE_WRITE);  // FILE_WRITE starts the file over
  if (f) {
    f.println(LOG_HEADER);
    bytes = f.size();
    f.close();
  }
  full = false;
  Serial.println(F("[wifi] log erased"));
}

void dumpLog() {
  Print &out = Serial;
  out.println(F("=== JERRYOS WIFI LOG BEGIN ==="));
  if (mounted) {
    Lock l(fileLock);
    File f = LittleFS.open(LOG_PATH, FILE_READ);
    if (f) {
      uint8_t buf[256];
      while (f.available()) {
        const size_t n = f.read(buf, sizeof buf);
        if (!n) break;
        out.write(buf, n);
      }
      f.close();
    }
  }
  out.printf("=== JERRYOS WIFI LOG END %lu ===\n", (unsigned long)bytes);
}

}  // namespace wifi
