#!/usr/bin/env python3
"""Restore device-smoke samples as persistent Pixel2 frontend content."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SMOKE_SCRIPT = ROOT / "scripts/smoke-test-pixel2-romset.py"
DEFAULT_REPORTS = (
    "pixel2-device-romset-smoke-final.json",
    "pixel2-device-romset-smoke-etc.json",
    "pixel2-device-romset-smoke-retry.json",
    "pixel2-device-romset-smoke-final-retry.json",
    "pixel2-device-romset-smoke-shared-dirs.json",
    "pixel2-device-romset-smoke-cps-final.json",
    "pixel2-device-romset-smoke-easyrpg-final.json",
    "pixel2-device-romset-smoke-scummvm-final.json",
)


def load_smoke() -> Any:
    spec = importlib.util.spec_from_file_location("pixel2_smoke", SMOKE_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {SMOKE_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class Device:
    def __init__(self, adb: Path) -> None:
        self.adb = str(adb)

    def run(self, *args: str, capture: bool = False, check: bool = True) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [self.adb, *args],
            check=check,
            text=True,
            stdout=subprocess.PIPE if capture else None,
            stderr=subprocess.STDOUT if capture else None,
        )

    def shell(self, command: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        return self.run("shell", command, capture=True, check=check)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom-root", required=True)
    parser.add_argument(
        "--app-root", default="output/app-layer/pixel2/plumos"
    )
    parser.add_argument(
        "--report-dir", default="output/validation"
    )
    parser.add_argument("--report", action="append", default=[])
    parser.add_argument(
        "--adb", default=str(Path.home() / "Library/Android/sdk/platform-tools/adb")
    )
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--manifest")
    args = parser.parse_args()

    rom_root = Path(args.rom_root).expanduser().resolve()
    app_root = (ROOT / args.app_root).resolve() if not Path(args.app_root).is_absolute() else Path(args.app_root)
    report_dir = (ROOT / args.report_dir).resolve() if not Path(args.report_dir).is_absolute() else Path(args.report_dir)
    adb = Path(args.adb).expanduser().resolve()
    for required in (rom_root, app_root, report_dir, SMOKE_SCRIPT):
        if not required.exists():
            parser.error(f"required path does not exist: {required}")

    smoke = load_smoke()
    systems_data = json.loads(
        (app_root / "config/frontend/systems.json").read_text(encoding="utf-8")
    )
    systems = {entry["id"]: entry for entry in systems_data["systems"]}
    report_names = args.report or list(DEFAULT_REPORTS)
    samples: set[tuple[str, str]] = set()
    passed_profiles: set[tuple[str, str]] = set()
    for name in report_names:
        path = Path(name)
        if not path.is_absolute():
            path = report_dir / path
        data = json.loads(path.read_text(encoding="utf-8"))
        for row in data.get("launches", []):
            if row.get("status") != "pass":
                continue
            system_id = row["system"]
            samples.add((system_id, row["sample_rom"]))
            passed_profiles.add((system_id, row["profile"]))

    files: dict[str, Path] = {}
    synthetic: dict[str, str] = {}
    sample_manifest: list[dict[str, Any]] = []
    for system_id, relative in sorted(samples):
        system = systems.get(system_id)
        if system is None or system.get("enabled") is False:
            raise RuntimeError(f"report references unavailable system: {system_id}")
        aliases = [
            entry.get("name", "")
            for entry in system.get("directory_aliases", [])
            if entry.get("name")
        ]
        if not aliases:
            raise RuntimeError(f"system has no device directory alias: {system_id}")
        source = (rom_root / relative).resolve()
        try:
            source.relative_to(rom_root)
        except ValueError as exc:
            raise RuntimeError(f"sample escapes ROM root: {source}") from exc
        if not source.exists():
            raise RuntimeError(f"sample is missing: {source}")

        source_files = smoke.staged_content(system_id, source)
        parent_tree = system_id in smoke.PARENT_TREE_SYSTEMS
        source_root = source if source.is_dir() else source.parent
        device_base = f"/mnt/plumos-user/roms/{aliases[0]}"
        if parent_tree:
            device_base += f"/{source_root.name}"
        for source_file, staged_relative in source_files:
            target_name = staged_relative.as_posix() if parent_tree else source_file.name
            target = f"{device_base}/{target_name}"
            previous = files.get(target)
            if previous is not None and previous.resolve() != source_file.resolve():
                raise RuntimeError(f"target collision: {target}: {previous}: {source_file}")
            files[target] = source_file
        marker = smoke.SYNTHETIC_LAUNCH_NAMES.get(system_id)
        if marker:
            synthetic[f"{device_base}/{marker}"] = smoke.SYNTHETIC_LAUNCH_CONTENT.get(system_id, "")
        sample_manifest.append(
            {
                "system": system_id,
                "sample": relative,
                "device_base": device_base,
                "files": len(source_files) + (1 if marker else 0),
                "bytes": sum(path.stat().st_size for path, _ in source_files),
            }
        )

    planned_bytes = sum(path.stat().st_size for path in files.values())
    result: dict[str, Any] = {
        "format": "plumos-pixel2-visual-rom-restore-v1",
        "rom_root": str(rom_root),
        "reports": report_names,
        "systems": len({system for system, _ in samples}),
        "passed_profiles": len(passed_profiles),
        "samples": len(samples),
        "files": len(files) + len(synthetic),
        "bytes": planned_bytes,
        "content": sample_manifest,
    }
    print(
        "restore_plan "
        f"systems={result['systems']} profiles={result['passed_profiles']} "
        f"samples={result['samples']} files={result['files']} bytes={planned_bytes}"
    )

    if args.apply:
        device = Device(adb)
        devices = device.run("devices", capture=True).stdout.splitlines()[1:]
        connected = [line for line in devices if "\tdevice" in line]
        if len(connected) != 1:
            raise RuntimeError(f"expected exactly one ADB device, found {len(connected)}")
        free_fields = device.shell("df -k /mnt/plumos-user | tail -n 1").stdout.split()
        free_bytes = int(free_fields[3]) * 1024 if len(free_fields) >= 4 else 0
        if free_bytes < planned_bytes + 128 * 1024 * 1024:
            raise RuntimeError(
                f"insufficient user-volume space: need={planned_bytes} free={free_bytes}"
            )

        transferred = 0
        skipped = 0
        for index, (target, source) in enumerate(sorted(files.items()), 1):
            expected = sha256(source)
            quoted_target = shlex.quote(target)
            probe = device.shell(
                f"test -f {quoted_target} && sha256sum {quoted_target} || true"
            ).stdout.strip()
            if probe:
                actual = probe.split()[0]
                if actual == expected:
                    skipped += 1
                    continue
                raise RuntimeError(f"refusing to overwrite different device file: {target}")
            parent = str(Path(target).parent)
            temporary = f"{target}.plumos-restore-new"
            device.shell(f"mkdir -p {shlex.quote(parent)}")
            print(f"restore_push index={index}/{len(files)} bytes={source.stat().st_size} target={target}")
            device.run("push", str(source), temporary)
            actual = device.shell(f"sha256sum {shlex.quote(temporary)}").stdout.split()[0]
            if actual != expected:
                raise RuntimeError(f"device SHA mismatch: {target}")
            device.shell(f"mv {shlex.quote(temporary)} {quoted_target}")
            transferred += 1

        for target, content in sorted(synthetic.items()):
            parent = str(Path(target).parent)
            device.shell(f"mkdir -p {shlex.quote(parent)}")
            device.shell(f"printf %s {shlex.quote(content)} > {shlex.quote(target)}")
        device.shell("sync")
        result["transferred_files"] = transferred
        result["skipped_identical_files"] = skipped
        print(f"restore_result transferred={transferred} skipped={skipped} result=ok")

    if args.manifest:
        manifest = Path(args.manifest)
        if not manifest.is_absolute():
            manifest = ROOT / manifest
        manifest.parent.mkdir(parents=True, exist_ok=True)
        manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"restore_manifest={manifest}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
