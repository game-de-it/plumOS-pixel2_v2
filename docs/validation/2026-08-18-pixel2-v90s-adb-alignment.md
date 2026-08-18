# Pixel2 V90S-derived ADB alignment

Date: 2026-08-18
Status: third physical acceptance FAIL; fourth offline deployment PASS; physical acceptance pending

## Regression

System/Runtime `0.1.0-dev-8a98e3e` recovered a logical FunctionFS transport
loss while VBUS stayed present, but a subsequent physical unplug/replug left
the frontend ADB status at `waiting`. Normal reboot and full power-off cold
boot with the Mac cable attached did not recover it.

The Pixel2-only service used `/sys/class/power_supply/usb/online=0` to change
the shared OTG port to host role. Once host role was selected, that same
power-supply value could remain zero with the Mac attached, so every later boot
selected host again. Commit `ecf4d16` tried a bounded device-role probe before
falling back to host, but the physical device still remained at `waiting`.

## V90S reference

V90S commit `d1721a9` keeps an enabled FunctionFS gadget bound independently
of charger state. Its disconnect recovery listens for the kernel disconnect,
waits for the event to settle, and rebinds the existing gadget once. It never
changes the USB role based on a power-supply reading. V90S also keeps the ADB
log on persistent storage.

Pixel2 now follows the same contract:

- enabled ADB always requests device role and binds FunctionFS;
- `usb/online` is not an ADB start, status, or replug input;
- an `android_usb/USB_STATE=DISCONNECTED` kernel event schedules one bounded
  recover operation;
- an initial protocol `offline` state is diagnostic only and never schedules a
  timed replug;
- no startup timer interprets an unattached UDC while the host is still
  enumerating;
- recovery never changes the port to host role;
- ADB recovery output is persistent under `/state/plumos/logs/adbd.log`.

Pixel2 has one hardware-specific arbitration rule because ADB and its optional
USB Wi-Fi dongle share a single OTG port. ADB ON always owns the port regardless
of saved Wi-Fi state. Wi-Fi may assign host role only after ADB is explicitly
turned OFF. The FAT32 `plumos-enable-adb` marker remains an offline recovery
override for an explicit OFF setting.

The first physical acceptance attempt reached frontend `RUNNING`, but macOS
reported the transport `offline`. After a physical cable replug the frontend
returned to `waiting`. Toggling ADB OFF then ON exposed a separate ownership
bug: OFF removed `plumos-enable-adb`, while ON only persisted `adb_enabled=1`
and did not recreate the marker. A saved Wi-Fi configuration therefore kept
winning the shared-port arbitration. ADB ON now recreates the FAT
marker.

The second physical acceptance attempt used System `0.1.0-dev-0694c47` and
again stopped at frontend ADB `waiting`. macOS enumerated the parent
`plumOS Pixel2 ADB` device as `18d1:4ee7`, but it published no
`IOUSBHostInterface`; both ADB 35.0.2 and 36.0.2 therefore returned an empty
device list rather than `offline` or `unauthorized`. The installed Runtime was
still `8a98e3e`, whose hardware-key daemon scheduled a replug after three
seconds of protocol `offline`. System `0694c47` independently scheduled a
second startup replug after four seconds. Those two closely spaced UDC rebinds
were the initial cause hypothesis. The later `5535fa8` failure proved that this
was not sufficient: the remaining UDC-state startup watchdog could reproduce
the same destructive restart without either protocol timer.

The replacement removes both protocol-state timers. System writes the
diagnostic state to `/run/plumos/adbd-protocol.state`, so an older installed
Runtime cannot consume it, and the rebuilt Runtime removes its polling code.
System starts the V90S-style blocking kernel uevent monitor only after the
gadget is bound.

## Host gates

The fixture covers healthy recovery, stale UDC rebind, action serialization,
normal cold boot reclaiming device role, ADB ON suppressing saved Wi-Fi, the
ADB UI toggle creating/removing the ownership marker, an offline initial
protocol state causing no replug, a matching kernel disconnect causing exactly
one recover request, explicit ADB OFF allowing Wi-Fi host ownership, and the FAT
recovery marker overriding explicit OFF. The existing USB-host reset,
power/sleep, app-layer script, and System init tests remain enabled.

Physical acceptance remains open until the signed System and Runtime are
installed and the following all pass:

1. cold boot with recovery marker and Mac cable reaches `adb shell`;
2. one physical cable unplug/replug returns the same maintenance path;
3. removal of the marker plus saved Wi-Fi ON boots the tested 8821CU dongle;
4. persistent ADB and hardware-key logs explain any failed transition.

## Fifth physical attempt and single-port arbitration defect

System `0.1.0-dev-f8a5608` removed the destructive ADB startup timer, but the
next cold boot and cable replug still produced only the macOS parent device
`18d1:4ee7`; no FunctionFS interface or `adb devices` transport appeared. The
frontend could report `RUNNING` from UDC `configured`, so that label was not
accepted as transport proof.

