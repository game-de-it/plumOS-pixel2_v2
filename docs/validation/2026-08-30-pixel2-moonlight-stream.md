# Pixel2 Moonlight New Connection and Display Validation

Date: 2026-08-30  
Target: PortMaster adapter 53 / plumOS Runtime `0.1.4-dev-aecb3ec`

## Problem and Root Cause

The GUI returning to the frontend after selecting a paired host was not a
Moonlight binary crash. The GUI reloaded `apps.txt` before its asynchronous
`moonlight list` command had completed and attempted to stream the placeholder
entry `Load apps first`.

The connected Desktop also had red and blue exchanged. This occurred only when
Moonlight Embedded 2.7.0 used the common SDL GLES2 renderer on the stock Pixel2
KMS/Mesa stack. Interpreting the same scanout as RGB restored the expected
colors, identifying an R/B swap in the stream presentation path.

## Implementation

- Commit `fc2e986` waits for `moonlight list`, filters placeholders, and keeps
  pairing asynchronous.
- The Pixel2 GUI font now has a 28-pixel minimum.
- A pass-through shim prevents an empty `ESUDO` command from being executed.
- The installed GUI is patched atomically after a hash-named backup, and only a
  known upstream source contract is accepted.
- Commit `aecb3ec` sets `SDL_RENDER_DRIVER=software` only for
  `Moonlight New.sh`. Other PortMaster applications retain the default GLES2
  renderer.

## Validation Result

The Moonlight GUI fixture, PortMaster runtime test, app-layer script regression,
strict app-layer build, and license audit passed on the host. The managed delta
was transferred to temporary device files, verified by SHA-256, and atomically
installed. All 11,332 managed files passed checksum validation. User keys,
settings, and PortMaster-managed content were preserved.

The normal frontend route passed the following device checks:

- Upright GUI, 28-pixel minimum text, and both `Steam Big Picture` and `Desktop`
- Physical A selection of the host and Desktop, followed by H.264 stream startup
- Upright 640x480 logical output with correct colors, including operator review
- ALSA stream in `RUNNING` state with an advancing hardware pointer
- No Moonlight or launcher process after managed stop; exactly one frontend
- Zero session mounts, zero temporary session directories, and
  `session-cleanup result=clean`

SHA-256 values of the logical captures:

```text
5f2892e1dde35fb3232f2e9d9011a19d8cc9eae2c9f1c23f3c4986449a8c62e0  moonlight-gui-logical.png
5af9384615b4416052ad98bd800b59f1855508d5c767e3dd82b2bb271ccfc11f  moonlight-stream-logical.png
```

The adapter 52 GLES2 comparison capture is
`5807e65fea741240e6ff51c0c9a54fc9b0b6c8f4ad950bfe584bdbd4688d2f85` and is
retained only as evidence of the R/B swap.
