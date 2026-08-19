# Pixel2 PortMaster and Ports acceptance

Date: 2026-08-17  
Device runtime: `0.1.0-dev-04ec0c7`  
PortMaster: `2026.06.23-0015`  
Pixel2 adapter: `25`

## Reference implementations

The Pixel2 implementation keeps the shared plumOS PortMaster contract and
adapts only the hardware boundary.  The following existing work was reviewed
before changing the Pixel2 runtime:

- V90S `0ef55c7`, `fefe731`, `cbaa567`, `db183c0`, `646fc28`, and `b00a9c8`:
  integration, bounded update persistence, UINPUT controls, audio routing,
  compatibility libraries, and FRT input handoff;
- XU20 `c10739c`, `b3865da`, `1b08fd8`, `ce60678`, and `7a2f795`: GUI/game
  scaling, UINPUT, and physical-game acceptance;
- MF `e1b8132`, `bff3da0`, `bc8fc0e`, and `80c2a87`: isolated runtime
  libraries, cairo/LOVE compatibility, and native-port fonts.

Pixel2 does not copy another device's identity or binaries.  It uses the same
launcher/state model with Pixel2 AArch64 libraries, RK3326 KMS/GBM devices,
640x480 logical output, 480x640 native scanout, RK817 ALSA route, and the
Pixel2 controller GUID.

## Problems found and fixed

The official GUI initially exposed several host assumptions that are not
present in the stock boot substrate.  The implementation now provides the
required BusyBox applets, GNU Bash, `libgcc_s`, cairo/font/runtime libraries,
an isolated SDL2_mixer 2.6.2 audio path, Rockchip/Panfrost KMS environment,
and atomic adapter metadata updates.  Mutable upstream data remains under
`/mnt/plumos/state/portmaster/data`; ROMs remain on the user FAT partition.

OpenSyobon then proved that a successful SDL render was not a successful
physical presentation.  SDL saw the panel as 480x640 while the port expected
640x480.  Commits `9dca38a`, `30938ec`, and `04ec0c7` add a common AArch64
SDL_Renderer interposer which gives ports a 640x480 logical surface and
presents it at 270 degrees to the native 480x640 scanout.  This is a common
launcher fix, not an edit to the installed game's script.  `c92008f` restores
the FE with BusyBox `setsid` after standalone PortMaster/game launches.

## Network install and mutable state

With the RTL8821CU USB Wi-Fi adapter connected, PortMaster refreshed the
official catalog and installed the Ready-to-Run OpenSyobon package.  The final
cold-boot network address was `192.168.10.110/24`; it remained assigned while
the GUI and game owned the display and after both returned to the FE.

The signed Runtime update preserved:

```text
/mnt/plumos/state/portmaster/data          173860 KiB
PortMaster catalog files                  1949
/mnt/plumos-user/roms/PORTS/OpenSyobon.sh present
/mnt/plumos-user/roms/PORTS/opensyobon/   present
```

This separates immutable plumOS adapters from PortMaster updates, installed
ports, user game data, and saves.  A later plumOS Runtime update therefore
does not roll the catalog or installed games back to build-time content.

## Physical-device acceptance

PortMaster was started through its installed launcher.  The disclaimer and
main menu were upright, filled the physical landscape view, and continued
running with Wi-Fi available.  Stopping it returned to exactly one live FE
process after the FAT/state sync boundary.

OpenSyobon was started through the normal `ports` FE backend rather than by
executing its ELF directly:

```text
script=/mnt/plumos-user/roms/PORTS/OpenSyobon.sh
[GPTK]: Running in UINPUT output mode.
[plumOS] PortMaster SDL rotation: 640x480 -> 480x640 @ 270
ALSA state: RUNNING
ALSA owner_pid: OpenSyobon
Wi-Fi: 192.168.10.110/24
```

The final display is upright at the game's intended aspect ratio.  The 46 px
side bars come from OpenSyobon's own 548x480 presentation and are not clipping
or an incorrect panel transform.  Stopping the port removed every live game
and gptokeyb process and restored one FE process; Wi-Fi remained connected.

Capture evidence (ignored build output, not release payload):

```text
b7c9e262433a9c8c0709eaa7028eb7fe8efd28772497fae27c20f2d06bd117f3  opensyobon-physical.png
cdcff6918a03982c88130e21cd864c8f7849c46f696220601ebd9eea2c5ffef1  portmaster-physical.png
b6f55f2bcf347138793c808229e33cc95b7ebf8c2bdf90688519483e747c3851  portmaster-menu-physical.png
```

