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


def seven_zip() -> str:
    command = shutil.which("7zz") or shutil.which("7z")
    if command is None:
        raise SystemExit("7-Zip is required; install 7zz or 7z")
    return command


def compress_image(image: Path, destination: Path) -> None:
    command = seven_zip()
    run(
        command,
        "a",
        "-t7z",
        "-mx=7",
        "-mmt=on",
        "-mtm=off",
        "-mta=off",
        "-mtr=off",
        str(destination),
        image.name,
        cwd=image.parent,
    )
    run(command, "t", str(destination), stdout=subprocess.DEVNULL)


def decompressed_sha256(archive: Path, member: str) -> str:
    process = subprocess.Popen(
        [seven_zip(), "x", "-so", str(archive), member],
        stdout=subprocess.PIPE,
    )
    assert process.stdout is not None
    digest = hashlib.sha256()
    for chunk in iter(lambda: process.stdout.read(1024 * 1024), b""):
        digest.update(chunk)
    if process.wait() != 0:
        raise SystemExit(f"failed to verify compressed image: {archive}")
    return digest.hexdigest()


def write_notes(
    path: Path,
    version: str,
    archive_name: str,
    archive_sha256: str,
    image_name: str,
    image_sha256: str,
) -> None:
    path.write_text(
        f"""# plumOS Pixel2 v{version}

## English

plumOS Pixel2 v{version} is a GKD Pixel2-specific SD-card Linux distribution.
It retains the stock Pixel2 Linux 5.10.198 boot substrate while plumOS manages
the frontend, emulators, applications, settings, storage, networking, updates,
power behavior, and user data.

The release image passed the repository's strict host gates and was booted and
read back on real GKD Pixel2 hardware.

### Asset

- `{archive_name}`: compressed SD-card image for GKD Pixel2.
- `plumOS-Pixel2-v{version}-source.tar.gz`: exact release source, recipes, and patches.
- `RELEASE_MANIFEST.json`: source/image provenance and immutable hashes.
- `SHA256SUMS`: checksums for every release asset.

### Install

1. Use a separate SD card of at least 16 GB.
2. Verify the downloaded files with `SHA256SUMS`.
3. Extract `{image_name}` from `{archive_name}`.
4. Write the `.img` with Raspberry Pi Imager, balenaEtcher, or an equivalent writer.
5. Insert the card into Pixel2 and keep power stable during first setup.
6. First boot expands `PLUMOS_SYS`, creates `PLUMOS_USER`, and may reboot once.

Writing the image erases the selected card. Do not write it to the original
stockOS card. Back up ROMs, BIOS files, saves, and settings before replacing an
existing plumOS card. This image is only for GKD Pixel2.

The public initial SSH login is `root` / `plumos`. Change it after installation
if network services are enabled.

### Included

- plumOS frontend, six-system default grid, settings, Apps, and POWER menu;
- RetroArch and the Pixel2 libretro catalog, PicoArch, PCSX-ReARMed, DraStic,
  PPSSPP, OpenBOR, Pyxel, and user-supplied PICO-8 support;
- PortMaster, File Manager, Music Player, scraping, and network services;
- first-boot storage expansion, signed Runtime updates, and A/B System updates;
- Pixel2 display/input/audio/power integration and USB Wi-Fi support.

### Pixel2-Specific Limits

- Pixel2 has one USB port. USB Wi-Fi and charging cannot be used simultaneously
  without external hardware.
- ADB is not included. Remote maintenance uses USB Wi-Fi and SSH/SFTP.
- Saturn is not supported on the RK3326 performance target.
- ROMs, game BIOS files, proprietary PICO-8 files, and user content are not included.

### Checksums

```text
{archive_sha256}  {archive_name}
{image_sha256}  {image_name}
```

### Build From Source

From a clean checkout with the registered local stock boot inputs:

```sh
./scripts/prepare-pixel2-release.sh --version {version}
```

Docker, internet access for pinned upstream sources, 7-Zip, and sufficient free
working space are required.

### License

plumOS-authored material is MIT-licensed. Stock/vendor files and bundled
third-party components retain their own terms. See `LICENSE`, `NOTICE.md`, and
`THIRD_PARTY_NOTICES.md` in the release source and app-layer license bundle.

---

## 日本語

plumOS Pixel2 v{version}は、GKD Pixel2専用のSDカードLinux
ディストリビューションです。Pixel2 stock Linux 5.10.198のboot基盤を維持しながら、
frontend、emulator、Apps、設定、storage、network、update、電源動作、user dataを
plumOS側で管理します。

このrelease imageはrepositoryのstrict host gateに合格し、GKD Pixel2実機で起動・
readback確認済みです。

### 配布ファイル

- `{archive_name}`: GKD Pixel2用の圧縮SDカードimage
- `plumOS-Pixel2-v{version}-source.tar.gz`: 対応するsource、recipe、patch
- `RELEASE_MANIFEST.json`: source/imageのprovenanceと固定hash
- `SHA256SUMS`: 全release assetのchecksum

### インストール

1. 16 GB以上の別のSDカードを用意します。
2. `SHA256SUMS`でdownloadしたfileを確認します。
3. `{archive_name}`から`{image_name}`を展開します。
4. Raspberry Pi Imager、balenaEtcherなどで`.img`をSDカードへ書き込みます。
5. Pixel2へ挿入し、初回setup中は安定した電源を維持します。
6. 初回起動時に`PLUMOS_SYS`が拡張され、`PLUMOS_USER`が作成されます。必要な場合は
   1回だけ自動再起動します。

書き込み先SDカードは消去されます。元のstockOSカードへ書き込まないでください。
既存plumOSカードを置き換える場合は、ROM、BIOS、save、設定をbackupしてください。
このimageはGKD Pixel2専用です。

公開初期SSH loginは`root` / `plumos`です。network serviceを有効にする場合は、
install後に変更してください。

### 主な機能

- 6-system標準grid、設定、Apps、POWER menuを備えたplumOS frontend
- RetroArchとPixel2 libretro catalog、PicoArch、PCSX-ReARMed、DraStic、
  PPSSPP、OpenBOR、Pyxel、利用者が用意するPICO-8への対応
- PortMaster、File Manager、Music Player、scraping、network service
- 初回storage拡張、署名Runtime update、A/B System update
- Pixel2向けdisplay、input、audio、power、USB Wi-Fi統合

### Pixel2固有の制限

- Pixel2のUSB portは1つです。外部hardwareなしではUSB Wi-Fiと充電を同時利用できません。
- ADBは含みません。remote保守にはUSB Wi-FiとSSH/SFTPを使います。
- RK3326の性能要件によりSaturnは対応対象外です。
- ROM、game BIOS、proprietary PICO-8 file、user contentは含みません。

### チェックサム

```text
{archive_sha256}  {archive_name}
{image_sha256}  {image_name}
```

### sourceからのbuild

登録済みのlocal stock boot入力を用意したclean checkoutで実行します。

```sh
./scripts/prepare-pixel2-release.sh --version {version}
```

Docker、pin済みupstream source取得用のinternet接続、7-Zip、十分な作業空き容量が
必要です。

### ライセンス

plumOS作成部分はMIT licenseです。stock/vendor fileと同梱third-party componentには
それぞれの条件が適用されます。release sourceとapp-layer license bundleの`LICENSE`、
`NOTICE.md`、`THIRD_PARTY_NOTICES.md`を確認してください。
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

    compressed = release_dir / f"plumOS-Pixel2-v{version}-sd-image.7z"
    source_archive = release_dir / f"plumOS-Pixel2-v{version}-source.tar.gz"
    notes = release_dir / "RELEASE_NOTES.md"
    manifest = release_dir / "RELEASE_MANIFEST.json"
    checksums = release_dir / "SHA256SUMS"

    compress_image(image, compressed)
    if decompressed_sha256(compressed, image.name) != image_meta["image_sha256"]:
        raise SystemExit("compressed image round-trip SHA-256 mismatch")
    make_source_archive(source_archive, version)
    write_notes(
        notes,
        version,
        compressed.name,
        sha256(compressed),
        image.name,
        image_meta["image_sha256"],
    )

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
            "archive_format": "7z",
            "archive_member": image.name,
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
    print(f"image_7z={compressed.name} sha256={sha256(compressed)}")
    print(f"source_archive={source_archive.name} sha256={sha256(source_archive)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
