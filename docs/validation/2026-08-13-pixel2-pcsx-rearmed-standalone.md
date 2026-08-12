# Pixel2 PCSX-ReARMed standalone validation

Date: 2026-08-13
Implementation commit: `8b54b97`

## Build contract

The Pixel2 standalone component builds PCSX-ReARMed r26l from pinned source,
with pinned libpicofe and sdl12-compat inputs. The target is AArch64 Cortex-A35
with NEON, ari64 dynarec, the built-in threaded GPU, a Pixel2 fbdev presenter,
and the `plumos_output` ALSA route.

The presenter converts the emulator's logical 640x480 output to the physical
480x640 framebuffer with counter-clockwise rotation and 4:3 geometry. Physical
Pixel2 buttons are mapped in the emulator integration, including D-pad, ABXY,
START/SELECT, L/R, L2/R2, and FUNCTION for the emulator menu. Mutable config,
memory cards, save states, cheats, patches, and screenshots live outside the
managed package.

## Reproducible host result

The unfiltered standalone build completed with PCSX-ReARMed, PPSSPP, DraStic,
and OpenBOR marked `built`. Repeating the PCSX filtered build produced the same
binary hashes:

```text
5b6ba9247e40a874854542e48584b602b23e08af90fa2cee08d79c114067839b  pcsx
5d246be54ba4b1464c8241a9d7682fa55ebccfb68cdb53c0e367e330d7ca61d3  libSDL-1.2.so.0
```

The clean versioned app-layer was generated with:

```sh
PLUMOS_PIXEL2_VERSION=0.1.0-dev-8b54b97 \
  ./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh audit
./scripts/validate-romset-routes.py \
  --app-root output/app-layer/pixel2/plumos \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown output/validation/pixel2-romset-routes-8b54b97.md \
  --json output/validation/pixel2-romset-routes-8b54b97.json
```

Results:

```text
app_layer_verify=result-ok
app_layer=result-ok strict=1
source_ref=8b54b97
standalone=4 built / 6 pending
release_blockers=1
systems_with_rom=30
route_ok=30
route_pending_binary=0
```

The remaining automated release blocker is the published alternate
`standalone:yabasanshiro`; the default Saturn route remains the built
`retroarch:yabasanshiro` core.

## Signed Runtime deployment

The installed `e47ce97` checksum inventory was pulled read-only and used as the
delta baseline. The signed package contained 29 managed files, no deletions,
and included the PCSX binary, standalone component metadata, root manifest,
root checksum, and version in one transaction:

```text
package=plumos-pixel2-runtime-0.1.0-dev-8b54b97.tar.gz
sha256=251c7d38cf0b8526ff9ae1b1e18aee292ac69eed731c1e37b29fdb5e36ad4c7a
compressed_bytes=11942243
payload_uncompressed_bytes=28647803
source_version=0.1.0-dev-e47ce97
version=0.1.0-dev-8b54b97
```

Host public-key verification, device readback SHA-256, and on-device updater
inspection all passed. Immediately after reboot the transaction was
`pending_health` and FE readiness was absent. After the first FE render it
became `runtime_healthy`; the pending marker was removed, exactly one FE process
was running, and all 3468 root app-layer checksums passed. The two PCSX runtime
hashes on the device matched the reproducible host values above.

## Representative ROM staging

The smallest single-image PSX sample in the supplied ROM set was copied to the
user volume without placing it in the managed app-layer:

```text
source=/Volumes/public-1/02/motoki/emu/ROM/rom2/psx/chroQW.img
target=/mnt/plumos-user/roms/PSX/chroQW.img
size=80290224
sha256=b7ac35f7827557dc967e4f08082d79da9a17c2cf28aafcf48493a5d1ca573f77
```

The host and device hashes matched. The Pixel2 library scanner found one PSX
entry, and an explicit `standalone:pcsx_rearmed` launch plan reported
`can_execute=yes`.

## Physical acceptance remaining

The following still requires observation while PCSX owns the foreground:

1. game image is correctly rotated and retains 4:3 geometry;
2. D-pad, ABXY, START/SELECT, L/R, L2/R2, and FUNCTION menu mapping;
3. 48 kHz `plumos_output` audio and global volume keys;
4. emulator exit, clean FE display/input reacquisition, and second launch;
5. memory-card/save-state persistence across FE return and reboot.
