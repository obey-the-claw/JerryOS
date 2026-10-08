// Unit tests for tracker detection + persistence + BLE Nearby labels (runs on a PC).
//   run with tools/sim/run_tests.sh
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "ble_nearby.h"
#include "tracker_detect.h"
#include "tracker_watch.h"
#include "wifi_nets.h"
#include "follow_watch.h"
#include "attack_watch.h"
#include "status_led.h"
#include "camera_watch.h"
#include "camera_ouis.h"
#include "sound_level.h"
#include "rig_link.h"
#include "funk.h"

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

static void testNearby() {
  printf("BLE Nearby labels + table\n");
  char label[22];
  auto describe = [&](const Bytes &b) { return describeAdvert(b.data(), b.size(), label, sizeof label); };

  const Bytes named = {0x02, 0x01, 0x06, 0x0B, 0x09, 'J', 'B', 'L', ' ', 'F', 'l', 'i', 'p', ' ', '5'};
  CHECK(describe(named) == 3 && strcmp(label, "JBL Flip 5") == 0);
  const Bytes iphone = {0x02, 0x01, 0x1A, 0x0A, 0xFF, 0x4C, 0x00, 0x10, 0x05, 0x01, 0x18, 0, 0, 0};
  CHECK(describe(iphone) == 2 && strcmp(label, "Apple device") == 0);
  const Bytes airpods = {0x08, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x0E, 0x20};
  CHECK(describe(airpods) == 2 && strcmp(label, "AirPods/Beats") == 0);
  const Bytes garmin = {0x05, 0xFF, 0x87, 0x00, 0x01, 0x02};
  CHECK(describe(garmin) == 1 && strcmp(label, "Garmin") == 0);
  const Bytes mystery = {0x05, 0xFF, 0x34, 0x12, 0xAB, 0xCD};
  CHECK(describe(mystery) == 0 && strcmp(label, "unknown") == 0);
  CHECK(describe(serviceData(0xFE2C, {0x00, 0x12, 0x34})) == 2 && strcmp(label, "Fast Pair device") == 0);
  CHECK(describe(appleAdv(0x10, true)) == 2 && strcmp(label, "AirTag (lost)") == 0);
  CHECK(describe(appleAdv(0x10, false)) == 2 && strcmp(label, "AirTag (own)") == 0);
  // Long and non-printable names are cut short and made safe for the screen.
  Bytes longName = {0x1F, 0x09};
  for (int i = 0; i < 30; i++) longName.push_back(i == 3 ? 0x07 : 'A' + i % 26);
  CHECK(describe(longName) == 3 && strlen(label) == 20 && label[3] == '?');
  // Broken lengths never read past the end.
  const Bytes broken = {0x05, 0xFF, 0x4C};
  CHECK(describe(broken) == 0);
  CHECK(describe(Bytes{}) == 0);

  NearbyTable t;
  const uint8_t a[6] = {1, 0, 0, 0, 0, 1}, b[6] = {2, 0, 0, 0, 0, 2};
  t.update(a, -90, iphone.data(), iphone.size(), 1000);
  t.update(b, -50, mystery.data(), mystery.size(), 1000);
  t.update(b, -50, named.data(), named.size(), 2000);    // name arrives in another advert
  t.update(b, -50, mystery.data(), mystery.size(), 3000);  // ...and isn't forgotten
  NearbyDevice out[8];
  int n = t.snapshot(out, 8, 4000);
  CHECK(n == 2 && t.count(4000) == 2);
  CHECK(strcmp(out[0].label, "JBL Flip 5") == 0);         // strongest first
  CHECK(strcmp(out[1].label, "Apple device") == 0);
  t.update(a, -50, iphone.data(), iphone.size(), 5000);    // one spike is smoothed out
  n = t.snapshot(out, 8, 5000);
  CHECK(out[1].rssi < -70);
  CHECK(t.count(3000 + NearbyTable::WINDOW_MS) == 1);  // b silent for a minute: dropped
  CHECK(t.count(5000 + NearbyTable::WINDOW_MS) == 0);  // then a too
  // A crowd bigger than the table: newest replace oldest, count stays capped.
  NearbyTable crowd;
  for (int i = 0; i < 300; i++) {
    uint8_t addr[6] = {(uint8_t)i, (uint8_t)(i >> 8), 0, 0, 0, 9};
    crowd.update(addr, -60 - i % 30, iphone.data(), iphone.size(), 1000 + i);
  }
  CHECK(crowd.count(2000) == NearbyTable::MAX);
  NearbyDevice few[5];
  n = crowd.snapshot(few, 5, 2000);
  CHECK(n == 5 && few[0].rssi >= few[1].rssi && few[3].rssi >= few[4].rssi && few[0].rssi == -60);
}

