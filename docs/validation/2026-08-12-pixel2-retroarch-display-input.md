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
- initially tested `video_rotation=0`, then `1` and `2`, but the LCD remained
  rotated because RetroArch's plain DRM driver did not implement
  `set_rotation`;
- set `config_save_on_exit=false` so temporary validation settings do not
  overwrite the mutable RetroArch config;
- launcher now appends required Pixel2 runtime settings every launch, so stale
  mutable RetroArch config cannot disable input/audio/video settings.

## Follow-up Rotation and Button Repair

After physical testing showed the game remained rotated even with
`video_rotation=2`, the display root cause was narrowed to RetroArch's plain
DRM backend: `video_drm` had `set_rotation = NULL`, so the configured
`video_rotation` value did not affect scanout on Pixel2.

The follow-up implementation adds a Pixel2 software-rotation path to the patched
plain DRM backend:

- caches RetroArch's final rotation in the DRM video state;
- treats 90/270 degree rotation as a logical `640x480` viewport on the native
  `480x640` panel;
- rotates and nearest-neighbour scales each software frame into the physical
  dumb framebuffer before page flip;
- exposes `drm_set_rotation`, so `video_rotation` changes now recreate the
  surface using the requested orientation.

The default Pixel2 RetroArch rotation is restored to `video_rotation=3`,
matching the frontend's validated `ccw` panel correction. The launcher also
defaults `PLUMOS_RETROARCH_VIDEO_ROTATION` to `3` so stale mutable config cannot
return the game to native portrait output.

The same follow-up repair fixes the RetroArch face-button autoconfig to match
the physical capture:

```text
input_a_btn = "0"
input_b_btn = "1"
```

Commit:

```text
c4a5cf2 fix: rotate Pixel2 RetroArch DRM output
```

Host validation:

```text
app_layer_scripts=result-ok
retroarch_component=result-ok output=/work/output/retroarch/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

The generated app-layer and RetroArch component manifests both recorded:

```text
source_ref = c4a5cf2
```

Live deployment was staged on the Pixel2, verified before installation, then
copied into `/mnt/plumos` as a checksum-complete managed-file unit:

```text
stage_verify=ok
deployed_verify=ok
/mnt/plumos/manifest.json:  "source_ref": "c4a5cf2",
/mnt/plumos/components/retroarch/manifest.json:  "source_ref": "c4a5cf2",
```

Deployed key hashes:

```text
a1014294307b392358618e3ffcce8b911ca256459ec29b495a6b5598f7e057f0  /mnt/plumos/bin/retroarch
d9a084ef4976bc00921f66033f6b01c46f899e0efae8417e97fc5e0fa1f238e5  /mnt/plumos/factory-defaults/retroarch/autoconfig/linuxraw/pixel2_joypad.cfg
e734b59f9aef9e47cb48b89f3919f51ed22a97dd6dd6a7ac73fb0528114ad24b  /mnt/plumos/factory-defaults/retroarch/retroarch.cfg
```

After deployment, a direct NES launch wrote:

```text
video_driver = "drm"
video_rotation = "3"
input_joypad_driver = "linuxraw"
joypad_autoconfig_dir = "/mnt/plumos/factory-defaults/retroarch/autoconfig"
```

Physical LCD orientation and in-game button behavior are awaiting operator
confirmation.

Runtime append after deployment:

```text
audio_driver = "alsa"
audio_device = "default"
video_driver = "drm"
video_rotation = "3"
input_driver = "udev"
input_joypad_driver = "linuxraw"
joypad_autoconfig_dir = "/mnt/plumos/factory-defaults/retroarch/autoconfig"
config_save_on_exit = "false"
savefile_directory = "/mnt/plumos/saves/nes"
savestate_directory = "/mnt/plumos/states/nes"
system_directory = "/mnt/plumos-user/bios"
```

## Follow-up Aspect and udev Input Repair

After physical testing confirmed the LCD orientation was correct, two remaining
issues were reported:

- NES aspect ratio was wrong on the rotated DRM output;
- controls were scrambled, including D-pad DOWN acting as START and physical B
  acting as A.

The input root cause is that the previous Pixel2 default used RetroArch's
`linuxraw` joypad driver. `linuxraw` reads `/dev/input/js0` joystick button
numbers, while the Pixel2 evidence and stock kernel contract are evdev based.
The Pixel2 D-pad is exposed as `ABS_X`/`ABS_Y`, not as discrete button indices,
so a button-only js0-style map can collide with START/face-button bindings.

The repaired Pixel2 contract is:

```text
input_driver = "udev"
input_joypad_driver = "udev"
input_a_btn = "1"        # BTN_EAST, physical A
input_b_btn = "0"        # BTN_SOUTH, physical B
input_x_btn = "2"        # BTN_NORTH
input_y_btn = "3"        # BTN_WEST
input_l_btn = "4"        # BTN_TL
input_r_btn = "5"        # BTN_TR
input_l2_btn = "6"       # BTN_TL2
input_r2_btn = "7"       # BTN_TR2
input_select_btn = "8"   # BTN_SELECT
input_start_btn = "9"    # BTN_START
input_left_axis = "-0"   # ABS_X
input_right_axis = "+0"  # ABS_X
input_up_axis = "-1"     # ABS_Y
input_down_axis = "+1"   # ABS_Y
```

The display follow-up keeps `video_rotation = "3"` and disables core-auto
aspect for Pixel2's logical `640x480` surface:

```text
video_force_aspect = "true"
video_aspect_ratio_auto = "false"
aspect_ratio_index = "0"
video_aspect_ratio = "1.333333"
```

Commit:

```text
de7b3de fix: use Pixel2 udev input for RetroArch
```

Host validation:

```text
app_layer_scripts=result-ok
system_rootfs_scripts=result-ok
retroarch_component=result-ok output=/work/output/retroarch/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Generated and deployed manifests recorded:

