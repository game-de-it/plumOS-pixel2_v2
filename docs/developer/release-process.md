# Release Process

[日本語](release-process.ja.md)

This is the publication contract for a plumOS Pixel2 release. A successful
build is only a local candidate; it is not accepted until the exact image has
been written to a separate SD card and validated on a physical Pixel2.

## Preconditions

- Work from a clean, committed tree on the commit intended for the release tag.
- Keep the registered stock Pixel2 boot prefix and boot artifacts under
  `artifacts/`. They are local build inputs and are not added to the source
  archive.
- Confirm that ROMs, user BIOS files, saves, credentials, proprietary PICO-8
  runtime files, and mutable user state are absent from release inputs.
- Use a semantic version without the `v` prefix, for example `0.1.0`. The
  intended Git tag is recorded as `v0.1.0`.

## Build and Local Gate

Run the complete preparation command from the repository root:

```sh
./scripts/prepare-pixel2-release.sh --version 0.1.0
```

This command performs the following without publishing anything:

1. rebuilds every required Pixel2 component, using four concurrent workers for
   the independent libretro core catalog and parallel shared-App builds;
2. creates the strict app layer, stock-kernel SYSTEM, and compact SD image;
3. runs source contracts, identity/content checks, implementation audit,
   license audit, app/System/image checksum verification, and first-boot image
   tests;
4. assembles the SD image twice from the quiescent component tree after the
   strict gate, requires identical SHA-256 values, and validates the exact
   second image again before bundling;
5. compresses the image, archives the exact Git `HEAD`, writes release notes,
   provenance metadata, and `SHA256SUMS`; and
6. decompresses the `.img.xz` and verifies that its size and SHA-256 match the
   original image.

The output directory is:

```text
dist/plumOS-Pixel2-v0.1.0/
  plumOS-Pixel2-v0.1.0.img.xz
  plumOS-Pixel2-v0.1.0-source.tar.gz
  RELEASE_NOTES.md
  RELEASE_MANIFEST.json
  SHA256SUMS
```

Re-run the non-mutating bundle verification with:

```sh
./scripts/verify-pixel2-release-bundle.py dist/plumOS-Pixel2-v0.1.0
```

## Physical Release Acceptance

Write the exact decompressed image from the release directory to a separate SD
card. Record its SHA-256 and test at least:

- cold boot and the complete first-boot setup;
- final `PLUMOS_SYS` and `PLUMOS_USER` partition layout and preservation over a
  second boot;
- FE navigation, POWER menu, shutdown, reboot, sleep, and resume;
- USB Wi-Fi association, DHCP, reconnect after reinsert, SSH, and SFTP;
- representative RetroArch, PicoArch, and standalone game launch/menu/exit;
- display orientation/aspect, physical controls, audio, and volume controls;
- charging while booted and charge-mode behavior after shutdown.

Do not accept a candidate from build output, Raspberry Pi Imager completion, or
a single frontend boot alone. Record failures in `TODO.md` and a dated
`docs/validation/` entry before rebuilding.

## Publication Boundary

Only after physical acceptance, create tag `v0.1.0` at the exact
`source_ref` in `RELEASE_MANIFEST.json`, then upload all five files from the
release directory. Do not rebuild assets after tagging.

After GitHub has stored the assets, re-download and verify them rather than
trusting the upload request:

```sh
./scripts/verify-pixel2-release-bundle.py \
  dist/plumOS-Pixel2-v0.1.0 \
  --download-base \
  https://github.com/OWNER/REPOSITORY/releases/download/v0.1.0/
```

The GitHub release is complete only when this re-download check passes.
