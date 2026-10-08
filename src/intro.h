// intro.h - the boot intros (Jeep Jam, Bluff Creek, Selfie, Cruisin'), each
// with its own funk groove. The shell opens one at power-on (Settings ->
// Intro picks which, or random); it plays for 4-6 s, then waits on the title
// until any button is pressed (any button also skips it).
#pragma once
#include "screen.h"

constexpr int INTRO_COUNT = 4;

Screen &introScreen();
const char *introName(int i);  // 0..INTRO_COUNT-1
// Play intro i the next time introScreen() opens (Settings uses it to preview).
void replayIntro(int i);
