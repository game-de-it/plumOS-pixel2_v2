# Pixel2 stock boot substrate image validation

Date: 2026-08-12
Scope: host-side image generation after adopting the stock boot substrate

> Superseded on 2026-08-19 for runtime-DTB policy only. The historical build
> below used the one-property VBUS addition; current images require the exact
> checksum-registered stock DTB.

## Result

The Pixel2 build now generates a plumOS `SYSTEM` and SD image that retain the
stock Pixel2 boot substrate:

- stock `Image`
- checksum-registered stock `rk3326s-gkd-pixel2.dtb` as the runtime-DTB input
- one gated `vbus-supply = <&otg_switch>` runtime-DTB addition
- stock kernel ABI `5.10.198`
- stock kernel-overlay modules and selected USB Wi-Fi firmware

plumOS ownership begins at the generated `SYSTEM` `/sbin/init`. The stock
`SYSTEM` SquashFS is still not copied into the image.

## Host checks

Static script checks passed:

```text
tests/test-system-rootfs-scripts.sh
tests/test-sd-image-scripts.sh
tests/test-stock-capture-scripts.sh
tests/test-app-layer-scripts.sh
tests/test-kernel-scripts.sh
```

`./scripts/build-system-rootfs.sh` completed with:

```text
kernel-runtime=result-ok release=5.10.198 firmware_source=stock-kernel-overlay boot_substrate=stock
system_rootfs=result-ok image=/work/output/system-rootfs/pixel2/payload/SYSTEM
```

`./scripts/build-sd-image.sh` completed with:

```text
system_rootfs=result-ok image=/tmp/plumos-pixel2-verify.xnKrXl/SYSTEM
app_layer_verify=result-ok root=/tmp/plumos-pixel2-verify.xnKrXl/app-layer
sd_image=result-ok image=/work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
```

## Generated artifact identity

| Artifact | SHA-256 |
| --- | --- |
| stock `Image` | `853eb041f1042a5f54ab66143cc8babb3942936f5c5209bc0c05d439ec3bd466` |
| stock `rk3326s-gkd-pixel2.dtb` | `a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a` |
| generated `SYSTEM` | `74d62b69d31ceabca20214ecec4432f2b4e4fae02daf5120181ced478ea099e1` |
| generated SD image | `cfc1f3d5d6478293dc3872699da85b5eec03ddb1367cc7bc40ebe26a5031b401` |

The SD image verifier extracts the boot FAT, compares `Image` directly against
`artifacts/vendor/pixel2-stock/boot/`, and compares the runtime DTB against the
strictly generated stock-plus-VBUS artifact. The DTB builder decompiles both
trees and rejects any difference besides the single DWC2 `vbus-supply` line.
The generated boot manifest contains
`boot_substrate=stock-pixel2`, and the generated System manifest contains
`"boot_substrate": "stock-pixel2"`.

The generated manifests record `source_ref=d18bc0e`.

## Remaining physical gates

- Confirm that stock initramfs accepts the generated plumOS `SYSTEM` and starts
  `/sbin/init`.
- Confirm `/proc/version` reports stock `5.10.198`.
- Confirm FE, ADB, emulator launch, audio and input still work.
- Confirm charging-state reboot returns to plumOS instead of stopping at the
  charging screen.
