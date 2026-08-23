#!/usr/bin/env python3
"""Assemble GitHub Release-ready plumOS Pixel2 assets without publishing them."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(*args: str, **kwargs: object) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(args, check=True, **kwargs)  # type: ignore[arg-type]


def git_value(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True
    ).strip()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_kv(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = value
    return result


def safe_version(value: str) -> str:
    value = value.removeprefix("v").strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?", value):
        raise SystemExit(f"invalid release version: {value!r}")
    return value


def ensure_clean(allow_dirty: bool) -> str:
    status = git_value("status", "--porcelain")
    if status and not allow_dirty:
        raise SystemExit("working tree is dirty; formal release bundles require committed source")
    return status


def find_image(version: str) -> Path:
    expected = ROOT / "output/image/pixel2" / f"plumOS-Pixel2-{version}.img"
    if expected.is_file():
        return expected
    raise SystemExit(
        f"release image not found: {expected}\n"
        f"run PLUMOS_PIXEL2_VERSION={version} ./scripts/docker-build.sh release-image"
    )


def make_source_archive(destination: Path, version: str) -> None:
    prefix = f"plumOS-Pixel2-v{version}/"
    with tempfile.TemporaryDirectory(prefix="plumos-pixel2-source-") as temp:
        tar_path = Path(temp) / "source.tar"
        with tar_path.open("wb") as output:
            run(
                "git", "-C", str(ROOT), "archive", "--format=tar",
                f"--prefix={prefix}", "HEAD", stdout=output,
            )
        with destination.open("wb") as output:
            run("gzip", "-n", "-9", "-c", str(tar_path), stdout=output)


def compress_image(image: Path, destination: Path) -> None:
    with destination.open("wb") as output:
        run("xz", "-T1", "-6", "-c", str(image), stdout=output)


def decompressed_sha256(archive: Path) -> str:
    process = subprocess.Popen(["xz", "-dc", str(archive)], stdout=subprocess.PIPE)
    assert process.stdout is not None
    digest = hashlib.sha256()
    for chunk in iter(lambda: process.stdout.read(1024 * 1024), b""):
        digest.update(chunk)
    if process.wait() != 0:
        raise SystemExit(f"failed to verify compressed image: {archive}")
    return digest.hexdigest()


def write_notes(path: Path, version: str, image_name: str) -> None:
    path.write_text(
        f"""# plumOS Pixel2 v{version}

## Asset

- `{image_name}`: complete SD-card image for GKD Pixel2.
- `plumOS-Pixel2-v{version}-source.tar.gz`: exact tagged source, recipes, and patches.
- `RELEASE_MANIFEST.json`: source/image provenance and immutable hashes.
- `SHA256SUMS`: checksums for every release asset.

## Install

1. Use a separate SD card of at least 16 GB.
2. Verify the downloaded image with `SHA256SUMS`.
3. Decompress `{image_name}` and write the resulting `.img` with Raspberry Pi Imager.
4. Insert the card into Pixel2 and keep power connected during first setup.
5. First boot expands `PLUMOS_SYS`, creates `PLUMOS_USER`, and may reboot once.

Do not write this image to the original stockOS card. Back up ROMs, BIOS files,
saves, and settings before replacing an existing plumOS card.

## Included

- plumOS frontend, six-system default grid, settings, Apps, and POWER menu;
- RetroArch and the Pixel2 libretro catalog, PicoArch, PCSX-ReARMed, DraStic,
  PPSSPP, OpenBOR, Pyxel, and user-supplied PICO-8 support;
- PortMaster, File Manager, Music Player, scraping, and network services;
- first-boot storage expansion, signed Runtime updates, and A/B System updates;
- Pixel2 display/input/audio/power integration and USB Wi-Fi support.

## Pixel2-Specific Limits

- Pixel2 has one USB port. USB Wi-Fi and charging cannot be used simultaneously
  without external hardware.
- ADB is not included. Remote maintenance uses USB Wi-Fi and SSH/SFTP.
- Saturn is not supported on the RK3326 performance target.
- ROMs, game BIOS files, proprietary PICO-8 files, and user content are not included.

## License

