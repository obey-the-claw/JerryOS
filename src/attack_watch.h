// attack_watch.h - "is someone messing with the radios around me?"
//
// Three listen-only detectors:
//   Kickoffs   Wi-Fi "deauth" floods: a flood of fake "disconnect" messages
//              that knocks cameras, doorbells and phones off their Wi-Fi.
//   Popups     Bluetooth pop-up spam (Flipper Zero and friends): dozens of
//              fake "connect your AirPods / earbuds / keyboard" broadcasts,
//              each from a new address, within seconds.
//   Fakes      Fake networks: an "evil twin" (an open copy of a network name
//              that's normally locked), or one box broadcasting lots of
//              network names at once (a "Pineapple"-style trap).
//
// Pure logic, unit-tested on a PC. The radio code feeds it.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "wifi_nets.h"

// Which kind of "pop-up" broadcast this is, or None. Pop-up broadcasts are
// normal one at a time (your AirPods send them); a flood from many addresses
// is the attack.
enum class Popup : uint8_t { None, Apple, Android, Windows, Samsung };
Popup popupKind(const uint8_t *payload, size_t len);
const char *popupName(Popup p);  // "Apple", "Android"...

enum class Threat : uint8_t { Kickoff, Popup, Fake, COUNT };
const char *threatName(Threat t);  // "WiFi kickoffs", "BT popup spam", "Fake networks"

struct ThreatState {
  bool active;        // happening right now
  bool alerting;      // started since you last looked
  uint32_t lastMs;    // last time it was seen
  uint32_t peak;      // biggest burst (frames / addresses / names)
  char detail[24];    // e.g. "Apple x37", "'Cafe' open+locked"
};

class AttackWatch {
 public:
  static constexpr uint32_t WINDOW_MS = 10000;   // look at the last 10 s
  static constexpr uint32_t KICKOFF_FLOOD = 30;  // deauth frames in 10 s
  static constexpr uint32_t POPUP_FLOOD = 12;    // new pop-up addresses in 10 s
  static constexpr int MANY_NAMES = 8;           // names from one box (offices use up to ~6)
  static constexpr uint32_t QUIET_MS = 60000;    // calm this long = over

  // Wi-Fi: how many deauth/disassoc frames were heard since the last call.
  // Call about once a second.
  void onKickoffs(uint32_t frames, uint32_t now);
  // Bluetooth: one pop-up style broadcast from this address.
  void onPopup(const uint8_t addr[6], Popup kind, uint32_t now);
  // Wi-Fi: the networks heard recently (after each scan).
  void checkNetworks(const WifiNet *nets, int n, uint32_t now);

  // Call regularly. Returns true when a threat newly starts.
  bool tick(uint32_t now);

  const ThreatState &state(Threat t) const { return st_[(int)t]; }
  bool anyAlert() const;
  bool anyActive() const;
  void acknowledgeAll();

 private:
  void raise(Threat t, uint32_t amount, const char *detail, uint32_t now);

  ThreatState st_[(int)Threat::COUNT] = {};
  bool fresh_[(int)Threat::COUNT] = {};  // started since last tick()

  // Kickoffs: frames per second for the last 10 seconds.
  uint32_t kick_[10] = {};
  uint32_t kickSec_ = 0;

  // Popups: recent (address fingerprint, kind, time).
  struct Seen { uint32_t hash; uint32_t ms; Popup kind; };
  Seen pop_[64] = {};
  int popNext_ = 0;
};
