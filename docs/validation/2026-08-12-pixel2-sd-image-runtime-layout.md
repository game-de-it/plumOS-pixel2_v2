# Pixel2 SD image runtime layout validation

Date: 2026-08-12

Scope:

- Pixel2 SD image layout expansion for the current full app-layer
- p1 boot area growth for the System A/B design
- p2 `PLUMOS_SYS` growth so all built libretro cores, PicoArch, standalone
  launcher, frontend, and services fit in the seeded runtime
- Docker wrapper recovery after `docker run --rm` attach hangs and cleanup
  failures

Implemented layout:

```text
image size      4 GiB
raw prefix      sectors 0..32767      16 MiB Pixel2 Rockchip boot prefix
p1 FAT32 boot   sectors 32768..1081343 512 MiB
p2 ext4 sys     sectors 1081344..5275647 2048 MiB
p3 FAT32 user   sectors 5275648..8388607 remainder
```

Host commands:

```sh
./scripts/docker-build.sh standalone
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh system-rootfs
./scripts/docker-build.sh sd-image
(cd output/image/pixel2 && sha256sum -c checksums.sha256)
fdisk output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
hdiutil attach -readonly -nomount output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
hdiutil detach /dev/disk6
```

Results observed:

```text
created: /work/output/standalone/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
system_rootfs=result-ok image=/work/output/system-rootfs/pixel2/payload/SYSTEM
plumOS-Pixel2-0.1.0-dev.img: OK
image.manifest: OK
```

`fdisk` confirmed:

```text
1: start=32768   size=1048576  type=0C bootable
2: start=1081344 size=4194304  type=83
3: start=5275648 size=3112960  type=0C
```

`hdiutil attach -readonly -nomount` recognized:

```text
/dev/disk6    FDisk_partition_scheme
/dev/disk6s1  Windows_FAT_32
/dev/disk6s2  Linux
/dev/disk6s3  Windows_FAT_32
```

Docker runtime note:

- The image was generated from a dirty tree while fixing the layout and wrapper.
- The Docker Desktop backend later returned
  `meta.db: input/output error` during container cleanup and then lost the
  CLI API socket.
- Because of that host-side degradation, the generated file passed checksum and
  partition recognition, but a clean-commit rebuild and Docker-side
  `verify-sd-image.sh` full filesystem extraction remain required.

Clean rebuild:

```sh
docker desktop stop --force --timeout 30
open -a Docker
find /Users/kroot/plumOS-pixel2_v2/output/build -mindepth 1 -delete
./scripts/docker-build.sh sd-image
(cd output/image/pixel2 && sha256sum -c checksums.sha256)
```

Result:

```text
system_rootfs=result-ok image=/tmp/plumos-pixel2-verify.vMs6wb/SYSTEM
app_layer_verify=result-ok root=/tmp/plumos-pixel2-verify.vMs6wb/app-layer
sd_image=result-ok image=/work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
created: /work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
plumOS-Pixel2-0.1.0-dev.img: OK
image.manifest: OK
```

Final manifest:

```text
image_size=4294967296
image_sha256=3f8e1f3df28b5c553e854748bc2530f8e13e57e69bcc30268e24a6d4794c4101
source_ref=631c30b
```

Follow-up:

- Continue standalone emulator binary implementation and real-device route
  validation.
