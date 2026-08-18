# Pixel2 ADB enumeration recovery validation

Date: 2026-08-15
System: `0.1.0-dev-0b9b609`
Implementation commit: `0b9b609`

## Failure boundary

After the Runtime reboot, the frontend setting still contained
`adb_enabled=1`, adbd was alive, and the configfs gadget was bound. The UDC
nevertheless remained `not attached`, while adbd repeatedly logged
`timed out while waiting for FUNCTIONFS_BIND`. The old `recover` path treated
any live adbd plus bound gadget as healthy, so neither cable reconnection nor a
status check caused a rebind. A second OS reboot happened to enumerate the
same configuration successfully.

## Recovery contract

The System service now distinguishes `configured`/`suspended` from an
unhealthy bound gadget. Startup schedules a detached check after four seconds
and returns immediately, so frontend startup is not delayed. Healthy USB does
nothing. An unhealthy UDC receives one bounded unbind/rebind attempt; only if
that fails is adbd cleanly restarted once. There is no continuous boot-time
probe or unbounded retry loop.

The host fixture covers both a healthy no-op and `not attached` changing to
`configured` after rebind:

```text
pixel2_adbd_recovery=result-ok
system_rootfs_scripts=result-ok
```

## Device acceptance

A signed System package was written to inactive slot B and promoted only after
the frontend readiness proof:

```text
package=plumos-pixel2-system-0.1.0-dev-0b9b609.tar.gz
package_sha256=5cf89c51a43361509e1957d0c58e244b8c6eea07eba4eafb307df6f9dab4e59d
system_sha256=783b6ef6094183a9387146356830aa435a9e554d7e804c0cfa988bfbf9328228
result=system_healthy
active=b
booted=b
```

ADB returned after the update boot, with UDC `configured`, FE running, and the
watchdog recording `result=watchdog-healthy state=configured`. A second normal
safe reboot from active slot B returned ADB in four seconds and produced the
same healthy no-op result. The persisted user setting remained enabled.

## Physical cable replug regression and acceptance

On 2026-08-16, a normal cable unplug/replug exposed a second failure mode.
FunctionFS logged `SUSPEND`, `DISABLE`, and destruction of the host transport,
but the UDC had already returned to `configured` when the delayed recovery ran.
The generic `recover` action therefore recorded `recover-not-needed` and left
the macOS transport offline until the device was rebooted.

Commit `45b4505` separates the two contracts:

- boot watchdog `recover` remains a no-op for a genuinely healthy configured
  gadget, preserving boot time;
- a physical offline/online transition requests `replug` after 2000 ms even if
  the UDC reports `configured`;
- `replug` attempts one bounded UDC unbind/rebind and falls back to one clean
  adbd restart;
- start, stop, restart, recover, and replug share a PID-owned action lock, so
  the prior duplicate adbd/JDWP/FunctionFS collision cannot recur.

The clean commit was built as signed System and Runtime updates and promoted
only after their normal health checks:

```text
system_package_sha256=b248c77b0fbb3989d4da3899d03a2a4bc912241b75e8751f7795b5163dcd1f79
runtime_package_sha256=1f12cf9e6f738b3709f8a5f3ae43631f81606b6f3d3ec131068a3c73fba02fe8
system_version=0.1.0-dev-45b4505
runtime_version=0.1.0-dev-45b4505
result=system_healthy
result=runtime_healthy
```

The operator then physically unplugged and reconnected USB. The live service
recorded one offline/online transition pair and one replug request. The bounded
rebind did not complete on this attach, so the intended clean-restart fallback
created a new FunctionFS connection and reached `BIND`/`ENABLE`:

```text
hardware-keys: event=usb-power-transition online=0 guard_ms=1500 adb_recovery_ms=0
hardware-keys: event=usb-power-transition online=1 guard_ms=1500 adb_recovery_ms=2000
hardware-keys: action=adb-usb-replug online=1 rc=0
service=adb action=rebind reason=usb-replug state=configured
service=adb result=replug-rebind-failed state=configured action=clean-restart
service=adb result=stopped
adbd.bin USB event: FUNCTIONFS_BIND
adbd.bin USB event: FUNCTIONFS_ENABLE
service=adb result=started udc=ff300000.usb
```

ADB returned without an OS reboot. Final state was one adbd process, one
hardware-key service, UDC `configured`, policy enabled, and a working shell.
No duplicate-daemon, `Address already in use`, or UDC-bind error occurred in
the new transaction.

## Protocol transport recovery without a VBUS transition

On 2026-08-18, a longer diagnostic session exposed the remaining blind spot.
The DWC2 gadget briefly emitted `DISCONNECTED` and returned to `CONFIGURED`
while `/sys/class/power_supply/usb/online` stayed at one.  FunctionFS logged
`SUSPEND`, `DISABLE`, and destruction of `UsbFfs`, but the live adbd process and
configured UDC made the old status check report healthy.  Because VBUS never
changed, the physical-replug watcher did not run and macOS retained an
`offline` transport until the cable was removed.

Commit `8a98e3e` changes the recovery signal from VBUS alone to the ADB
protocol transport:

- the owned adbd build writes `online` or `offline` to the tmpfs marker
  `/run/plumos/adbd-transport.state` from `handle_online()` and
  `handle_offline()`;
- the existing hardware-key daemon samples that marker every 500 ms;
- `usb_online=1` plus a transport that stays offline for 3000 ms requests one
  serialized `replug` action;
- an online transition, USB removal, or natural host reconnection cancels the
  pending recovery;
- one attempt is allowed per offline episode, preserving the bounded recovery
  and duplicate-daemon contract;
- the former unconditional rebind two seconds after every physical attach is
  removed, so a successful natural enumeration is not disconnected again.

Both parts were built from the same clean commit and installed as signed
System and Runtime updates:

```text
commit=8a98e3e
system_version=0.1.0-dev-8a98e3e
runtime_version=0.1.0-dev-8a98e3e
system_package_sha256=f9055025c060d611606c6a3768a8f1ab2e10db62ec0a3661ba9a378129947e5f
runtime_package_sha256=7ab9c8cdde0660e9bb0a27728c73943830f2b2770ff5e4ccafaa92b2de6d486f
system_image_sha256=a5a5ca2ef946eb0e74b5930ae6bd5f9e768a72920b1d1bd73b3814b005c42ba2
result=system_healthy
result=runtime_healthy
runtime_verify=result-ok
```

A host ADB server restart returned inside the grace interval without changing
the device adbd PID.  A controlled UDC unbind then reproduced the failure
without removing VBUS.  The device recorded the offline marker, scheduled one
recovery, restarted FunctionFS once, and returned to a working shell in eight
seconds without an OS reboot or cable action:

```text
hardware-keys: event=adb-transport-state online=0 usb_online=1
hardware-keys: action=adb-transport-recovery-scheduled delay_ms=3000
hardware-keys: action=adb-transport-replug usb_online=1 rc=0
service=adb action=replug-restart reason=daemon-or-gadget-missing
service=adb result=started udc=ff300000.usb access=default-on-no-explicit-setting
hardware-keys: event=adb-transport-state online=1 usb_online=1
```

Final state was one adbd process, one hardware-key process, UDC `configured`,
transport `online`, and the same healthy System and Runtime versions.  A final
physical unplug/replug is retained as the operator-visible acceptance for the
natural-enumeration cancellation path.
