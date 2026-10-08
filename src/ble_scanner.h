// ble_scanner.h - passive Bluetooth LE listening for Tracker Watch.
//
// PASSIVE ONLY: the radio just listens to broadcasts that are already in the
// air. It never sends scan requests, never connects, never advertises, and
// never interferes with anything.
#pragma once
#include <stdint.h>

#include "attack_watch.h"
#include "ble_nearby.h"
#include "camera_watch.h"
#include "rig_link.h"
#include "tracker_watch.h"

namespace ble {

void begin();  // start listening (call once)

// Move tracker, pop-up and camera sightings heard since the last call
// into the watchers. Call every loop.
void drain(TrackerWatch &watch, AttackWatch &attacks, CameraWatch &cams, RigLink &rig);

int takeNewCameras();      // how many new cameras the last drain()s found
int nearbyDevices();       // distinct BLE devices heard in the last minute
// Copy of everything heard in the last minute, strongest signal first.
int nearbySnapshot(NearbyDevice *out, int max);
uint32_t advertsHeard();   // total broadcasts heard since boot (diagnostics)
uint32_t trackerAdverts(); // of those, how many were trackers
bool listening();

}  // namespace ble