Files are under
`output/live/2026-08-17-portmaster/capture-04ec0c7/`.

## Build, deployment, and integrity

The PortMaster component and complete app layer were rebuilt before creating
the signed update.  The deployed package was:

```text
package=plumos-pixel2-runtime-0.1.0-dev-04ec0c7.tar.gz
sha256=6386fbc24b9c7741e7551fd08d76cdc966ab8542dcb4f0124e1488edee070f1e
adapter_version=25
update_result=runtime_healthy
runtime_pending=absent
```

After the final GUI and game cycles, device-side verification was run from
`/mnt/plumos` and every root `checksums.sha256` entry passed.  Host gates also
passed:

```text
portmaster_pixel2_runtime=result-ok
pixel2_update=result-ok
app_layer_verify=result-ok root=output/app-layer/pixel2/plumos
git diff --check: pass
```

This acceptance proves the official PortMaster GUI, catalog/update retention,
one official Ready-to-Run native SDL port, common Pixel2 display/input/audio
handoff, and FE recovery.  It intentionally does not claim that every
third-party PortMaster engine or commercial game-data package has been tested;
those must be added as named representative acceptance cases rather than
treated as implicitly compatible.

## 2026-08-18 Balatro and SDL/OpenGL acceptance

Balatro exposed a second display path which the SDL_Renderer interposer could
not cover.  LÖVE presents directly through SDL/OpenGL, so it still rendered a
landscape image into the panel-native 480x640 scanout.  Runtime adapter 29 adds
`libplumos-portmaster-gl-rotate.so`: OpenGL ports receive a logical 640x480
default framebuffer, and the common PortMaster launcher rotates that texture
onto the native 480x640 KMS framebuffer at swap time.  The SDL_Renderer
interposer remains active for native SDL ports.

The purchased game data was copied from the operator-provided ROM set without
modifying the PortMaster package or existing saves:

```text
source=/Volumes/public/02/motoki/emu/ROM/rom2/ports/Balatro/Balatro.exe
target=/roms/ports/balatro/Balatro.exe
sha256=0d75fe164accf3312734d4b37ac98788dd15f0b8e4f9bb8b7f90c4e59de93f47
```

The actual frontend `external:port` route launched both Balatro display setup
and the PortMaster patcher through the managed adapter.  DRM scanout captures
show both views upright at 640x480 with the intended aspect ratio.  The
original `Balatro.exe` and selected display setup retained their SHA-256 after
the launch/stop cycles.  The patcher's interactive physical-A confirmation and
the resulting `Balatro_pm` game are a separate operator acceptance gate; no
completed game build is claimed here.

```text
runtime=0.1.0-dev-7b0d69f
adapter_version=29
package_sha256=68093a20cc84d236529051d06baa5b3ee2e49a0aabc3dfb597f6ddcd4af38bea
update_result=runtime_healthy
runtime_verify=result-ok
portmaster_pixel2_runtime=result-ok
pixel2_update=result-ok
app_layer_verify=result-ok
```

Capture evidence is under
`output/live/2026-08-18-portmaster/capture-balatro-managed/`.

```text
e0cea5c55792bd607ca9d3d56ffaa83cca15012e006636662570e10cb98aff0f  balatro-managed2-physical.png
4fd7f7f3c00403dbcc30a60f10c38b484aa73befc42f10f766aa5775dae4c351  balatro-patcher-physical.png
```

## 2026-08-18 SDL display-mode and layout acceptance

Apotris exposed a layout problem which was separate from final scanout
rotation.  The common SDL interposer already reported a 640x480 window and
rotated the final renderer output, but Apotris imported
`SDL_GetCurrentDisplayMode()` directly.  It therefore calculated its internal
layout from the panel-native 480x640 mode before the correctly oriented final
present.  The result was an upright game shifted left, with the HOLD area
partially outside the visible screen.

Adapter 30 initially reported the Pixel2 logical 640x480 mode through
`SDL_GetCurrentDisplayMode()`, `SDL_GetDesktopDisplayMode()`, and
`SDL_GetDisplayMode()` while rotation was enabled.  Refresh rate and pixel
format remained the values returned by SDL.  This fixed Apotris layout, but
the desktop/enumerated-mode interception was later found to be too broad; see
the adapter 31 regression record below.

