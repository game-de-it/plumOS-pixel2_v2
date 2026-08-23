#!/usr/bin/env python3
"""Build a source-only app-layer shape fixture for CI implementation audits."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = (
    "frontend", "retroarch", "libretro-cores", "picoarch", "standalone",
    "audio-router", "pyxel", "network-services", "nextcommander",
    "music-player", "portmaster",
)
PLACEHOLDER_HELPERS = (
    "plumos-nextcommander-launch", "plumos-music-player-launch",
    "plumos-retroarch-menu-launch", "plumos-pyxel-setup",
    "plumos-portmaster-launch", "plumos-portmaster-update",
    "plumos-portmaster-port-launch",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"fixture output already exists: {output}")
    output.mkdir(parents=True)

    shutil.copytree(ROOT / "package/app-layer-pixel2", output, dirs_exist_ok=True)
    shutil.copytree(ROOT / "vendor/plumos-frontend/seed", output, dirs_exist_ok=True)
    subprocess.run(
        [
            "python3",
            str(ROOT / "scripts/generate-pixel2-system-logos.py"),
            str(output / "themes/default/logos/systems"),
        ],
        check=True,
    )
    (output / "bin").mkdir(exist_ok=True)
    (output / "components").mkdir(exist_ok=True)

    for helper in PLACEHOLDER_HELPERS:
        path = output / "bin" / helper
        path.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        path.chmod(0o755)

    for component in COMPONENTS:
        directory = output / "components" / component
        directory.mkdir(parents=True)
        payload: dict[str, object] = {
            "name": f"Pixel2 CI fixture {component}",
            "component": component,
            "device": "pixel2",
        }
        if component == "standalone":
            payload["emulators"] = [
                {"id": "pcsx_rearmed", "status": "built"},
                {"id": "ppsspp", "status": "built"},
                {"id": "drastic", "status": "built"},
                {"id": "openbor", "status": "built"},
                {"id": "pico8", "status": "built"},
                {"id": "scummvm", "status": "libretro-route"},
                {"id": "easyrpg", "status": "libretro-route"},
                {"id": "flycast", "status": "libretro-route"},
                {"id": "nxengine-evo", "status": "libretro-route"},
            ]
        (directory / "manifest.json").write_text(
            json.dumps(payload, indent=2) + "\n", encoding="utf-8"
        )

    print(f"audit_fixture=result-ok output={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