static void testWifi() {
  printf("Wi-Fi table + log lines\n");
  WifiTable t;
  const uint8_t a[6] = {0x3c, 0x52, 0x82, 1, 2, 3}, b[6] = {0x3c, 0x52, 0x82, 1, 2, 4};
  CHECK(t.update(a, "5quatch HQ", 6, 3, -45, 1000) == true);   // new -> log it
  CHECK(t.update(a, "5quatch HQ", 6, 3, -60, 21000) == false); // seen again -> don't
  CHECK(t.update(b, "", 11, 0, -80, 21000) == true);
  CHECK(t.uniqueSeen() == 2 && t.count(30000) == 2);
  const WifiNet *n = t.find(a);
  CHECK(n && n->bestRssi == -45 && n->rssi == -60 && n->sightings == 2);
  WifiNet out[4];
  CHECK(t.snapshot(out, 4, 30000) == 2 && out[0].rssi == -60);
  CHECK(t.count(21000 + WifiTable::WINDOW_MS) == 0);  // out of range for 90 s

  char line[160];
  formatLogLine(line, sizeof line, 7, 123, *n);
  CHECK(strcmp(line, "7,123,3c:52:82:01:02:03,\"5quatch HQ\",WPA2,6,-60,,") == 0);
  formatLogLine(line, sizeof line, 7, 123, *n, true, 335123456, -1121234567);  // Phoenix-ish
  CHECK(strcmp(line, "7,123,3c:52:82:01:02:03,\"5quatch HQ\",WPA2,6,-60,33.5123456,-112.1234567") == 0);
  formatLogLine(line, sizeof line, 7, 123, *n, true, -5, 1800000000);
  CHECK(strstr(line, ",-0.0000005,180.0000000") != nullptr);
  // Quotes doubled, newline made harmless, so the CSV never breaks.
  WifiNet evil = *n;
  snprintf(evil.ssid, sizeof evil.ssid, "Bob's \"Fast\"\nWiFi,ok");
  formatLogLine(line, sizeof line, 1, 1, evil);
  CHECK(strstr(line, "\"Bob's \"\"Fast\"\"?WiFi,ok\"") != nullptr);
  CHECK(strchr(line, '\n') == nullptr);
  // A 32-character name full of quotes still fits and stays valid.
  memset(evil.ssid, '"', 32);
  evil.ssid[32] = 0;
  const int len = formatLogLine(line, sizeof line, 4000000000u, 4000000000u, evil, true,
                                -900000000, -1800000000);
  CHECK(len > 0 && len < (int)sizeof line && strstr(line, ",-90.0000000,-180.0000000"));
  char tiny[8];
  CHECK(formatLogLine(tiny, sizeof tiny, 1, 1, *n) == 7 && strlen(tiny) == 7);

  char shown[34];
  displaySsid(*t.find(b), shown, sizeof shown);
  CHECK(strcmp(shown, "(hidden)") == 0);
  WifiNet emoji = *n;
  snprintf(emoji.ssid, sizeof emoji.ssid, "Caf\xc3\xa9 \xf0\x9f\x8d\xa9");  // "Café 🍩"
  displaySsid(emoji, shown, sizeof shown);
  CHECK(strcmp(shown, "Caf? ?") == 0);
  CHECK(strcmp(authName(0), "open") == 0 && strcmp(authName(6), "WPA3") == 0 &&
        strcmp(authName(99), "other") == 0);

  WifiTable crowd;  // a city block: table stays capped, every new one still reported
  int fresh = 0;
  for (int i = 0; i < 300; i++) {
    uint8_t m[6] = {1, 2, 3, 4, (uint8_t)(i >> 8), (uint8_t)i};
    fresh += crowd.update(m, "x", 1, 3, -70, 1000 + i);
  }
  CHECK(fresh == 300 && crowd.count(2000) == WifiTable::MAX && crowd.uniqueSeen() == 300);
}