```text
source_ref = de7b3de
```

Live deployment was staged on the Pixel2 from:

```text
output/live/2026-08-12-retroarch-udev-aspect-de7b3de/app-layer-de7b3de.tar.gz
sha256 = 14f1b7a9f0552935a809373f972b755277a850aa61dfe3d9921e434715876e1c
```

The staged tree and the live `/mnt/plumos` tree both passed checksum
verification after deployment:

```text
stage_verify=ok
deployed_verify=ok
/mnt/plumos/manifest.json:  "source_ref": "de7b3de",
/mnt/plumos/components/retroarch/manifest.json:  "source_ref": "de7b3de",
stale_linuxraw=removed
```

The previous `factory-defaults/retroarch/autoconfig/linuxraw/pixel2_joypad.cfg`
was removed from the live app-layer so stale js0-style bindings cannot be
selected later.

Direct NES launch after deployment created this runtime append:

```text
video_driver = "drm"
video_rotation = "3"
video_force_aspect = "true"
video_aspect_ratio_auto = "false"
aspect_ratio_index = "0"
video_aspect_ratio = "1.333333"
input_driver = "udev"
input_joypad_driver = "udev"
joypad_autoconfig_dir = "/mnt/plumos/factory-defaults/retroarch/autoconfig"
config_save_on_exit = "false"
```

Physical confirmation still required:

- NES image has the correct orientation and 4:3 aspect on the LCD;
- D-pad no longer triggers START;
- physical B no longer triggers RetroPad A;
- A/B/X/Y, START/SELECT, shoulders, and hotkey exit work.

## Follow-up udev database shim

Physical testing reported that controls did not respond at all with the `udev`
joypad driver. The Pixel2 rootfs uses busybox `mdev`, so `/run/udev` did not
contain libudev properties for `/dev/input/event2` even though the device node
and sysfs capabilities existed:

```text
/sys/class/input/event2/device/name = pixel2_joypad
/sys/class/input/event2/device/uevent includes EV=20000b, ABS=3
/run/udev/data/c13:66 was absent
```

RetroArch's udev joypad driver enumerates devices with
`ID_INPUT_JOYSTICK=1`. A live shim test wrote that property for event2 and
RetroArch immediately detected the controller:

```text
[INFO] [udev] Keyboard #0: "pixel2_joypad" (/dev/input/event2).
[INFO] [Autoconf] pixel2_joypad configured in port 1.
[INFO] [Input] Found joypad driver: "udev".
```

`plumos-ensure-udev-input-db` now creates the minimal `/run/udev/data/cMAJ:MIN`
record for `pixel2_joypad`, and `plumos-retroarch-launch` runs it before
starting RetroArch. This keeps the shared udev button/axis map while preserving
the minimal mdev-based System.

Commit:

```text
8b8da8d fix: seed Pixel2 udev joystick properties
```

Host validation:

