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

Adapter 30 reports the Pixel2 logical 640x480 mode through
`SDL_GetCurrentDisplayMode()`, `SDL_GetDesktopDisplayMode()`, and
`SDL_GetDisplayMode()` while rotation is enabled.  Refresh rate and pixel
format remain the values returned by SDL.  This is part of the common Pixel2
SDL Ports boundary and does not patch Apotris or any installed mutable port.

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