// A world for the follower tests: Wi-Fi networks per place, and BLE devices.
static void scanPlace(SceneTracker &sc, int place, uint32_t now) {
  uint8_t nets[12][6];
  for (int i = 0; i < 12; i++) {
    const uint8_t b[6] = {0xAA, (uint8_t)place, (uint8_t)(place >> 8), 0, 0, (uint8_t)i};
    memcpy(nets[i], b, 6);
  }
  sc.onScan(nets, 12, now);
}
static NearbyDevice dev(uint8_t id, const char *label, int rssi = -60) {
  NearbyDevice d = {};
  const uint8_t a[6] = {id, 0x11, 0x22, 0x33, 0x44, 0x55};
  memcpy(d.addr, a, 6);
  snprintf(d.label, sizeof d.label, "%s", label);
  d.rssi = (int8_t)rssi;
  d.used = true;
  return d;
}

static void testFollow() {
  printf("Followers\n");
  {  // "Have I moved?": same networks = same place; new networks (twice) = moved.
    SceneTracker sc;
    scanPlace(sc, 0, 0);
    scanPlace(sc, 0, 20000);
    CHECK(sc.place() == 0);
    scanPlace(sc, 1, 40000);
    CHECK(sc.place() == 0);  // one odd scan isn't a move
    scanPlace(sc, 1, 60000);
    CHECK(sc.place() == 1 && sc.lastMoveMs() == 60000);
    sc.onScan(nullptr, 0, 80000);  // no Wi-Fi at all: can't tell, stays put
    CHECK(sc.place() == 1);
  }
  {  // Home for 2 hours with a gadget nearby: never a follower.
    SceneTracker sc;
    FollowWatch fw;
    NearbyDevice d[1] = {dev(1, "Garmin")};
    int alerts = 0;
    for (uint32_t t = 0; t < 2 * 3600000u; t += 20000) {
      scanPlace(sc, 0, t);
      alerts += fw.tick(d, 1, sc.place(), t);
    }
    CHECK(alerts == 0 && fw.candidates(2 * 3600000u) == 0);
  }
  {  // A 40-minute drive (new place every ~2 min). A gadget rides along; a
     // passenger's phone changes address every 15 min; a speaker is passed once.
    SceneTracker sc;
    FollowWatch fw;
    int alerts = 0;
    uint32_t firstAlertMs = 0;
    for (uint32_t t = 0; t < 40 * 60000u; t += 20000) {
      const int place = t / 120000;
      scanPlace(sc, place, t);
      NearbyDevice d[3];
      int n = 0;
      d[n++] = dev(7, "unknown", -55);                                    // the stowaway
      d[n++] = dev((uint8_t)(100 + t / (15 * 60000u)), "Apple device");   // rotating phone
      if (place == 5) d[n++] = dev(50, "JBL Flip 5", -80);                // drive-by
      if (fw.tick(d, n, sc.place(), t) && !alerts++) firstAlertMs = t;
    }
    const Follower *list[8];
    const int n = fw.followers(list, 8);
    CHECK(n == 1 && list[0]->addr[0] == 7 && fw.flaggedCount() == 1);
    CHECK(alerts == 1 && firstAlertMs >= FollowWatch::FOLLOW_MIN * 60000);
    CHECK(fw.anyAlert());
    fw.acknowledgeAll();
    CHECK(!fw.anyAlert() && fw.followers(list, 8) == 1);  // still listed, just not alerting
    // "It's mine": gone, and never flagged again.
    fw.trust(list[0]->addr);
    CHECK(fw.followers(list, 8) == 0 && fw.trustedCount() == 1);
    NearbyDevice again[1] = {dev(7, "unknown")};
    int later = 0;
    for (uint32_t t = 40 * 60000u; t < 90 * 60000u; t += 20000) {
      scanPlace(sc, t / 120000, t);
      later += fw.tick(again, 1, sc.place(), t);
    }
    CHECK(later == 0);
  }
  {  // A gap longer than 5 minutes starts the count over.
    SceneTracker sc;
    FollowWatch fw;
    int alerts = 0;
    for (uint32_t t = 0; t < 30 * 60000u; t += 20000) {
      scanPlace(sc, t / 120000, t);
      NearbyDevice d[1] = {dev(9, "unknown")};
      const bool present = t < 12 * 60000u || t >= 19 * 60000u;  // gone 12-19 min
      alerts += fw.tick(d, present ? 1 : 0, sc.place(), t);
    }
    CHECK(alerts == 0);  // only 11 min since it came back
  }
  {  // Trackers are Tracker Watch's job, not ours.
    SceneTracker sc;
    FollowWatch fw;
    int alerts = 0;
    for (uint32_t t = 0; t < 40 * 60000u; t += 20000) {
      scanPlace(sc, t / 120000, t);
      NearbyDevice d[1] = {dev(3, "AirTag (lost)")};
      alerts += fw.tick(d, 1, sc.place(), t);
    }
    CHECK(alerts == 0);
  }
  {  // Trusted list: save/load round trip, and it caps at 16 (oldest dropped).
    FollowWatch a, b;
    for (int i = 0; i < 20; i++) {
      const uint8_t addr[6] = {(uint8_t)i, 1, 2, 3, 4, 5};
      a.trust(addr);
    }
    CHECK(a.trustedCount() == FollowWatch::MAX_TRUSTED);
    b.loadTrusted(a.trustedData(), a.trustedCount());
    const uint8_t first[6] = {0, 1, 2, 3, 4, 5}, last[6] = {19, 1, 2, 3, 4, 5};
    CHECK(!b.isTrusted(first) && b.isTrusted(last));
  }
}

