# Pixel2 DRM cursor-plane cleanup

## Symptom

After using PortMaster, a white mouse pointer remained at the physical
upper-right corner in both PortMaster and the plumOS frontend. No power-menu
overlay or other display process was active.

## Device evidence

The active frontend owned `/dev/dri/card0`. A universal-plane capture showed:

```text
plane=58 crtc=80 fb=100 width=480 height=640 format=XR24 crtc_xy=0,0
  property=type value=1
  property=zpos value=0
plane=69 crtc=80 fb=98 width=64 height=64 format=AR24 crtc_xy=0,0
  property=type value=2
  property=zpos value=2
```

The ARGB plane contained SDL's standard white arrow cursor. The native panel
is rotated for the user, so DRM coordinate `(0,0)` appears at the physical
upper-right. `/dev/fb0` was the expected opaque-black base frame; the pointer
was therefore not baked into either the framebuffer or the FE rendering.

## Fix contract

- PortMaster adapter 39 calls `SDL_ShowCursor(SDL_DISABLE)` when constructing
  the Pixel2 renderer and before every presentation. Pixel2 has no pointer
  input, so the PortMaster UI is controller-only.
- The frontend's DRM initialization enumerates universal planes after its
  primary modeset and disables only active planes whose immutable `type`
  property is `DRM_PLANE_TYPE_CURSOR`. Primary and overlay planes are left
  untouched.
- The FE cleanup is intentionally independent of PortMaster. Any SDL/KMS
  application that leaves a hardware cursor behind is cleaned at the common
  application-to-FE handoff.

## Acceptance

- Host component and PortMaster adapter tests must pass.
- On hardware, cursor plane 69 must be inactive in FE.
- PortMaster must show no pointer, and returning to FE must not restore one.

The signed Runtime delta was deployed to the device as
`0.1.0-dev-cc6dbfb`. Component checksum verification passed for both the
frontend (198 entries) and PortMaster (172 entries), followed by the full
Runtime verification and healthy-state transition.

The live universal-plane captures then established the complete handoff:

```text
frontend after update:
plane=58 crtc=80 fb=... width=480 height=640 format=XR24 type=1 zpos=0

PortMaster active:
plane=58 crtc=80 fb=96 width=480 height=640 format=XR24 type=1 zpos=0

frontend after PortMaster:
plane=58 crtc=80 fb=99 width=480 height=640 format=XR24 type=1 zpos=0
```

No active `type=2` cursor plane was present in any of the three captures.
PortMaster ran through its managed launcher, and the frontend returned as PID
9868 after the test. This closes the device-side DRM-plane acceptance gate;
the remaining user-visible check is that the physical LCD no longer shows the
pointer.
