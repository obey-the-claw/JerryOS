#!/usr/bin/env python3
"""
inspect_backup.py - sanity-check a full ESP32-S3 flash dump.

A flash read can "succeed" and still give you junk (all 0xFF, all 0x00, a
short file, an encrypted blob). This script opens the dump and proves it
looks like real, bootable ESP32 firmware:

  1. Size matches the expected flash size (16 MB by default).
  2. The bootloader at 0x0 starts with the ESP image magic byte (0xE9).
  3. The partition table at 0x8000 parses, and its MD5 checksum matches.
  4. Every app partition starts with 0xE9 and has a readable app
     description (project name, version, build date, ESP-IDF version).

No third-party packages needed. Usage:

    python tools/inspect_backup.py backup/aipi-lite-stock.bin
    python tools/inspect_backup.py backup/aipi-lite-stock.bin --expect-size 16MB

Exit code 0 = looks good, 1 = something is wrong (read the output).
"""

import argparse
import hashlib
import struct
import sys

ESP_IMAGE_MAGIC = 0xE9
APP_DESC_MAGIC = 0xABCD5432
PART_TABLE_OFFSET = 0x8000
PART_TABLE_MAX = 0xC00  # partition table sector holds at most 0xC00 bytes of entries
PART_ENTRY_MAGIC = b"\xAA\x50"
PART_MD5_MAGIC = b"\xEB\xEB"

PART_TYPES = {0x00: "app", 0x01: "data"}
APP_SUBTYPES = {0x00: "factory", 0x20: "test"}
APP_SUBTYPES.update({0x10 + i: f"ota_{i}" for i in range(16)})
DATA_SUBTYPES = {
    0x00: "ota", 0x01: "phy", 0x02: "nvs", 0x03: "coredump", 0x04: "nvs_keys",
    0x05: "efuse", 0x06: "undefined", 0x80: "esphttpd", 0x81: "fat",
    0x82: "spiffs", 0x83: "littlefs",
}


def parse_size(text):
    text = text.strip().upper()
    for suffix, mult in (("MB", 1024 * 1024), ("KB", 1024)):
        if text.endswith(suffix):
            return int(text[: -len(suffix)]) * mult
    return int(text, 0)


def cstr(raw):
    return raw.split(b"\x00", 1)[0].decode("ascii", errors="replace")


def human(n):
    if n >= 1024 * 1024 and n % (1024 * 1024) == 0:
        return f"{n // (1024 * 1024)} MB"
    if n >= 1024:
        return f"{n / 1024:g} KB"
    return f"{n} B"


def parse_partition_table(data):
    """Return (entries, md5_ok). md5_ok is None if the table has no MD5 entry."""
    table = data[PART_TABLE_OFFSET:PART_TABLE_OFFSET + PART_TABLE_MAX]
    entries = []
    md5_ok = None
    for pos in range(0, len(table), 32):
        entry = table[pos:pos + 32]
        if entry[:2] == PART_ENTRY_MAGIC:
            ptype, subtype, offset, size = struct.unpack_from("<BBII", entry, 2)
            label = cstr(entry[12:28])
            flags = struct.unpack_from("<I", entry, 28)[0]
            entries.append(dict(type=ptype, subtype=subtype, offset=offset,
                                size=size, label=label, flags=flags))
        elif entry[:2] == PART_MD5_MAGIC:
            stored = entry[16:32]
            md5_ok = hashlib.md5(table[:pos]).digest() == stored
        else:
            break  # 0xFF padding = end of table
    return entries, md5_ok


def describe_subtype(ptype, subtype):
    if ptype == 0x00:
        return APP_SUBTYPES.get(subtype, f"0x{subtype:02x}")
    if ptype == 0x01:
        return DATA_SUBTYPES.get(subtype, f"0x{subtype:02x}")
    return f"0x{subtype:02x}"


