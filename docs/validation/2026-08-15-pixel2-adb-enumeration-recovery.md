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