plumOS-authored material is MIT-licensed. Stock/vendor files and bundled
third-party components retain their own terms. See `LICENSE`, `NOTICE.md`, and
`THIRD_PARTY_NOTICES.md` in the tagged source and the app-layer license bundle.
""",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--image", type=Path)
    parser.add_argument("--app-root", type=Path, default=ROOT / "output/app-layer/pixel2/plumos")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--allow-dirty", action="store_true")
    parser.add_argument("--skip-image-verifier", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    version = safe_version(args.version)
    dirty = ensure_clean(args.allow_dirty)
    if args.skip_image_verifier and os.environ.get("PLUMOS_PIXEL2_RELEASE_TESTING") != "1":
        raise SystemExit("--skip-image-verifier is restricted to release-script tests")

    image = (args.image or find_image(version)).resolve()
    image_manifest = image.with_name("image.manifest")
    if not image.is_file() or not image_manifest.is_file():
        raise SystemExit("release image or image.manifest is missing")
    image_meta = parse_kv(image_manifest)
    if image_meta.get("file") != image.name:
        raise SystemExit("image.manifest does not describe the selected image")
    if image_meta.get("image_sha256") != sha256(image):
        raise SystemExit("image SHA-256 does not match image.manifest")

    if not args.skip_image_verifier:
        boot_prefix = ROOT / "artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin"
        run(str(ROOT / "scripts/verify-sd-image.sh"), str(image), str(boot_prefix))
    run(
        str(ROOT / "scripts/audit-pixel2-release-content.py"),
        "--app-root", str(args.app_root),
        "--image", str(image),
        "--image-manifest", str(image_manifest),
    )

    release_dir = (args.output_dir or ROOT / "dist" / f"plumOS-Pixel2-v{version}").resolve()
    dist_root = (ROOT / "dist").resolve()
    if release_dir == dist_root or dist_root not in release_dir.parents:
        raise SystemExit(f"release output must be a child of {dist_root}")
    if release_dir.exists():
        raise SystemExit(f"release output already exists: {release_dir}")
    release_dir.mkdir(parents=True)

    compressed = release_dir / f"plumOS-Pixel2-v{version}.img.xz"
    source_archive = release_dir / f"plumOS-Pixel2-v{version}-source.tar.gz"
    notes = release_dir / "RELEASE_NOTES.md"
    manifest = release_dir / "RELEASE_MANIFEST.json"
    checksums = release_dir / "SHA256SUMS"

    compress_image(image, compressed)
    if decompressed_sha256(compressed) != image_meta["image_sha256"]:
        raise SystemExit("compressed image round-trip SHA-256 mismatch")
    make_source_archive(source_archive, version)
    write_notes(notes, version, compressed.name)

    source_ref = git_value("rev-parse", "HEAD")
    source_epoch = int(git_value("show", "-s", "--format=%ct", "HEAD"))
    payload = {
        "format": "plumos-pixel2-github-release-v1",
        "device": "pixel2",
        "version": version,
        "intended_tag": f"v{version}",
        "source_ref": source_ref,
        "source_date_epoch": source_epoch,
        "git_dirty": bool(dirty),
        "image": {
            "uncompressed_file": image.name,
            "uncompressed_size": image.stat().st_size,
            "uncompressed_sha256": image_meta["image_sha256"],
            "compressed_file": compressed.name,
            "compressed_size": compressed.stat().st_size,
            "compressed_sha256": sha256(compressed),
        },
        "source_archive": {
            "file": source_archive.name,
            "size": source_archive.stat().st_size,
            "sha256": sha256(source_archive),
        },
        "boot_substrate": "stock-pixel2-5.10.198",
        "runtime_dtb_policy": "exact-stock",
        "user_filesystem": "created-on-first-boot",
    }
    manifest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    checksum_inputs = [compressed, source_archive, notes, manifest]
    checksums.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in checksum_inputs),
        encoding="utf-8",
    )
    verify_args = [str(ROOT / "scripts/verify-pixel2-release-bundle.py"), str(release_dir)]
    if dirty:
        verify_args.append("--allow-dirty")
    run(*verify_args)

    print(f"release_bundle=result-ok version={version}")
    print(f"release_dir={release_dir}")
    print(f"image_xz={compressed.name} sha256={sha256(compressed)}")
    print(f"source_archive={source_archive.name} sha256={sha256(source_archive)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
