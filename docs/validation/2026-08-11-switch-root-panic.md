# Pixel2 first-boot switch_root panic

Date: 2026-08-11
Status: host fix complete; physical retest pending

## Observed failure

The first physical boot reached the plumOS initramfs and successfully found and
mounted `SYSTEM`. Immediately before the panic, the display showed:

```text
mount: mounting /dev on /newroot/dev failed: No such file or directory
mount: mounting /proc on /newroot/proc failed: No such file or directory
mount: mounting /sys on /newroot/sys failed: No such file or directory
plumos-initramfs=result-switch-root
mountpoint: applet not found
mkdir: can't create directory '/dev/': Read-only file system
```

This proves the boot prefix, U-Boot handoff, kernel, MMC access, boot FAT, loop
device and SquashFS path were working. The failure was at the initramfs to
System mount namespace handoff.

## Root cause

Empty runtime mountpoint directories were not present in the generated
SquashFS. `mount --move` therefore failed before `switch_root`. PID 1 then tried
to create the directories on the read-only SquashFS and exited. Debian's static
BusyBox also does not provide the `mountpoint` applet used by the initial PID 1
implementation.

## Fix

Commit `65c2265`:

- creates `/dev/pts`, `/proc`, `/sys`, `/run`, `/tmp`, `/boot`, `/state`,
  `/roms` and `/root` before SquashFS generation;
- replaces the unavailable `mountpoint` applet with `/proc/mounts` checks;
- makes every initramfs `mount --move` operation mandatory and enters an
  emergency shell instead of continuing to a PID 1 panic;
- adds `/dev/console` (5:1) and `/dev/null` (1:3) to the embedded initramfs;
- verifies all required mountpoints after re-extracting `SYSTEM`.

## Corrected host artifacts

- kernel Image SHA-256:
  `2b1983f172e9790a66dbac70b5731c7770c2da870a5dc5b049fc45cc02306ef5`
- SYSTEM SHA-256:
  `0b87cd76b3406f648de0cd513f27ba5e54d40643a918da77116f57d263fb788f`
- SD image SHA-256:
  `d87e4e213cbd9249707ca1a9987761c9f04de1c138d6352e3c0dcee6c6eb7319`

The corrected SD image passed partition, filesystem, prefix, embedded payload,
SquashFS, module ABI and branding gates. Physical boot remains the required
validation boundary.
