// Fake radio for the PC simulator: plays a scripted scene through the real
// tracker detection + persistence code.
//   - minute 0+  : your own AirTag, travelling with your phone (never alerts)
//   - minute 0+  : a separated AirTag riding along, address rotating every 15 min
//   - minute 3-6 : a Tile you drive past
#include <Arduino.h>

#include "ble_scanner.h"
#include "tracker_detect.h"

namespace ble {
namespace {
uint32_t lastFeed = 0, adverts = 0, trackers = 0;
bool started = false;
void feed(TrackerWatch &w, const uint8_t *payload, size_t len, uint8_t a5, uint8_t a0, int rssi) {
  TrackerAdvert adv;
  adverts++;
  if (!classifyAdvert(payload, len, adv)) return;
  trackers++;
  uint8_t addr[6] = {a0, 0x3a, 0x11, 0x22, 0x33, a5};
  w.onSighting(addr, adv, rssi, millis());
}
}  // namespace

void begin() { started = true; }

void drain(TrackerWatch &w) {
  const uint32_t now = millis();
  if (now - lastFeed < 20000) return;  // a burst every 20 s
  lastFeed = now;
  const uint32_t minute = now / 60000;
  static const uint8_t airtagLost[] = {0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, 0x10, 1, 2, 3, 4, 5,
                                       6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                       21, 22, 0x01, 0x00};
  static const uint8_t airtagMine[] = {0x07, 0xFF, 0x4C, 0x00, 0x12, 0x02, 0x10, 0x01};
  static const uint8_t tile[] = {0x02, 0x01, 0x06, 0x03, 0x03, 0xED, 0xFE};
  static const uint8_t phone[] = {0x02, 0x01, 0x1A, 0x0A, 0xFF, 0x4C, 0x00, 0x10, 0x05,
                                  0x01, 0x18, 0x00, 0x00, 0x00};
  feed(w, airtagMine, sizeof airtagMine, 0x01, 0x7e, -48);
  feed(w, airtagLost, sizeof airtagLost, 0x02, (uint8_t)(0xc4 + minute / 15), -71);
  if (minute >= 3 && minute < 6) feed(w, tile, sizeof tile, 0x03, 0x9b, -83);
  for (int i = 0; i < 20; i++) feed(w, phone, sizeof phone, 0x04, (uint8_t)i, -90);
}

int nearbyDevices() { return started ? 23 : 0; }
uint32_t advertsHeard() { return adverts; }
uint32_t trackerAdverts() { return trackers; }
bool listening() { return started; }

}  // namespace ble
