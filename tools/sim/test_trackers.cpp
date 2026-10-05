// Unit tests for tracker detection + persistence (runs on a PC).
//   g++ -std=gnu++17 -I src tools/sim/test_trackers.cpp src/tracker_detect.cpp src/tracker_watch.cpp
#include <stdio.h>
#include <string.h>

#include <vector>

#include "tracker_detect.h"
#include "tracker_watch.h"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                  \
  do {                                                               \
    checks++;                                                        \
    if (!(cond)) {                                                   \
      failures++;                                                    \
      printf("  FAIL line %d: %s\n", __LINE__, #cond);               \
    }                                                                \
  } while (0)

using Bytes = std::vector<uint8_t>;

// Apple Find My advert. full=true -> separated (0x19 long form).
static Bytes appleAdv(uint8_t status, bool full) {
  if (full) {
    Bytes b = {0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, status};
    for (int i = 0; i < 22; i++) b.push_back(0xA0 + i);  // public key part
    b.push_back(0x01);
    b.push_back(0x00);
    return b;
  }
  return {0x07, 0xFF, 0x4C, 0x00, 0x12, 0x02, status, 0x01};
}
static Bytes serviceData(uint16_t uuid, std::vector<uint8_t> data) {
  Bytes b = {0x02, 0x01, 0x06, (uint8_t)(3 + data.size()), 0x16, (uint8_t)(uuid & 0xFF),
             (uint8_t)(uuid >> 8)};
  b.insert(b.end(), data.begin(), data.end());
  return b;
}
static bool classify(const Bytes &b, TrackerAdvert &out) { return classifyAdvert(b.data(), b.size(), out); }

static void testDetection() {
  printf("detection\n");
  TrackerAdvert a;
  CHECK(classify(appleAdv(0x10, true), a) && a.kind == TrackerKind::AirTag && a.separated);
  CHECK(classify(appleAdv(0x10, false), a) && a.kind == TrackerKind::AirTag && !a.separated);
  CHECK(classify(appleAdv(0x20, true), a) && a.kind == TrackerKind::FindMy && a.separated);  // accessory
  CHECK(classify(appleAdv(0x30, true), a) && a.kind == TrackerKind::FindMy);                 // AirPods
  CHECK(classify(appleAdv(0x50, true), a) && a.kind == TrackerKind::AirTag);  // battery bits ignored
  CHECK(!classify(appleAdv(0x00, true), a));  // lost iPhone/Mac: not an item tracker
  // Apple "Nearby"/Continuity (type 0x10), not Find My:
  CHECK(!classify(Bytes{0x0A, 0xFF, 0x4C, 0x00, 0x10, 0x05, 0x01, 0x18, 0x00, 0x00, 0x00}, a));

  Bytes tileList = {0x02, 0x01, 0x06, 0x03, 0x03, 0xED, 0xFE};
  CHECK(classify(tileList, a) && a.kind == TrackerKind::Tile && a.separated);
  CHECK(classify(serviceData(0xFEED, {0x02, 0x00, 1, 2, 3, 4, 5, 6, 7, 8}), a) && a.kind == TrackerKind::Tile);
  CHECK(classify(serviceData(0xFD84, {0x01}), a) && a.kind == TrackerKind::Tile);

  CHECK(classify(serviceData(0xFD5A, {0x13, 1, 2, 3}), a) && a.kind == TrackerKind::SmartTag && a.separated);
  CHECK(classify(serviceData(0xFD5A, {0x12, 1, 2, 3}), a) && a.separated);    // offline
  CHECK(classify(serviceData(0xFD5A, {0x15, 1, 2, 3}), a) && !a.separated);   // connected
  CHECK(classify(serviceData(0xFD5A, {0x11, 1, 2, 3}), a) && !a.separated);   // premature offline
  CHECK(!classify(serviceData(0xFD5A, {0x20, 1, 2, 3}), a));                  // not a SmartTag frame
  CHECK(!classify(serviceData(0xFD69, {0x13}), a));                           // Samsung phone

  CHECK(classify(serviceData(0xFEAA, {0x41, 9, 9}), a) && a.kind == TrackerKind::Google && a.separated);
  CHECK(classify(serviceData(0xFEAA, {0x40, 9, 9}), a) && !a.separated);
  CHECK(!classify(serviceData(0xFEAA, {0x10, 0x00, 0x03}), a));  // Eddystone-URL beacon

  // Junk must never crash or match.
  CHECK(!classify(Bytes{}, a));
  CHECK(!classify(Bytes{0x05, 0xFF, 0x4C}, a));      // length runs past the end
  CHECK(!classify(Bytes{0x02, 0xFF, 0x4C}, a));      // too short for Apple data
  CHECK(!classify(Bytes{0x01, 0x16}, a));            // empty service data
  uint8_t noise[31];
  for (int seed = 0; seed < 20000; seed++) {          // fuzz: random bytes
    for (auto &x : noise) x = (uint8_t)((seed * 1103515245u + (&x - noise) * 12345u) >> 7);
    classifyAdvert(noise, sizeof noise, a);
  }
}

// Simulate sightings every `everySec` seconds from `startMin` for `durMin`
// minutes, rotating the address every `rotateMin` minutes.
struct Sim {
  TrackerWatch w;
  int alertsFired = 0;
  int firstAlertMin = -1;
  void run(TrackerAdvert adv, int startMin, int durMin, int everySec, int rotateMin, int window,
           int gapPatternMin = 0) {
    for (int t = startMin * 60; t < (startMin + durMin) * 60; t += everySec) {
      const int minute = t / 60;
      const bool inGap = gapPatternMin && (minute % (gapPatternMin + 1)) != 0;
      if (!inGap) {
        uint8_t addr[6] = {0xC0, 0x01, 0x02, 0x03, 0x04, (uint8_t)(rotateMin ? minute / rotateMin : 0)};
        w.onSighting(addr, adv, -70, (uint32_t)t * 1000);
      }
      if (w.tick((uint32_t)t * 1000, window)) {
        alertsFired++;
        if (firstAlertMin < 0) firstAlertMin = minute;
      }
    }
  }
};

static void testPersistence() {
  printf("persistence\n");
  const TrackerAdvert airtagLost = {TrackerKind::AirTag, true};
  const TrackerAdvert airtagHome = {TrackerKind::AirTag, false};
  const TrackerAdvert tile = {TrackerKind::Tile, true};

  {  // A separated AirTag rides along for 40 min, address rotating every 15 min.
    Sim s;
    s.run(airtagLost, 0, 40, 20, 15, 30);
    printf("  rotating AirTag: first alert at minute %d\n", s.firstAlertMin);
    CHECK(s.alertsFired == 1);
    CHECK(s.firstAlertMin >= 29 && s.firstAlertMin <= 31);
  }
  {  // Your own AirTag, with you, for an hour: never alerts.
    Sim s;
    s.run(airtagHome, 0, 60, 20, 15, 30);
    CHECK(s.alertsFired == 0);
    CHECK(s.w.summary(TrackerKind::AirTag, 3599000).withOwner == 1);
    CHECK(s.w.summary(TrackerKind::AirTag, 3599000).devicesNow == 0);
  }
  {  // Drive past a lost AirTag for 5 minutes: no alert.
    Sim s;
    s.run(airtagLost, 0, 5, 20, 0, 30);
    s.run(TrackerAdvert{TrackerKind::Tile, true}, 5, 60, 600, 0, 30);  // just time passing
    CHECK(s.alertsFired == 0);
  }
  {  // Seen only every 3rd minute (2-minute gaps) still counts as following.
    Sim s;
    s.run(tile, 0, 40, 20, 0, 30, 2);
    CHECK(s.alertsFired == 1);
  }
  {  // 3-minute gaps break the streak: no alert.
    Sim s;
    s.run(tile, 0, 40, 20, 0, 30, 3);
    CHECK(s.alertsFired == 0);
  }
  {  // Shorter window from Settings (10 min) alerts sooner.
    Sim s;
    s.run(tile, 0, 15, 20, 0, 10);
    CHECK(s.firstAlertMin >= 9 && s.firstAlertMin <= 11);
  }
  {  // Acknowledge: quiet for one window, then alerts again if still there.
    Sim s;
    s.run(tile, 0, 31, 20, 0, 30);
    CHECK(s.w.anyAlert());
    s.w.acknowledge(TrackerKind::Tile, 31 * 60000, 30);
    CHECK(!s.w.anyAlert());
    s.run(tile, 31, 20, 20, 0, 30);
    CHECK(s.alertsFired == 1);  // still quiet 20 min later
    s.run(tile, 51, 15, 20, 0, 30);
    CHECK(s.alertsFired == 2);  // window passed, still following: alert again
  }
  {  // Static-address Tile: "seen for" view.
    Sim s;
    s.run(tile, 0, 31, 20, 0, 30);
    const TrackedDevice *list[8];
    const int n = s.w.listDevices(TrackerKind::Tile, 31 * 60000, list, 8);
    CHECK(n == 1);
    CHECK(n == 1 && (list[0]->lastSeenMs - list[0]->firstSeenMs) / 60000 >= 30);
    KindSummary k = s.w.summary(TrackerKind::Tile, 31 * 60000 - 1);
    CHECK(k.devicesNow == 1 && k.streakMin >= 30 && k.alerting);
  }
  {  // Many addresses (busy parking lot) never overflow the table.
    TrackerWatch w;
    for (int i = 0; i < 500; i++) {
      uint8_t addr[6] = {1, 2, 3, 4, (uint8_t)(i >> 8), (uint8_t)i};
      w.onSighting(addr, airtagLost, -80, i * 1000);
      w.tick(i * 1000, 30);
    }
    const TrackedDevice *list[60];
    CHECK(w.listDevices(TrackerKind::AirTag, 500000, list, 60) == TrackerWatch::MAX_DEVICES);
  }
  {  // millis() wraps around after ~49.7 days: still works.
    TrackerWatch w;
    const uint32_t base = 0xFFFFFFFFu - 10 * 60000;
    int alerts = 0;
    for (uint32_t t = 0; t < 40 * 60; t += 20) {
      uint8_t addr[6] = {9, 9, 9, 9, 9, 9};
      w.onSighting(addr, tile, -60, base + t * 1000);
      alerts += w.tick(base + t * 1000, 30);
    }
    CHECK(alerts <= 1);
  }
}

int main() {
  testDetection();
  testPersistence();
  printf("\n%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
