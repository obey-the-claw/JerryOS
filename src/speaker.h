// speaker.h - plays Jerry's boot groove (funk.cpp) through the speaker.
//
// The only sound JerryOS makes. It runs in the background while the intro
// plays, then switches the audio chip and amplifier off again (no hiss, no
// battery drain). Settings -> Intro tune turns it off.
#pragma once

namespace speaker {

void playIntro();  // start the groove (returns at once)
void stop();       // cut it short (e.g. the intro was skipped)
bool busy();       // playing (the microphone waits until it's done)

}  // namespace speaker