static WifiNet net(const char *ssid, uint8_t auth, uint8_t b4, uint8_t b5) {
  WifiNet n = {};
  const uint8_t b[6] = {0x10, 0x20, 0x30, 0x40, b4, b5};
  memcpy(n.bssid, b, 6);
  snprintf(n.ssid, sizeof n.ssid, "%s", ssid);
  n.auth = auth;
  n.used = true;
  return n;
}

static void testAttacks() {
  printf("Attack detector\n");
  // Pop-up broadcast types.
  const Bytes airpods = {0x08, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x0E, 0x20};
  const Bytes iphone = {0x02, 0x01, 0x1A, 0x0A, 0xFF, 0x4C, 0x00, 0x10, 0x05, 0x01, 0x18, 0, 0, 0};
  const Bytes swift = {0x07, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80, 0x01};
  const Bytes samsung = {0x07, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x81, 0x02};
  CHECK(popupKind(airpods.data(), airpods.size()) == Popup::Apple);
  CHECK(popupKind(iphone.data(), iphone.size()) == Popup::None);
  CHECK(popupKind(swift.data(), swift.size()) == Popup::Windows);
  CHECK(popupKind(samsung.data(), samsung.size()) == Popup::Samsung);
  const Bytes fastpair = serviceData(0xFE2C, {0x00, 0x12, 0x34});
  CHECK(popupKind(fastpair.data(), fastpair.size()) == Popup::Android);
  const Bytes cut = {0x08, 0xFF, 0x4C};
  CHECK(popupKind(cut.data(), cut.size()) == Popup::None);

  {  // Your AirPods all day: one address, never spam.
    AttackWatch a;
    const uint8_t mine[6] = {1, 2, 3, 4, 5, 6};
    int alerts = 0;
    for (uint32_t t = 0; t < 3600000; t += 200) {
      a.onPopup(mine, Popup::Apple, t);
      alerts += a.tick(t);
    }
    CHECK(alerts == 0 && !a.anyAlert());
  }
  {  // A Flipper: a new address every 100 ms for 20 s.
    AttackWatch a;
    int alerts = 0;
    for (uint32_t t = 0; t < 20000; t += 100) {
      const uint8_t addr[6] = {0xC0, (uint8_t)(t >> 8), (uint8_t)t, 9, 9, 9};
      a.onPopup(addr, Popup::Apple, t);
      alerts += a.tick(t);
    }
    CHECK(alerts == 1 && a.anyAlert() && a.state(Threat::Popup).active);
    CHECK(strncmp(a.state(Threat::Popup).detail, "Apple x", 7) == 0);
    a.acknowledgeAll();
    a.tick(20000 + AttackWatch::QUIET_MS + 1);
    CHECK(!a.anyAlert() && !a.state(Threat::Popup).active);  // over after a calm minute
  }
  {  // Wi-Fi kick-offs: a few now and then is normal; a flood isn't.
    AttackWatch a;
    int alerts = 0;
    for (uint32_t s = 0; s < 600; s++) {
      a.onKickoffs(s % 30 == 0 ? 3 : 0, s * 1000);
      alerts += a.tick(s * 1000);
    }
    CHECK(alerts == 0);
    for (uint32_t s = 600; s < 610; s++) {
      a.onKickoffs(25, s * 1000);
      alerts += a.tick(s * 1000);
    }
    CHECK(alerts == 1 && a.state(Threat::Kickoff).active && a.state(Threat::Kickoff).peak >= 30);
  }
  {  // Networks: a normal street, a mesh, an office AP with 5 names: all fine.
    AttackWatch a;
    WifiNet street[] = {net("Home", 3, 1, 1), net("Home", 3, 2, 1), net("Home", 3, 3, 1),
                        net("xfinitywifi", 0, 9, 1), net("XFINITY", 3, 9, 2),
                        net("Corp", 3, 7, 1), net("Corp-Guest", 0, 7, 2), net("Corp-IoT", 3, 7, 3),
                        net("Corp-Voice", 3, 7, 4), net("eduroam", 5, 7, 5)};
    a.checkNetworks(street, sizeof street / sizeof street[0], 1000);
    CHECK(!a.tick(1000) && !a.anyActive());
    // Evil twin: an open "Home" appears.
    WifiNet twin[] = {net("Home", 3, 1, 1), net("Home", 0, 0x66, 1)};
    a.checkNetworks(twin, 2, 2000);
    CHECK(a.tick(2000) && a.state(Threat::Fake).active);
    CHECK(strstr(a.state(Threat::Fake).detail, "Home") != nullptr);
  }
  {  // One box shouting 10 names (Pineapple-style).
    AttackWatch a;
    WifiNet box[10];
    const char *names[] = {"attwifi", "Starbucks", "Free WiFi", "linksys", "NETGEAR",
                           "Airport", "Hotel", "McDonalds", "Guest", "Free Public WiFi"};
    for (int i = 0; i < 10; i++) box[i] = net(names[i], 0, 0x77, (uint8_t)i);
    a.checkNetworks(box, 10, 5000);
    CHECK(a.tick(5000) && strstr(a.state(Threat::Fake).detail, "10 names") != nullptr);
  }
}