Code inspection then found a second independent owner of the same controller.
After `10-adbd` selected device role and bound FunctionFS, boot service
`15-usb-host-reenumerate` still consulted only saved Wi-Fi state and could
unbind/rebind `ff300000.usb`. `20-usb-wifi` and the Runtime hotplug monitor also
treated saved Wi-Fi as sufficient. This race became reachable after the
physical Wi-Fi validation stored credentials and explains why ADB degraded
late in development rather than at initial bring-up.

The corrected contract is exclusive and deterministic: ADB ON suppresses the
host re-enumeration worker, the Wi-Fi boot service, and Runtime Wi-Fi recovery.
Wi-Fi scan/connect is refused with `stage=usb_owned_by_adb`. Wi-Fi can own the
port only after ADB is explicitly OFF. Host fixtures verify that saved Wi-Fi
cannot write either DWC2 unbind or bind while ADB is enabled. A new signed
System/Runtime deployment and cold-boot acceptance remain pending.

## Fifth signed package

Commit `1e065fb` was built as System and strict Runtime
`0.1.0-dev-1e065fb`. System was based on installed `f8a5608`; the Runtime delta
was based on installed `5535fa8`. The System rootfs and network services were
built in parallel, then the frontend component was rebuilt so the two changed
Runtime scripts and both component manifests/checksum lists remained atomic.

ADB recovery, USB-host reset, Wi-Fi recovery, network-control, power/sleep,
System init, full app-layer, strict app-layer checksum, and implementation audit
gates passed. Both archives passed the real updater's signature, source-version,
ABI, target, and manifest inspection:

- System archive:
  `d09809110093f364efb65439f990999281444b4c1be584dd2b6b74d30031d35c`;
- System squashfs:
  `336a109153801b64daef1c346c0fa8956605dc838951ea3d312524b495aa0485`;
- Runtime archive:
  `0ae854c136fdcdc5933287e46bbefab12acec3822406936786e5c28e61c71eb1`;
- Runtime root checksum list:
  `abeba9128fa6ca43078b870f3cd6693dc62b1f2a016795e18077002015e19a50`.

Packages are retained under `output/live/2026-08-19-adb-usb-owner/`. No live
device write was attempted because the host still had no ADB interface.

## Fifth offline deployment

The attached 62.5 GB Pixel2 card was identified as `/dev/disk4` with the
expected 536.9 MB `PLUMOS_BOOT`, 8.6 GB Linux Runtime, and 53.4 GB
`PLUMOS_USER` partitions. Active slot A read back as `f8a5608` with the
expected SHA-256 before any write. Its five managed files were preserved under
`output/live/2026-08-19-adb-usb-owner/offline-backup-system-a-f8a5608/`.

Only slot A's squashfs, text manifest, checksum, signed JSON manifest, and
signature were atomically replaced. Readback matched System SHA-256
`336a109153801b64daef1c346c0fa8956605dc838951ea3d312524b495aa0485`,
and the on-card signature verified against the packaged Pixel2 public key.
Inactive slot B was not modified and retained SHA-256
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

Runtime `1e065fb` was added to the FAT32 update inbox with readback SHA-256
`0ae854c136fdcdc5933287e46bbefab12acec3822406936786e5c28e61c71eb1`.
The existing zero-byte `plumos-enable-adb` marker was preserved. ROM, BIOS,
settings, saves, installed ports, and the Linux Runtime partition were not
modified. Because macOS could not read the ext4 Runtime version without an
interactive sudo mount, the signed delta remains updater-gated on source
`5535fa8`; a mismatch will be rejected without changing the installed Runtime.

## Second offline deployment

Commit `0694c47` was built as System and Runtime `0.1.0-dev-0694c47` after the
ADB recovery, System rootfs, and full app-layer checksum gates passed. The
signed System was installed to active slot A with readback SHA-256
`4f5553c8f51449c7bf181aa53dd5a19c4b14516c8adf1068d897fc4ea54e4a35`.
Its on-card manifest signature verified against the packaged Pixel2 public
key. Inactive slot B was left unchanged.

The Runtime delta from installed `0.1.0-dev-8a98e3e` was copied to the user
update inbox with readback SHA-256
`7fbb2f44bb5d3043a67e9c10c013466aa7469fa3e041c7d6611e455de86b8728`.
The FAT32 `plumos-enable-adb` marker was recreated as a zero-byte file. The
previous active-A generation `0be5e17` and its complete metadata were retained
under `output/live/2026-08-18-adb-usb-reclaim/` for recovery.

Physical boot of this generation failed the acceptance gate described above;
it must not be promoted as a release baseline.

## Third signed package

Commit `5535fa8` was rebuilt as complete System and strict app-layer Runtime
`0.1.0-dev-5535fa8`. The System squashfs verifier confirmed the executable
uevent helper, blocking listener, diagnostic-only protocol marker, and absence
of the startup transport-offline replug. The ADB recovery, power/sleep, System
rootfs, and full app-layer tests passed. The tool image's AArch64 BusyBox also
advertises the required `uevent` applet.

