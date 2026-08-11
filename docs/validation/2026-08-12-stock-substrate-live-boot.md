# Pixel2 stock substrate live boot validation

Date: 2026-08-12
Scope: physical Pixel2 boot after adding stock-initramfs handoff diagnostics

## Result

The physical Pixel2 booted through the stock boot substrate into plumOS. The
frontend started and ADB was reachable.

## Kernel and handoff evidence

ADB reported:

```text
Linux plumos-pixel2 5.10.198 #1 SMP Thu Jan 16 15:55:36 HKT 2025 aarch64 GNU/Linux
```

The live kernel command line was the stock Pixel2 U-Boot command line:

```text
earlyprintk swiotlb=1 console=ttyFIQ0,1500000n8 rw root=/dev/mmcblk0p2 rootwait rw fsck.repair=yes net.iframes=0 quiet console=tty0 systemd.debug_shell=ttyFIQ0 fbcon=rotate:3 consoleblank=0
```

PID 1 is plumOS init running under BusyBox shell:

```text
/bin/busybox
/bin/sh /sbin/init
```

The stock initramfs hooks and shim were observed in `dmesg` and diagnostics:

```text
plumos-stock-initramfs=post-flash flash-mounted=1
plumos-stock-initramfs=post-sysroot system-mounted=1
plumos-stock-initramfs=post-sysroot handoff-target=present
plumos-stock-handoff=shim-start path=/usr/lib/systemd/systemd
plumos-stock-handoff=shim-args args=
plumos-init=result-start device=pixel2
```

This proves:

1. stock initramfs mounted `/flash`;
2. stock initramfs mounted `/flash/SYSTEM` at `/sysroot`;
3. the fixed `/usr/lib/systemd/systemd` handoff target existed;
4. the compatibility shim ran;
5. plumOS `/sbin/init` ran after `switch_root`.

## Mount contract

The live mounts matched the stock-substrate layout:

```text
/dev/mmcblk0p1 /flash vfat ro,noatime
/dev/mmcblk0p2 /storage ext4 rw,noatime
/dev/mmcblk0p1 /boot vfat ro,noatime
/dev/mmcblk0p2 /mnt/plumos ext4 rw,noatime
/dev/mmcblk0p3 /mnt/plumos-user vfat rw,relatime
/dev/mmcblk0p2 /state ext4 rw,noatime
/dev/mmcblk0p3 /roms vfat rw,relatime
```

`/storage/plumos/logs/stock-handoff.log` contained:

```text
plumos-stock-handoff=shim-start path=/usr/lib/systemd/systemd
plumos-stock-handoff=shim-args args=
```

`/state/plumos/logs/init.log` contained:

```text
plumos-init=mounts-ready boot=1 runtime=1 user=1
```

## Runtime services

ADB was running:

```text
/lib/ld-linux-aarch64.so.1 --library-path /usr/lib/plumos/adbd/lib:/lib/aarch64-linux-gnu /usr/lib/plumos/adbd/adbd.bin
```

The frontend was running:

```text
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event1
```

The frontend log reported:

```text
frontend=app-layer-verified root=/mnt/plumos
frontend=result-starting renderer=fbdev input=/dev/input/event1 fb=/dev/fb0
frontend=result-started pid=335
```

## Captured logs

Live logs were pulled to:

```text
output/live/2026-08-12-stock-substrate-booted/logs
```

Important SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `boot-diagnostics.txt` | `8eab3375d1e0955707429e221135824bb44776a0d1ff2487f3e4a8731eb0113e` |
| `runtime-diagnostics.txt` | `76b308e94f611e1cc2e31a3e456c0d85e5fdd2f8727684937b89d48ace6131d7` |
| `frontend.log` | `07b3cc5ed4d30a2647ebd9a986d165282bfc36444a4365f028ed3d562dbe94d5` |
| `adbd.log` | `da81fe67921738e332503ffabbbff970bb0c45e6b4161cc147e62b210f0da1ce` |
| `init.log` | `14a5bc40ac28baaaeda0a1391f0a345d09bcef4082b7b8df4b37e3a5c2cd397b` |

## Remaining gate

The remaining power-management gate is to reboot while charging and confirm the
stock-substrate plumOS image returns to the OS instead of stopping at the
bootloader charging screen.