A temporary one-function shim first proved the diagnosis on the device.  The
same behavior was then integrated into the common interposer, rebuilt from
commit `92d754c`, assembled into the strict app layer, and delivered through a
signed Runtime delta from `0.1.0-dev-7b0d69f`:

```text
runtime=0.1.0-dev-92d754c
adapter_version=30
package_sha256=8cb63608f910d650fce626b606215584b5a54970710aba7bbeb4b4f7589feb22
payload_files=11
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
portmaster_pixel2_runtime=result-ok
app_layer_verify=result-ok
```

The actual frontend `external:port` route was used for the final Apotris and
OpenSyobon launches.  Apotris now keeps the HOLD frame, centered playfield,
and NEXT frame inside the 640x480 logical view.  OpenSyobon remains upright
with its intentional 46 px side bars, proving that the existing SDL Renderer
presentation was not regressed.  Stopping each port restored exactly one FE
process.

The update manifest contained no ROM, installed-port, PortMaster catalog,
configuration, or save path.  The live Apotris script, binary, and save
remained on the mutable user volume; their post-test hashes were recorded as:

```text
03b004c4ea8572b481459c5a3dc07dbce91e206d2e94880c0d65af6b11b92d49  Apotris.sh
86ba76c37c45ba56271a51c01688a90c37bf27d067d302b994fbb0e8216d0c06  Apotris.aarch64
2c3fea4c5f885245a1152be5f45b98e4b9ff98bf20e678ae02a6f2a20505accf  Apotris.sav
```

Capture evidence is under
`output/live/2026-08-18-portmaster/capture-ports-position/`:

```text
a4432a134c08fbe7a4081526d374fd8abfcb4a589c8c3539412bbb3663e539fe  apotris-position-physical.png
2e3a44b2d2dc8478f23bbdabd8c82c9f8d79c18cf4cb21e1fe6e85411fd5e4f4  apotris-92d754c-physical.png
ae33151583c5a0a927e16161064423494e7324d054d74e0f892ba5ca90a4c3cb  opensyobon-92d754c-physical.png
```

## 2026-08-18 Balatro KMSDRM regression and adapter 31

Balatro's first-run LÖVE patcher failed after adapter 30 with:

```text
Error: Could not initialize SDL video subsystem (kmsdrm not available)
Patch failed; no partial build was installed and the purchased game was not modified.
```

This was not a black rendered frame.  The patcher had already terminated
because SDL KMSDRM used the intercepted desktop/enumerated display modes while
selecting a connector mode.  The interposer supplied synthetic 640x480 even
though the physical Pixel2 connector only exposes 480x640.

Adapter 31 keeps the real `SDL_GetDesktopDisplayMode()` and
`SDL_GetDisplayMode()` results for KMS mode selection and changes only
`SDL_GetCurrentDisplayMode()` to the logical 640x480 application layout.
Apotris imports the latter directly, so its adapter 30 layout fix remains in
place without lying to the KMSDRM backend about physical connector modes.

The common PortMaster component and strict app layer were rebuilt from commit
`c55da25`.  A signed Runtime delta from adapter 30 was applied to the device:

```text
runtime=0.1.0-dev-c55da25
adapter_version=31
package_sha256=87d3e42ebb5cf5f16c63a62f7578693d123734d7dbbaab4404953643983a4f10
payload_files=11
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
portmaster_pixel2_runtime=result-ok
app_layer_verify=result-ok
```

The signed update contains no ROM, installed-port content, configuration, or
save path.  Through the normal FE-equivalent `ports` launch route, the Balatro
patcher now initializes KMSDRM, presents its 640x480 screen upright, and waits
for the required physical-A confirmation instead of returning to a black
screen.  The purchased `Balatro.exe` and existing display selection remain on
the mutable user volume.

Capture evidence is under
`output/live/2026-08-18-portmaster/capture-balatro-c55da25/`:

```text
8dd3f8806eb547058883661fe2d3cf9e9823de5446063f2146b42a939c644cda  balatro-c55da25-glass.png
```

## 2026-08-18 Balatro patch completion and adapter 34

The physical A press on the first-run patcher was accepted.  It produced the
handheld archive and build stamp without modifying the purchased source:

