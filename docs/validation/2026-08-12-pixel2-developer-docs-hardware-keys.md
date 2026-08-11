# Pixel2 Developer Docs and Hardware Keys

Date: 2026-08-12

## Goal

Use the MF and V90S developer documentation as the reference contract, identify
Pixel2 gaps that should be implemented immediately, and record the resulting
implementation and validation.

## Reference Docs

Reviewed MF and V90S developer documentation under:

```text
/Users/kroot/plumOS-MF/docs/developer
/Users/kroot/plumOS-V90S_v2-public/docs/developer
```

Common contracts relevant to Pixel2 now:

- developer-facing guide separate from validation records;
- app-layer deployment as an atomic metadata/checksum unit;
- one foreground display/input owner;
- runtime logs and physical-device evidence;
- global volume keys and SELECT-plus-volume brightness outside the FE process;
- safe power actions that stop runtime services before reboot/shutdown.

Large contracts intentionally left as TODO because Pixel2 does not yet have the
required runtime components:

- transactional Runtime/System updater;
- full baseline libretro catalog;
- PicoArch and standalone emulators;
- PortMaster, Pyxel, File Manager, and Music Player;
- final RK817 ALSA audio router and mixer backend.

## Implementation

Commit:

```text
380a006 feat: add Pixel2 global hardware key service
```

Added:

- `vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c`
- `bin/plumos-hardware-keys` in the frontend component
- `package/app-layer-pixel2/bin/plumos-hardware-keys-service`
- boot integration from `rootfs/pixel2/usr/lib/plumos/init.d/40-frontend`
- safe-shutdown stop hook for the hardware-key service
- app-layer verification gates for the daemon and service

The daemon opens:

```text
/dev/input/event2 name=pixel2_joypad
/dev/input/event1 name=gpio-keys
```

Behavior:

- `KEY_VOLUMEUP` / `KEY_VOLUMEDOWN`: logical volume runtime up/down
- `BTN_SELECT` held while pressing volume: display brightness runtime up/down
- completed changes are persisted after idle

Also added Pixel2 developer documentation:

```text
docs/developer/README.md
docs/developer/architecture.md
docs/developer/build.md
docs/developer/runtime.md
docs/developer/hardware-services.md
docs/developer/frontend-emulators.md
docs/developer/storage-and-updates.md
docs/developer/validation.md
```

## Host Validation

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh system-rootfs
```

Results:

```text
app_layer_scripts=result-ok
system_rootfs_scripts=result-ok
frontend_component=result-ok output=/work/output/frontend/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
system_rootfs=result-ok image=/work/output/system-rootfs/pixel2/payload/SYSTEM
```

Generated hashes:

```text
5496f152060dfaec2b891e3b80a59caf0784a66651879e626813c2bc7344d568  output/app-layer/pixel2/plumos/manifest.json
09d0040df82ab65e2edb0de9ddbcf64ea8750334da8896224eeea91a2a1309b4  output/app-layer/pixel2/plumos/checksums.sha256
46da1a010d1902b31cba6f072368ce26ecb6f20bdea1a91445d44aed585497e4  output/app-layer/pixel2/plumos/bin/plumos-hardware-keys
233ae1a209e6f211bdf12ba804b7709fdd5eacb894d9f87853aa13f5ac66696e  output/app-layer/pixel2/plumos/bin/plumos-hardware-keys-service
8b8a9f728b25d42d23fa6d4e289d043f0f4e46d2478caa0ef9389a81e2ae45df  output/system-rootfs/pixel2/payload/SYSTEM
fa9b4ba72ffac6164334555a82e392eeb267bfeced96dbaf281e57ea1a67e7ce  output/system-rootfs/pixel2/payload/SYSTEM.manifest
```

Both app-layer and SYSTEM manifests recorded:

```text
source_ref=380a006
```

## Live Deployment

App-layer managed files were deployed with the generated `checksums.sha256` and
verified on-device.

SYSTEM was deployed to `/boot/SYSTEM`; `/boot` was remounted writable only for
the copy and returned to read-only afterward.

Live after reboot:

```text
app_source_ref=380a006
system_source_ref=380a006
frontend_pid=649
hardware_keys=running
pid=646
```

Processes:

```text
/mnt/plumos/bin/plumos-hardware-keys
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event2
```

Hardware-key daemon log:

```text
hardware-keys: start owner=plumos device=pixel2
hardware-keys: opened=/dev/input/event2 name=pixel2_joypad
hardware-keys: opened=/dev/input/event1 name=gpio-keys
```

## Helper Round-Trip Validation

Direct helper calls used by the daemon were tested and restored to their
original values:

```text
initial
backend=pixel2-state-only
volume=8
backend=pixel2-pwm-backlight
brightness=10
backlight_raw=28

after_up
volume=9
brightness=11
backlight_raw=39

after_restore
volume=8
brightness=10
backlight_raw=28
```

Active settings after restore:

```json
{
  "version": 1,
  "volume": 8,
  "audio_output": "headphone",
  "brightness": 10,
  "lid_suspend_enabled": true
}
```

## Remaining Physical Check

The current Pixel2 rootfs does not include `getevent` or `sendevent`, so event
injection was not available from ADB. The daemon is running and has both input
devices open; the remaining acceptance check is physical:

- volume up/down changes logical volume;
- SELECT + volume up/down changes brightness;
- behavior continues while a frontend child or future standalone app owns the
  display.
