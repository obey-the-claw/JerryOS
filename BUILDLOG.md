# JerryOS build log

Short notes for each checkpoint: what we did, what broke, how we fixed it.
This is raw material for the blog and video.

---

## Checkpoint 1: back up the original firmware

**Goal:** before flashing anything, save a perfect copy of what's on the
AIPI Lite so we can always put it back.

### What we did

- **Researched the hardware instead of trusting the box.** Found four
  community projects that had already mapped the AIPI Lite's pins. Wrote
  them up in `docs/HARDWARE.md` with credit, and flagged where they disagree.
  - Brief's guess (ESP32-S3 N16R8, 16 MB flash, 8 MB PSRAM, ST7735 128×128):
    **matches the community sources.** The device still has to confirm it
    itself (that's part of this checkpoint).
  - Surprises worth knowing before we write code:
    - The **left button is the power button.** It's wired into the power
      latch. We have to make sure "hold LEFT = go home" never turns Jerry off.
    - **GPIO 10 is a "keep me alive" pin.** On battery, the firmware must hold
      it high or the device switches itself off.
    - **Sources disagree on screen colour inversion.** We'll settle it with a
      test card at Checkpoint 2.
- **Wrote `tools/backup_firmware.ps1`.** One command on Windows. It:
  1. installs esptool (Espressif's official tool),
  2. finds the device's USB port on its own,
  3. asks the chip what it is (model, flash, PSRAM, MAC, security locks),
  4. reads all 16 MB of flash **twice** and checks both reads match,
  5. makes the chip checksum its own flash and compares that to the file,
  6. checks the file really contains firmware (`tools/inspect_backup.py`).
- **Wrote `tools/restore_firmware.ps1`.** The undo button. It checks the
  backup's fingerprint and makes you type `RESTORE` before writing.
- **Tested both scripts in the sandbox** against a fake device: good dump,
  blank dump, truncated dump, corrupted partition table, Secure Boot on, and
  a tampered backup file. Every bad case stops with a clear message.

### Plain-English: why so paranoid about a backup?

A flash read can "succeed" and still hand you junk: a loose cable, a chip
that's encrypted, a read that silently stopped early. If we only found out
after flashing JerryOS over the original, the stock firmware would be gone
for good. So we don't take "it finished" as proof. We read twice, have the
chip check itself, and open the file to look for a real bootloader and
partition table inside.

### What broke / how we fixed it

- `inspect_backup.py` crashed on a truncated dump (it tried to look past the
  end of the file). Caught in testing. Now it reports "outside the dump" and
  fails cleanly.

### On the real device (2026-10-04)

**Result: BACKUP CONFIRMED.** ✅

- First try, the script couldn't find the device: Windows only listed two
  Bluetooth COM ports. A moment later Windows saw it as **COM7**
  (`USB\VID_303A&PID_1001`, Espressif's built-in USB). Re-running with
  `-Port COM7` worked. *(Lesson: if auto-detect misses it, check
  `Get-PnpDevice` for VID_303A and pass the port by hand.)*
- What the chip says it is: **ESP32-S3 rev v0.2, 8 MB embedded PSRAM, 16 MB
  flash.** It matches the brief exactly.
- Security: **Secure Boot off, flash encryption off.** The door is open for
  custom firmware.
- Two full 16 MB reads, about 2.5 min each, came out byte-identical. The
  chip's own checksum matched the file too.
- SHA256 of the backup:
  `47eff76e932958084df49ce7550f213c894aa26901927698cc19251c52680305`
- What's inside: the **XiaoZhi** AI chatbot firmware, with *two* versions
  side by side (v1.2.5 and v1.2.7). That's an A/B update setup: the device
  downloads updates into the spare slot and flips over. There's also a 960 KB
  "model" partition for the "hey, wake up" voice model.
- 61% of the chip is empty space. Lots of room for Jerry.
- [ ] Copy the `.bin` off the laptop (USB stick / cloud).
