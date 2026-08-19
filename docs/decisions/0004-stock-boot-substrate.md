# 0004: Pixel2 stock boot substrate and plumOS init ownership

Date: 2026-08-12
Amended: 2026-08-19
Status: Accepted, exact-stock DTB amendment

## Decision

Pixel2 uses the stockOS Rockchip boot prefix, stock `Image`, and the initramfs
embedded in that stock `Image` as the boot substrate. The runtime DTB is the
checksum-registered stock Pixel2 DTB byte-for-byte; plumOS does not add,
remove, or rewrite DTB properties.
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
3. stock U-Boot DTB and the exact stock runtime DTB;
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
  `rk3326s-gkd-pixel2.dtb` directly into `PLUMOS_BOOT`. The image verifier
  rejects a runtime DTB that is not byte-identical to the registered stock
  artifact.
- The former `/usb@ff300000/vbus-supply = <&OTG_SWITCH>` addition is withdrawn.
  It crossed the stock boot-substrate boundary and coincided with the period in
  which the shared DWC2 controller stopped returning reliably from Wi-Fi host
  operation to ADB gadget operation. Wi-Fi must first be proven with the stock
  DTB and stock-compatible userspace sequencing.
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
