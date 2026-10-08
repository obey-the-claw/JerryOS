// intro.h - the boot intro (Jerry walks out of the woods and drives off in
// his Jeep). The shell opens it at power-on; it plays for ~4 s, then waits on
// the title until any button is pressed (any button also skips it).
#pragma once
#include "screen.h"

Screen &introScreen();
