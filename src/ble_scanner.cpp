// ble_scanner.cpp - NimBLE glue for Tracker Watch (device only; the PC
// simulator uses tools/sim/ble_scanner_sim.cpp instead).
//
// The BLE stack calls onResult() from its own task for every broadcast it
// hears. That callback stays tiny: classify the broadcast, and if it's a
// tracker, drop a small record into a queue. The main loop picks the queue
// up in drain(). This way the two tasks never touch the same data at once.

#include "ble_scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>

#include <atomic>

#include "tracker_detect.h"

namespace ble {
namespace {

struct Sighting {
  uint8_t addr[6];
  TrackerAdvert adv;
  int8_t rssi;
  uint32_t ms;
};

QueueHandle_t queue = nullptr;
std::atomic<uint32_t> adverts{0}, trackerCount{0}, dropped{0};
std::atomic<int> nearby{0};
bool started = false;

// "How many devices are around": a small table of address fingerprints
// (hashes) with when we last heard each. Only touched by the BLE task.
struct Recent { uint32_t hash, ms; };
Recent recent[128];

uint32_t hashAddr(const uint8_t *a) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (int i = 0; i < 6; i++) h = (h ^ a[i]) * 16777619u;
  return h ? h : 1;
}

void noteNearby(const uint8_t *addr, uint32_t now) {
  const uint32_t h = hashAddr(addr);
  int slot = -1, oldest = 0;
  for (int i = 0; i < 128 && slot < 0; i++) {
    if (recent[i].hash == h || recent[i].hash == 0) slot = i;              // known, or empty
    else if (now - recent[i].ms > now - recent[oldest].ms) oldest = i;     // longest unheard
  }
  recent[slot < 0 ? oldest : slot] = {h, now};

  int count = 0;
  for (const auto &r : recent)
    if (r.hash && now - r.ms < 60000) count++;
  nearby = count;
}

class Listener : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *dev) override {
    const uint32_t now = millis();
    adverts++;
    const uint8_t *addr = dev->getAddress().getNative();
    noteNearby(addr, now);

    TrackerAdvert adv;
    if (!classifyAdvert(dev->getPayload(), dev->getPayloadLength(), adv)) return;
    trackerCount++;
    Sighting s;
    memcpy(s.addr, addr, 6);
    s.adv = adv;
    s.rssi = (int8_t)dev->getRSSI();
    s.ms = now;
    if (xQueueSend(queue, &s, 0) != pdTRUE) dropped++;
  }
};
Listener listener;

void startScan() {
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&listener, /*wantDuplicates=*/true);
  scan->setActiveScan(false);     // PASSIVE: never send scan requests
  scan->setDuplicateFilter(false);
  scan->setMaxResults(0);         // don't keep results in memory
  scan->setInterval(100);         // listen 60 ms out of every 100 ms
  scan->setWindow(60);
  scan->start(0, nullptr, false); // 0 = keep scanning forever
}

}  // namespace

void begin() {
  if (started) return;
  queue = xQueueCreate(64, sizeof(Sighting));
  NimBLEDevice::init("");  // no name: we never advertise
  startScan();
  started = true;
  Serial.println(F("[ble] passive tracker scan started"));
}

void drain(TrackerWatch &watch) {
  if (!started) return;
  Sighting s;
  while (xQueueReceive(queue, &s, 0) == pdTRUE) watch.onSighting(s.addr, s.adv, s.rssi, s.ms);

  // If the stack ever stops scanning (it shouldn't), quietly restart it.
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (!NimBLEDevice::getScan()->isScanning()) {
      Serial.println(F("[ble] scan had stopped - restarting"));
      startScan();
    }
  }
}

int nearbyDevices() { return nearby; }
uint32_t advertsHeard() { return adverts; }
uint32_t trackerAdverts() { return trackerCount; }
bool listening() { return started && NimBLEDevice::getScan()->isScanning(); }

}  // namespace ble
