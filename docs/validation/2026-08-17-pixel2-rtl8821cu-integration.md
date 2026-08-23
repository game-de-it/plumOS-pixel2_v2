# Pixel2 RTL8821CU integration

Date: 2026-08-17

Status: host integration PASS; stock-kernel load/unload PASS; 2.4/5 GHz
adapter acceptance PASS; SFTP upload/replug/cold-boot acceptance OPEN

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

## 2026-08-23 UGREEN driver-disk completion

The V90S-compatible mode-switch logic was already present, but Pixel2's
immutable System did not contain the `eject` executable used by that logic.
The host fixture passed only because it injected a fake eject command, so it
did not prove the release root filesystem contract.

Pixel2 now installs Debian's `eject` utility and its runtime libraries at
`/usr/bin/eject`, records the package copyright in the System license set, and
requires the executable in `verify-system-rootfs.sh`. The network controller
uses that exact path while retaining the V90S safety boundary: only an `sr*`
block node below USB ID `0bda:1a2b` is ejected; unrelated storage is untouched.

The Pixel2-only OTG release worker now waits eight seconds after the intentional
`1a2b` removal. This exceeds the complete five-second V90S re-enumeration
window and prevents a same-second race that could return the shared PHY to OTG
before `c811` appears. It still rechecks for any downstream device before
releasing host mode.

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

## Signed System deployment

The ABI correction was committed as `13ad915` and rebuilt into a signed System
package:

```text
package=plumos-pixel2-system-0.1.0-dev-13ad915.tar.gz
package_sha256=eda4eed49a019dcb15416478dd9c087c7d8583074ad4721669ed681c81ea76d8
system_sha256=90cea37621a16f339feb7942dd1887e6a579dcce6e38e4f10dab157f66b595b8
target_slot=b
```

The package and inactive slot both passed full readback. After the updater's
staging reboot, the device booted System `0.1.0-dev-13ad915`; FE readiness
promoted B to active and removed pending/attempted state. The installed System
then passed:

```text
kernel-runtime checksum=40/40
8821cu vermagic=5.10.198 SMP mod_unload aarch64
8821cu module_sha256=5fb2db9bbbe46d9769c49bfd400c94e018b3a7362c16af7e2d1e7ca7005dd94f
0bda:c811 alias=present
0bda:c820 alias=present
installed modprobe/rmmod=PASS
frontend processes=1
adbd processes=1
last update result=system_healthy
```

Slot A remains the rollback generation. The actual RTL8811CU adapter was not
connected during deployment, so RF/network acceptance remains open.

## Physical `0bda:c820` result

After the signed System deployment, a Realtek RTL8821CU adapter was connected
to Pixel2's only USB port. Unlike the UGREEN storage-mode adapter, this device
enumerates directly as a composite `0bda:c820` device and therefore does not
exercise the `1a2b -> c811` mode-switch path. It passed the direct-alias path:

```text
USB ID=0bda:c820 (802.11ac NIC)
module=8821cu
wlan0 driver=rtl8821cu
kernel=5.10.198
System=0.1.0-dev-13ad915
IPv4=192.168.10.120/24
default gateway=192.168.10.1
```

The first association used WPA2-PSK on channel 13 at 2472 MHz. Signal stayed
between -34 and -46 dBm and the reported transmit bitrate was 72.2 Mbit/s.
Twenty gateway pings had zero loss and a 1.753 ms average, with one 9.246 ms
maximum sample. The adapter advertises both 2.4 GHz HT and 5 GHz VHT support;
the same access point's `k-home-1` BSSID was visible at 5220 MHz and -45 dBm.

The 2.4 GHz throughput comparison used zero-filled data so storage contents and
user files were not involved. The SFTP round trip used a temporary file on
`PLUMOS_USER`, compared SHA-256 on both ends, and removed both copies:

| Path | Result |
| --- | --- |
| SSH device to Mac, 64 MiB | 12.70 s, about 5.04 MiB/s |
| SSH Mac to device `/dev/null`, 8 MiB | 16.18 s, about 0.49 MiB/s |
| SFTP device to Mac, 8 MiB | 2.10 s, about 3.81 MiB/s |
| SFTP Mac to device, 2 MiB | 34.739 s, about 59 KiB/s; SHA-256 matched |

