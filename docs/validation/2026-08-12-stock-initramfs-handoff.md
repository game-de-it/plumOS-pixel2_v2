# Pixel2 stock initramfs handoff repair

Date: 2026-08-12
Scope: Pixel-logo hang after switching release image generation to the stock
boot substrate

## Symptom

The device stopped at the Pixel boot logo when booting the stock-substrate
plumOS image.

## Root cause

The stock `Image` contains a gzip-compressed initramfs at offset `0xdc09ac`.
After extraction, its `/init` showed a LibreELEC-style fixed handoff:

```text
exec /usr/bin/busybox switch_root /sysroot /usr/lib/systemd/systemd $INIT_ARGS $INIT_UNIT
```

The generated plumOS `SYSTEM` provided `/sbin/init`, but did not provide
`/usr/lib/systemd/systemd`. The stock initramfs therefore could mount
`/flash/SYSTEM` but failed its final handoff check:

```text
[ -f "/sysroot/usr/lib/systemd/systemd" ] || error "final_check" "Could not find systemd!"
```

## Fix

`rootfs/pixel2/usr/lib/systemd/systemd` is now a minimal compatibility
entrypoint that execs plumOS `/sbin/init`. The generated SYSTEM also includes
`/flash` and `/storage`, because the stock initramfs moves those mounts into
the new root before `switch_root`.

The plumOS init now reuses the stock-provided `/flash` and `/storage` mounts
when present, binding them to `/boot` and `/mnt/plumos` instead of assuming a
plumOS-owned initramfs performed the mount sequence.

## Host validation

Static checks passed:

```text
tests/test-system-rootfs-scripts.sh
tests/test-sd-image-scripts.sh
tests/test-stock-capture-scripts.sh
tests/test-app-layer-scripts.sh
```

`./scripts/build-system-rootfs.sh && ./scripts/build-sd-image.sh` completed
with:

```text
system_rootfs=result-ok image=/tmp/plumos-pixel2-verify.Bncs7E/SYSTEM
app_layer_verify=result-ok root=/tmp/plumos-pixel2-verify.Bncs7E/app-layer
sd_image=result-ok image=/work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
```

## Generated artifact identity

| Artifact | SHA-256 |
| --- | --- |
| generated `SYSTEM` | `b420f7f10db110dd2c5f718d0fc4ba3de708109f515b264f68ccbf070ac9e365` |
| generated SD image | `12b9948871f1172afeda007474247cd1896e268e527e8b90b212a523174339f8` |

The generated manifests record `source_ref=f1c58c1`.

## Remaining physical gate

Boot this image on Pixel2 and confirm the stock initramfs reaches the shim,
then plumOS `/sbin/init`, ADB and FE.
