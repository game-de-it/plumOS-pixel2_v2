#!/usr/bin/env python3
"""Validate Pixel2 frontend routes against a user-provided ROM set.

This is a host-side, non-mutating validator. It does not copy ROMs and it does
not execute emulators. It checks every launch profile exposed by each enabled
frontend system against the packaged runtime, and separately records whether a
representative ROM exists for exercising the default route on hardware.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Any


EXTRA_DIR_ALIASES = {
    "arcade": ["mame"],
    "atari2600": ["ATARI/2600"],
    "atari5200": ["ATARI/5200"],
    "atari7800": ["ATARI/7800"],
    "atari800": ["ATARI/800"],
    "lynx": ["ATARI/Lynx"],
    "jaguar": ["ATARI/Jaguar"],
    "cps1": ["mame"],
    "cps2": ["mame"],
    "cps3": ["mame"],
    "msx": ["msx2"],
    "cpc": ["amstradcpc"],
    "colecovision": ["coleco"],
    "pc98": ["pc-9800"],
    "virtualboy": ["viretualboy"],
}


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def directory_index(root: Path) -> dict[str, Path]:
    out: dict[str, Path] = {}
    for current, subdirs, _files in os.walk(root):
        current_path = Path(current)
        depth = len(current_path.relative_to(root).parts) if current_path != root else 0
        if depth >= 2:
            subdirs[:] = []
        if current_path == root:
            for subdir in subdirs:
                path = root / subdir
                out.setdefault(subdir.lower(), path)
        else:
            rel = current_path.relative_to(root).as_posix()
            out.setdefault(rel.lower(), current_path)
        subdirs.sort()
    return out


def candidate_dirs(system: dict[str, Any], rom_dirs: dict[str, Path]) -> list[Path]:
    names: list[str] = []
    for alias in system.get("directory_aliases", []):
        name = alias.get("name")
        if isinstance(name, str):
            names.append(name)
    names.append(system["id"])
    names.extend(EXTRA_DIR_ALIASES.get(system["id"], []))
    # The supplied archival ROM set keeps less frequently used systems below
    # `_etc/<system>`. Treat those directories as additional read-only aliases;
    # the device staging path remains the canonical Pixel2 FE directory.
    names.extend(
        f"_etc/{name}"
        for name in list(names)
        if not name.lower().startswith("_etc/")
    )
    seen: set[Path] = set()
    out: list[Path] = []
    for name in names:
        path = rom_dirs.get(name.lower())
        if path and path not in seen:
            out.append(path)
            seen.add(path)
    return out


def find_representative_rom(dirs: list[Path], extensions: set[str]) -> Path | None:
    for directory in dirs:
        for current, subdirs, files in os.walk(directory):
            subdirs.sort()
            for filename in sorted(files):
                suffix = Path(filename).suffix.lower().lstrip(".")
                if suffix in extensions:
                    return Path(current) / filename
    return None


def profile_status(app_root: Path, profile: str, standalone_ids: set[str]) -> tuple[str, str]:
    if profile.startswith("retroarch:"):
        core = profile.split(":", 1)[1]
        core_path = app_root / "cores" / f"{core}_libretro.so"
        if core_path.exists():
            return "ok", str(core_path.relative_to(app_root))
        return "missing-core", str(core_path.relative_to(app_root))
    if profile.startswith("picoarch:"):
        core = profile.split(":", 1)[1]
        launcher = app_root / "bin/plumos-picoarch-launch"
        core_paths = [
            app_root / "picoarch/cores" / f"{core}_libretro.so",
            app_root / "cores" / f"{core}_libretro.so",
        ]
        if not launcher.exists():
            return "missing-launcher", str(launcher.relative_to(app_root))
        for core_path in core_paths:
            if core_path.exists():
                return "ok", str(core_path.relative_to(app_root))
        return "missing-core", str(core_paths[-1].relative_to(app_root))
    if profile.startswith("standalone:"):
        emulator = profile.split(":", 1)[1]
        launcher = app_root / "bin/plumos-standalone-launch"
        if not launcher.exists():
            return "missing-launcher", str(launcher.relative_to(app_root))
        if emulator not in standalone_ids:
            return "unknown-emulator", emulator
        exe_candidates = [
            app_root / "standalone" / emulator / "bin",
            app_root / "emulator" / "standalone" / emulator,
        ]
        if any(path.exists() for path in exe_candidates):
            return "ok", emulator
        return "pending-binary", emulator
    if profile.startswith("pyxel:"):
        pyxel_profile = profile.split(":", 1)[1]
        launcher = app_root / "bin/plumos-pyxel-pixel2-launch"
        python = app_root / "apps/python/bin/python3.11"
        package = app_root / "apps/pyxel/site/pyxel/__init__.py"
        if pyxel_profile != "pixel2":
            return "unknown-profile", profile
        if not launcher.exists():
            return "missing-launcher", str(launcher.relative_to(app_root))
        if not python.exists():
            return "missing-runtime", str(python.relative_to(app_root))
        if not package.exists():
            return "missing-runtime", str(package.relative_to(app_root))
        return "ok", "pyxel"
    return "unknown-profile", profile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-root", default="output/app-layer/pixel2/plumos")
    parser.add_argument("--rom-root", required=True)
    parser.add_argument("--markdown", default="")
    parser.add_argument("--json", default="")
    args = parser.parse_args()

    app_root = Path(args.app_root)
    rom_root = Path(args.rom_root)
    systems_path = app_root / "config/frontend/systems.json"
    systems = load_json(systems_path)["systems"]
    standalone_manifest = load_json(app_root / "components/standalone/manifest.json")
    standalone_ids = {entry["id"] for entry in standalone_manifest.get("emulators", [])}
    rom_dirs = directory_index(rom_root)

    rows: list[dict[str, Any]] = []
    profile_rows: list[dict[str, str]] = []
    for system in systems:
        if system.get("enabled") is False:
            continue
        system_id = system["id"]
        dirs = candidate_dirs(system, rom_dirs)
        extensions = {ext.lower().lstrip(".") for ext in system.get("extensions", [])}
        rom = find_representative_rom(dirs, extensions) if dirs else None
        profile = system.get("default_launch_profile")
        if not profile and system.get("launch_profiles"):
            profile = system["launch_profiles"][0]
        status, detail = profile_status(app_root, profile or "", standalone_ids)
        profiles = system.get("launch_profiles", [])
        for listed_profile in profiles:
            listed_status, listed_detail = profile_status(
                app_root, listed_profile, standalone_ids
            )
            profile_rows.append(
                {
                    "system": system_id,
                    "profile": listed_profile,
                    "route_status": listed_status,
                    "route_detail": listed_detail,
                }
            )
        rows.append(
            {
                "system": system_id,
                "rom_dirs": [str(path.relative_to(rom_root)) for path in dirs],
                "sample_rom": str(rom.relative_to(rom_root)) if rom else "",
                "default_launch_profile": profile or "",
                "route_status": status,
                "route_detail": detail,
                "profile_routes": [
                    row for row in profile_rows if row["system"] == system_id
                ],
            }
        )

    rom_dir_to_systems = {
        path.name: [
            row["system"]
            for row in rows
            if path.name in row["rom_dirs"]
        ]
        for path in sorted(rom_root.iterdir())
        if path.is_dir()
    }
    unmapped_rom_dirs = [
        name for name, mapped in rom_dir_to_systems.items() if not mapped and name != "bios"
    ]

    summary = {
        "enabled_systems": len(rows),
        "launch_profiles": len(profile_rows),
        "profile_routes_ok": sum(
            1 for row in profile_rows if row["route_status"] == "ok"
        ),
        "profile_routes_failed": sum(
            1 for row in profile_rows if row["route_status"] != "ok"
        ),
        "systems_with_rom": sum(1 for row in rows if row["sample_rom"]),
        "route_ok": sum(1 for row in rows if row["sample_rom"] and row["route_status"] == "ok"),
        "route_pending_binary": sum(
            1 for row in rows if row["sample_rom"] and row["route_status"] == "pending-binary"
        ),
        "systems_without_rom": sum(1 for row in rows if not row["sample_rom"]),
        "unmapped_rom_dirs": unmapped_rom_dirs,
    }
    result = {"summary": summary, "profile_routes": profile_rows, "systems": rows}

    if args.json:
        json_path = Path(args.json)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
    if args.markdown:
        markdown_path = Path(args.markdown)
        markdown_path.parent.mkdir(parents=True, exist_ok=True)
        lines = [
            "# Pixel2 ROM set route validation",
            "",
            f"ROM root: `{rom_root}`",
            f"App root: `{app_root}`",
            "",
            "## Summary",
            "",
            f"- enabled systems: {summary['enabled_systems']}",
            f"- launch profiles: {summary['launch_profiles']}",
            f"- profile routes OK: {summary['profile_routes_ok']}",
            f"- profile routes failed: {summary['profile_routes_failed']}",
            f"- systems with representative ROM: {summary['systems_with_rom']}",
            f"- route OK: {summary['route_ok']}",
            f"- standalone pending binary: {summary['route_pending_binary']}",
            f"- systems without matching ROM: {summary['systems_without_rom']}",
            f"- unmapped ROM directories: {', '.join(summary['unmapped_rom_dirs']) or 'none'}",
            "",
            "## Systems with ROMs",
            "",
            "| system | sample ROM | default profile | route status | route detail |",
            "| --- | --- | --- | --- | --- |",
        ]
        for row in rows:
            if not row["sample_rom"]:
                continue
            lines.append(
                "| {system} | `{sample_rom}` | `{default_launch_profile}` | {route_status} | `{route_detail}` |".format(
                    **row
                )
            )
        lines.extend(["", "## Systems without matching ROM", ""])
        missing = [row["system"] for row in rows if not row["sample_rom"]]
        lines.append(", ".join(missing) if missing else "none")
        lines.extend(["", "## Failed launch-profile routes", ""])
        failed_profiles = [
            row for row in profile_rows if row["route_status"] != "ok"
        ]
        if failed_profiles:
            lines.extend(
                [
                    "| system | profile | status | detail |",
                    "| --- | --- | --- | --- |",
                ]
            )
            for row in failed_profiles:
                lines.append(
                    "| {system} | `{profile}` | {route_status} | `{route_detail}` |".format(
                        **row
                    )
                )
        else:
            lines.append("none")
        markdown_path.write_text("\n".join(lines) + "\n")

    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
    return 1 if any(row["route_status"] != "ok" for row in profile_rows) else 0


if __name__ == "__main__":
    raise SystemExit(main())
