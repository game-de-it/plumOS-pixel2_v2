# Pixel2 PortMaster GPTokeYB Exit Recovery

Date: 2026-08-31

Source under test: `f86e4cea39ced5cfbd32a967d1dbd0af211ef773`

## Symptom and cause

After multiple buttons were pressed in Rockbox, the Rockbox display remained
visible but no longer accepted input, while GPTokeYB2 had exited. Rockbox's
control file maps `guide` (FUNCTION on Pixel2) plus `start` to GPTokeYB2's
built-in exit hotkey, which asks `pkill rockbox` to stop the watched process.

The Pixel2 `pkill` safety shim rejected arbitrary process-name kills. GPTokeYB2
still exited after that rejection, leaving Rockbox alive without its virtual
input devices. This is not a Rockbox rendering defect; it is a common session
exit failure for PortMaster apps that use GPTokeYB's built-in exit hotkey.

## Common fix

Adapter 54 implements the following contract:

- validate `gptokeyb.pid` / `gptokeyb2.pid` against the live command line;
- inspect up to eight caller ancestors and accept only a wrapper-owned GPTokeYB;
- translate that authenticated request into the existing PID-, start-time-, and
  launcher-path-checked `plumos-portmaster-port-stop stop` operation;
- continue rejecting arbitrary `pkill` calls from a normal port shell.

This stops and recovers the owned PortMaster session without granting a global
process-name kill capability or adding per-application process exceptions.

## Validation

The dynamic Linux fixture proved that the owned GPTokeYB path stops its session
process group, while the same `pkill rockbox` request from an ordinary port shell
is rejected.

```text
portmaster_pixel2_gptokey_exit=result-ok
portmaster_pixel2_runtime=result-ok
```

Strict PortMaster and app-layer builds passed. The adapter 54 managed delta was
deployed atomically to the device, where 180 PortMaster component checks and all
11,334 app-layer checks passed. Rockbox's `.resume.cfg` SHA-256 remained the same
before and after deployment.

```text
b2074cfdd05b3031670e207ea1f84c383cb0ac089c7a95ea7989349f78b2bbe5
```

Rockbox and GPTokeYB2 were observed running together on the device, and the
operator visually confirmed return to the frontend after `FUNCTION+START`.
Wi-Fi SSH was unreachable immediately after exit, so the final process and mount
readback was not captured. That uncollected readback is kept distinct from the
host session-cleanup fixture and the physical frontend observation.