`iw` initially reported power save enabled and the module reported
`rtw_power_mgnt=2`, `rtw_ips_mode=1`, and USB autosuspend disabled. A runtime
`iw ... set power_save off` comparison changed SSH upload only from about 0.49
to 0.54 MiB/s and download from about 5.04 to 5.64 MiB/s. That is not enough to
explain or repair the asymmetric SFTP result, so no persistent power setting
was changed. RX errors/drops and TX errors stayed at zero; the pre-existing TX
drop counter stayed at eight through the tests.

This proves the Pixel2-built module, `c820` alias, 2.4 GHz association, DHCP,
gateway, SSH/SFTP integrity, and 5 GHz scan capability. It does not yet accept
bulk upload performance. The same approximately 59 KiB/s SFTP upload symptom
seen with stock `r8188eu` survived a different adapter and driver. The 5 GHz
controlled comparison below separates the shared 2.4 GHz AP/channel path from
the remaining SFTP behavior.

## Physical 5 GHz comparison

The same adapter was then associated with `k-home-1` at 5220 MHz without
changing the driver or saved service settings. It kept `192.168.10.120`,
reported a 434.0 Mbit/s transmit bitrate at -48 to -49 dBm, and passed twenty
gateway pings with zero loss and a 1.465 ms average.

| 5 GHz path | Result |
| --- | --- |
| SSH device to Mac, 64 MiB | 10.28 s, about 6.23 MiB/s |
| SSH Mac to device `/dev/null`, 32 MiB | 15.19 s, about 2.11 MiB/s |
| SFTP device to Mac, 8 MiB | 2.17 s, about 3.69 MiB/s |
| SFTP Mac to `PLUMOS_USER`, 8 MiB | 25.11 s, about 0.32 MiB/s; SHA-256 matched |
| anonymous FTP Mac to `PLUMOS_USER`, 8 MiB | 3.658 s, about 2.19 MiB/s; SHA-256 matched |

This accepts the 8821CU 5 GHz radio, DHCP, gateway, SSH, FTP, and read-side
SFTP paths. The approximately fivefold SFTP-upload improvement over 2.4 GHz
also proves that the previous AP/channel path was a major bottleneck, but the
remaining SFTP asymmetry is not accepted yet.

An 8 MiB SFTP upload to RAM-backed `/tmp` took 26.09 seconds, essentially the
same as the FAT32 result, while FTP to the same user storage reached 2.19 to
3.22 MiB/s. Increasing the OpenSSH client outstanding request count from 64 to
256 changed the RAM-backed SFTP run only from 26.09 to 22.83 seconds. During a
controlled 4 MiB comparison, SFTP added 22 TCP retransmissions and four TCP
timeouts; FTP added two retransmissions and no timeout. A repeated 8 MiB SFTP
run with cfg80211 power save disabled took 23.60 seconds and added 53
retransmissions and twenty timeouts, so power save was restored to ON and no
persistent tuning was applied. The Wi-Fi netdev RX/TX error counters stayed at
zero and its TX drop count stayed at eight.

The remaining SFTP issue is therefore above storage and below ordinary
filesystem semantics: its SSH/SFTP receive traffic pattern triggers TCP
retransmission on this path, while FTP and a raw SSH stream remain usable.
This must remain a separate release item instead of invalidating the verified
8821CU driver and 5 GHz integration.

## Hotplug recovery diagnosis

Signed Runtime `0.1.0-dev-42bcb46` installed the V90S-style blocking kernel
uevent monitor and passed Runtime health promotion. On the first physical
test, the monitor was running, but the direct adapter appeared at uptime
1799.87 as `0bda:c820` and `8821cu.ko` was not loaded until the operator used
Wi-Fi OFF/ON at uptime 1817. The saved connection then recovered at
`192.168.10.120`.

The cause was an incomplete event filter: it accepted the V90S storage-mode
ID `0bda:1a2b` and a later `wlan*` add, but not Pixel2's already-observed
direct `c811/c820` USB aliases. With no general userspace modalias loader in
the minimal System, a direct USB add cannot produce a wlan add until the
external module has first been loaded. The Pixel2 helper now accepts all
three RTL8821CU USB IDs. Its existing recovery lock coalesces the USB add and
the subsequent wlan add into one bounded network-control attempt. Unrelated
Realtek IDs remain ignored.

