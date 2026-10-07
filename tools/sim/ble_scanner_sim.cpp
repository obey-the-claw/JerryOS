// Fake radio for the PC simulator: plays a scripted scene through the real
// tracker detection, persistence and "BLE Nearby" code.
//   - minute 0+  : your own AirTag, travelling with your phone (never alerts)
//   - minute 0+  : a separated AirTag riding along, address rotating every 15 min
//   - minute 3-6 : a Tile you drive past
//   - minute 0-8 : at home: a Windows laptop, earbuds, AirPods, a Samsung
//                  phone and 14 iPhones (addresses change every 12 min)
//   - minute 10  : someone with a Flipper sprays fake AirPods pop-ups
//   - always     : your JBL speaker and a mystery gadget, which ride along
//                  on the drive from minute 8 (see wifi_scanner_sim.cpp)
#include <Arduino.h>

#include "ble_scanner.h"
#include "tracker_detect.h"

namespace ble {
namespace {
uint32_t lastFeed = 0, adverts = 0, trackers = 0;
bool started = false, fedOnce = false;
NearbyTable table;

void feed(TrackerWatch &w, const uint8_t *payload, size_t len, uint8_t a5, uint8_t a0, int rssi) {
  adverts++;
  uint8_t addr[6] = {a0, 0x3a, 0x11, 0x22, 0x33, a5};
  table.update(addr, rssi, payload, len, millis());
  TrackerAdvert adv;
  if (!classifyAdvert(payload, len, adv)) return;
  trackers++;
  w.onSighting(addr, adv, rssi, millis());
}
}  // namespace

void begin() { started = true; }

void drain(TrackerWatch &w, AttackWatch &attacks) {
  const uint32_t now = millis();
  if (fedOnce && now - lastFeed < 20000) return;  // a burst every 20 s
  fedOnce = true;
  lastFeed = now;
  const uint32_t minute = now / 60000;
  static const uint8_t airtagLost[] = {0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, 0x10, 1, 2, 3, 4, 5,
                                       6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                       21, 22, 0x01, 0x00};
  static const uint8_t airtagMine[] = {0x07, 0xFF, 0x4C, 0x00, 0x12, 0x02, 0x10, 0x01};
  static const uint8_t tile[] = {0x02, 0x01, 0x06, 0x03, 0x03, 0xED, 0xFE};
  static const uint8_t phone[] = {0x02, 0x01, 0x1A, 0x0A, 0xFF, 0x4C, 0x00, 0x10, 0x05,
                                  0x01, 0x18, 0x00, 0x00, 0x00};
  static const uint8_t speaker[] = {0x02, 0x01, 0x06, 0x0B, 0x09, 'J', 'B', 'L', ' ',
                                    'F', 'l', 'i', 'p', ' ', '5'};
  static const uint8_t laptop[] = {0x1E, 0xFF, 0x06, 0x00, 0x01, 0x09, 0x20, 0x02, 1, 2, 3,
                                   4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                   20, 21, 22, 23};
  static const uint8_t earbuds[] = {0x02, 0x01, 0x06, 0x06, 0x16, 0x2C, 0xFE, 0x00, 0x12, 0x34};
  static const uint8_t airpods[] = {0x08, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x0E, 0x20};
  static const uint8_t galaxy[] = {0x02, 0x01, 0x1A, 0x03, 0x03, 0x69, 0xFD};
  static const uint8_t mystery[] = {0x02, 0x01, 0x06, 0x05, 0xFF, 0x34, 0x12, 0xAB, 0xCD};
  feed(w, airtagMine, sizeof airtagMine, 0x01, 0x7e, -48);
  feed(w, airtagLost, sizeof airtagLost, 0x02, (uint8_t)(0xc4 + minute / 15), -71);
  if (minute >= 3 && minute < 6) feed(w, tile, sizeof tile, 0x03, 0x9b, -83);
  const bool home = minute < 8;
  if (home) {
    for (int i = 0; i < 14; i++)
      feed(w, phone, sizeof phone, (uint8_t)(0x40 + minute / 12), (uint8_t)i, -66 - i * 2);
    feed(w, laptop, sizeof laptop, 0x05, 0x11, -61);
    feed(w, earbuds, sizeof earbuds, 0x05, 0x12, -74);
    feed(w, airpods, sizeof airpods, 0x05, 0x13, -57);
    feed(w, galaxy, sizeof galaxy, 0x05, 0x14, -79);
  }
  if (minute == 10)  // 30 "AirPods" from 30 different addresses in one go
    for (int i = 0; i < 30; i++) {
      const uint8_t spam[] = {0x08, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x0E, 0x20};
      const uint8_t addr[6] = {(uint8_t)i, 0xF1, 0x1B, 0x9E, (uint8_t)(now >> 8), 0xC2};
      table.update(addr, -60, spam, sizeof spam, now);
      attacks.onPopup(addr, popupKind(spam, sizeof spam), now);
    }
  feed(w, speaker, sizeof speaker, 0x05, 0x10, -52);
  feed(w, mystery, sizeof mystery, 0x05, 0x15, home ? -92 : -64);
}

int nearbyDevices() { return table.count(millis()); }
int nearbySnapshot(NearbyDevice *out, int max) { return table.snapshot(out, max, millis()); }
uint32_t advertsHeard() { return adverts; }
uint32_t trackerAdverts() { return trackers; }
bool listening() { return started; }

}  // namespace ble
