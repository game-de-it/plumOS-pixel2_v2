#!/usr/bin/env python3
"""Verify local or re-downloaded plumOS Pixel2 GitHub release assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_checksums(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        digest, name = line.split(None, 1)
        name = name.strip()
        if Path(name).name != name or len(digest) != 64:
            raise SystemExit(f"unsafe SHA256SUMS entry: {line}")
        result[name] = digest
    return result


def verify_dir(root: Path, allow_dirty: bool = False) -> None:
    checksums = load_checksums(root / "SHA256SUMS")
    required = {"RELEASE_NOTES.md", "RELEASE_MANIFEST.json"}
    if not required <= checksums.keys():
        raise SystemExit("release metadata is missing from SHA256SUMS")
    for name, expected in checksums.items():
        path = root / name
        if not path.is_file() or sha256(path) != expected:
            raise SystemExit(f"release checksum mismatch: {name}")

    manifest = json.loads((root / "RELEASE_MANIFEST.json").read_text(encoding="utf-8"))
    if manifest.get("device") != "pixel2":
        raise SystemExit("release manifest is not for Pixel2")
    if manifest.get("git_dirty") is not False and not allow_dirty:
        raise SystemExit("release manifest is not a clean Pixel2 release")
    version = manifest.get("version")
    if not isinstance(version, str) or manifest.get("intended_tag") != f"v{version}":
        raise SystemExit("release version/tag mismatch")
    source_ref = manifest.get("source_ref")
    if not isinstance(source_ref, str) or not re.fullmatch(r"[0-9a-f]{40}", source_ref):
        raise SystemExit("release source_ref is not a full Git commit")
    image = manifest["image"]
    expected_image = f"plumOS-Pixel2-v{version}.img.xz"
    if image.get("compressed_file") != expected_image:
        raise SystemExit("release image filename mismatch")
    compressed = root / image["compressed_file"]
    if sha256(compressed) != image["compressed_sha256"]:
        raise SystemExit("compressed image manifest mismatch")
    if compressed.stat().st_size != image["compressed_size"]:
        raise SystemExit("compressed image size mismatch")

    process = subprocess.Popen(["xz", "-dc", str(compressed)], stdout=subprocess.PIPE)
    assert process.stdout is not None
    digest = hashlib.sha256()
    size = 0
    for chunk in iter(lambda: process.stdout.read(1024 * 1024), b""):
        size += len(chunk)
        digest.update(chunk)
    if process.wait() != 0:
        raise SystemExit("compressed image integrity test failed")
    if digest.hexdigest() != image["uncompressed_sha256"] or size != image["uncompressed_size"]:
        raise SystemExit("compressed image round-trip mismatch")

    source = root / manifest["source_archive"]["file"]
    expected_source = f"plumOS-Pixel2-v{version}-source.tar.gz"
    if source.name != expected_source:
        raise SystemExit("release source archive filename mismatch")
    if sha256(source) != manifest["source_archive"]["sha256"]:
        raise SystemExit("source archive manifest mismatch")
    members = subprocess.check_output(["tar", "-tzf", str(source)], text=True).splitlines()
    prefix = f"plumOS-Pixel2-v{version}/"
    if not members or any(not member.startswith(prefix) for member in members):
        raise SystemExit("source archive top-level directory mismatch")


def download_assets(base_url: str, names: set[str], destination: Path) -> None:
    destination.mkdir(parents=True)
    for name in sorted(names):
        url = urllib.parse.urljoin(base_url.rstrip("/") + "/", name)
        urllib.request.urlretrieve(url, destination / name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("release_dir", type=Path)
    parser.add_argument(
        "--download-base",
        help="Download every checksummed asset from this GitHub release URL before verifying",
    )
    parser.add_argument("--allow-dirty", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    source = args.release_dir.resolve()
    if not source.is_dir():
        raise SystemExit(f"release directory not found: {source}")

    if args.download_base:
        checksums_source = source / "SHA256SUMS"
        names = set(load_checksums(checksums_source)) | {"SHA256SUMS"}
        with tempfile.TemporaryDirectory(prefix="plumos-pixel2-redownload-") as temp:
            target = Path(temp)
            download_assets(args.download_base, names, target)
            verify_dir(target, args.allow_dirty)
            print(f"release_redownload=result-ok assets={len(names)}")
    else:
        verify_dir(source, args.allow_dirty)
        print("release_bundle_verify=result-ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
