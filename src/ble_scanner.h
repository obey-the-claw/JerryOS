// ble_scanner.h - passive Bluetooth LE listening for Tracker Watch.
//
// PASSIVE ONLY: the radio just listens to broadcasts that are already in the
// air. It never sends scan requests, never connects, never advertises, and
// never interferes with anything.
#pragma once
#include <stdint.h>

#include "attack_watch.h"
#include "ble_nearby.h"
#include "tracker_watch.h"

namespace ble {

void begin();  // start listening (call once)

// Move tracker sightings and pop-up broadcasts heard since the last call
// into the watchers. Call every loop.
void drain(TrackerWatch &watch, AttackWatch &attacks);

int nearbyDevices();       // distinct BLE devices heard in the last minute
// Copy of everything heard in the last minute, strongest signal first.
int nearbySnapshot(NearbyDevice *out, int max);
uint32_t advertsHeard();   // total broadcasts heard since boot (diagnostics)
uint32_t trackerAdverts(); // of those, how many were trackers
bool listening();

}  // namespace ble
