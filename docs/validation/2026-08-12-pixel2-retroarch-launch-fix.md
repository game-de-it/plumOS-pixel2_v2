# Pixel2 RetroArch Launch Fix

Date: 2026-08-12

## Symptom

NES entries were visible in the frontend, but selecting a game returned to the
frontend without launching RetroArch.

The frontend launch log showed:

```text
warning: missing safe hotkeyd: /mnt/plumos/bin/plumos-safe-hotkeyd
error: launch command failed with status 512
execute: failed
```

`retroarch-launch.log` was empty, so the failure happened before RetroArch
itself started.

## Root Cause

Pixel2 mounts the user ROM directory at both:

```text
/mnt/plumos-user/roms
/roms
```

The frontend/text UI resolved the selected ROM to:

```text
/mnt/plumos-user/roms/nes/Super Mario Bros..nes
```

but the launcher safety gate used `PLUMOS_ROM_ROOT=/roms` as the only allowed
root. The real ROM path was therefore rejected before the launcher wrote its
start log.

## Fix

Commit:

```text
ad3cc96 fix: launch Pixel2 RetroArch games from FE
```

Changes:

- `plumos-retroarch-launch` now accepts the canonical ROM root and the
  `PLUMOS_SDCARD_ROOT/roms` root as equivalent safe content roots.
- A Pixel2 ALSA default config is packaged as
  `factory-defaults/alsa/alsa.conf`.
- The launcher copies that default into `config/alsa/alsa.conf` when needed and
  exports `ALSA_CONFIG_PATH`.
- RetroArch component checksums now cover all `factory-defaults`, including the
  ALSA default.
- App-layer verification requires the ALSA default.

## Host Validation

```text
app_layer_scripts=result-ok
retroarch_component=result-ok output=/work/output/retroarch/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Generated app-layer source refs:

```text
output/app-layer/pixel2/plumos/manifest.json:  "source_ref": "ad3cc96",
output/app-layer/pixel2/plumos/components/retroarch/manifest.json:  "source_ref": "ad3cc96",
```

## Live Deployment

The generated full app-layer was staged on-device and verified before copying
managed files into `/mnt/plumos`.

Staged verification:

```text
stage_verify=ok
manifest.json:  "source_ref": "ad3cc96",
components/retroarch/manifest.json:  "source_ref": "ad3cc96",
```

Deployed verification:

```text
deployed_verify=ok
manifest.json:  "source_ref": "ad3cc96",
components/retroarch/manifest.json:  "source_ref": "ad3cc96",
```

Deployed hashes:

```text
ac4e729628306471e9b237aa9a509921a39bfab43954a9acd575c4103e11a5af  bin/plumos-retroarch-launch
dff961c51a67248bd2577a8fe676c29be912ed0c2cd4f23d5f87195b15551911  factory-defaults/alsa/alsa.conf
554f69284b609cae80f06b5bfce899c4b87a879576e5ada440166792ffea6297  manifest.json
a83586fe285e73811c65b96f09cd82de59db798526725acc12470ccef84ced5f  checksums.sha256
f63c06742c6c3e32438c84be09ea95e7cf64785955b98d98f6c7bbb5cb25d71f  components/retroarch/manifest.json
2063bf43665bc3a669b39f583f4348395ad0c78b98ca9defd1d1cab1fb5f41dd  components/retroarch/checksums.sha256
```

## Live Launch Validation

With the deployed launcher and the standard frontend environment
`PLUMOS_ROM_ROOT=/roms`, an ADB launch reached RetroArch and continued until the
test timeout:

```text
standard-env-after-deploy
rc=143
retroarch=result-start system=nes core=quicknes_libretro.so rom=Super Mario Bros..nes
```

The frontend was restarted after the timeout:

```text
start_rc=0
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event2
```

## Audio Notes

Before the ALSA default was added, RetroArch could not access
`/usr/share/alsa/alsa.conf`. With the packaged Pixel2 ALSA config, verbose
RetroArch validation reached PCM playback initialization:

```text
[INFO] [ALSA] Initialized PLAYBACK device "default".
[INFO] [Audio] Started synchronous audio driver.
```

Non-fatal control/MIDI warnings remain:

```text
ALSA lib control.c:1528:(snd_ctl_open_noupdate) Invalid CTL hw:0
ALSA lib seq.c:935:(snd_seq_open_noupdate) Unknown SEQ default
```

## Remaining Physical Check

Launch NES from the physical frontend and confirm:

- the selected game appears and returns to the frontend after exit;
- D-pad, ABXY, START/SELECT, shoulders, and hotkey exit behave correctly;
- real speaker/headphone audio is audible;
- volume up/down changes real game volume.