```text
0d75fe164accf3312734d4b37ac98788dd15f0b8e4f9bb8b7f90c4e59de93f47  Balatro.exe
ca7a1a5b1033e33ce736811e1951b7b5665ec7925d6265b660c3142fd5f8c203  Balatro_pm
4d7ff21327570de64c7f14a20e6fe2bf3acde01f86d0d8cd37e6fe389cb60a9d  .balatro-build.txt
```

The apparent lack of progress combined three separate screens: patch
confirmation, patch-complete confirmation, and Balatro's own button setup.
Adapter 33 makes the common PortMaster patcher readable on Pixel2 by changing
its dialog font from 16 px to 24 px and allocating a 128 px wrapped-text box.
The override is session-scoped and does not modify the downloaded PortMaster
tree.

After patching, the game itself exited with `Segmentation fault` while the
same archive stayed alive when the Pixel2 GL rotation interposer was disabled.
The redirected logical framebuffer had only a colour attachment, unlike the
depth/stencil-capable SDL window framebuffer expected by LÖVE.  The patcher did
not exercise that contract, while Balatro did on its first game frame.

Adapter 34 adds a packed 24-bit depth and 8-bit stencil renderbuffer to the
logical 640x480 framebuffer and restores the prior renderbuffer binding after
initialisation.  A signed Runtime delta from adapter 33 was then applied:

```text
runtime=0.1.0-dev-a64fde3
adapter_version=34
package_sha256=7469fdbb9c12e7b2b24eb3261079fee1375e440460b2d6e8a3d347c5213fcdcf
payload_files=11
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
portmaster_pixel2_runtime=result-ok
app_layer_verify=result-ok strict=1
```

The first reboot was intentionally rolled back because it was requested before
frontend readiness promoted the pending Runtime.  Reapplying it, restarting
the frontend on the new Runtime, and promoting health before reboot exercised
the complete safety contract.  The next reboot retained adapter 34.

The normal managed Ports launcher now keeps `love.aarch64 Balatro_pm` running
and presents the correctly rotated title screen.  The purchased source,
generated game, build stamp, display selection, and existing button-map file
kept their pre-update SHA-256 values.  The package has no ROM, installed-port,
configuration, or save target.

Capture evidence is under
`output/live/2026-08-18-portmaster/capture-balatro-a64fde3/`:

```text
1545792ead42b8735445da3a084721e0657098da0a59eae9c0270f9e18dec9a8  balatro-a64fde3-physical.png
```

## 2026-08-18 Balatro audio and intermittent-present fix

The running adapter 34 process owned the RK817 playback PCM, but it never
submitted its first buffer:

```text
state: PREPARED
owner_pid: 2527
hw_ptr: 0
appl_ptr: 0
```

Balatro uses the packaged OpenAL Soft runtime, which opens ALSA `default`
instead of honoring `AUDIODEV=plumos_output`.  Pixel2 still placed an outer
`plug` PCM around `plumos_hotplug` and enabled its stable-descriptor poll proxy.
That combination did not satisfy OpenAL's first-write wait.  V90S commit
`db183c0` had already established the required contract: make `pcm.!default`
the direct hotplug ioplug and expose the selected physical PCM descriptors.
Pixel2 now uses that same direct default route and disables the poll proxy only
for the managed PortMaster process tree.  Other Pixel2 SDL clients retain their
existing hotplug policy.

The intermittent display symptom had no Panfrost fault and no black frame in
the initial scanout sample.  The GL rotation presenter nevertheless inherited
LÖVE's current colour-write mask.  LÖVE can leave all colour channels disabled
after a stencil pass, so a physical present at that boundary can expose the
previous KMS buffer.  Adapter 35 follows the established plumOS A30 presenter
contract: save `GL_COLOR_WRITEMASK`, force all four channels writable for the
physical full-screen draw, and restore the application mask afterward.

The two fixes were committed separately:

```text
526c0ad fix: route Pixel2 PortMaster OpenAL audio
651e557 fix: preserve Pixel2 PortMaster color writes
```

The audio-router and PortMaster components were built in parallel.  The strict
app layer passed, and a signed Runtime delta from adapter 34 was installed:

```text
runtime=0.1.0-dev-651e557
adapter_version=35
package_sha256=9489809a8c78d6ba476aedd414e6227277f91e927c738c108ad1a365d6b7b262
payload_files=15
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
audio_router_component=result-ok
portmaster_component=result-ok
```

