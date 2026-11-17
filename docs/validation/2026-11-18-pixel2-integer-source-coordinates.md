# Pixel2 integer scaling in source coordinates

[日本語](2026-11-18-pixel2-integer-source-coordinates.ja.md)

Implementation: `62b76ce`, patch 038. This supersedes the retained integer-size
policy described in the previous RGUI timing/geometry validation.

## Diagnosis

On QuickNES / Akumajou Densetsu (256x224), Integer ON/OFF did change Ozone's
size. The discrepancy was between backends: RGUI produced logical 334x256,
whereas GLES/Ozone produced 512x392. Both called the common integer helper
with inconsistent combinations of logical display size, panel rotation and
aspect. Consequently Axis Y did not refer to the original game's vertical axis.

The common helper now has an explicit-rotation entry point; its original entry
point retains the normal automatic rotation behavior. Pixel2 DRM uses zero
source rotation because it separately rotates software-scaled pixels. GLES
computes the integer rectangle in source/landscape coordinates and rotates
that rectangle into the portrait scanout. Non-integer fallback also uses the
supplied aspect, rather than re-reading an aspect for a different coordinate
system. Game viewport state is restored after graphical-menu rendering.

## Verification

- Full AArch64 RetroArch build passed.
- Actual-function harness `tests/test-pixel2-integer-source-coordinates.py`
  verifies NES logical 586x448 / physical 448x586, Y/X integer axes, 90/270 degree
  rectangle transforms and fullscreen viewport bypass.
- Existing `tests/test-pixel2-drm-integer.py` passed with explicit source rotation.
- Patch reverse-application check passed.
- Final binary SHA-256:
  `881016cb7ddea4fe6b10e0eefd4b036d5e55bb8b744652a458cfe4939d007ec9`.
- Scoped deployment included current device manifests/checksum entries and
  retained earlier power-menu and RGUI fixes. Backup:
  `/mnt/plumos/backups/retroarch-integer-1794927822`.

Temporary append configs selected the same ROM and integer settings in each
backend without saving them to the user's config. Early scripted comparisons
did not reach valid game frames and are not acceptance evidence. The final
comparison uses active DRM properties/captures after rendering had settled.
GLES uses the primary plane; plain DRM uses an overlay, so primary-plane black
captures alone do not describe the RGUI game.

RGUI's live plane was `x=16,y=27,w=448,h=586`, i.e. logical 586x448, before and
after menu return; RGUI itself used physical 480x640. Ozone's primary-plane
captures show the matching centered content size after integer selection and
menu return. ON/OFF and repeated Ozone menu paths were exercised. These are
machine/capture checks; final physical LCD acceptance remains with the operator.

With Axis Y, 224 source lines become 448 display lines; horizontal width is
586 because Core Provided includes non-square-pixel aspect correction. Axis
Y/X can enforce integer factors on both raw pixel axes instead.

The saved RetroArch cfg was unchanged during temporary comparisons:
`627eb7842a4a3fdceb9fb1f273429c988791de8a4fd84e860e42cbfd2a80631b`.
Test processes were stopped and FE restored. No release or SD image was made.