Both archives passed Ed25519 signature validation, manifest validation, every
declared payload SHA-256/size check, and archive readback checks:

- System, base `0.1.0-dev-0694c47`:
  `42a99acf8df991d8876f07d1aa3c8f03af3a63b1b239a01298b3a1873cc6b7e0`;
- Runtime, base `0.1.0-dev-8a98e3e`:
  `bdc9fd7aa844fef8390c6c8e80d2c5cae09b15ff87afedead36bffb09116986f`.

The packages are retained under
`output/live/2026-08-18-adb-uevent-recovery/`. The completed offline deployment
is recorded below; the four physical acceptance gates remain pending.

## Third offline deployment

The attached 62.5 GB Pixel2 card was identified as `/dev/disk4` with the
expected `PLUMOS_BOOT`, Linux Runtime, and `PLUMOS_USER` partitions. Active
slot A read back as failed generation `0694c47` with SHA-256
`4f5553c8f51449c7bf181aa53dd5a19c4b14516c8adf1068d897fc4ea54e4a35`
before any write. Its squashfs and four metadata files were preserved under
`output/live/2026-08-18-adb-uevent-recovery/offline-backup-system-a-0694c47/`.

Only active A's five managed files were replaced with `5535fa8`. The on-card
System image read back byte-identical at
`3522285aa0ce1041ed99f8f24f83670081ead8104ef64e8d991077bf20287cbd`,
and its loose Ed25519 manifest signature verified. Inactive B was not modified
and retained SHA-256
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

Runtime `5535fa8` was added to the FAT32 update inbox without removing older
packages. Its on-card readback SHA-256 was
`bdc9fd7aa844fef8390c6c8e80d2c5cae09b15ff87afedead36bffb09116986f`,
and the updater accepted its signature, target, source `8a98e3e`, and version.
The existing zero-byte `plumos-enable-adb` recovery marker was preserved.
ROM, BIOS, settings, saves, installed ports, and the Linux Runtime partition
were not modified by this offline operation.

## Third physical acceptance and captured cause

Cold boot of `5535fa8` again showed frontend ADB `waiting`. macOS still saw the
`18d1:4ee7` parent device but no ADB interface. The card was then mounted
read-only with ext4fuse and the complete persistent evidence was captured under
`output/live/2026-08-18-adb-uevent-recovery/capture-5535fa8-waiting/`.

The current boot is unambiguous. At `2026-08-18T14:51:08Z`, adbd opened ep0,
constructed `UsbFfsConnection`, and received `FUNCTIONFS_BIND`. It did not yet
receive `FUNCTIONFS_ENABLE`. At `14:51:13Z`, the four-second startup watchdog
observed the normal pre-enumeration UDC state `not attached` and invoked
`recover`. Its rebind failed, so it stopped adbd and started a second instance.
The second instance reached `FUNCTIONFS_BIND` at `14:51:18Z` but never reached
`FUNCTIONFS_ENABLE`. This exactly explains the host-visible parent descriptor
without an interface.

The kernel uevent helper did not initiate that sequence. The remaining startup
watchdog did. V90S `d1721a9` has no equivalent startup timer: it binds once and
waits for the host. Pixel2 now removes the watchdog entirely. The blocking
disconnect helper also calls `recover`, as V90S does, instead of the stronger
manual `replug` action. Explicit replug remains available for sleep resume and
manual diagnostics, but it is not part of normal boot or uevent recovery.

## Fourth signed package

Commit `f8a5608` was built as System `0.1.0-dev-f8a5608` with source version
`0.1.0-dev-5535fa8`. Runtime was intentionally not rebuilt because this change
only affects the System-owned ADB service and helper. System squashfs
verification, the slow-host five-second regression fixture, ADB recovery,
power/sleep, System rootfs, and app-layer regression tests passed.

The signed archive passed Ed25519 signature, manifest, declared payload size,
and payload SHA-256 verification:

- archive:
  `2ac0c0de402e55dafb157609f8857026305399c57b984ad869c42b213e79128b`;
- System squashfs:
  `fd5fdea3f659ef829409f9396787234f0d905e2de572810b8c94d0482723ccc7`.

The package is retained under
`output/live/2026-08-19-adb-enumeration-wait/`. The completed offline
replacement is recorded below; physical cold-boot acceptance remains pending.

## Fourth offline deployment

After the read-only evidence mount was explicitly unmounted, active A
`5535fa8` read back at its expected SHA-256 and its five managed files were
preserved under
`output/live/2026-08-19-adb-enumeration-wait/offline-backup-system-a-5535fa8/`.
Only active A was replaced with System `f8a5608`.

The on-card squashfs read back byte-identical at
`fd5fdea3f659ef829409f9396787234f0d905e2de572810b8c94d0482723ccc7`,
all four metadata files matched the signed package/build output, and the loose
Ed25519 signature verified. Inactive B retained SHA-256
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.
The zero-byte ADB ownership marker remained present. Runtime, ROM, BIOS,
settings, saves, installed ports, and user data were not modified.