A second audit against V90S commit `138514a` found a separate cold-boot
integration gap. V90S invokes `plumos-wifi-recovery sync` when the frontend
starts; that call both starts the monitor and schedules an initial recovery
after the boot path has settled. Pixel2 had copied the current helper but did
not call its `sync` command from the frontend. Instead, System service
`20-usb-wifi` made one earlier direct network-control attempt. Once that early
attempt reported `no_usb_wifi_dongle`, there was no later initial recovery.

Pixel2 now uses the same monitor plus initial-recovery contract from both the
early Wi-Fi service and frontend startup. Both calls are non-blocking with
respect to association/DHCP, and the existing recovery lock coalesces overlap.
The direct network-control path remains only as compatibility fallback for an
older Runtime without the shared recovery helper.

After signed System `0.1.0-dev-4b24314` became healthy, the operator replaced
the ADB cable with the direct `0bda:c820` adapter and made no frontend Wi-Fi
change. The monitor loaded `8821cu.ko`, created `wlan0`, associated with
`k-home-1` at 5220 MHz, and obtained `192.168.10.120`; SSH, FTP, SFTP, and
Samba all returned to running. This proves the automatic ADB-to-dongle
hotplug path.

The first successful recovery occupied the blocking BusyBox helper while
several USB/net add events queued. They were processed after the recovery lock
was released, causing four harmless but unnecessary connected-state recovery
calls. Recovery now checks the read-only network status after settling and
skips queued events once Wi-Fi already has IPv4. This preserves event-driven
behavior without adding a timer or polling loop.

The subsequent reboot with the adapter left attached did not power its LED and
never returned SSH. This is below Wi-Fi recovery: System starts ADB before
Wi-Fi, and the Pixel2 ADB service forced the only dual-role OTG port to
`device` whenever ADB was saved ON, even when no upstream USB VBUS existed.
Consequently the kernel could not enumerate the dongle and emitted no USB
event for the monitor.

Pixel2 now arbitrates the shared port from the hardware power-supply signal.
Only `usb/online=1` may select device role and bind FunctionFS. With
`usb/online=0`, ADB remains enabled as user intent but releases the gadget and
selects host role for Wi-Fi. The existing hardware-key transition handler can
still start/rebind ADB when a Mac/PC cable is connected later.

The first physical reboot with that arbitration installed proved the ADB
portion but exposed a second, lower-level Pixel2 difference from V90S. At
uptime 0 the DWC2 root hub was registered and ADB logged
`waiting-usb-upstream role=host`, but no downstream USB device was present in
sysfs. The initial Wi-Fi recovery therefore failed with
`stage=no_usb_wifi_dongle`. The already-inserted direct `c820` appeared only at
uptime 733 seconds when it was physically unplugged and replugged; association
and DHCP then completed normally at `192.168.10.110`.

A bounded live probe unbound only `ff300000.usb` from the `dwc2` platform
driver, waited two seconds, and rebound the same controller. The inserted
adapter re-enumerated as `0bda:c820` within one second and the saved Wi-Fi path
returned to `192.168.10.110` without a physical replug. The System now performs
that controller-local reset asynchronously before Wi-Fi startup only when all
of these conditions hold: Wi-Fi is saved ON with credentials, upstream USB
VBUS is absent, and no downstream USB device is already enumerated. A Mac/PC
ADB cable and an already-working USB device are explicit no-op paths.

The signed System carrying that service booted from slot A and became healthy.
Its log proves the worker unbound DWC2 at uptime 5.5 seconds and rebound it at
7.6 seconds, but the downstream device was still absent. Kernel diagnostics
explain the difference from the successful live reset: the exact stock runtime
DTB has no `vbus-supply` on `/usb@ff300000`, so DWC2 reports a dummy regulator
and cannot remove 5 V from a device that remained powered across reboot. The
physical replug at uptime 501 seconds immediately enumerated `0bda:c820`.

The release DTB is therefore generated from the checksum-registered stock DTB
with exactly one property added: `/usb@ff300000/vbus-supply` points to the
existing RK817 `OTG_SWITCH` phandle. Both trees are decompiled and a strict
gate removes that one line and requires identical DTS text afterward. The
stock `Image`, embedded initramfs, kernel ABI, U-Boot DTB, and runtime-DTB
`dr_mode = "otg"` remain unchanged. No forced host role is introduced, so ADB
and charging behavior remain physical release gates alongside saved-Wi-Fi
boot recovery.

