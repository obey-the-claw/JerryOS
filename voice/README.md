# voice/: Jerry's recorded voice lines (optional)

Without anything here, Jerry uses his built-in Sasquatch sounds (whoops,
wood knocks, grunts). To give him real lines, record them and save them
here as `.wav` files named after the moment they play:

| File | Plays when | Idea |
|---|---|---|
| `boot.wav` | power on | "Jerry's on the job!" |
| `happy.wav` | poke, settings saved | "Gear approved!" |
| `rec_on.wav` | REC started | "Recording WiFi!" |
| `rec_off.wav` | REC stopped | "Got 'em." |
| `alert.wav` | tracker following you | "Something's following us!" |
| `low_batt.wav` | battery at 15% | "Jerry's getting sleepy..." |
| `tick.wav` | volume preview | a quick grunt (keep it very short) |
| `erase.wav` | log erased | "Poof. Gone." |

- **Several for one moment:** `happy.wav`, `happy_2.wav`, `happy_3.wav`...
  Jerry takes turns, which is great for pokes.
- **Under 4 seconds each.** Silence at the ends is trimmed and the volume is
  evened out automatically.
- **Any WAV** works (phone, Audacity, CapCut, Descript export). Pitch your
  voice down for extra Squatch.
- Skip any you don't want: those moments keep the Squatch sounds.

Then push the files (GitHub Desktop) and ask for a rebuild, or run
`py tools\make_voice.py` yourself and build.
