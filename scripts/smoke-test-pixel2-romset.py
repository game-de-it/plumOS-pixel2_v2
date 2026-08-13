#!/usr/bin/env python3
"""Exercise every packaged Pixel2 launch route backed by the supplied ROM set.

The script keeps only one system's representative content on the device at a
time, under a hidden uniquely-named directory.  It suspends the frontend,
streams the device helper over stdin, checks that each selected emulator process
survives the requested startup window, removes the staged content, restores the
frontend recent/resume files, and starts the frontend again.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts/pixel2-device-launch-smoke.sh"
ROUTE_VALIDATOR = ROOT / "scripts/validate-romset-routes.py"
DEFAULT_ADB = Path.home() / "Library/Android/sdk/platform-tools/adb"

# Some cores require a ROM revision that matches their own database.  Selecting
# one alphabetically-first file for every profile produces false failures for
# arcade sets and can miss format-specific paths such as BlueMSX disk images.
# These are file-name hints only: content still comes exclusively from the
# user-supplied ROM root and a missing hint falls back to the system sample.
PROFILE_SAMPLE_NAMES = {
    "retroarch:fbneo": "1942a.zip",
    "retroarch:fbalpha2012": "1942a.zip",
    "retroarch:mame2003_plus": "ddragon.zip",
    "retroarch:km_mame2003_xtreme": "ddragon.zip",
    "retroarch:mame2000": "ddragon.zip",
    "retroarch:mba_mini": "varthj.zip",
    "retroarch:bluemsx": "Ys2-p.dsk",
    "retroarch:fmsx": "XGR1Trial.rom",
    "retroarch:km_duckswanstation_xtreme_amped": "chroQW.img",
    "retroarch:parallel_n64": "SUPERMARIO64.Z64",
    "pyxel:pixel2": "LastEmulator.pyxapp",
}


def load_route_validator() -> Any:
    spec = importlib.util.spec_from_file_location("pixel2_routes", ROUTE_VALIDATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {ROUTE_VALIDATOR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(command: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, **kwargs)


def resolve_case_insensitive(parent: Path, relative: str) -> Path | None:
    current = parent
    for part in Path(relative.replace("\\", "/")).parts:
        if part in {"", "."}:
            continue
        if part == "..":
            current = current.parent
            continue
        exact = current / part
        if exact.exists():
            current = exact
            continue
        try:
            match = next(
                child for child in current.iterdir() if child.name.casefold() == part.casefold()
            )
        except (StopIteration, OSError):
            return None
        current = match
    return current if current.is_file() else None


def referenced_content(sample: Path) -> list[Path]:
    """Return the sample plus local descriptor companions needed at launch."""
    found: set[Path] = {sample.resolve()}
    suffix = sample.suffix.lower()
    text = ""
    if suffix in {".cue", ".gdi", ".m3u", ".m3u8"}:
        text = sample.read_text(encoding="utf-8", errors="replace")
    references: list[str] = []
    if suffix == ".cue":
        for line in text.splitlines():
            match = re.match(r'^\s*FILE\s+(?:"([^"]+)"|(\S+))', line, re.IGNORECASE)
            if match:
                references.append(match.group(1) or match.group(2))
    elif suffix == ".gdi":
        for line in text.splitlines()[1:]:
            fields = shlex.split(line, posix=True)
            if len(fields) >= 5:
                references.append(fields[4])
    elif suffix in {".m3u", ".m3u8"}:
        references.extend(
            line.strip()
            for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        )
    for reference in references:
        path = resolve_case_insensitive(sample.parent, reference)
        if path is None:
            raise RuntimeError(f"missing descriptor companion: {sample}: {reference}")
        found.add(path.resolve())
    if suffix in {".img", ".ccd", ".sub"}:
        for candidate in sample.parent.iterdir():
            if candidate.is_file() and candidate.stem.casefold() == sample.stem.casefold():
                if candidate.suffix.lower() in {".img", ".ccd", ".sub", ".cue"}:
                    found.add(candidate.resolve())
    return sorted(found)


def report_sample_path(sample: Path, rom_root: Path) -> str:
    """Return a stable report label even when a test root uses symlinks."""
    try:
        return str(sample.relative_to(rom_root))
    except ValueError:
        try:
            return str(sample.resolve().relative_to(rom_root.resolve()))
        except ValueError:
            return sample.name


def find_named_sample(dirs: list[Path], name: str) -> Path | None:
    wanted = name.casefold()
    for directory in dirs:
        for current, subdirs, files in os.walk(directory):
            subdirs.sort()
            for filename in sorted(files):
                if filename.casefold() == wanted:
                    return Path(current) / filename
    return None


class Device:
    def __init__(self, adb: Path) -> None:
        self.adb_path = str(adb)
        self.helper = HELPER.read_text(encoding="utf-8")

    def run_adb(self, *args: str, check: bool = True, capture: bool = False) -> subprocess.CompletedProcess[str]:
        return run(
            [self.adb_path, *args],
            check=check,
            stdout=subprocess.PIPE if capture else None,
            stderr=subprocess.STDOUT if capture else None,
        )

    def shell(self, command: str, *, check: bool = True) -> subprocess.CompletedProcess[str]:
        return run(
            [self.adb_path, "shell", command],
            check=check,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def helper_action(self, action: str, **values: str) -> subprocess.CompletedProcess[str]:
        assignments = {"ACTION": action, **values}
        prefix = "".join(f"{name}={shlex.quote(value)}\n" for name, value in assignments.items())
        return run(
            [self.adb_path, "shell", "sh", "-s"],
            input=prefix + self.helper,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )


def markdown_report(result: dict[str, Any]) -> str:
    summary = result["summary"]
    lines = [
        "# Pixel2 device ROM-set launch smoke",
        "",
        f"ROM root: `{result['rom_root']}`",
        f"Device app version: `{result.get('device_version', '')}`",
        "",
        "## Summary",
        "",
        f"- enabled systems: {summary['enabled_systems']}",
        f"- systems with representative ROM: {summary['systems_with_rom']}",
        f"- systems without representative ROM: {summary['systems_without_rom']}",
        f"- launch profiles selected: {summary['profiles_selected']}",
        f"- launch profiles passed: {summary['profiles_passed']}",
        f"- launch profiles failed: {summary['profiles_failed']}",
        "",
        "## Device launch results",
        "",
        "| system | profile | sample | result | runtime |",
        "| --- | --- | --- | --- | --- |",
    ]
    for row in result["launches"]:
        lines.append(
            f"| {row['system']} | `{row['profile']}` | `{row['sample_rom']}` | "
            f"{row['status']} | `{row.get('runtime_exe', '')}` |"
        )
    lines.extend(["", "## Enabled systems without matching ROM", ""])
    lines.append(", ".join(result["systems_without_rom"]) or "none")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom-root", required=True)
    parser.add_argument("--app-root", default="output/app-layer/pixel2/plumos")
    parser.add_argument("--adb", default=str(DEFAULT_ADB))
    parser.add_argument("--seconds", type=int, default=8)
    parser.add_argument("--system", action="append", default=[])
    parser.add_argument("--profile", action="append", default=[])
    parser.add_argument("--default-only", action="store_true")
    parser.add_argument("--report-json", default="output/validation/pixel2-device-romset-smoke.json")
    parser.add_argument("--report-markdown", default="output/validation/pixel2-device-romset-smoke.md")
    args = parser.parse_args()

    if args.seconds < 3 or args.seconds > 60:
        parser.error("--seconds must be between 3 and 60")
    rom_root = Path(args.rom_root).resolve()
    app_root = (ROOT / args.app_root).resolve() if not Path(args.app_root).is_absolute() else Path(args.app_root)
    adb = Path(args.adb).expanduser().resolve()
    for required in (rom_root, app_root, adb, HELPER):
        if not required.exists():
            parser.error(f"required path does not exist: {required}")

    route = load_route_validator()
    systems = route.load_json(app_root / "config/frontend/systems.json")["systems"]
    standalone_manifest = route.load_json(app_root / "components/standalone/manifest.json")
    standalone_ids = {entry["id"] for entry in standalone_manifest.get("emulators", [])}
    enabled = [system for system in systems if system.get("enabled") is not False]
    if args.system:
        requested = set(args.system)
        enabled = [system for system in enabled if system["id"] in requested]
        missing = requested - {system["id"] for system in enabled}
        if missing:
            parser.error("unknown or disabled system: " + ", ".join(sorted(missing)))
    requested_profiles = set(args.profile)
    available_profiles = {
        profile
        for system in enabled
        for profile in system.get("launch_profiles", [])
    }
    missing_profiles = requested_profiles - available_profiles
    if missing_profiles:
        parser.error("profile not enabled for selected systems: " + ", ".join(sorted(missing_profiles)))

    all_profile_failures: list[str] = []
    for system in enabled:
        for profile in system.get("launch_profiles", []):
            status, detail = route.profile_status(app_root, profile, standalone_ids)
            if status != "ok":
                all_profile_failures.append(f"{system['id']}:{profile}:{status}:{detail}")
    if all_profile_failures:
        print("static route validation failed:", file=sys.stderr)
        print("\n".join(all_profile_failures), file=sys.stderr)
        return 2

    rom_dirs = route.directory_index(rom_root)
    selected: list[tuple[dict[str, Any], list[tuple[Path, list[str]]]]] = []
    without_rom: list[str] = []
    for system in enabled:
        dirs = route.candidate_dirs(system, rom_dirs)
        extensions = {ext.lower().lstrip(".") for ext in system.get("extensions", [])}
        sample = route.find_representative_rom(dirs, extensions) if dirs else None
        if sample is None:
            without_rom.append(system["id"])
            continue
        profiles = [system.get("default_launch_profile", "")] if args.default_only else list(system.get("launch_profiles", []))
        profiles = [profile for profile in profiles if profile]
        if requested_profiles:
            profiles = [profile for profile in profiles if profile in requested_profiles]
        if not profiles:
            continue
        grouped: dict[Path, list[str]] = {}
        for profile in profiles:
            hinted_name = PROFILE_SAMPLE_NAMES.get(profile)
            profile_sample = (
                find_named_sample(dirs, hinted_name) if hinted_name else None
            )
            grouped.setdefault((profile_sample or sample).resolve(), []).append(profile)
        selected.append((system, list(grouped.items())))

    device = Device(adb)
    devices = device.run_adb("devices", capture=True)
    connected = [line for line in devices.stdout.splitlines()[1:] if "\tdevice" in line]
    if len(connected) != 1:
        print(f"expected exactly one ADB device, found {len(connected)}", file=sys.stderr)
        return 2
    manifest = device.shell("sed -n '1,24p' /mnt/plumos/manifest.json").stdout
    version_match = re.search(r'"version"\s*:\s*"([^"]+)"', manifest)
    device_version = version_match.group(1) if version_match else "unknown"

    stamp = f"{int(time.time())}-{os.getpid()}"
    stage_name = f"__plumos_smoke_{stamp}"

    launches: list[dict[str, str]] = []
    prepared = False
    staged_paths: list[str] = []
    try:
        prepared_result = device.helper_action("prepare")
        print(prepared_result.stdout, end="")
        if prepared_result.returncode != 0 or "SMOKE_RESULT=prepared" not in prepared_result.stdout:
            raise RuntimeError("device preparation failed")
        prepared = True

        for system, sample_groups in selected:
            system_id = system["id"]
            aliases = [
                alias.get("name", "")
                for alias in system.get("directory_aliases", [])
                if isinstance(alias.get("name"), str) and alias.get("name")
            ]
            device_directory = aliases[0] if aliases else system_id
            parts = Path(device_directory.replace("\\", "/")).parts
            if not parts or any(part in {"", ".", ".."} for part in parts):
                raise RuntimeError(f"unsafe device directory alias: {device_directory!r}")
            stage_relative = f"{Path(*parts).as_posix()}/{stage_name}"
            remote_system = f"/mnt/plumos-user/roms/{stage_relative}"
            if not remote_system.startswith("/mnt/plumos-user/roms/") or stage_name not in remote_system:
                raise RuntimeError("unsafe smoke staging path")
            device.shell(f"mkdir -p {shlex.quote(remote_system)}")
            staged_paths.append(remote_system)
            companions = sorted(
                {
                    companion
                    for sample, _profiles in sample_groups
                    for companion in referenced_content(sample)
                }
            )
            total_bytes = sum(path.stat().st_size for path in companions)
            free_line = device.shell("df -k /mnt/plumos-user | tail -n 1").stdout.split()
            free_bytes = int(free_line[3]) * 1024 if len(free_line) >= 4 else 0
            if free_bytes and total_bytes + 64 * 1024 * 1024 > free_bytes:
                raise RuntimeError(
                    f"insufficient device space for {system_id}: need {total_bytes}, free {free_bytes}"
                )
            for source in companions:
                local_relative = Path(source.name)
                remote_parent = remote_system
                device.shell(f"mkdir -p {shlex.quote(remote_parent)}")
                remote_path = f"{remote_system}/{local_relative.as_posix()}"
                print(f"PUSH system={system_id} file={source.name} bytes={source.stat().st_size}")
                device.run_adb("push", str(source), remote_path)

            scan = device.helper_action("scan", SMOKE_SYSTEM=system_id)
            print(scan.stdout, end="")
            if scan.returncode != 0 or f"SMOKE_RESULT=scanned system={system_id}" not in scan.stdout:
                raise RuntimeError(f"device scan failed for {system_id}")
            for sample, profiles in sample_groups:
                sample_relative = f"{stage_relative}/{sample.name}"
                for profile in profiles:
                    print(f"LAUNCH system={system_id} profile={profile} sample={sample.name}")
                    outcome = device.helper_action(
                        "launch",
                        SMOKE_SYSTEM=system_id,
                        SMOKE_RELATIVE=sample_relative,
                        SMOKE_PROFILE=profile,
                        SMOKE_SECONDS=str(args.seconds),
                    )
                    print(outcome.stdout, end="")
                    match = re.search(r"^SMOKE_RUNTIME_EXE=(.*)$", outcome.stdout, re.MULTILINE)
                    passed = outcome.returncode == 0 and f"SMOKE_RESULT=pass profile={profile}" in outcome.stdout
                    launches.append(
                        {
                            "system": system_id,
                            "profile": profile,
                            "sample_rom": report_sample_path(sample, rom_root),
                            "status": "pass" if passed else "fail",
                            "runtime_exe": match.group(1) if match else "",
                            "output": outcome.stdout[-8000:],
                        }
                    )
            device.shell(f"rm -rf {shlex.quote(remote_system)}")
            staged_paths.remove(remote_system)
    finally:
        for staged_path in staged_paths:
            device.shell(f"rm -rf {shlex.quote(staged_path)}", check=False)
        if prepared:
            restored = device.helper_action("restore")
            print(restored.stdout, end="")
            if restored.returncode != 0:
                print("warning: frontend restore failed", file=sys.stderr)

    summary = {
        "enabled_systems": len(enabled),
        "systems_with_rom": len(selected),
        "systems_without_rom": len(without_rom),
        "profiles_selected": sum(
            len(profiles)
            for _, sample_groups in selected
            for _, profiles in sample_groups
        ),
        "profiles_passed": sum(row["status"] == "pass" for row in launches),
        "profiles_failed": sum(row["status"] != "pass" for row in launches),
    }
    result = {
        "rom_root": str(rom_root),
        "app_root": str(app_root),
        "device_version": device_version,
        "startup_seconds": args.seconds,
        "summary": summary,
        "systems_without_rom": without_rom,
        "launches": launches,
    }
    json_path = ROOT / args.report_json
    markdown_path = ROOT / args.report_markdown
    json_path.parent.mkdir(parents=True, exist_ok=True)
    markdown_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    markdown_path.write_text(markdown_report(result), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
    return 1 if summary["profiles_failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