def read_app_desc(data, offset):
    """
    An ESP app image = 24-byte image header + 8-byte segment header, then the
    first segment begins with esp_app_desc_t (magic 0xABCD5432).
    """
    if data[offset] != ESP_IMAGE_MAGIC:
        return None
    desc = offset + 24 + 8
    magic = struct.unpack_from("<I", data, desc)[0]
    if magic != APP_DESC_MAGIC:
        return {}
    return dict(
        version=cstr(data[desc + 16:desc + 48]),
        project=cstr(data[desc + 48:desc + 80]),
        time=cstr(data[desc + 80:desc + 96]),
        date=cstr(data[desc + 96:desc + 112]),
        idf=cstr(data[desc + 112:desc + 144]),
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump", help="path to the full flash dump (.bin)")
    ap.add_argument("--expect-size", default="16MB",
                    help="expected flash size, e.g. 16MB (default) or 8MB")
    args = ap.parse_args()

    with open(args.dump, "rb") as f:
        data = f.read()

    problems = []
    expect = parse_size(args.expect_size)

    print(f"File:    {args.dump}")
    print(f"Size:    {len(data):,} bytes ({human(len(data))})")
    print(f"SHA256:  {hashlib.sha256(data).hexdigest()}")
    if len(data) != expect:
        problems.append(f"size is {human(len(data))}, expected {human(expect)}")

    if len(data) < PART_TABLE_OFFSET + PART_TABLE_MAX:
        problems.append("file is too small to contain a bootloader + partition table")
        return report(problems)

    # How much of the chip is empty (erased flash reads as 0xFF)?
    empty = data.count(0xFF) / len(data) * 100
    zeros = data.count(0x00) / len(data) * 100
    print(f"Empty:   {empty:.1f}% of bytes are 0xFF (erased flash)")
    if empty > 99.9:
        problems.append("dump is essentially all 0xFF - the read probably failed or the chip is blank")
    if zeros > 99.9:
        problems.append("dump is essentially all 0x00 - the read almost certainly failed")

    # Bootloader
    print()
    if data[0] == ESP_IMAGE_MAGIC:
        segments = data[1]
        print(f"Bootloader @ 0x0: OK (magic 0xE9, {segments} segments)")
    else:
        problems.append(f"no bootloader magic at 0x0 (found 0x{data[0]:02X}, expected 0xE9) "
                        "- flash may be encrypted or the dump is bad")

    # Partition table
    entries, md5_ok = parse_partition_table(data)
    print()
    if not entries:
        problems.append("no partition table found at 0x8000 - flash may be encrypted or the dump is bad")
    else:
        md5_text = {True: "MD5 OK", False: "MD5 MISMATCH", None: "no MD5 entry"}[md5_ok]
        print(f"Partition table @ 0x8000 ({len(entries)} entries, {md5_text}):")
        print(f"  {'label':<16} {'type':<5} {'subtype':<10} {'offset':>10} {'size':>10}")
        for e in entries:
            t = PART_TYPES.get(e["type"], f"0x{e['type']:02x}")
            st = describe_subtype(e["type"], e["subtype"])
            print(f"  {e['label']:<16} {t:<5} {st:<10} {e['offset']:>#10x} {human(e['size']):>10}")
            if e["offset"] + e["size"] > len(data):
                problems.append(f"partition '{e['label']}' runs past the end of the dump")
        if md5_ok is False:
            problems.append("partition table MD5 does not match - the dump may be corrupted")

    # App partitions
    apps = [e for e in entries if e["type"] == 0x00]
    if apps:
        print()
        print("Firmware images found:")
    for e in apps:
        if e["offset"] + 256 > len(data):
            print(f"  {e['label']:<10} (outside the dump)")
            continue
        desc = read_app_desc(data, e["offset"])
        if desc is None:
            # An unused OTA slot is normally blank (0xFF) - that's fine.
            if data[e["offset"]] == 0xFF:
                print(f"  {e['label']:<10} (empty slot)")
            else:
                print(f"  {e['label']:<10} unknown contents (no 0xE9 magic)")
        elif not desc:
            print(f"  {e['label']:<10} app image present (no app description block)")
        else:
            print(f"  {e['label']:<10} project='{desc['project']}' version='{desc['version']}' "
                  f"built {desc['date']} {desc['time']} with ESP-IDF {desc['idf']}")
    if apps and not any(read_app_desc(data, e["offset"]) is not None for e in apps
                        if e["offset"] + 256 <= len(data)):
        problems.append("no app partition contains a firmware image")

    return report(problems)


def report(problems):
    print()
    if problems:
        print("RESULT: PROBLEMS FOUND")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("RESULT: backup looks like a complete, valid ESP32 flash image.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
