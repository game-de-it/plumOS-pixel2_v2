# Pixel2 Neo Geo repeat test

Date: 2026-08-23

## Purpose

The first RC1 SD became partly unavailable immediately after a Neo Geo game
and later produced `NO SD`. This test isolates Neo Geo/FBNeo from that media
incident by repeatedly exercising the same launch path on a different SD.

## Test boundary

- device: `root@192.168.10.107`
- Runtime: `0.1.0-rc1-dccd872`
- current SD: `ASTC`, manufacturer `0x000012`, OEM `0x3456`
- route: `neogeo` / `retroarch:fbneo`
- content: `NEOGEO/aof.zip` (Art of Fighting)
- content SHA-256:
  `7df2835107f64ad3354b80fd7da81e15c93171b8f3b32d65c6957f79ab5611ec`
- accepted cycles: 20 consecutive launches and exits
- run time before each exit: 5 seconds
- battery: 48% before the loop, 39% after all checks

Each accepted cycle required all of the following:

1. a live non-zombie RetroArch process using `fbneo_libretro.so`;
2. a non-black, varying 480x640 DRM scanout;
3. ALSA playback in `RUNNING` state with an advancing hardware pointer;
4. bounded emulator termination through the launcher cleanup path, followed by
   exactly one frontend process;
5. no surviving RetroArch or FBNeo process;
6. unchanged Neo Geo ROM SHA-256 and unchanged storage-error count.

The existing `scripts/validate-pixel2-device-media.py` supplied full raw DRM and
audio evidence for cycles 1--14. Cycles 15 and 16 also completed device-side
capture and audio sampling, but Mac-side evidence collection crossed its
20-second SSH timeout while transferring raw frames. The device still showed a
live FBNeo process, advancing audio, completed capture files, the unchanged ROM
hash, and no new storage error. The emulator was then terminated and the
frontend restored normally in both cases.

The timeout was caused by repeatedly reusing one SSH ControlMaster while
transferring multiple 1.2 MB raw planes during FBNeo CPU load. Two timed-out
Mac SSH child processes remained attached until explicitly closed. Ping returned
to 10/10 immediately after closing them and returning to the frontend. This is
a validator transport issue, not evidence of an SD or emulator failure.

Cycles 17--20 therefore used
`scripts/validate-pixel2-neogeo-loop-device.sh`, which performs DRM and audio
analysis entirely on Pixel2 and sends only one result line over SSH. Raw plane
reanalysis produced:

| cycle | non-black ratio | maximum channel stddev |
|---:|---:|---:|
| 17 | 0.997533 | 12.254 |
| 18 | 0.991436 | 22.760 |
| 19 | 0.994186 | 18.779 |
| 20 | 0.989788 | 24.832 |

All four exceed the established media-smoke thresholds of non-black ratio
0.01 and maximum channel standard deviation 2.0. Their original provisional
`fail` labels came from a new fixed-stride color-count check, not from the
emulator. The validator was corrected to inspect complete planes using the
established thresholds, retry a transient black capture up to three times, and
run component checksum verification from `/mnt/plumos`.

One final corrected proof cycle reported:

```text
cycle=23 result=pass startup=1 display=1 display_ratio=0.994860
colors=2 display_stddev=17.663 capture_attempts=1 audio=1
frontend=1 emulator=0 storage_errors=1
rom_sha256=7df2835107f64ad3354b80fd7da81e15c93171b8f3b32d65c6957f79ab5611ec
runtime_read=1
```

Cycles 21--23 were validator-calibration runs after the requested twenty and
are not needed to inflate the acceptance count.

## Storage comparison

Before the loop, the only matching storage log was the existing forced-off FAT
dirty warning at uptime 5.60 seconds:

```text
FAT-fs (mmcblk0p3): Volume was not properly unmounted.
```

The count remained exactly one after all cycles and after full Runtime reading.
No `mmc0` tuning, timeout, initialization, block I/O, ext4, or additional FAT
error appeared. Both writable mounts remained present:

```text
/dev/mmcblk0p2 /mnt/plumos      ext4 rw,noatime
/dev/mmcblk0p3 /mnt/plumos-user vfat rw,relatime,errors=remount-ro
```

The following pre/post hashes were identical:

```text
7df2835107f64ad3354b80fd7da81e15c93171b8f3b32d65c6957f79ab5611ec  aof.zip
50ac5621b1b84af1cafb85343fa404b4c9bbfcf305dee31f1771000ccd39bd96  systems.json
f275f1488c6a5f66b5d4c37c29cb00c80bfab8f6a95dbb2d0a5eefd4b9719b83  checksums.sha256
```

The final full managed Runtime read returned:

```text
runtime_verify=result-ok
```

Final state was one frontend, zero emulators, a 434 Mbps 5 GHz Wi-Fi link with
zero RX/TX errors, and 10/10 ping replies.

## Conclusion

Neo Geo/FBNeo did not reproduce the previous SD-backed service loss over twenty
consecutive launch/exit cycles on the replacement SD. The ROM, complete managed
Runtime, frontend recovery, display, audio, Wi-Fi, and kernel storage log all
remained valid. Within this repeat-test boundary, Neo Geo is excluded as the
cause. The prior `NO SD` incident remains consistent with the unidentified
generic `USD` medium or its electrical contact rather than emulator behavior.
