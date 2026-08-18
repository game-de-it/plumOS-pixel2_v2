# Pixel2 V90S-derived ADB alignment

Date: 2026-08-18
Status: host implementation PASS; second offline deployment PASS; physical acceptance pending

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
- an offline ADB transport schedules one bounded replug per offline episode;
- recovery never changes the port to host role;
- hardware-key/recovery output is persistent under
  `/mnt/plumos/logs/hardware-keys.log`.

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
marker, and the System startup watchdog replugs a transport that reports
`offline` even while the UDC still reports `configured`.

## Host gates

The fixture covers healthy recovery, stale UDC rebind, action serialization,
normal cold boot reclaiming device role, saved Wi-Fi selecting host role, the
ADB UI toggle creating/removing the ownership marker, transport-offline startup
replug, and the FAT recovery marker overriding saved Wi-Fi. The existing USB-host reset,
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
