"""Turn your recorded Jerry voice lines into firmware.

Put .wav files in the voice/ folder, named after the moment they play:

    boot.wav       power on                 ("Jerry's on the job!")
    happy.wav      poke / settings saved    ("Gear approved!")
    rec_on.wav     REC started              ("Recording WiFi!")
    rec_off.wav    REC stopped              ("Got 'em.")
    alert.wav      tracker following you    ("Something's following us!")
    low_batt.wav   battery at 15%           ("Jerry's getting sleepy...")
    tick.wav       volume preview           (keep it very short)
    erase.wav      log erased               ("Poof. Gone.")

More than one clip for the same moment? Add _2, _3...: happy.wav,
happy_2.wav, happy_3.wav. Jerry takes turns. Moments without a clip keep
the built-in Sasquatch sounds.

Any .wav works (mono or stereo, any sample rate, 8/16/24-bit). Keep each
clip under 4 seconds; longer ones are cut. Silence at the start and end is
trimmed and the volume is evened out.

    py tools\\make_voice.py

writes src/voice_clips.h. Then the firmware gets rebuilt with the clips in it.
"""
import array
import pathlib
import re
import sys
import wave

RATE = 16000
MAX_SECONDS = 4.0
SOUNDS = ["boot", "happy", "rec_on", "rec_off", "alert", "low_batt", "tick", "erase"]

ROOT = pathlib.Path(__file__).resolve().parent.parent
VOICE = ROOT / "voice"
OUT = ROOT / "src" / "voice_clips.h"


def read_wav(path):
    """Return (samples as floats -1..1, mono, at the file's rate), rate."""
    with wave.open(str(path), "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        vals = [(b - 128) / 128 for b in raw]
    elif width == 2:
        vals = [v / 32768 for v in array.array("h", raw)]
    elif width == 3:
        vals = []
        for i in range(0, len(raw) - 2, 3):
            v = int.from_bytes(raw[i:i + 3], "little", signed=True)
            vals.append(v / 8388608)
    elif width == 4:
        vals = [v / 2147483648 for v in array.array("i", raw)]
    else:
        raise ValueError(f"unsupported sample width {width}")
    if ch > 1:  # mix down to mono
        vals = [sum(vals[i:i + ch]) / ch for i in range(0, len(vals) - ch + 1, ch)]
    return vals, rate


def resample(vals, src, dst):
    if src == dst or not vals:
        return vals
    out_len = int(len(vals) * dst / src)
    out = []
    for i in range(out_len):
        x = i * src / dst
        j = int(x)
        f = x - j
        a = vals[j]
        b = vals[j + 1] if j + 1 < len(vals) else a
        out.append(a + (b - a) * f)
    return out


def trim_and_level(vals):
    if not vals:
        return vals
    peak = max(abs(v) for v in vals)
    if peak < 1e-4:
        return []
    gate = peak * 0.04
    start = next(i for i, v in enumerate(vals) if abs(v) > gate)
    end = len(vals) - next(i for i, v in enumerate(reversed(vals)) if abs(v) > gate)
    start = max(0, start - RATE // 50)  # keep 20 ms before / after
    end = min(len(vals), end + RATE // 50)
    vals = vals[start:end][: int(RATE * MAX_SECONDS)]
    gain = 0.92 / peak
    out = [v * gain for v in vals]
    fade = min(len(out) // 4, RATE // 100)  # 10 ms fades: no clicks
    for i in range(fade):
        out[i] *= i / fade
        out[-1 - i] *= i / fade
    return out


def main():
    files = sorted(VOICE.glob("*.wav")) if VOICE.exists() else []
    clips = []
    for f in files:
        m = re.fullmatch(r"([a-z_]+?)(?:_(\d+))?", f.stem.lower())
        name = m.group(1) if m else ""
        if name not in SOUNDS:
            print(f"  skipping {f.name}: name must start with one of {', '.join(SOUNDS)}")
            continue
        vals, rate = read_wav(f)
        vals = trim_and_level(resample(vals, rate, RATE))
        if not vals:
            print(f"  skipping {f.name}: it's silent")
            continue
        pcm = bytes(max(0, min(255, int(round(v * 127)) + 128)) for v in vals)
        clips.append((SOUNDS.index(name), f.stem.lower(), pcm))
        print(f"  {f.name:<20} -> {name:<9} {len(pcm) / RATE:4.1f} s")

    lines = [
        "// voice_clips.h - recorded voice clips for Jerry (GENERATED - don't edit).",
        "//",
        "// Made by tools/make_voice.py from the .wav files in voice/. With no clips,",
        "// Jerry uses his built-in Sasquatch sounds (squatch_voice.cpp).",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "struct VoiceClip {",
        "  uint8_t sound;       // audio::Sound number",
        "  const uint8_t *pcm;  // 16 kHz, mono, 8-bit (128 = silence)",
        "  uint32_t len;",
        "};",
        "",
    ]
    if not clips:
        lines += ["static const VoiceClip kVoiceClips[1] = {{255, nullptr, 0}};",
                  "static const int kVoiceClipCount = 0;"]
    else:
        for _, stem, pcm in clips:
            lines.append(f"static const uint8_t kClip_{stem}[{len(pcm)}] = {{")
            for i in range(0, len(pcm), 24):
                lines.append("  " + ",".join(str(b) for b in pcm[i:i + 24]) + ",")
            lines.append("};")
        lines.append(f"static const VoiceClip kVoiceClips[{len(clips)}] = {{")
        for idx, stem, pcm in clips:
            lines.append(f"  {{{idx}, kClip_{stem}, {len(pcm)}}},  // {stem}")
        lines += ["};", f"static const int kVoiceClipCount = {len(clips)};"]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    total = sum(len(p) for _, _, p in clips)
    print(f"Wrote {OUT.relative_to(ROOT)}: {len(clips)} clips, {total // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