static void testLed() {
  printf("Status LED\n");
  const LedInputs calm{false, false, false}, alert{true, true, true}, watch{false, true, false},
      rec{false, false, true};
  // Off means off, whatever happens.
  bool anyLit = false;
  for (uint32_t t = 0; t < 10000; t += 7) {
    const Rgb c = ledColor(alert, LedMode::Off, t);
    anyLit |= c.r || c.g || c.b;
  }
  CHECK(!anyLit);
  // Alert: red blinking (both on and off moments), never green/blue, beats everything.
  int on = 0, off = 0;
  bool otherColour = false;
  for (uint32_t t = 0; t < 2000; t += 5) {
    const Rgb c = ledColor(alert, LedMode::Alerts, t);
    (c.r ? on : off)++;
    otherColour |= c.g || c.b;
  }
  CHECK(on > 100 && off > 100 && !otherColour);
  // All clear: green only in "all" mode, dark in "alerts" mode.
  CHECK(ledColor(calm, LedMode::Alerts, 1234).g == 0);
  CHECK(ledColor(calm, LedMode::All, 1234).g > 0 && ledColor(calm, LedMode::All, 1234).r == 0);
  // Watching: amber (red + some green, no blue) in both modes.
  const Rgb w = ledColor(watch, LedMode::Alerts, 600);
  CHECK(w.r > w.g && w.g > 0 && w.b == 0);
  // Recording: a short blue blip every 3 s.
  CHECK(ledColor(rec, LedMode::Alerts, 3050).b > 0 && ledColor(rec, LedMode::Alerts, 4000).b == 0);
  // Gentle: never brighter than 60 on any channel.
  int peak = 0;
  for (uint32_t t = 0; t < 10000; t += 3)
    for (const LedInputs *in : {&calm, &alert, &watch, &rec}) {
      const Rgb c = ledColor(*in, LedMode::All, t);
      peak = c.r > peak ? c.r : c.g > peak ? c.g : c.b > peak ? c.b : peak;
    }
  CHECK(peak <= 60);
  CHECK(strcmp(ledModeName(LedMode::Alerts), "alerts") == 0);
}

static Bytes named(const char *name) {
  Bytes b = {0x02, 0x01, 0x06, (uint8_t)(strlen(name) + 1), 0x09};
  for (const char *c = name; *c; c++) b.push_back((uint8_t)*c);
  return b;
}

