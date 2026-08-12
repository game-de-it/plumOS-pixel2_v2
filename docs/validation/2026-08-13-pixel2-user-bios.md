# Pixel2 user BIOS staging and device placement

Date: 2026-08-13
App-layer source commit: `b33b877`
Inventory implementation commits: `9b76346`, `849da5a`

## Ownership contract

BIOS content remains user-owned and is never copied into the repository,
SYSTEM, app-layer, update package, or release image. The inventory tool writes
only to an ignored host staging directory. Live deployment merges that staging
directory into `/mnt/plumos-user/bios`; it does not delete or replace unrelated
user files.

## Inventory method

`scripts/prepare-pixel2-bios.py` reads the enabled Pixel2 system catalog,
launch profiles, and generated libretro `.info` files. Explicit aliases map
plumOS profile identifiers to their canonical core-info names, so alternate
routes such as `beetle_saturn` cannot silently omit their firmware. DraStic
BIOS requirements and BlueMSX database/machine directory requirements are
included as well.

The supplied operator ROM set was staged with:

```sh
./scripts/prepare-pixel2-bios.py \
  --app-root output/app-layer/pixel2/plumos \
  --rom-root /path/to/rom2 \
  --output output/bios-staging/pixel2 \
  --report output/validation/pixel2-bios-report.json
```

Result:

```text
requirements=231
files=340
bytes=121611612
missing_required=5
missing_optional=35
```

The staging metadata hashes were:

```text
4236525987951a10befd24a601a3c273b6ee1654d466c8019eadf36d1d7c7026  plumos-bios-manifest.json
09428d7d76d09bf45b78e86e55831ee0db2bd708cd0004894388cf1255037e8b  plumos-bios-checksums.sha256
```

Host `sha256sum -c` passed for all 340 payload files.

## Live-device result

The staging tree was merge-copied to `/mnt/plumos-user/bios`. On Pixel2,
`sha256sum -c plumos-bios-checksums.sha256` passed 340/340. Representative
PlayStation, Game Boy Advance, Saturn, Mega CD, PC Engine CD, Dreamcast,
Neo Geo, DraStic, BlueMSX, PC-98, and X68000 firmware paths were present.

The central PlayStation `scph5500.bin` was imported without overwriting an
existing PCSX user file. Central and PCSX-state copies both had SHA-256:

```text
9c0421858e217805f4abe18698afea8d5aa36ff0727eb8484944e00eb5e7eadb
```

PCSX retained `Bios = scph5500.bin` and `Gpu3 = builtin_gpu`. The final BIOS
tree occupied 116.8 MiB. The user partition had 1.2 GiB free after placement.

## ROM-set gaps

The supplied ROM set did not contain five required entries:

| Required path | Consumer |
| --- | --- |
| `ecwolf.pk3` | ECWolf core support data |
| `kick34005.CDTV` | PUAE/PUAE2021/UAE4ARM Amiga CDTV firmware |
| `sl31253.bin` | FreeChaF Channel F firmware |
| `sl31254.bin` | FreeChaF Channel F firmware |
| `sl90025.bin` | FreeChaF Channel F firmware |

`ecwolf.pk3` is core support data rather than a hardware BIOS, but the upstream
core-info contract marks it required, so it remains in the blocking inventory.
The report also records 35 optional firmware gaps. No missing file is reported
as placed, and completion remains open until required gaps reach zero and each
enabled system is exercised on physical hardware.
