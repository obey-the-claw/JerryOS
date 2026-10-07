"""Build src/camera_ouis.h: address prefixes of companies that make cameras.

Source: the IEEE MA-L registry (oui.txt from https://standards-oui.ieee.org/oui/oui.txt;
flock-you ships a copy). Only companies whose Wi-Fi products are mostly
cameras are included, matched on their exact registered names, so a
look-alike name (e.g. "Carlo Gavazzi" for "Arlo") can't sneak in.

    python3 tools/make_camera_ouis.py path/to/oui.txt
"""
import pathlib
import re
import sys

# brand label shown on Jerry -> regex for the exact registered company names
BRANDS = [
    ("Hikvision", r"^(Hangzhou Hikvision Digital Technology Co\.,Ltd\.|Prama Hikvision India Private Limited)$"),
    ("EZVIZ", r"^Hangzhou Ezviz Software Co\.,Ltd\.$"),
    ("Dahua", r"^(Zhejiang Dahua Technology Co\., Ltd\.|ZHEJIANG DAHUA TECHNOLOGYCO\.,LTD)$"),
    ("Uniview", r"^Zhejiang Uniview Technologies Co\., ?Ltd\.$"),
    ("Ring", r"^Ring LLC$"),
    ("Wyze", r"^Wyze Labs Inc$"),
    ("Axis", r"^Axis Communications AB$"),
    ("Arlo", r"^Arlo Technology$"),
    ("Amcrest", r"^Amcrest Technologies$"),
    ("Swann", r"^Swann communications Pty Ltd$"),
    ("Lorex", r"^Lorex Technology Inc\.$"),
    ("Reolink", r"^Reolink Innovation Limited$"),
    ("Canary", r"^Canary Connect, Inc\.$"),
    ("Hanwha", r"^(Hanwha Vision VietNam|HANWHA VISION VIETNAM COMPANY LIMITED)$"),
    ("Reecam", r"^Shenzhen Reecam Tech\.Ltd\.$"),
    ("Yoosee", r"^Shenzhen Gwelltimes Technology Co\.,Ltd$"),
]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    found = []
    for line in text.splitlines():
        m = re.match(r"^([0-9A-F]{6})\s+\(base 16\)\s+(.*?)\s*$", line)
        if not m:
            continue
        prefix, org = m.group(1), m.group(2)
        for i, (_, rx) in enumerate(BRANDS):
            if re.match(rx, org):
                found.append((prefix, i))
    found.sort()
    out = [
        "// camera_ouis.h - Wi-Fi address prefixes of camera makers (GENERATED).",
        "//",
        "// Made by tools/make_camera_ouis.py from the IEEE MA-L registry. Only",
        "// companies whose Wi-Fi products are mostly cameras. Sorted for binary search.",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "static const char *const kCamBrands[] = {" + ", ".join(f'"{b}"' for b, _ in BRANDS) + "};",
        "",
        "struct CamOui {",
        "  uint8_t b0, b1, b2, brand;",
        "};",
        "",
        f"static const CamOui kCamOuis[{len(found)}] = {{",
    ]
    for prefix, i in found:
        b = [int(prefix[k:k + 2], 16) for k in (0, 2, 4)]
        out.append(f"    {{0x{b[0]:02x}, 0x{b[1]:02x}, 0x{b[2]:02x}, {i}}},  // {BRANDS[i][0]}")
    out += ["};", f"static const int kCamOuiCount = {len(found)};", ""]
    dest = pathlib.Path(__file__).resolve().parent.parent / "src" / "camera_ouis.h"
    dest.write_text("\n".join(out), encoding="utf-8")
    counts = {}
    for _, i in found:
        counts[BRANDS[i][0]] = counts.get(BRANDS[i][0], 0) + 1
    print(f"{len(found)} prefixes -> {dest.name}: " + ", ".join(f"{k} {v}" for k, v in counts.items()))


if __name__ == "__main__":
    main()