static void testCameras() {
  printf("Flock cameras\n");
  CamMatch m;
  auto ble = [&](const Bytes &b) { return matchCameraBle(b.data(), b.size(), m); };
  CHECK(ble(named("Penguin-1234567890")) && m.kind == CamKind::FlockBattery && m.sure);
  CHECK(ble(named("PENGUIN-0000000001")) && m.sure);
  CHECK(ble(named("FS Ext Battery")) && m.kind == CamKind::FlockBattery && m.sure);
  CHECK(!ble(named("Penguin-12345")));           // too few digits
  CHECK(!ble(named("Penguin-12345678901")));     // too many
  CHECK(!ble(named("Penguin Speaker")));
  CHECK(!ble(named("5551234567")));              // a bare number isn't enough
  CHECK(!ble(named("FS Ext Battery Pro")));
  Bytes svc = {0x02, 0x01, 0x06, 0x11, 0x07, 0x6f, 0x2e, 0x17, 0xdf, 0x14, 0x18, 0xe5, 0x9f,
               0xa8, 0x46, 0x32, 0x95, 0x38, 0xbb, 0xcc, 0xe8};
  CHECK(ble(svc) && m.kind == CamKind::FlockCamera && m.sure);
  svc[6] ^= 1;  // one byte off: not Flock
  CHECK(!ble(svc));
  const Bytes raven = {0x02, 0x01, 0x06, 0x05, 0x03, 0x0F, 0x18, 0x00, 0x32};  // 0x180F, 0x3200
  CHECK(ble(raven) && m.kind == CamKind::Raven && !m.sure);
  const Bytes xuntong = {0x05, 0xFF, 0xC8, 0x09, 0x01, 0x02};
  CHECK(ble(xuntong) && m.kind == CamKind::FlockBattery && !m.sure);
  const Bytes battery = {0x02, 0x0A, 0x00, 0x03, 0x03, 0x0F, 0x18};  // ordinary battery service
  CHECK(!ble(battery));
  const Bytes broken = {0x13, 0x09, 'P', 'e', 'n'};
  CHECK(!ble(broken));

  const uint8_t flockMac[6] = {0xb4, 0x1e, 0x52, 1, 2, 3}, other[6] = {0x3c, 0x71, 0xbf, 1, 2, 3};
  CHECK(matchCameraWifi(flockMac, "", m) && m.sure);
  CHECK(matchCameraWifi(other, "Flock-1a2b3c", m) && m.kind == CamKind::FlockCamera);
  CHECK(!matchCameraWifi(other, "Flockton Library", m));
  CHECK(!matchCameraWifi(other, "NETGEAR47", m) && !isFlockWifiAddr(other));

  CameraWatch w;
  const uint8_t a[6] = {0xb4, 0x1e, 0x52, 9, 9, 9};
  CamMatch sure{CamKind::FlockCamera, true, "Flock WiFi"};
  CHECK(w.onSighting(a, sure, -70, 1000));            // new: tell
  CHECK(!w.onSighting(a, sure, -65, 20000));          // same one, still around: quiet
  CHECK(w.nearby(30000) == 1 && w.nearby(20000 + CameraWatch::NEAR_MS) == 0);
  CHECK(w.onSighting(a, sure, -70, 20000 + 11 * 60000));  // back after 11 min: tell again
  CHECK(w.passedToday() == 1);
  const CamSighting *list[4];
  CHECK(w.list(list, 4) == 1 && list[0]->rssi == -70);

  // Hidden-camera hints.
  const uint8_t wyze[6] = {0x2c, 0xaa, 0x8e, 1, 2, 3}, ring[6] = {0x00, 0x00, 0x00, 1, 2, 3};
  CHECK(cameraMaker(wyze) && strcmp(cameraMaker(wyze), "Wyze") == 0);
  CHECK(cameraMaker(ring) == nullptr && cameraMaker(other) == nullptr);
  CHECK(cameraMaker(flockMac) == nullptr);  // Flock is its own thing
  int sorted = 1;  // the generated table must be sorted for the binary search
  for (int i = 1; i < kCamOuiCount; i++) {
    const uint32_t a0 = kCamOuis[i - 1].b0 << 16 | kCamOuis[i - 1].b1 << 8 | kCamOuis[i - 1].b2;
    const uint32_t a1 = kCamOuis[i].b0 << 16 | kCamOuis[i].b1 << 8 | kCamOuis[i].b2;
    sorted &= a0 < a1;
  }
  CHECK(sorted && kCamOuiCount > 100);
  for (int i = 0; i < kCamOuiCount; i += 7) {  // every entry is findable
    const uint8_t mac[6] = {kCamOuis[i].b0, kCamOuis[i].b1, kCamOuis[i].b2, 0, 0, 0};
    CHECK(cameraMaker(mac) && strcmp(cameraMaker(mac), kCamBrands[kCamOuis[i].brand]) == 0);
  }
  CHECK(cameraHotspotName("HDWiFiCam-9F21") && cameraHotspotName("MV12345678"));
  CHECK(cameraHotspotName("ipcam_001") && cameraHotspotName("Front Camera"));
  CHECK(cameraHotspotName("Ring Setup 4C") && cameraHotspotName("GW_AP_1234"));
  CHECK(!cameraHotspotName("Campus WiFi") && !cameraHotspotName("MVP Sports"));
  CHECK(!cameraHotspotName("NETGEAR47") && !cameraHotspotName("") && !cameraHotspotName(nullptr));
  CHECK(!cameraHotspotName("MV12a45678"));
  CHECK(matchCameraWifi(wyze, "Home", m) && m.kind == CamKind::Hint && !isFlock(m.kind));
  CHECK(matchCameraWifi(other, "V380_PRO", m) && m.kind == CamKind::Hint);

  CameraWatch v;  // hints are counted apart from Flock
  CamMatch hint{CamKind::Hint, false, "Wyze"};
  v.onSighting(wyze, hint, -60, 1000);
  v.onSighting(a, sure, -70, 1000);
  CHECK(v.nearbyFlock(2000) == 1 && v.nearbyHints(2000) == 1 && v.passedToday() == 1 &&
        v.hintsSeen() == 1);
  v.onSighting(a, hint, -70, 3000);  // a weaker reason never downgrades Flock
  CHECK(v.nearbyFlock(4000) == 1);
}

