#!/usr/bin/env python3
"""Build signed plumOS Pixel2 Runtime or System update archives."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tarfile
import tempfile
from typing import Any


FORMAT_VERSION = 1
DEVICE_ID = "pixel2"
ARCHITECTURE = "aarch64"
VENDOR_RUNTIME = "pixel2-rockchip-r1"

MANAGED_ROOTS = {
    "apps", "bin", "components", "cores", "emulator", "factory-defaults",
    "fonts", "frontend", "info", "lib", "licenses", "network", "picoarch",
    "scraper", "share", "ssh", "standalone", "themes",
}

MANAGED_ROOT_FILES = {
    "COMPAT_VENDOR",
    "RUNTIME_ABI",
    "VERSION",
    "checksums.sha256",
    "manifest.json",
}

MANAGED_CONFIG_PREFIXES = (
    "config/frontend/apps.json",
    "config/frontend/feature-contract.json",
    "config/frontend/menus.json",
    "config/frontend/scraper-sources.tsv",
    "config/frontend/systems.json",
    "config/frontend/themes.json",
    "config/standalone/",
    "config/system/input-map.env",
    "config/system/input-map.json",
)

PERSISTENT_PREFIXES = (
    "apps/portmaster/upstream/",
    "apps/portmaster/upstream.previous/",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_json(data: dict[str, Any]) -> bytes:
    return (json.dumps(data, sort_keys=True, separators=(",", ":")) + "\n").encode()


def safe_relative(path: Path, root: Path) -> str:
    relative = path.relative_to(root).as_posix()
    pure = PurePosixPath(relative)
    if pure.is_absolute() or not relative or ".." in pure.parts:
        raise ValueError(f"unsafe relative path: {relative}")
    return relative


def managed_runtime_path(relative: str) -> bool:
    if relative in MANAGED_ROOT_FILES:
        return True
    if any(relative == prefix or relative.startswith(prefix) for prefix in PERSISTENT_PREFIXES):
        return False
    if relative.startswith(MANAGED_CONFIG_PREFIXES):
        return True
    return relative.split("/", 1)[0] in MANAGED_ROOTS


def entry_for_path(path: Path, root: Path) -> dict[str, Any]:
    relative = safe_relative(path, root)
    mode = stat.S_IMODE(path.lstat().st_mode)
    if path.is_symlink():
        target = os.readlink(path)
        pure_target = PurePosixPath(target)
        if pure_target.is_absolute() or not target or ".." in pure_target.parts:
            raise ValueError(f"runtime symlink is incompatible with updater: {relative} -> {target}")
        return {
            "path": relative,
            "type": "symlink",
            "target": target,
            "sha256": hashlib.sha256(target.encode()).hexdigest(),
            "size": len(target.encode()),
            "mode": mode,
        }
    if path.is_file():
        return {
            "path": relative,
            "type": "file",
            "sha256": sha256_file(path),
            "size": path.stat().st_size,
            "mode": mode,
        }
    raise ValueError(f"unsupported payload type: {path}")


def runtime_inventory(root: Path) -> dict[str, tuple[Path, dict[str, Any]]]:
    inventory: dict[str, tuple[Path, dict[str, Any]]] = {}
    for path in sorted(root.rglob("*")):
        if not (path.is_file() or path.is_symlink()):
            continue
        relative = safe_relative(path, root)
        if managed_runtime_path(relative):
            inventory[relative] = (path, entry_for_path(path, root))
    return inventory


def checksum_inventory(path: Path) -> dict[str, str]:
    inventory: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise SystemExit(f"error: cannot read --base-checksums: {path}") from exc
    for line_number, line in enumerate(lines, 1):
        fields = line.split(maxsplit=1)
        if len(fields) != 2 or len(fields[0]) != 64 or any(
            character not in "0123456789abcdefABCDEF" for character in fields[0]
        ):
            raise SystemExit(
                f"error: malformed --base-checksums line {line_number}: {path}"
            )
        relative = fields[1]
        if relative.startswith("*"):
            relative = relative[1:]
        pure = PurePosixPath(relative)
        if (
            not relative
            or pure.is_absolute()
            or ".." in pure.parts
            or str(pure) != relative
        ):
            raise SystemExit(
                f"error: unsafe --base-checksums path on line {line_number}: {relative}"
            )
        if managed_runtime_path(relative):
            inventory[relative] = fields[0].lower()
    return inventory


def read_required_line(path: Path, label: str) -> str:
    try:
        value = path.read_text(encoding="utf-8").strip()
    except OSError as exc:
        raise SystemExit(f"error: {label} is missing: {path}") from exc
    if not value:
        raise SystemExit(f"error: {label} is empty: {path}")
    return value


def add_bytes(archive: tarfile.TarFile, name: str, payload: bytes, mode: int = 0o644) -> None:
    info = tarfile.TarInfo(name)
    info.size = len(payload)
    info.mode = mode
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "root"
    archive.addfile(info, io.BytesIO(payload))


def add_payload_path(archive: tarfile.TarFile, source: Path, entry: dict[str, Any]) -> None:
    name = f"payload/{entry['path']}"
    info = tarfile.TarInfo(name)
    info.mode = int(entry["mode"])
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "root"
    if entry["type"] == "symlink":
        info.type = tarfile.SYMTYPE
        info.linkname = str(entry["target"])
        archive.addfile(info)
        return
    info.size = int(entry["size"])
    with source.open("rb") as handle:
        archive.addfile(info, handle)


def sign_manifest(manifest_bytes: bytes, key: Path | None, unsigned: bool) -> bytes:
    if unsigned:
        return b"UNSIGNED-DEVELOPMENT-PACKAGE\n"
    if key is None or not key.is_file():
        raise SystemExit("error: --signing-key is required unless --unsigned is used")
    with tempfile.TemporaryDirectory(prefix="plumos-update-sign-") as temp:
        manifest_path = Path(temp) / "manifest.json"
        signature_path = Path(temp) / "manifest.sig"
        manifest_path.write_bytes(manifest_bytes)
        subprocess.run(
            [
                "openssl",
                "pkeyutl",
                "-sign",
                "-rawin",
                "-inkey",
                str(key),
                "-in",
                str(manifest_path),
                "-out",
                str(signature_path),
            ],
            check=True,
        )
        return signature_path.read_bytes()


def build_runtime(args: argparse.Namespace) -> tuple[dict[str, Any], list[tuple[Path, dict[str, Any]]]]:
    root = args.input.resolve()
    if not root.is_dir():
        raise SystemExit(f"error: runtime input is not a directory: {root}")
    if read_required_line(root / "VERSION", "runtime VERSION") != args.version:
        raise SystemExit("error: --version does not match runtime VERSION")
    if read_required_line(root / "COMPAT_VENDOR", "runtime vendor ID") != args.vendor_runtime:
        raise SystemExit("error: --vendor-runtime does not match runtime COMPAT_VENDOR")
    if read_required_line(root / "RUNTIME_ABI", "runtime ABI") != args.runtime_abi:
        raise SystemExit("error: --runtime-abi does not match runtime RUNTIME_ABI")
    inventory = runtime_inventory(root)
    base_inventory: dict[str, tuple[Path, dict[str, Any]]] = {}
    base_hashes: dict[str, str] = {}
    if args.base_dir:
        base_inventory = runtime_inventory(args.base_dir.resolve())
    elif args.base_checksums:
        base_hashes = checksum_inventory(args.base_checksums.resolve())
    changed: list[tuple[Path, dict[str, Any]]] = []
    for relative, item in inventory.items():
        base = base_inventory.get(relative)
        if base and base[1] == item[1]:
            continue
        if item[1]["type"] == "file" and base_hashes.get(relative) == item[1]["sha256"]:
            continue
        changed.append(item)
    base_paths = set(base_inventory) | set(base_hashes)
    deleted = sorted(base_paths - set(inventory))
    payload_size = sum(int(entry["size"]) for _, entry in changed)
    manifest: dict[str, Any] = {
        "format": FORMAT_VERSION,
        "package_type": "runtime",
        "device_id": DEVICE_ID,
        "architecture": ARCHITECTURE,
        "vendor_runtime": args.vendor_runtime,
        "source_version": args.base_version,
        "version": args.version,
        "system_abi": args.system_abi,
        "runtime_abi": args.runtime_abi,
        "payload_uncompressed_bytes": payload_size,
        "full_payload": not bool(args.base_dir or args.base_checksums),
        "files": [entry for _, entry in changed],
        "delete": deleted,
    }
    return manifest, changed


def build_system(args: argparse.Namespace) -> tuple[dict[str, Any], list[tuple[Path, dict[str, Any]]]]:
    source = args.input.resolve()
    if not source.is_file():
        raise SystemExit(f"error: system SquashFS is missing: {source}")
    source_ref = args.version
    source_date_epoch = 0
    if os.environ.get("PLUMOS_UPDATE_SKIP_EMBEDDED_CHECK") != "1":
        if not shutil.which("unsquashfs"):
            raise SystemExit(
                "error: unsquashfs is required to verify the embedded System "
                "version and ABI; use scripts/docker-build.sh update-package"
            )
        for embedded_path, expected, label in (
            ("etc/plumos-system-version", args.version, "System version"),
            ("etc/plumos-system-abi", args.system_abi, "System ABI"),
        ):
            result = subprocess.run(
                ["unsquashfs", "-cat", str(source), embedded_path],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                check=False,
            )
            if result.returncode != 0 or result.stdout.strip() != expected:
                raise SystemExit(f"error: {label} does not match embedded {embedded_path}")
        result = subprocess.run(
            ["unsquashfs", "-cat", str(source), "usr/lib/plumos/system-manifest.json"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        try:
            embedded_manifest = json.loads(result.stdout)
            source_ref = str(embedded_manifest["source_ref"])
            source_date_epoch = int(embedded_manifest["source_date_epoch"])
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
            raise SystemExit("error: embedded System source metadata is invalid") from exc
        if result.returncode != 0 or not source_ref or "\n" in source_ref:
            raise SystemExit("error: embedded System source metadata is invalid")
    entry = {
        "path": "system.squashfs",
        "type": "file",
        "sha256": sha256_file(source),
        "size": source.stat().st_size,
        "mode": 0o644,
    }
    manifest: dict[str, Any] = {
        "format": FORMAT_VERSION,
        "package_type": "system",
        "device_id": DEVICE_ID,
        "architecture": ARCHITECTURE,
        "vendor_runtime": args.vendor_runtime,
        "source_version": args.base_version,
        "version": args.version,
        "system_abi": args.system_abi,
        "runtime_abi": args.runtime_abi,
        "source_ref": source_ref,
        "source_date_epoch": source_date_epoch,
        "kernel_update": False,
        "payload_uncompressed_bytes": int(entry["size"]),
        "files": [entry],
        "delete": [],
    }
    return manifest, [(source, entry)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--type", choices=("runtime", "system"), required=True)
    parser.add_argument("--input", type=Path, required=True)
    base_group = parser.add_mutually_exclusive_group()
    base_group.add_argument("--base-dir", type=Path)
    base_group.add_argument("--base-checksums", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-version", default="*")
    parser.add_argument("--system-abi", default="plumos-pixel2-v1")
    parser.add_argument("--runtime-abi", default="plumos-pixel2-app-layer-v1")
    parser.add_argument("--vendor-runtime", default=VENDOR_RUNTIME)
    parser.add_argument(
        "--signing-key",
        type=Path,
        default=Path(os.environ.get(
            "PLUMOS_PIXEL2_UPDATE_SIGNING_KEY",
            "artifacts/update-signing/plumos-pixel2-ed25519-private.pem",
        )),
    )
    parser.add_argument("--unsigned", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    args = parser.parse_args()

    if args.type == "runtime":
        manifest, payload = build_runtime(args)
    else:
        if args.base_dir or args.base_checksums:
            parser.error("--base-dir/--base-checksums are valid only for runtime packages")
        manifest, payload = build_system(args)

    manifest_bytes = canonical_json(manifest)
    signature = sign_manifest(manifest_bytes, args.signing_key, args.unsigned)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / f"plumos-pixel2-{args.type}-{args.version}.tar.gz"
    with tarfile.open(output, "w:gz", format=tarfile.PAX_FORMAT) as archive:
        add_bytes(archive, "META/manifest.json", manifest_bytes)
        add_bytes(archive, "META/manifest.sig", signature)
        for source, entry in payload:
            add_payload_path(archive, source, entry)
    digest = sha256_file(output)
    checksum = output.with_suffix(output.suffix + ".sha256")
    checksum.write_text(f"{digest}  {output.name}\n", encoding="ascii")
    print(f"created: {output}")
    print(f"sha256: {digest}")
    print(f"type: {args.type}")
    print(f"version: {args.version}")
    print(f"payload_files: {len(payload)}")
    print(f"deleted_files: {len(manifest['delete'])}")


if __name__ == "__main__":
    main()
