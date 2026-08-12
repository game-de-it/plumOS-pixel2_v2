# Pixel2 ADB explicit opt-in

## Contract

- missing configuration means ADB OFF;
- `adb_enabled=1` in `/state/plumos/config/network/services.conf` enables ADB;
- `/mnt/plumos-user/plumos-enable-adb` enables recovery ADB independently of
  the runtime configuration;
- the frontend toggle writes the persistent configuration and applies on the
  next reboot so the active maintenance transport is not cut mid-session;
- disabling ADB also removes the recovery marker;
- status reports both service state and `opted_in`.

The daemon does not implement Android host-key authentication. Security is
therefore an explicit physical-media/device-setting opt-in, not host identity.

## Host evidence

- system rootfs static tests: pass;
- app-layer tests: pass;
- implementation audit release blockers: 4 before this unit, 3 after it;
- existing live device config was explicitly migrated to `adb_enabled=1` before
  building the new default-off SYSTEM contract.

## Device gates

- new SYSTEM cold boot with setting ON retains ADB;
- setting OFF prevents gadget creation after reboot;
- FAT32 recovery marker restores ADB when the setting is absent/OFF;
- UI ON/OFF state matches boot state and recovery documentation.