static void testSound() {
  printf("Sound level\n");
  int16_t silence[1600] = {}, tone[1600], dc[1600], loud[1600];
  for (int i = 0; i < 1600; i++) {
    tone[i] = (int16_t)(3276 * sinf(i * 0.2f));      // 1/10 of full scale
    dc[i] = 5000;                                     // a constant offset is not sound
    loud[i] = (int16_t)(32000 * sinf(i * 0.2f));
  }
  CHECK(blockDbfs(silence, 1600) <= -89);
  CHECK(blockDbfs(dc, 1600) <= -89);
  const float t = blockDbfs(tone, 1600);
  CHECK(t > -25 && t < -21);                          // 0.1 amplitude sine = -23 dBFS
  CHECK(blockDbfs(loud, 1600) > -4);
  CHECK(blockDbfs(tone, 0) <= -89);
  LevelMeter m;
  CHECK(!m.hasData());
  for (int i = 0; i < 50; i++) m.add(-60);
  const int quiet = m.db();
  m.add(-10);                                         // a bang: rises fast...
  CHECK(m.db() > quiet + 20 && m.peak() >= (int)(-10 + LevelMeter::SPL_OFFSET));
  m.add(-60);
  CHECK(m.db() > quiet + 10);                         // ...falls slowly
  for (int i = 0; i < 60; i++) m.add(-60);
  CHECK(m.db() <= quiet + 1);
  m.resetPeak();
  CHECK(m.peak() <= quiet + 1);
  LevelMeter c;  // calibration shifts the reading, and never below 0
  c.add(-50);
  const int before = c.db();
  c.setCalibration(-9);
  CHECK(c.db() == before - 9);
  c.setCalibration(-300);
  CHECK(c.db() == 0);
    CHECK(strcmp(loudnessName(30), "very quiet") == 0 && strcmp(loudnessName(65), "talking") == 0 &&
        strcmp(loudnessName(120), "painful!") == 0);
}

