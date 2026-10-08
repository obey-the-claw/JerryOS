// ble_scanner.cpp - NimBLE glue for Tracker Watch (device only; the PC
// simulator uses tools/sim/ble_scanner_sim.cpp instead).
//
// The BLE stack calls onResult() from its own task for every broadcast it
// hears. That callback stays tiny: classify the broadcast, and if it's a
// tracker, drop a small record into a queue. The main loop picks the queue
// up in drain(). This way the two tasks never touch the same data at once.
// The "BLE Nearby" table is shared, so it sits behind a lock (mutex) that
// each side holds for a fraction of a millisecond.

#include "ble_scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

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

struct PopupSighting {
  uint8_t addr[6];
  Popup kind;
  uint32_t ms;
};

QueueHandle_t queue = nullptr;
QueueHandle_t popupQueue = nullptr;  // pop-up style broadcasts (for spam detection)

struct CamSightingMsg {
  uint8_t addr[6];
  CamMatch match;
  int8_t rssi;
  uint32_t ms;
};
QueueHandle_t camQueue = nullptr;  // Flock gear

struct RigMsg {
  uint8_t addr[6];
  RigStatus status;
  int8_t rssi;
  uint32_t ms;
};
QueueHandle_t rigQueue = nullptr;  // status beacons from the Pi Zero box
SemaphoreHandle_t nearbyLock = nullptr;
NearbyTable nearbyTable;  // every device heard lately (guarded by nearbyLock)
std::atomic<uint32_t> adverts{0}, trackerCount{0}, dropped{0};
bool started = false;
int newCameras_ = 0;  // new cameras found during drain() (main loop only)

class Listener : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *dev) override {
    const uint32_t now = millis();
    adverts++;
    const uint8_t *addr = dev->getAddress().getNative();
    const uint8_t *payload = dev->getPayload();
    const size_t len = dev->getPayloadLength();
    xSemaphoreTake(nearbyLock, portMAX_DELAY);
    nearbyTable.update(addr, dev->getRSSI(), payload, len, now);
    xSemaphoreGive(nearbyLock);

    const Popup popup = popupKind(payload, len);
    if (popup != Popup::None) {
      PopupSighting p;
      memcpy(p.addr, addr, 6);
      p.kind = popup;
      p.ms = now;
      xQueueSend(popupQueue, &p, 0);  // full = we're already flooded; dropping is fine
    }

    RigMsg rig;
    if (parseRigBeacon(payload, len, rig.status)) {
      memcpy(rig.addr, addr, 6);
      rig.rssi = (int8_t)dev->getRSSI();
      rig.ms = now;
      xQueueSend(rigQueue, &rig, 0);
      return;
    }

    CamMatch cam;
    if (matchCameraBle(payload, len, cam)) {
      CamSightingMsg c;
      memcpy(c.addr, addr, 6);
      c.match = cam;
      c.rssi = (int8_t)dev->getRSSI();
      c.ms = now;
      xQueueSend(camQueue, &c, 0);
    }

    TrackerAdvert adv;
    if (!classifyAdvert(payload, len, adv)) return;
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
  popupQueue = xQueueCreate(48, sizeof(PopupSighting));
  camQueue = xQueueCreate(16, sizeof(CamSightingMsg));
  rigQueue = xQueueCreate(4, sizeof(RigMsg));
  nearbyLock = xSemaphoreCreateMutex();
  NimBLEDevice::init("");  // no name: we never advertise
  startScan();
  started = true;
  Serial.println(F("[ble] passive tracker scan started"));
}

void drain(TrackerWatch &watch, AttackWatch &attacks, CameraWatch &cams, RigLink &rigLink) {
  if (!started) return;
  Sighting s;
  while (xQueueReceive(queue, &s, 0) == pdTRUE) watch.onSighting(s.addr, s.adv, s.rssi, s.ms);
  PopupSighting p;
  while (xQueueReceive(popupQueue, &p, 0) == pdTRUE) attacks.onPopup(p.addr, p.kind, p.ms);
  RigMsg r;
  while (xQueueReceive(rigQueue, &r, 0) == pdTRUE) rigLink.onBeacon(r.addr, r.status, r.rssi, r.ms);
  CamSightingMsg c;
  while (xQueueReceive(camQueue, &c, 0) == pdTRUE)
    if (cams.onSighting(c.addr, c.match, c.rssi, c.ms)) newCameras_++;

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

int takeNewCameras() {
  const int n = newCameras_;
  newCameras_ = 0;
  return n;
}

int nearbyDevices() {
  if (!started) return 0;
  xSemaphoreTake(nearbyLock, portMAX_DELAY);
  const int n = nearbyTable.count(millis());
  xSemaphoreGive(nearbyLock);
  return n;
}

int nearbySnapshot(NearbyDevice *out, int max) {
  if (!started) return 0;
  xSemaphoreTake(nearbyLock, portMAX_DELAY);
  const int n = nearbyTable.snapshot(out, max, millis());
  xSemaphoreGive(nearbyLock);
  return n;
}
uint32_t advertsHeard() { return adverts; }
uint32_t trackerAdverts() { return trackerCount; }
bool listening() { return started && NimBLEDevice::getScan()->isScanning(); }

}  // namespace ble
