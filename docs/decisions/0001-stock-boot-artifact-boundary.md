# Decision 0001: retain boot artifacts, replace the complete userspace

Date: 2026-08-11
Status: Accepted

## Decision

The Pixel2 port keeps only artifacts required to cross the vendor boot and
kernel boundary:

1. the Rockchip boot payload from the unpartitioned area before partition 1;
2. the vendor Linux `Image` while an independently maintained kernel is not yet
   available;
3. the Pixel2 runtime DTB and the U-Boot DTB when the bootloader requires both;
4. kernel modules and firmware that must exactly match the retained kernel.

The stock `SYSTEM` SquashFS is an analysis input only. It is never copied into a
plumOS image. Its init system, services, frontend, emulators, settings, themes,
logos, update code and product identity are replaced by plumOS-owned files.

## Runtime ownership

The retained kernel starts a plumOS-owned initramfs or an equivalent verified
handoff that mounts a plumOS System SquashFS. From the first userspace process,
plumOS owns PID 1, services, display, input, audio, network, USB gadget,
frontend, applications, updates and diagnostics.

Build verification must reject stock product names and stock service paths in
the generated rootfs and SD payload. Documentation and provenance manifests may
name the analyzed source OS so the retained binary boundary remains auditable.

## Development access

Pixel2 has no built-in Wi-Fi. USB ADB is the default first-boot maintenance
path. Supported USB Wi-Fi dongles are an optional network path and must use
plumOS-packaged firmware, supplicant, DHCP and SSH services. Neither path may
execute binaries from the analyzed stock SquashFS.

## Recovery

Development and image builds use a captured golden source. The original stock
SD is not modified. Boot experiments are written only to a separate card.