```text
app_layer_scripts=result-ok
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Live deployment was staged on the Pixel2 from:

```text
output/live/2026-08-12-udev-db-shim-8b8da8d/app-layer-8b8da8d.tar.gz
sha256 = 240fd4c27b59ef78f16f0b27766d414f11c46648db04df996875ccf3f950d31c
```

The staged tree and live `/mnt/plumos` tree passed checksum verification:

```text
stage_verify=ok
deployed_verify=ok
/mnt/plumos/manifest.json:  "source_ref": "8b8da8d",
/mnt/plumos/components/retroarch/manifest.json:  "source_ref": "de7b3de",
```

The helper created the expected live database entry:

```text
udev_input_db=result-ok event=event2 name=pixel2_joypad data=/run/udev/data/c13:66
E:ID_INPUT=1
E:ID_INPUT_JOYSTICK=1
E:ID_BUS=platform
E:ID_INPUT_KEY=1
E:NAME=pixel2_joypad
```

NES was relaunched through `plumos-retroarch-launch`; the runtime append still
uses `input_joypad_driver = "udev"` and the launcher log records
`udev_input_db=result-ok`.

## Follow-up D-pad axis binding

Physical testing after the udev database shim confirmed that START and A work
in-game, but the D-pad still did not move the NES character. This means
RetroArch is now detecting `pixel2_joypad` and applying button bindings, but
the ABS D-pad axes need to be forced into the runtime player-1 map.

Pixel2 now binds the D-pad axes in two ways:

```text
input_player1_left_axis = "-0"
input_player1_right_axis = "+0"
input_player1_up_axis = "-1"
input_player1_down_axis = "+1"
input_player1_l_x_minus_axis = "-0"
input_player1_l_x_plus_axis = "+0"
input_player1_l_y_minus_axis = "-1"
input_player1_l_y_plus_axis = "+1"
input_player1_analog_dpad_mode = "1"
```

The direct `player1_*_axis` entries target RetroPad D-pad state. The left-stick
entries plus `analog_dpad_mode=1` provide a second path for cores/input layers
that map analog-left to digital D-pad.

## Follow-up D-pad key binding

Physical testing still reported no D-pad movement after the axis fallback. A
raw `/dev/input/event2` capture was taken while pressing left, right, up, and
down. It showed that physical D-pad presses are evdev keys, not ABS events:

```text
EV_KEY code=0x222 value=1/0  # BTN_DPAD_LEFT  = 546
EV_KEY code=0x223 value=1/0  # BTN_DPAD_RIGHT = 547
EV_KEY code=0x220 value=1/0  # BTN_DPAD_UP    = 544
EV_KEY code=0x221 value=1/0  # BTN_DPAD_DOWN  = 545
```

RetroArch's udev button scan order therefore assigns:

```text
input_up_btn = "10"
input_down_btn = "11"
input_left_btn = "12"
input_right_btn = "13"
```

The launcher now also appends the same `input_player1_*_btn` entries and
disables `input_player1_analog_dpad_mode`.

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

- the game display orientation is correct with the DRM software rotation and
  `video_rotation=3`;
- D-pad, ABXY, START/SELECT, shoulders, and hotkey exit work;
- game audio is audible;
- volume buttons change real game volume.

## Frame Pacing Follow-up

After the udev D-pad button binding was deployed, the device could be operated
with the physical D-pad. The next observed issue was unstable frame pacing and
audio dropouts while NES was running.

ADB showed the live CPU governor was:

```text
ondemand
```

The launcher receives the frontend CPU policy. Pixel2 follows that policy
instead of forcing an emulator-specific `performance` governor; NES currently
uses the frontend's `ondemand` policy. Cross-project review found the existing plumOS
audio-routing fix for similar audio/FPS instability, so the first Pixel2
runtime profile avoids a performance-governor workaround and applies the
plumOS audio route plus RetroArch-side frame/audio settings first:

```text
audio_device = "plumos_output"
ALSA_CONFIG_PATH = "/run/plumos/audio/asound.conf"
ALSA_PLUGIN_DIR = "/mnt/plumos/lib/alsa-lib"
audio_latency = "96"
video_threaded = "true"
```

The launcher validates and applies the frontend-requested governor when the
kernel exposes it. `performance` remains a policy/profile choice to use only as
a later diagnostic or last resort, not a Pixel2 NES default.

Pixel2 now has an `audio-router` app-layer component. It exposes the logical
ALSA PCM `plumos_output`, resolves RK817 by the `rockchiprk817` ALSA card ID
instead of card number, and uses the same ioplug delay boundary as the existing plumOS
router: logical delay is derived from physical ring occupancy via
`snd_pcm_avail_update()` instead of raw `snd_pcm_delay()`.

The mutable user RetroArch config is not overwritten; these settings are
appended at launch time and are also present in factory defaults for newly
provisioned configs.

## Audio Router Live Deployment

Commits:

```text
21cf972 fix: tune Pixel2 RetroArch frame pacing
1f20a70 feat: add Pixel2 audio router
69a47a7 fix: use Pixel2 ALSA base for audio router
99fe4e7 fix: avoid ALSA shorthand in Pixel2 router
aabb19f fix: enable Pixel2 RetroArch audio state routing
```

Host validation:

```text
app_layer_scripts=result-ok
audio_router=result-ok output=/work/output/audio-router/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Live deployment:

