# Pixel2 PortMaster Generic Compatibility Layer

Date: 2026-08-27  
Implementation commits: `e56af80`, `8ad0606`, `6d7335b`

## Scope

The Pixel2 adapter must not grow a permanent launcher patch for every third-
party port. This change moves compatibility checks to three shared boundaries:

1. static inspection before an installed package is executed;
2. environment repair whenever the port starts a child ELF;
3. session cleanup after the foreground launcher exits or fails.

It does not claim that arbitrary commercial data or an unknown rendering
engine is physically accepted. Those still require a representative runtime-
family test on the device.

## Findings That Drove the Design

The installed Moonlight New package retained the V90S FFmpeg 4.4 and libevdev
compatibility work, but Pixel2 stores Avahi and nghttp2 in separately owned
network and scraper components. The normal PortMaster path therefore stopped
at `libavahi-common.so.3`, and adding the network directory exposed a second
missing `libnghttp2.so.14`. The complete reviewed closure is now projected by
SONAME into `/run/plumos/portmaster/lib`; the complete Samba or scraper library
directories are not added to the global search path.

The installed Rockbox launcher replaced `LD_PRELOAD` with its private SDL
scaler. That discarded Pixel2's SDL/OpenGL presentation interposers. This is a
runtime-family problem rather than a Rockbox identity problem: any port which
replaces `LD_PRELOAD` can do the same thing.

## Static Audit

`plumos_portmaster_audit.py` parses little- or big-endian ELF32/ELF64 program
headers without `readelf` or third-party Python modules. It reports:

- non-AArch64 content and `PORT_32BIT=Y`;
- the recursive `DT_NEEDED` closure for referenced executables;
- missing SONAMEs;
- launchers that replace inherited `LD_LIBRARY_PATH` or `LD_PRELOAD`;
- unsupported host commands such as direct `sudo`, `service`, `modprobe`, or
  non-Pixel2 `systemctl` operations.

The installed port root is discovered from the standard `GAMEDIR` contract.
Results are keyed by policy version plus port file metadata and cached under
mutable PortMaster state. The scan runs on first port launch after installation
or content change, not during OS boot. A referenced executable with an
unsupported architecture or unresolved SONAME is rejected before display and
input ownership changes.

## Child Execution Guard

`libplumos-portmaster-exec-guard.so` is loaded into the managed port shell and
interposes `execve`, `execveat`, `posix_spawn`, and `posix_spawnp`. Immediately
before each child image is started, it restores:

- the isolated Pixel2 PortMaster library path;
- the execution guard and SDL/OpenGL presentation preload chain;
- the immutable PortMaster session identity.

Port-provided library paths retain their requested order. Required Pixel2
display/session preloads remain first because they own the hardware boundary;
port-private scalers are chained afterward. The guard changes the child
environment only when required entries are missing, and does not modify an
installed launcher.

## Failed-session Containment

The launcher still terminates the foreground process group first. It then uses
the exact session identity in `/proc/<pid>/environ` to terminate reparented or
background children before stopping GPTokeYB, releasing session mounts, and
returning to the frontend. The identity is reinserted at every guarded exec,
so an upstream script cannot accidentally drop ownership by replacing its
environment. PID reuse is checked immediately before each signal.

## Host Evidence

The following gates pass:

```text
portmaster_pixel2_audit=result-ok
portmaster_pixel2_exec_guard=result-ok
portmaster_pixel2_session_cleanup=result-ok
portmaster_pixel2_runtime=result-ok
```

Named fixtures cover Moonlight-style Avahi/nghttp2 closure and Rockbox-style
private `LD_PRELOAD` replacement. The execution test proves that the private
scaler remains first while the required Pixel2 guard and renderer libraries are
restored. The session fixture proves that only processes with the exact session
identity are signalled.

## Remaining Device Acceptance

- build adapter 47 and the strict app layer;
- deploy it with matching component metadata and checksums;
- run the installed Moonlight New and Rockbox routes from the frontend;
- verify orientation, input, audio, exit, one restored frontend, no GPTokeYB or
  session processes, and no session mounts;
- retain the generated audit reports as device evidence.
