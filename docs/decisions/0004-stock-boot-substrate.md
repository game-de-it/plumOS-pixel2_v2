# 0004: Pixel2 stock boot substrate and plumOS init ownership

Date: 2026-08-12
Status: Accepted

## Decision

Pixel2 uses the stockOS Rockchip boot prefix, stock `Image`, and the initramfs
embedded in that stock `Image` as the boot substrate. The runtime DTB is
generated from the exact stock Pixel2 DTB with one bounded hardware fix:
`/usb@ff300000/vbus-supply` references the stock RK817 `OTG_SWITCH` regulator.
plumOS ownership begins at the SquashFS `SYSTEM` handoff: the mounted
`SYSTEM` image provides `/sbin/init`, runtime services, frontend launch,
emulator launch, audio routing, connectivity, update tooling and diagnostics.

The stock `SYSTEM` SquashFS remains an analysis input only. It is never copied
into the generated plumOS image. The generated `SYSTEM` is a plumOS SquashFS
shaped to satisfy the stock initramfs contract.

## Boundary

Allowed stock-derived boot artifacts:

1. 16 MiB Rockchip boot prefix;
2. stock Linux `Image` including its embedded initramfs;
3. stock U-Boot DTB and the stock runtime DTB as the registered patch input;
4. stock kernel modules and firmware required by the retained `5.10.198`
   kernel ABI.

plumOS-owned artifacts:

1. generated `SYSTEM` SquashFS containing `/sbin/init`;
2. `/etc`, `/usr`, init scripts, ADB/USB Wi-Fi/SSH services and diagnostics;
3. app-layer mount/provisioning logic;
4. frontend, menu actions, emulator launchers, RetroArch/cores and audio
   routing;
5. runtime/update metadata, manifests and checksum gates.

## Rationale

The original “plumOS-owned kernel” design removed the stock initramfs by
building Linux 6.12.79 with a plumOS initramfs. That worked well enough for
frontend bring-up, but Pixel2 charge/reboot behavior diverged from stockOS:
while charging, stockOS rebooted back into the OS, whereas the plumOS-built
kernel path entered the bootloader charging screen.

The user goal is not to own the kernel boundary. The desired ownership starts
after the stock boot sequence has mounted `SYSTEM` and transferred control to
init. Using the stock boot substrate preserves the vendor PMIC, charger and
restart behavior while still allowing plumOS to own the practical OS
experience: FE, emulators, ALSA/audio routing, connectivity and updates.

## Implementation notes

- The image builder must install stock `Image` and stock
  `rk3326s-gkd-pixel2.dtb` plus the single-property VBUS linkage into
  `PLUMOS_BOOT`. A decompiled-tree gate rejects any additional DTB change.
- The stock DTB leaves the DWC2 `vbus-supply` absent, so unbinding the
  controller cannot power-cycle an already-inserted USB device. Linking the
  existing `otg_switch` lets the unmodified stock DWC2 driver own VBUS while
  retaining stock `dr_mode = "otg"`; no forced host default is added.
- The System builder must target the stock `5.10.198` module ABI instead of
  `6.12.79-plumos-pixel2`.
- Host verifiers must compare boot files against `artifacts/vendor/pixel2-stock`
  rather than `output/kernel/pixel2`.
- Runtime identity gates still reject stock/foreign product names in generated
  plumOS-owned files. Provenance manifests and documentation may name stockOS
  as the analyzed boot source.
- The boot handoff must be validated on real hardware by proving:
  `/proc/version` is stock `5.10.198`, `/proc/1/root` is the generated plumOS
  `SYSTEM`, FE starts, ADB works, Wi-Fi survives saved-ON boot, and
  charging-state reboot returns to plumOS.

## Supersedes

This decision supersedes 0002 for Pixel2 release builds. The Linux 6.12 build
script may remain temporarily as an experiment, but release-image generation
must not depend on it unless a later decision explicitly restores plumOS kernel
ownership.
