#!/usr/bin/env python3
"""Prepare user-provided Pixel2 BIOS files from a ROM collection.

This tool never writes BIOS content into the repository or app-layer. It reads
the enabled Pixel2 launch profiles and generated libretro `.info` files, copies
matching user-provided firmware into an external staging directory, and emits
source/destination/hash and missing-firmware reports.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tempfile
from typing import Any
import zipfile


STANDALONE_FIRMWARE = {
    "drastic_bios_arm7.bin": ("standalone:drastic", False),
    "drastic_bios_arm9.bin": ("standalone:drastic", False),
}

# The frontend uses stable plumOS profile IDs while a few upstream core-info
# files retain their historical canonical names. Keep these aliases explicit so
# firmware inventory cannot silently skip an alternate route.
CORE_INFO_ALIASES = {
    "beetle_saturn": "mednafen_saturn",
    "dosbox_pure_0.9.7": "dosbox_pure",
    "km_duckswanstation_xtreme_amped": "pcsx_rearmed",
    "km_mame2003_xtreme": "mame2003_plus",
    "km_puae_xtreme_amped": "puae",
    "mba_mini": "fbneo",
}

# FreeChaF's core-info currently marks all three Channel F images required,
# although its own description states that sl90025 only supersedes sl31253
# when present.  The two original 1 KiB images remain the actual minimum set.
OPTIONAL_FIRMWARE_OVERRIDES = {
    ("freechaf", "sl90025.bin"),
}

CHANNEL_F_COMBINED_HALVES = (
    ("sl31253.bin", "ac9804d4c0e9d07e33472e3726ed15c3"),
    ("sl31254.bin", "da98f4bb3242ab80d76629021bb27585"),
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_info(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in raw:
            continue
        key, value = raw.split("=", 1)
        values[key.strip()] = value.strip().strip('"')
    return values


def safe_relative(value: str) -> str:
    pure = PurePosixPath(value)
    if not value or pure.is_absolute() or ".." in pure.parts or str(pure) != value:
        raise SystemExit(f"error: unsafe firmware path in core info: {value}")
    return value


def collect_requirements(app_root: Path) -> dict[str, dict[str, Any]]:
    systems_path = app_root / "config/frontend/systems.json"
    systems = json.loads(systems_path.read_text(encoding="utf-8"))["systems"]
    cores: set[str] = set()
    standalone: set[str] = set()
    for system in systems:
        if not system.get("enabled"):
            continue
        for profile in system.get("launch_profiles", []):
            if profile.startswith(("retroarch:", "picoarch:")):
                cores.add(profile.split(":", 1)[1])
            elif profile.startswith("standalone:"):
                standalone.add(profile.split(":", 1)[1])

    requirements: dict[str, dict[str, Any]] = {}
    for core in sorted(cores):
        info_id = CORE_INFO_ALIASES.get(core, core)
        info_path = app_root / "info" / f"{info_id}_libretro.info"
        if not info_path.is_file():
            continue
        values = parse_info(info_path)
        try:
            count = int(values.get("firmware_count", "0"))
        except ValueError as exc:
            raise SystemExit(f"error: invalid firmware_count: {info_path}") from exc
        for index in range(count):
            relative = values.get(f"firmware{index}_path", "")
            if not relative:
                continue
            relative = safe_relative(relative)
            optional = (
                values.get(f"firmware{index}_opt", "false") == "true"
                or (core, relative) in OPTIONAL_FIRMWARE_OVERRIDES
            )
            entry = requirements.setdefault(
                relative,
                {"consumers": set(), "optional": True, "folder": False},
            )
            entry["consumers"].add(f"libretro:{core}")
            entry["optional"] = bool(entry["optional"] and optional)
            description = values.get(f"firmware{index}_desc", "")
            if "folder" in description.lower() and "/" in relative:
                entry["folder"] = True

    for relative, (consumer, optional) in STANDALONE_FIRMWARE.items():
        if consumer.split(":", 1)[1] not in standalone:
            continue
        entry = requirements.setdefault(
            relative,
            {"consumers": set(), "optional": True, "folder": False},
        )
        entry["consumers"].add(consumer)
        entry["optional"] = bool(entry["optional"] and optional)
    return requirements


def source_roots(rom_root: Path) -> list[Path]:
    roots = [rom_root / "bios"]
    roots.extend(
        sorted(
            child / "bios"
            for child in rom_root.iterdir()
            if child.is_dir() and (child / "bios").is_dir()
        )
    )
    return [root for root in roots if root.is_dir()]


def build_source_index(roots: list[Path]) -> tuple[dict[str, list[Path]], dict[str, list[Path]]]:
    by_relative: dict[str, list[Path]] = {}
    by_name: dict[str, list[Path]] = {}
    for root in roots:
        for path in sorted(root.rglob("*")):
            if not path.is_file():
                continue
            relative_key = path.relative_to(root).as_posix().casefold()
            by_relative.setdefault(relative_key, []).append(path)
            by_name.setdefault(path.name.casefold(), []).append(path)
    return by_relative, by_name


def choose_source(
    expected: str,
    roots: list[Path],
    by_relative: dict[str, list[Path]],
    by_name: dict[str, list[Path]],
) -> tuple[Path | None, str]:
    exact = by_relative.get(expected.casefold(), [])
    if exact:
        return exact[0], "relative"
    candidates = by_name.get(PurePosixPath(expected).name.casefold(), [])
    if not candidates:
        return None, "missing"

    def rank(path: Path) -> tuple[int, int, str]:
        for root_index, root in enumerate(roots):
            try:
                relative = path.relative_to(root)
                return root_index, len(relative.parts), relative.as_posix().casefold()
            except ValueError:
                continue
        return len(roots), 999, path.as_posix().casefold()

    return sorted(candidates, key=rank)[0], "basename"


def source_relative(path: Path, rom_root: Path) -> str:
    return path.relative_to(rom_root).as_posix()


def copy_one(
    source: Path,
    destination: Path,
    expected: str,
    consumers: set[str],
    optional: bool,
    match: str,
    rom_root: Path,
    records: list[dict[str, Any]],
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    records.append(
        {
            "destination": expected,
            "source": source_relative(source, rom_root),
            "sha256": sha256_file(destination),
            "size": destination.stat().st_size,
            "optional": optional,
            "match": match,
            "consumers": sorted(consumers),
        }
    )


def expand_bluemsx_distribution(
    archives: list[Path],
    staging: Path,
    requirements: dict[str, dict[str, Any]],
    rom_root: Path,
    records: list[dict[str, Any]],
) -> None:
    """Expand user-provided blueMSX machine data required by the core.

    Common ROM collections keep the full standalone data tree in
    blueMSXv282full.zip.  The libretro core needs the contents of Machines and
    Databases, not the archive itself.  Only those two safe relative trees are
    accepted here; BIOS content remains in the ignored external staging tree.
    """
    folder_consumers: dict[str, set[str]] = {}
    for expected, detail in requirements.items():
        if not detail["folder"] or not expected.startswith(("Machines/", "Databases/")):
            continue
        if "libretro:bluemsx" not in detail["consumers"]:
            continue
        folder_consumers.setdefault(PurePosixPath(expected).parts[0], set()).update(
            detail["consumers"]
        )
    if not folder_consumers or not archives:
        return

    archive = sorted(archives, key=lambda path: path.as_posix().casefold())[0]
    with zipfile.ZipFile(archive) as bundle:
        for member in sorted(bundle.infolist(), key=lambda item: item.filename.casefold()):
            if member.is_dir():
                continue
            relative = PurePosixPath(member.filename.replace("\\", "/"))
            if (
                relative.is_absolute()
                or ".." in relative.parts
                or not relative.parts
                or relative.parts[0] not in folder_consumers
            ):
                continue
            destination = staging.joinpath(*relative.parts)
            if destination.exists():
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(member) as source, destination.open("wb") as target:
                shutil.copyfileobj(source, target)
            records.append(
                {
                    "destination": relative.as_posix(),
                    "source": f"{source_relative(archive, rom_root)}!{member.filename}",
                    "sha256": sha256_file(destination),
                    "size": destination.stat().st_size,
                    "optional": False,
                    "match": "archive",
                    "consumers": sorted(folder_consumers[relative.parts[0]]),
                }
            )


def expand_channel_f_combined(
    candidates: list[Path],
    staging: Path,
    requirements: dict[str, dict[str, Any]],
    rom_root: Path,
    records: list[dict[str, Any]],
) -> None:
    """Split the Analogue Pocket Channel F combined BIOS when user-supplied.

    Some ROM collections contain the original 1 KiB SL31253 and SL31254 dumps
    concatenated as ``cfbios.bin``.  Accept only the exact known half hashes;
    an arbitrary 2 KiB file must never be relabelled as firmware.
    """
    if any(name not in requirements for name, _ in CHANNEL_F_COMBINED_HALVES):
        return
    for candidate in sorted(set(candidates), key=lambda path: path.as_posix().casefold()):
        payload = candidate.read_bytes()
        if len(payload) != 2048:
            continue
        halves = (payload[:1024], payload[1024:])
        if any(
            hashlib.md5(part).hexdigest() != expected_md5
            for part, (_, expected_md5) in zip(halves, CHANNEL_F_COMBINED_HALVES)
        ):
            continue
        for index, (part, (destination_name, _)) in enumerate(
            zip(halves, CHANNEL_F_COMBINED_HALVES)
        ):
            destination = staging / destination_name
            if destination.exists():
                continue
            destination.write_bytes(part)
            detail = requirements[destination_name]
            records.append(
                {
                    "destination": destination_name,
                    "source": (
                        f"{source_relative(candidate, rom_root)}"
                        f"#bytes={index * 1024}:{(index + 1) * 1024}"
                    ),
                    "sha256": sha256_file(destination),
                    "size": destination.stat().st_size,
                    "optional": bool(detail["optional"]),
                    "match": "compound-split",
                    "consumers": sorted(detail["consumers"]),
                }
            )
        return


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-root", type=Path, required=True)
    parser.add_argument("--rom-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()

    app_root = args.app_root.resolve()
    rom_root = args.rom_root.resolve()
    if not (app_root / "config/frontend/systems.json").is_file():
        raise SystemExit(f"error: Pixel2 app-layer is incomplete: {app_root}")
    roots = source_roots(rom_root)
    if not roots or roots[0] != rom_root / "bios":
        raise SystemExit(f"error: ROM set BIOS directory is missing: {rom_root / 'bios'}")

    requirements = collect_requirements(app_root)
    by_relative, by_name = build_source_index(roots)
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    args.report.resolve().parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="pixel2-bios-", dir=output.parent) as temp_name:
        staging = Path(temp_name) / "bios"
        staging.mkdir()
        records: list[dict[str, Any]] = []
        missing: list[dict[str, Any]] = []
        expanded_folders: set[str] = set()

        expand_bluemsx_distribution(
            by_name.get("bluemsxv282full.zip", []),
            staging,
            requirements,
            rom_root,
            records,
        )
        channel_f_candidates = list(by_name.get("cfbios.bin", []))
        channel_f_candidates.extend(
            rom_root.glob("*/analogue pocket/Assets/channel_f/common/cfbios.bin")
        )
        expand_channel_f_combined(
            channel_f_candidates,
            staging,
            requirements,
            rom_root,
            records,
        )

        for expected, detail in sorted(requirements.items()):
            if (staging / expected).is_file():
                continue
            source, match = choose_source(expected, roots, by_relative, by_name)
            consumers = set(detail["consumers"])
            optional = bool(detail["optional"])
            if source is None:
                missing.append(
                    {
                        "destination": expected,
                        "optional": optional,
                        "consumers": sorted(consumers),
                    }
                )
                continue
            copy_one(
                source,
                staging / expected,
                expected,
                consumers,
                optional,
                match,
                rom_root,
                records,
            )

            if detail["folder"]:
                folder = PurePosixPath(expected).parts[0]
                if folder in expanded_folders:
                    continue
                folder_source = roots[0] / folder
                if not folder_source.is_dir():
                    continue
                expanded_folders.add(folder)
                for extra in sorted(folder_source.rglob("*")):
                    if not extra.is_file():
                        continue
                    extra_relative = extra.relative_to(roots[0]).as_posix()
                    if any(record["destination"] == extra_relative for record in records):
                        continue
                    copy_one(
                        extra,
                        staging / extra_relative,
                        extra_relative,
                        consumers,
                        optional,
                        "folder",
                        rom_root,
                        records,
                    )

        total_bytes = sum(record["size"] for record in records)
        report = {
            "format": 1,
            "device": "pixel2",
            "source_ref": json.loads(
                (app_root / "manifest.json").read_text(encoding="utf-8")
            ).get("source_ref", "unknown"),
            "rom_root": str(rom_root),
            "requirements": len(requirements),
            "files": records,
            "file_count": len(records),
            "total_bytes": total_bytes,
            "missing": missing,
            "missing_required": sum(not item["optional"] for item in missing),
            "missing_optional": sum(item["optional"] for item in missing),
        }
        report_bytes = (json.dumps(report, indent=2, sort_keys=True) + "\n").encode()
        (staging / "plumos-bios-manifest.json").write_bytes(report_bytes)
        checksum_lines = "".join(
            f"{record['sha256']}  {record['destination']}\n"
            for record in sorted(records, key=lambda item: item["destination"])
        )
        (staging / "plumos-bios-checksums.sha256").write_text(
            checksum_lines, encoding="ascii"
        )
        args.report.resolve().write_bytes(report_bytes)
        if output.exists():
            if output.is_symlink() or not output.is_dir():
                raise SystemExit(f"error: output is not a directory: {output}")
            shutil.rmtree(output)
        shutil.move(str(staging), str(output))

    print(f"bios_prepare=result-ok output={output}")
    print(f"requirements={len(requirements)}")
    print(f"files={len(records)}")
    print(f"bytes={total_bytes}")
    print(f"missing_required={report['missing_required']}")
    print(f"missing_optional={report['missing_optional']}")
    for item in missing:
        kind = "optional" if item["optional"] else "required"
        print(f"missing={kind}:{item['destination']}:{','.join(item['consumers'])}")


if __name__ == "__main__":
    main()
