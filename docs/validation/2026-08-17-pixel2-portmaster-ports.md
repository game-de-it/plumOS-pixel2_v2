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
