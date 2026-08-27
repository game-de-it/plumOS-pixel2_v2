# Pixel2 PICO-8 Splore Validation

Date: 2026-08-27  
Implementation source refs: `ce2ba3d`, `a03cd61`, `083ab3f`  
Validated managed Runtime: `0.1.1-dev-083ab3f`

## Scope

A user report established two PICO-8 Native/Splore failures:

1. a selected BBS cartridge could not be downloaded;
2. PICO-8 exit behavior needed to return foreground ownership to the frontend.

The proprietary PICO-8 runtime remains user-supplied under
`PLUMOS_USER/roms/pico-8/aarch64`. It is not copied into source, app-layer,
release images, or update packages.

## Root cause and implementation

PICO-8 0.2.6b invoked BusyBox wget for a BBS HTTP URL. The server redirected
the request to HTTPS, after which stock BusyBox wget failed with
`Connection reset by peer`. The managed plumOS curl and CA bundle completed the
same request.

`ce2ba3d` added a PICO-only wget adapter, forced `use_wget 1`, preserved mutable
configuration, and removed matching runtime PID/executable records after a
natural exit. BusyBox ash resolved its builtin wget applet ahead of PATH, so
`a03cd61` added a PICO-process-only `system()` preload shim that rewrites only
commands beginning with an exact `wget ` to the adapter's absolute path.
`083ab3f` added query-free transfer diagnostics.

The adapter accepts only the argument subset used by PICO-8, follows redirects
with managed curl, applies bounded connect/transfer timeouts and retries, and
publishes a successful download with an atomic rename. It does not replace the
system-wide wget path.

## Source and package gates

The implementation passed:

```text
./scripts/run-pixel2-source-gates.sh
pixel2_source_gates=result-ok tests=12
```

Each live update was rebuilt as a strict app-layer transaction with matching
managed manifests and checksums. The final signed Runtime update reached
`runtime_healthy` on the device.

## Runtime compatibility

The original user runtime was PICO-8 0.2.6b:

```text
_pico8_64 sha256=a2189ef2c500d2d2d79e1e4358c3176f89505cf779d671d757de3163c4f7e
```

After the network adapter fix it downloaded `crimson_night-5.p8.png`
successfully, but PICO-8 rejected that newer cartridge as a future version.
The user then supplied the Raspberry Pi PICO-8 0.2.7 package. Its AArch64
executable was validated before an atomic user-area replacement:

```text
pico8_64 sha256=ad56e8ed1ad812cab57a7e2679b6731eba367cdd626cbf40a12b514060b597aa
ELF machine=AArch64 (0x00b7)
pico8.dat sha256=91212d55b540ef2abf9d5df7bb46fb87f41c35f3e9d108ebd8680debab020be2
```

`pico8.dat` was unchanged. The previous executable was retained in the same
user directory as `._pico8_64.plumos-backup-0.2.6b-a2189ef2` during acceptance.
No proprietary file enters the managed Runtime or v0.1.2 artifacts.

## Device acceptance

The formal `standalone:pico8` launcher started Splore with PICO-8 0.2.7. During
the observed download window, 44 cartridge files were present, 43 adapter
transfers reported success, and no adapter error was recorded. The operator
selected a downloaded title and confirmed that the game ran.

The operator then used the normal PICO-8 exit path and confirmed correct
behavior. Readback after exit showed:

```text
pico_pid=<none>
pico_pid_record=absent
pico_exe_record=absent
frontend_pid=6392
```

This accepts both reported paths: Splore download/game launch and clean return
to the frontend.
