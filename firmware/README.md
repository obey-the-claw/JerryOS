# Ready-to-flash firmware

**Use the newest file: the highest `cp` number.** Right now that's
`jerryos-cp18-dashes.bin` (JerryOS v1.4.0). The older ones are kept as a
history of the build (one per checkpoint in [BUILDLOG.md](../BUILDLOG.md)).

Each `.bin` is a complete image for address `0x0`: bootloader, partition
table and JerryOS. Check it against its `.sha256` before flashing:

```bash
sha256sum -c jerryos-cp18-dashes.bin.sha256         # Linux
shasum -a 256 -c jerryos-cp18-dashes.bin.sha256     # macOS
```

(On Windows, `tools\flash_prebuilt.ps1` checks it for you.)

How to back up and flash: [the README](../README.md#install).
To build one yourself: `tools/build_firmware.sh`.
