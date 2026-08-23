# Third-Party Notices

This document records the principal third-party and vendor-derived components
distributed by plumOS Pixel2. Exact revisions remain in build recipes,
component manifests, and `package/licenses-pixel2/runtime-license-index.tsv`.

plumOS-authored material is licensed under the repository MIT License. That
license does not relicense third-party binaries, source, fonts, firmware,
ROMs, BIOS files, or user content. Releases do not include ROMs or game BIOS
files.

Japanese counterpart:
[THIRD_PARTY_NOTICES.ja.md](THIRD_PARTY_NOTICES.ja.md)

## Platform Boundary

The GKD Pixel2 Rockchip boot prefix, stock Linux 5.10.198 kernel, runtime DTB,
stock initramfs, selected modules, and firmware are the hardware compatibility
substrate. Vendor-derived material retains its original terms and is not
covered by the plumOS MIT License. The release records exact registered hashes
and includes `pixel2-stock-vendor-runtime-NOTICE.txt`.

## Bundled Runtime Families

| Runtime family | Upstream or origin | Packaged evidence |
| --- | --- | --- |
| plumOS frontend and hardware services | plumOS Pixel2 repository | `plumOS-MIT.txt`, component manifests, font notices |
| System | plumOS, BusyBox, Debian runtime packages, stock-kernel modules/firmware | System `/usr/share/licenses`, System manifest, stock vendor notice |
| RetroArch | <https://github.com/libretro/RetroArch> | Exact source-tree `COPYING` and component manifest |
| libretro cores | Immutable repositories/refs in `docker/pixel2-tools/libretro-core-recipes.tsv` | Collected upstream license-bearing files and core manifest |
| PicoArch and SDL compatibility | <https://github.com/shauninman/picoarch> and SDL compatibility projects | PicoArch and SDL license texts |
| Standalone emulators | PCSX-ReARMed, steward-fu/nds with DraStic, PPSSPP, and OpenBOR | Upstream license/notice files, pinned refs, hashes, standalone manifest |
| PICO-8 | User-supplied proprietary runtime | No PICO-8 binary or game data is included in a plumOS release |
| Pyxel/Python | CPython, Pyxel, pygame, NumPy, Pillow, and packaged modules | Python license and package-provided license metadata retained in the runtime |
| PortMaster | <https://github.com/PortsMaster/PortMaster-GUI> and compatibility libraries | Upstream `PortMaster/licenses` plus compatibility-library notices |
| File Manager | <https://github.com/LoveRetro/NextCommander> | Pinned revision, component manifest, and explicit upstream-license-status notice |
| Music Player | plumOS application and miniaudio | plumOS MIT license and miniaudio license material |
| Network services | BusyBox, Dropbear, OpenSSH SFTP, Samba, dosfstools, and dependencies | Component manifest and retained package/source license material |
| Fonts and graphical assets | Noto CJK, DejaVu, RetroArch assets, and packaged themes | Font/asset license files retained with their components |
| Audio and PortMaster compatibility libraries | ALSA plugin, FFmpeg, OpenAL Soft, libevdev, FLAC, libjpeg, Readline, SDL, SquashFS tools, and LZO | Individual copied license/copyright texts in the app-layer bundle |

## Explicit Upstream-License Status

The inspected pinned NextCommander source tree does not contain a separate
LICENSE, COPYING, or equivalent grant. The dedicated notice records this as
`NOASSERTION`; it is not replaced by the plumOS MIT License.

The steward-fu/nds integration source is LGPL-2.1, but the packaged DraStic
executable is separately authored closed software. The integration LGPL does
not apply to or relicense that executable. The official release README, exact
asset hash, component manifest, and dedicated notice are retained. plumOS
Pixel2 follows the same documented inclusion policy as the other plumOS
handheld releases without making an additional claim about DraStic rights.

## Distribution Checklist

Before publishing a binary release:

- retain `LICENSE`, `NOTICE.md`, both third-party notice files, the runtime
  license index, component manifests, and exact source refs;
- run `scripts/audit-pixel2-license-bundle.sh` against the final app layer and
  System output;
- run the strict content gate and verify the compressed image checksum;
- exclude ROMs, BIOS files, saves/states, credentials, personal keys,
  proprietary PICO-8 files, and user-installed Ports/Pyxel data;
- publish the corresponding tagged repository source, recipes, and patches for
  source-built components as required by their licenses;
- keep the NextCommander and DraStic status notices visible rather than
  describing them as MIT or otherwise relicensing them.
