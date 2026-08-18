# Pixel2 V90S-derived ADB alignment

Date: 2026-08-18
Status: host implementation PASS; second offline deployment FAIL; third signed package ready

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
  replug;
- an initial protocol `offline` state is diagnostic only and never schedules a
  timed replug;
- recovery never changes the port to host role;
- ADB recovery output is persistent under `/state/plumos/logs/adbd.log`.

Pixel2 has one hardware-specific exception because ADB and its optional USB
Wi-Fi dongle share a single OTG port. A saved-enabled Wi-Fi configuration
assigns the port to host role during normal boot. The documented FAT32
`plumos-enable-adb` marker overrides saved Wi-Fi and deterministically assigns
the port back to the V90S-style ADB gadget.

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
explain the surviving parent descriptor with the FunctionFS interface absent.

The replacement removes both protocol-state timers. System writes the
diagnostic state to `/run/plumos/adbd-protocol.state`, so an older installed
Runtime cannot consume it, and the rebuilt Runtime removes its polling code.
System starts the V90S-style blocking kernel uevent monitor only after the
gadget is bound. A genuine disconnect is coalesced, settled for one second,
and handled by one serialized `replug` action.

## Host gates

The fixture covers healthy recovery, stale UDC rebind, action serialization,
normal cold boot reclaiming device role, saved Wi-Fi selecting host role, the
ADB UI toggle creating/removing the ownership marker, an offline initial
protocol state causing no replug, a matching kernel disconnect causing exactly
one replug, and the FAT recovery marker overriding saved Wi-Fi. The existing USB-host reset,
power/sleep, app-layer script, and System init tests remain enabled.

Physical acceptance remains open until the signed System and Runtime are
installed and the following all pass:

1. cold boot with recovery marker and Mac cable reaches `adb shell`;
2. one physical cable unplug/replug returns the same maintenance path;
3. removal of the marker plus saved Wi-Fi ON boots the tested 8821CU dongle;
4. persistent ADB and hardware-key logs explain any failed transition.

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
`output/live/2026-08-18-adb-uevent-recovery/`. Offline SD deployment and the
four physical acceptance gates remain pending.
