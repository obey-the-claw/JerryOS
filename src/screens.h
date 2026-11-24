// screens.h - entry points into the screen tree (see screens.cpp).
#pragma once
#include "screen.h"

Screen &homeScreen();
Screen &menuScreen();
Screen &trackerScreen();
Screen &followersScreen();
Screen &shieldScreen();
Screen &camerasScreen();
Screen &tripScreen();     // the recap card after REC stops (extras.cpp)
Screen &rateScreen();
Screen &badgesScreen();
Screen &clockScreen();     // Settings -> Clock: set the time by hand
Screen &listenScreen();    // Listen: sound meter + Knock Detector
Screen &logbookScreen();   // Logbook: expeditions + badges
Screen &nearbyAllScreen(); // Nearby: Bluetooth + Wi-Fi in one list
