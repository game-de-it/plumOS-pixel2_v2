# Pixel2 RTL8821CU integration

Date: 2026-08-17

Status: host integration PASS; stock-kernel load/unload PASS; adapter acceptance OPEN

## Scope

V90S validated the UGREEN AC650 / RTL8811CU adapter through the following
sequence:

```text
0bda:1a2b Realtek DISK -> SCSI eject -> 0bda:c811 -> 8821cu -> wlan0
```

Its `8821cu.ko` was built for kernel 4.9.191 and cannot be copied to Pixel2's
stock 5.10.198 kernel. Pixel2 therefore builds the same v5.12.0.4 driver family
from pinned source commit `96c65c58b544241178638e810b333dcc9aa26b91`.

## Stock ABI gate

The build pins Pixel2 5.10.198 source commit
`883a9e03084bf1a2f1769ad6b369f5090bbd6588`, applies `px30_linux_defconfig`,
disables the defconfig's debug-spinlock instrumentation to match the stock
kernel's inline spinlock ABI, disables `FTRACE` and `CONFIG_KALLSYMS` to match
the stock Image's module-structure layout, and rebuilds the in-tree stock
`r8188eu` probe before building `8821cu`.
The captured and rebuilt probe must both report:

```text
srcversion=33E331B2DEB16477EAAB1D6
vermagic=5.10.198 SMP mod_unload aarch64
__this_module size=0x300
cleanup_module relocation offset=0x2d0
```

The generated `8821cu.ko` must use the same vermagic and contain the
`0bda:c811` and `0bda:c820` aliases. The source build keeps power saving enabled
and USB autosuspend disabled, matching the upstream/V90S feature configuration.

Build command:

```sh
./scripts/docker-build.sh kernel-modules
```

`system-rootfs` installs the module under
`/lib/modules/5.10.198/extra/8821cu.ko`, regenerates `modules.alias`/`modules.dep`,
and includes the pinned-source manifest, required-symbol list, license, and all
checksums in the immutable System image.

## Host controller coverage

`tests/test-pixel2-network-control.sh` uses fake USB/block/net sysfs to prove:

- exact `0bda:1a2b` storage-mode recognition;
- bounded eject of only the `sr*` node below that USB device;
- wait for `0bda:c811` re-enumeration;
- alias-driven load of `extra/8821cu.ko`;
- normal scan continuation after `wlan0` appears.

## Stock loader correction

The first device-side load attempt was intentionally performed from `/tmp`
before installing any System update. It was safely rejected because the plain
`px30_linux_defconfig` build required `_raw_spin_*` and
`__raw_spin_lock_init`, which the stock Image does not export. The failed
module and its signed update package were discarded.

Disabling `CONFIG_DEBUG_SPINLOCK` makes the matching ARM64 spin operations
inline. The build and System verifier now reject any `8821cu.ko` that imports
those absent symbols. The stock Image also has no `/proc/kallsyms`; leaving the
defconfig's `CONFIG_KALLSYMS=y` changed the `struct module` layout and made a
successfully loaded driver appear permanent. Direct comparison with the stock
`r8188eu.ko` then showed a `0x340` versus `0x300` `__this_module` size and a
`0x310` versus `0x2d0` exit relocation. Disabling the defconfig's complete
`FTRACE` menu, not only `FUNCTION_TRACER`, makes both values identical. These
two structural values are now hard build gates. A repeated `/tmp` load and
clean unload is required before update deployment.

The corrected module was copied to the running device by ADB and verified as:

```text
kernel=5.10.198
module_sha256=5fb2db9bbbe46d9769c49bfd400c94e018b3a7362c16af7e2d1e7ca7005dd94f
insmod=PASS
/sys/module/8821cu=present
rmmod=PASS
usbcore=registered then deregistered rtl8821cu
```

No matching Wi-Fi adapter was connected during this loader test. It proves the
stock kernel ABI and cleanup path, not radio operation.

## Physical release gate

The public kernel tree has a partial `Module.symvers`, stock has
`CONFIG_MODVERSIONS` disabled, and the running Image does not expose
`/proc/kallsyms`. The release gate therefore uses a SHA-verified temporary
`insmod`/`rmmod` with no matching adapter connected. Only after that loader
test succeeds is a signed inactive-slot System update used, so the existing
A/B rollback remains available.

Release acceptance remains open until the real device passes all of:

1. initial `1a2b` mode switch and `c811` binding without manual commands;
2. 2.4 GHz and 5 GHz scan, association, DHCP, and gateway reachability;
3. FE Information and all enabled network-service states;
4. SSH/SFTP transfer and measured throughput/error counters;
5. dongle unplug/replug recovery;
6. saved Wi-Fi recovery after a true power-off cold boot.
