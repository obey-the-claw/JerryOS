// intro.h - the boot intro (Jerry walks out of the woods and drives off in
// his Jeep). The shell opens it at power-on; it closes itself after ~2.6 s or
// when any button is pressed.
#pragma once
#include "screen.h"

Screen &introScreen();