The safe reboot retained the new Runtime and adapter.  The normal managed
Balatro launcher exported `PLUMOS_AUDIO_POLL_PROXY=0`, generated a direct
`pcm.!default`, and kept the physical PCM running.  Fifty samples found zero
non-running states while the hardware pointer advanced from 3,186,888 to
3,445,136.  Forty consecutive DRM scanouts had identical non-black frame
statistics, and no GPU/DRM fault was logged.  The retained capture is:

```text
61c0f6d5863d829878064fc9f1de1eb61189c80ac7be2e07e557679e60574e08  balatro-651e557-physical.png
```

The Runtime did not change the commercial source, generated game, or build
stamp; their SHA-256 values remain the three values recorded above.  Audible
speaker output and absence of LCD flicker remain physical operator acceptance
items because neither can be proven solely from ADB/DRM state.

## 2026-08-18 Balatro synchronized GL present and adapter 37

The operator confirmed audible output, but moving cards and animated regions
still flickered while static background regions remained stable.  The KMS
primary plane used three complete scanout buffers and the GL swap interval was
reported as one, so the failure was not a damaged page or a missing vsync
request.  The common rotated presenter submitted its full-screen GLES draw and
immediately called the SDL/KMS swap without a completion boundary.  On the
stock Pixel2 Mali/Panfrost stack, the physical page flip could therefore expose
the target buffer before that rotated draw completed.

Adapter 37 calls `glFinish()` after the rotated full-screen draw and before the
physical swap.  This follows the completion boundary used by the existing
plumOS A30 Mali presenters and applies to every PortMaster SDL/OpenGL title
using the Pixel2 rotation adapter, rather than special-casing Balatro.  The
operator confirmed that Balatro animation and cursor movement no longer
flicker.

The signed Runtime was installed through the normal transaction and explicitly
promoted after frontend readiness.  A deliberate first reboot before that
manual health promotion exercised the rollback guard and returned to adapter
36; the Runtime was then reapplied, marked healthy, and retained across the
next safe reboot:

```text
commit=3480628
runtime=0.1.0-dev-3480628
adapter_version=37
package_sha256=3cea90260e502eb96b9f1ebf2722334c4ab60472d964fc6c0e37a9681f5bb18a
gl_library_sha256=d395f31e7ed755b7bc9287a50fcdce69560914cbd3df977cca85dfc7839a63fb
update_result=runtime_healthy
runtime_verify=result-ok
portmaster_pixel2_runtime=result-ok
app_layer_verify=result-ok strict=1
```

The formal adapter 37 launch kept `love.aarch64 Balatro_pm` alive, used the
640x480-to-480x640 GL rotation path, and held the RK817 PCM in `RUNNING` state.
Stopping it removed all PortMaster session mounts and restored the frontend.
The subsequent reboot retained the Runtime, full checksum manifest, ADB, and
frontend.  The commercial source, generated game, and build stamp retained the
three SHA-256 values recorded above.

## 2026-08-20 color-scheme restart contract and adapter 38

Changing PortMaster from its default color scheme to `Dark Mode` wrote the
expected upstream control marker but made later frontend launches return
immediately. The selected values were both valid and persisted correctly:

```text
theme=default_theme
theme-scheme=Dark Mode
marker=/mnt/plumos/state/portmaster/data/upstream/PortMaster/.pugwash-reboot
```

The failure was in the Pixel2 launcher, not the theme. Official
`PortMaster.sh` removes a stale `.pugwash-reboot` before the first `pugwash`
invocation and loops after a new marker is created. Pixel2 runs its hardware
bootstrap directly and had omitted both parts of that outer-shell contract.
Upstream `pugwash` therefore saw the marker, skipped `pm.run()`, and exited
without drawing a GUI.

Adapter 38 consumes a stale marker before opening the GUI and restarts the
same managed PortMaster session when a theme or release-channel action creates
a new marker. The restart is bounded to eight requests so a damaged upstream
state cannot create an infinite foreground loop. Each stale/requested action
is written to `logs/apps/portmaster.log` for diagnosis.

The live recovery preserved the user's scheme and copied the original config
and marker to:

```text
/mnt/plumos/state/portmaster/backups/theme-recovery-20260819T204153Z/
```

Shell syntax, Python compilation, PortMaster runtime gates, `pgrep` shim, and
`df` shim tests passed. Physical acceptance after the signed Runtime update
must confirm the current `Dark Mode` launch, another scheme change with an
in-place GUI restart, normal exit, and frontend recovery.