Commit `b1f6228` generated runtime-DTB SHA-256
`89a32c94ebfae5983b1cf98209aaf8f11a6d8d2f7d29d66008d35125a2e328dc`.
It was installed with a verified stock backup at
`/flash/rk3326s-gkd-pixel2.dtb.stock-a7a438f7`, then read back before `/flash`
was returned to read-only. With the 8821CU inserted throughout a safe reboot,
the unmodified stock kernel enumerated `0bda:c820` at uptime 2.05 seconds,
loaded `8821cu` at 9.38 seconds, reported `wlan0` ready at 10.25 seconds, and
restored saved-SSID address `192.168.10.110` without a physical replug. The
boot re-enumeration service correctly skipped its later reset because the
downstream device was already present. This closes the saved-Wi-Fi reboot
gate; upstream ADB and charging-state reboot remain separate physical gates.

A subsequent true power-off cold boot passed with the same direct-`c820`
adapter left inserted. The stock kernel enumerated `0bda:c820` at uptime 2.05
seconds, loaded `8821cu` at 8.97 seconds, and reported `wlan0` ready at 10.07
seconds. The saved 5 GHz `k-home-1` network associated automatically with
`192.168.10.110`. Five gateway probes had zero loss, RX/TX error counters were
zero, and the boot re-enumeration service again skipped because the downstream
device was already present. This closes the saved-Wi-Fi cold-boot gate for the
tested direct-`c820` adapter.

## Physical release gate

The public kernel tree has a partial `Module.symvers`, stock has
`CONFIG_MODVERSIONS` disabled, and the running Image does not expose
`/proc/kallsyms`. The release gate therefore uses a SHA-verified temporary
`insmod`/`rmmod` with no matching adapter connected. Only after that loader
test succeeds is a signed inactive-slot System update used, so the existing
A/B rollback remains available.

Release acceptance remains open until the real device passes all of:

1. initial `1a2b` mode switch and `c811` binding without manual commands (the
   tested adapter is a direct `c820` device);
2. 2.4/5 GHz association, DHCP, and gateway reachability (pass on the tested
   direct-`c820` adapter);
3. FE Information and all enabled network-service states;
4. SSH/SFTP transfer and measured throughput/error counters;
5. dongle unplug/replug recovery (pass on the tested direct-`c820` adapter);
6. saved Wi-Fi recovery after a true power-off cold boot (pass on the tested
   direct-`c820` adapter without a physical replug).

## 2026-08-23 UGREEN RTL8811CU driver-disk release candidate

The V90S `0bda:1a2b -> eject -s /dev/sr0 -> 0bda:c811` contract was audited
against Pixel2. Pixel2 already carried the bounded mode-switch logic and the
`8821cu` aliases, but its immutable System did not contain the `eject` binary;
the host fixture had hidden that release-rootfs omission with a fake helper.
System now ships `/usr/bin/eject` and its complete ARM64 dependency closure,
the production control path names that binary explicitly, and the rootfs
verifier requires it. The delayed OTG release was also moved from five to
eight seconds so it cannot race the five-second driver-disk transition.

Signed System and Runtime packages from commit `3e60e41` were applied to the
physical Pixel2 through the A/B updater:

- System package SHA-256:
  `7d8f079876e9c7d4eed0dc4b269532529251f9ea626b92ebce67f8119e5e1bc6`
- Runtime package SHA-256:
  `90a7ac9f941ebbfb8bc9289c656a786da3ba1e7eacaae887bb7694aec966b4c7`
- booted System and Runtime version: `0.1.0-dev-3e60e41`
- post-boot app-layer verification: `runtime_verify=result-ok`
- installed helper: `eject from util-linux 2.38.1`
- managed `plumos-network-control` SHA-256:
  `e1b22bb8e1d81f5bdf090335d4f68d550ea71cb5522b13858d31bffc0f3fd2bf`

The currently attached direct-`c820` adapter loaded `8821cu`, restored
`wlan0` at `192.168.10.110`, brought up SSH/FTP/SFTP/Samba, and returned to
the frontend after both update reboots. This proves that the release
candidate preserves the already-accepted direct-adapter path. Physical UGREEN
acceptance remains open until the actual adapter proves `1a2b -> c811`,
association, DHCP, unplug/replug, transfer, and cold-boot restoration.
