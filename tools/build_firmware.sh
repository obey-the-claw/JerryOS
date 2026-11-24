#!/bin/bash
# Build JerryOS into one flashable file (Linux / macOS / WSL).
#
#   tools/build_firmware.sh firmware/jerryos-my-build.bin
#
# Needs arduino-cli (https://arduino.github.io/arduino-cli/) with:
#   arduino-cli config add board_manager.additional_urls \
#     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
#   arduino-cli core install esp32:esp32@2.0.17
#   arduino-cli lib install "NimBLE-Arduino@1.4.3" "LovyanGFX@1.2.7"
#   pip install pyserial    (esptool, which comes with the core, needs it)
#
# Exact versions matter: newer cores / libraries change APIs.
# ACLI_ARGS="..." adds arguments to every arduino-cli call (e.g. --config-file),
# BUILD_PROPS="key=value ..." adds --build-property overrides (e.g. tool paths),
# BOARD=vendor:arch:board if your ESP32 core is installed under another name.
set -e
OUT=${1:?give the output file, e.g. firmware/jerryos-my-build.bin}
REPO=$(cd "$(dirname "$0")/.." && pwd)
ACLI=${ARDUINO_CLI:-arduino-cli}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# arduino-cli wants a sketch folder named like its .ino: JerryOS/JerryOS.ino
# (empty: setup() and loop() are in src/main.cpp) with every source next to it.
SK="$WORK/JerryOS"
mkdir -p "$SK"
: > "$SK/JerryOS.ino"
cp "$REPO"/src/*.h "$REPO"/src/*.cpp "$REPO"/include/*.h "$SK/"

# AIPI Lite: ESP32-S3, 16 MB flash (DIO), 8 MB octal PSRAM, USB serial,
# the 16 MB default partition table (two 6.25 MB app slots + 3.4 MB storage).
FQBN="${BOARD:-esp32:esp32:esp32s3}:FlashSize=16M,FlashMode=qio,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc"
PROPS=(--build-property build.partitions=default_16MB --build-property upload.maximum_size=6553600)
for p in $BUILD_PROPS; do PROPS+=(--build-property "$p"); done

# shellcheck disable=SC2086
"$ACLI" $ACLI_ARGS compile --fqbn "$FQBN" "${PROPS[@]}" --output-dir "$WORK/build" "$SK"

# One file for address 0: bootloader + partition table + OTA data + the app.
B="$WORK/build"
ESPTOOL=$(find "${ESPTOOL_DIR:-$HOME/.arduino15/packages/esp32/tools/esptool_py}" -name esptool.py 2>/dev/null | head -1)
BOOT_APP0=$(find "${CORE_DIR:-$HOME/.arduino15/packages/esp32/hardware/esp32}" -name boot_app0.bin 2>/dev/null | head -1)
python3 "$ESPTOOL" --chip esp32s3 merge_bin -o "$OUT" --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 "$B/JerryOS.ino.bootloader.bin" 0x8000 "$B/JerryOS.ino.partitions.bin" \
  0xe000 "$BOOT_APP0" 0x10000 "$B/JerryOS.ino.bin"
(cd "$(dirname "$OUT")" && sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256")
echo "Built $OUT"
