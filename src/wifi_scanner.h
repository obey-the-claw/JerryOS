// wifi_scanner.h - passive Wi-Fi scanner + logger.
//
// PASSIVE ONLY: Jerry listens for the "beacons" every Wi-Fi router already
// broadcasts ~10 times a second. He never sends probe requests, never joins
// a network, and never transmits. (The ESP32-S3 hears 2.4 GHz only.)
//
// Saving is OFF until you ask for it (hold R on the home screen = REC).
// While recording, each network gets one line in /wifi.csv on the chip's
// storage area, once per recording session. The log survives power-off.
// Copy it to a PC with tools/pull_wifi_log.py.
#pragma once
#include <stdint.h>

#include "wifi_nets.h"

namespace wifi {

void begin();                    // start the background scan task (call once)
void setEnabled(bool on);        // Settings switch: scanning on/off
bool enabled();
bool scanning();                 // task running and radio up

int nearby();                    // networks heard in the last 90 s
uint32_t seenThisBoot();         // different networks since power-on
uint32_t session();              // recording session number, written in each log line
int snapshot(WifiNet *out, int max);  // heard recently, strongest first

// The log file
void setRecording(bool rec);     // start/stop saving networks (starts a new session)
bool recording();
uint32_t savedThisSession();     // networks saved since REC was pressed
bool logReady();                 // storage mounted
bool logFull();                  // hit the size cap, no longer logging
uint32_t logBytes();
void clearLog();                 // erase /wifi.csv
void dumpLog();                  // print the whole file over USB (for the PC script)

}  // namespace wifi
