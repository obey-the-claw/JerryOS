"""Point the web installer (docs/index.html, GitHub Pages) at the newest firmware.

    python3 tools/update_web_installer.py

Copies the newest firmware/jerryos-cp*.bin (highest checkpoint number) to
docs/install/jerryos.bin and puts the version from src/theme.h into
docs/install/manifest.json. Run it after building a new release.
"""
import json
import pathlib
import re
import shutil

repo = pathlib.Path(__file__).resolve().parent.parent
bins = sorted(repo.glob("firmware/jerryos-cp*.bin"), key=lambda p: int(re.search(r"cp(\d+)", p.name).group(1)))
newest = bins[-1]
version = re.search(r'JERRYOS_VERSION "([^"]+)"', (repo / "src/theme.h").read_text()).group(1)
shutil.copyfile(newest, repo / "docs/install/jerryos.bin")
manifest_path = repo / "docs/install/manifest.json"
manifest = json.loads(manifest_path.read_text())
manifest["version"] = version
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
print(f"web installer -> {newest.name} (JerryOS {version})")