```text
stage_verify=ok
deployed_verify=ok
/mnt/plumos/manifest.json:  "source_ref": "aabb19f",
/mnt/plumos/components/audio-router/manifest.json:  "source_ref": "99fe4e7",
/mnt/plumos/components/retroarch/manifest.json:  "source_ref": "1f20a70",
```

Runtime route after direct NES launch:

```text
audio_device = "plumos_output"
audio_latency = "96"
video_threaded = "true"
video_rotation = "3"
cpu governor = ondemand
/proc/asound/card0/pcm0p/sub0/status = RUNNING owner_pid=<retroarch>
plumos-hotplug: route=rk817_stereo card=0 pcm=hw
```

The router initially failed on Pixel2 because `/usr/share/alsa/alsa.conf` is
not provided by the current rootfs. The helper now uses the app-layer
`factory-defaults/alsa/alsa.conf` as its base. A second failure came from ALSA
`hw:0,0` / `plughw:0,0` shorthand names that are normally defined by the full
system ALSA config. Pixel2 now generates explicit `plumos_hw_cardN` and
`plumos_plughw_cardN` PCMs and the ioplug opens those names instead.

RetroArch exports `PLUMOS_AUDIO_FAST_FORWARD_DROP=1` and a per-launch
`PLUMOS_AUDIO_FAST_FORWARD_STATE` path, allowing the patched ALSA driver and
the router to keep the direct hardware route during normal play and drop only
excess fast-forward audio.

## Next Physical Check

The game is currently running through the Pixel2 audio router. Confirm on the
device:

- audio no longer skips during normal NES play;
- FPS feels stable without switching NES to the `performance` CPU governor;
- D-pad and ABXY remain correct after the audio-router launch path;
- volume keys affect actual game volume.

## Pixel2 volume gain and refresh control follow-up

User report after the audio-router fix:

- normal NES audio no longer skips;
- NES horizontal scrolling is still not smooth enough;
- volume buttons do not change perceived game volume.

Sibling project checks:

- MF kept RetroArch smooth on DRM with threaded video, VRR disabled, and the
  duplicate-frame DRM guard.
- V90S previously used a `video_refresh_rate` launch/config knob while testing
  panel cadence mismatches.
- V90S volume work applied runtime software gain for routes where a hardware
  mixer control was insufficient or unavailable.

Pixel2 already carries the DRM duplicate-frame, nearest-neighbor scaling,
software rotation, and page-flip pacing patches. The measured Pixel2 VOP
interrupt cadence during an active NES session was about 60.2 Hz over 10
seconds, so the default app-layer now exposes a refresh-rate override without
changing the default away from 60 Hz:

```text
video_refresh_rate = "60.000000"
vrr_runloop_enable = "false"
```

Commit `c8150cc` also makes the Pixel2 audio router read
`/run/plumos/volume/current` on every transfer and apply software gain for the
internal RK817 route, not only for USB/mono conversion routes. This mirrors the
V90S direction while keeping Pixel2's current `pixel2-state-only` control
backend.

Host validation:

```text
sh -n package/app-layer-pixel2/bin/plumos-retroarch-launch
sh -n package/app-layer-pixel2/bin/plumos-volume-control
sh -n package/app-layer-pixel2/bin/plumos-hardware-keys-service
bash tests/test-app-layer-scripts.sh
git diff --check
./scripts/docker-build.sh audio-router
./scripts/docker-build.sh retroarch
./scripts/docker-build.sh app-layer --strict
```

Live deploy validation:

```text
stage_verify=ok
deployed_verify=ok
manifest.json:  "source_ref": "c8150cc"
components/audio-router/manifest.json:  "source_ref": "c8150cc"
components/retroarch/manifest.json:  "source_ref": "c8150cc"
```

Runtime check after direct NES launch:

```text
audio_device = "plumos_output"
audio_latency = "96"
video_rotation = "3"
video_refresh_rate = "60.000000"
vrr_runloop_enable = "false"
aspect_ratio_index = "0"
video_aspect_ratio = "1.333333"
video_threaded = "true"
/proc/asound/card0/pcm0p/sub0/status = RUNNING owner_pid=<retroarch>
/run/plumos/volume/current = 8
plumos-hotplug: route=rk817_stereo card=0 pcm=hw
```

Physical result:

- volume up/down changes perceived NES game volume while RetroArch is running.

Next physical check:

- confirm whether scrolling is still uneven at the default 60.000000 Hz;
- if scrolling is still uneven, relaunch with
  `PLUMOS_RETROARCH_VIDEO_REFRESH_RATE=60.200000` and compare.