static void testRig() {
  printf("Rig link\n");
  RigStatus s{true, 335123456, -1121234567, 9, 66, 45, 23, 0, 87, 7};
  uint8_t adv[31];
  const size_t n = buildRigBeacon(s, adv, sizeof adv);
  CHECK(n == 29 && n <= 31);  // fits one ordinary broadcast
  RigStatus r{};
  CHECK(parseRigBeacon(adv, n, r));
  CHECK(r.fix && r.latE7 == 335123456 && r.lonE7 == -1121234567 && r.sats == 9 &&
        r.speedKmh == 66 && r.wifi == 45 && r.ble == 23 && r.battery == 87 && r.seq == 7);
  // The exact bytes jerry_beacon.py prints for --fake-gps 33.5123456,-112.1234567
  // (seq 1, no status file): company id + body.
  // ("ff ff | 4a 52 01 01 01 00 00 94 f9 13 79 51 2b bd 09 00 00 00 00 00 00 ff"),
  // wrapped in the flags + manufacturer-data records BlueZ puts around it.
  const Bytes py = {0x02, 0x01, 0x06, 0x19, 0xFF, 0xff, 0xff, 0x4a, 0x52, 0x01, 0x01, 0x01, 0x00,
                    0x00, 0x94, 0xf9, 0x13, 0x79, 0x51, 0x2b, 0xbd, 0x09, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0xff};
  CHECK(parseRigBeacon(py.data(), py.size(), r) && r.fix && r.latE7 == 335123456 &&
        r.lonE7 == -1121234567 && r.sats == 9 && r.battery == 255);
  adv[7] = 'X';  // wrong magic: not ours
  CHECK(!parseRigBeacon(adv, n, r));
  const Bytes cut = {0x02, 0x01, 0x06, 0x19, 0xFF, 0xff, 0xff, 0x4a};
  CHECK(!parseRigBeacon(cut.data(), cut.size(), r));
  s.latE7 = 950000000;  // off the planet: not a fix
  buildRigBeacon(s, adv, sizeof adv);
  CHECK(parseRigBeacon(adv, n, r) && !r.fix);

  RigLink link;
  const uint8_t box[6] = {1, 2, 3, 4, 5, 6}, stranger[6] = {9, 9, 9, 9, 9, 9};
  uint8_t cand[6];
  s.latE7 = 335123456;
  link.onBeacon(box, s, -60, 1000);
  CHECK(!link.paired() && link.candidate(2000, cand) && memcmp(cand, box, 6) == 0);
  CHECK(!link.candidate(1000 + RigLink::FRESH_MS + 1, cand));
  link.pair(box);
  int32_t lat, lon;
  CHECK(!link.connected(2000) && !link.location(2000, lat, lon));  // paired, not heard yet
  link.onBeacon(box, s, -55, 3000);
  CHECK(link.connected(4000) && link.location(4000, lat, lon) && lat == 335123456);
  link.onBeacon(stranger, RigStatus{true, 1, 1, 1, 1, 1, 1, 1, 1, 1}, -40, 5000);
  CHECK(link.status().latE7 == 335123456);  // a stranger's beacon is ignored
  CHECK(!link.location(3000 + RigLink::FRESH_MS + 1, lat, lon));  // stale fix: no location
  s.fix = false;
  link.onBeacon(box, s, -55, 20000);
  CHECK(link.connected(20001) && !link.location(20001, lat, lon));  // box there, no GPS
}

static void testFunk() {
  printf("Boot grooves\n");
  for (int g = 0; g < FunkIntro::GROOVES; g++) {
    FunkIntro f;
    f.start(g);
    int16_t buf[256];
    long n = 0;
    int peak = 0;
    double sum = 0;
    int last = 0;
    for (int k; (k = f.render(buf, 256)) > 0;) {
      n += k;
      last = buf[k - 1];
      for (int i = 0; i < k; i++) {
        peak = buf[i] > peak ? buf[i] : -buf[i] > peak ? -buf[i] : peak;
        sum += (double)buf[i] * buf[i];
      }
    }
    const double rms = sqrt(sum / n);
    printf("  %-12s %.2f s, peak %d, rms %.0f\n", FunkIntro::name(g), n / (double)FunkIntro::RATE, peak, rms);
    CHECK(n == (long)FunkIntro::lengthSamples(g) && !f.playing());
    CHECK(n > 4 * FunkIntro::RATE && n < 7 * FunkIntro::RATE);  // about its intro's length
    CHECK(peak > 16000 && peak < 32000);                        // loud, never clipping
    CHECK(rms > 5000 && rms < 9000);                            // the grooves are about as loud as each other
    CHECK(abs(last) < 200);                                     // fades out at the end (no click)
  }
  CHECK(strcmp(FunkIntro::name(1), "Bluff Creek") == 0);
  CHECK(FunkIntro::lengthSamples(99) == FunkIntro::lengthSamples(0));  // out of range: the original
}

int main() {
  testDetection();
  testPersistence();
  testNearby();
  testWifi();
  testFollow();
  testAttacks();
  testLed();
  testCameras();
  testSound();
  testRig();
  testFunk();
  printf("\n%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
