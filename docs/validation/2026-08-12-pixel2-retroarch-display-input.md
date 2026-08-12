# Pixel2 RetroArch Display and Input Validation

Date: 2026-08-12

## Symptom

After NES launch was fixed, the game started but:

- physical controls did not work in RetroArch;
- the game display was physically rotated;
- a host-side framebuffer capture did not show the game layer.

## Device Screenshot Attempt

With RetroArch running, `/dev/fb0` was captured from the live device:

```text
/sys/class/graphics/fb0/virtual_size = 480,640
/sys/class/graphics/fb0/bits_per_pixel = 32
/sys/class/graphics/fb0/stride = 1920
```

Output:

```text
output/live/2026-08-12-retroarch-rotation-input/fb0-capture.png
```

The capture showed a stale framebuffer/boot layer rather than RetroArch's game
plane. RetroArch uses the DRM plane directly, so `/dev/fb0` is not a reliable
capture source for game output on Pixel2.

The fb0 capture also showed stale `Powered by ROCKNIX` branding. A source scan
did not find that string in the current app-layer/rootfs sources, so this is
tracked as a stale framebuffer/boot-splash cleanup item rather than a current
app-layer asset.

## RetroArch Screenshot Attempt

RetroArch internal screenshots were captured with:

```text
--max-frames=90 --max-frames-ss --max-frames-ss-path=...
```

Output:

```text
output/live/2026-08-12-retroarch-rotation-input/ra-rotation/contact-sheet.png
```

Those screenshots capture the core framebuffer and do not reflect the final DRM
plane rotation on the LCD. They confirmed content rendering works, but physical
rotation must still be validated on the device screen.

## Root Cause: Input

The live input device is:

```text
/dev/input/event2: pixel2_joypad
```

The packaged RetroArch autoconfig was still named for and matching:

```text
gkd-pixel2-joypad
```

RetroArch therefore did not bind the Pixel2 joypad config.

## Fix

Commit:

```text
0d80a61 fix: align Pixel2 RetroArch display and input
```

Changes:

- renamed the RetroArch joypad autoconfig to the live `pixel2_joypad` device;
- packaged both `udev` and `linuxraw` autoconfig variants;
- switched Pixel2 RetroArch joypad runtime to `linuxraw`, which was verified to
  bind `pixel2_joypad`;
- changed Pixel2 RetroArch runtime rotation from `video_rotation=3` to
  `video_rotation=0`;
- set `config_save_on_exit=false` so temporary validation settings do not
  overwrite the mutable RetroArch config;
- launcher now appends required Pixel2 runtime settings every launch, so stale
  mutable RetroArch config cannot disable input/audio/video settings.

Runtime append after deployment:

```text
audio_driver = "alsa"
audio_device = "default"
video_driver = "drm"
video_rotation = "0"
input_driver = "udev"
input_joypad_driver = "linuxraw"
joypad_autoconfig_dir = "/mnt/plumos/factory-defaults/retroarch/autoconfig"
config_save_on_exit = "false"
savefile_directory = "/mnt/plumos/saves/nes"
savestate_directory = "/mnt/plumos/states/nes"
system_directory = "/mnt/plumos-user/bios"
```

## Host Validation

```text
app_layer_scripts=result-ok
retroarch_component=result-ok output=/work/output/retroarch/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Generated source refs:

```text
output/app-layer/pixel2/plumos/manifest.json:  "source_ref": "0d80a61",
output/app-layer/pixel2/plumos/components/retroarch/manifest.json:  "source_ref": "0d80a61",
```

## Live Deployment

The generated app-layer was staged and verified on-device, then managed files
and metadata were copied into `/mnt/plumos` as one checksum-complete unit.

```text
stage_verify=ok
deployed_verify=ok
/mnt/plumos/manifest.json:  "source_ref": "0d80a61",
/mnt/plumos/components/retroarch/manifest.json:  "source_ref": "0d80a61",
```

Deployed key hashes:

```text
9bda29a9628665fcac7036e27bde0b2c83fbb0b91c845bf334e7d937a668f2a8  bin/plumos-retroarch-launch
f1a10960d04f52882d93124bb3b6d7655bb614f3381b1cdd7fd84b9c06e4a654  factory-defaults/retroarch/retroarch.cfg
aa7a23e3810f625cf7adc275cd8acfea85b9cfc8c72f7854ee594b8faf3c4610  factory-defaults/retroarch/autoconfig/linuxraw/pixel2_joypad.cfg
ec1d946023e28888aeebb7c1a1e74c20f0549ed38f1ec1b56f58dee368afcd7a  factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg
```

## Live Runtime Validation

Direct verbose validation with the deployed runtime settings:

```text
[INFO] [LinuxRaw] Device name is "pixel2_joypad".
[INFO] [Autoconf] pixel2_joypad configured in port 1.
[INFO] [Input] Found joypad driver: "linuxraw".
[INFO] [ALSA] Initialized PLAYBACK device "default".
```

Non-fatal ALSA control/MIDI warnings remain:

```text
ALSA lib control.c:1528:(snd_ctl_open_noupdate) Invalid CTL hw:0
ALSA lib seq.c:935:(snd_seq_open_noupdate) Unknown SEQ default
```

## Remaining Physical Check

Launch NES from the frontend and confirm on the device:

- the game display orientation is correct with `video_rotation=0`;
- D-pad, ABXY, START/SELECT, shoulders, and hotkey exit work;
- game audio is audible;
- volume buttons change real game volume.
