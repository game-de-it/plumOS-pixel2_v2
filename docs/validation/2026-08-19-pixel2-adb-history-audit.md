# Pixel2 ADB history audit

Date: 2026-08-19
Scope: repository start through the persistent post-Wi-Fi ADB regression

## Questions

1. Did stockOS itself provide a working ADB service?
2. Which boot substrate first provided working plumOS ADB?
3. Did plumOS ADB work with the exact stock `Image`, DTB and kernel?
4. Which changes made host-mode ownership part of the ADB lifecycle?

This audit distinguishes a host build, frontend status, UDC state and parent
USB enumeration from the stronger evidence of a real `adb shell`, live command
output, or files pulled through that shell.

## Conclusions

| Claim | Result | Evidence |
| --- | --- | --- |
| stockOS userland had working ADB | **Unknown** | The original stock `SYSTEM` was intentionally analysis-only and was not retained. |
| ADB was added by plumOS | **Proven** | `10fc87a` introduced the first repo-owned adbd/configfs/FunctionFS service. |
| First plumOS ADB success used stock kernel | **No** | The first physical success at `bd1411a` followed the plumOS Linux 6.12 work. |
| plumOS ADB later worked with exact stock boot artifacts | **Proven** | `aff4656` captured stock 5.10.198, FunctionFS enable, UDC configured and live shell output. |
| exact stock DTB requires a sysfs role switch for ADB | **Disproven** | The successful capture has an empty `[usb-role]` section; natural OTG selected peripheral mode. |
| VBUS DTB addition alone caused all failures | **Disproven** | The current failure reproduces after byte-identical stock DTB restoration. |
| Wi-Fi work changed ADB/controller ownership | **Proven** | `2e2077f`, `49a4f15` and `b1f6228` added charger-state arbitration, DWC2 host reset and VBUS ownership. |

## StockOS evidence boundary

The initial capture registered these stock identities:

```text
Image  853eb041f1042a5f54ab66143cc8babb3942936f5c5209bc0c05d439ec3bd466
SYSTEM a01afb14d7124a65787c6040575c309509c34146bc264d1d16c84eb7c5a26c8c
DTB    a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a
```

`c6c1ff6` deliberately extracted only kernel modules and firmware from the
stock `SYSTEM`; its verifier rejected a copied stock `SYSTEM` and any extracted
`usr/bin` or systemd userland. The repository retains the stock initramfs, but
that initramfs only mounts `SYSTEM` and hands off to its systemd path. It does
not establish an ADB gadget itself.

Consequently the repository cannot confirm or deny an ADB service in the
original stockOS userland. The recollection that stockOS supported USB Wi-Fi
but not ADB is compatible with the evidence, but it is not yet proven. Closing
that question requires either the original `SYSTEM` with the registered hash
or a read-only service/USB capture from an untouched stockOS card.

## Working ADB chronology

### 1. Initial plumOS implementation on the plumOS-owned kernel

- `10fc87a` added the ARM64 adbd build, configfs gadget and FunctionFS startup.
- `8f2120a` and `53d9e12` built and integrated Linux 6.12.79 plus a plumOS
  initramfs.
- `bd1411a` records the first operational ADB shell and a live frontend binary
  deployment. This proves the plumOS implementation, but not stock-kernel
  compatibility.
- `6f022d4` added a daemon-existence watchdog and became the first stabilized
  service shape.

### 2. Exact-stock `Image`, DTB and kernel

- `d18bc0e` changed release images to stock `Image`, exact stock DTB and stock
  kernel ABI 5.10.198. Historical image commit `ff35746` records those exact
  hashes; no VBUS property was present in this generation.
- `f1c58c1` supplied the fixed handoff path expected by the stock initramfs.
- Physical record `aff4656` proves the device booted the stock kernel and
  reached a real ADB shell. The retained `adbd.log` contains
  `FUNCTIONFS_BIND` followed by `FUNCTIONFS_ENABLE`; diagnostics report
  `ff300000.usb state=configured`, and dmesg records high-speed gadget address
  assignment plus `USB_STATE=CONFIGURED`.
- The same capture's `[usb-role]` section is empty. The successful service's
  `/sys/class/usb_role/*/role` loop therefore performed no write. Exact-stock
  OTG hardware/kernel detection selected device mode without a role-switch
  property.
- `5932ef9` later booted the A/B dispatcher on the same stock substrate and
  enumerated ADB in about ten seconds. `7f16e6d` used ADB through inactive-slot
  promotion and a second boot.
- `e47ce97` performed signed Runtime and System updates from a live shell;
  ADB returned on both pending and active boots.
- `0b9b609` recovered an initially unattached gadget and passed two physical
  reboots. `45b4505` then passed a physical cable unplug/replug, reaching a new
  `FUNCTIONFS_ENABLE` and working shell without reboot.

These are independent proofs that the stock `Image`, exact stock DTB and stock
5.10.198 kernel can operate plumOS FunctionFS ADB.

## Wi-Fi ownership and regression boundary

The early USB Wi-Fi frontend/service code did not invalidate the exact-stock
ADB proof. The controller ownership model changed materially in this order:

1. `2e2077f` made `/sys/class/power_supply/usb/online=0` skip ADB startup and
   leave the port for host operation. This tied ADB selection to a charger
   signal that later proved unreliable for role ownership.
2. `49a4f15` added an asynchronous unbind/bind of `ff300000.usb` to enumerate
   a saved Wi-Fi dongle at boot.
3. `b1f6228` added `/usb@ff300000/vbus-supply` to the runtime DTB so that the
   DWC2 reset could also power-cycle an inserted dongle.
4. After credentials were saved, the host re-enumeration and Wi-Fi recovery
   paths could still run while ADB was enabled. `1e065fb` later added the
   missing ADB-priority guards, confirming that two owners had existed.

ADB was not immediately and permanently lost: `8a98e3e` still demonstrated a
controlled transport recovery to a working shell. The persistent regression
began after a subsequent physical cable unplug/replug. The 2026-08-18 record
states that normal reboot and full power-off with the Mac cable attached then
remained at `waiting`; the Pixel2-specific `usb/online=0` path repeatedly left
the port in host ownership.

Later monitors, timers, FunctionFS rebuilds, synchronous I/O experiments and
DWC2 resets attempted to recover that state. Several created additional races,
but none re-established the original physical acceptance. Removing those
monitors at `dacbc83`, restoring exact stock DTB at `1f27134`, and directly
forcing device mode at `8f3e5f5` all still failed real parent enumeration.

## Design consequence

The correct recovery reference is not stockOS ADB, because stockOS ADB has not
been established. It is the physically proven plumOS lifecycle at
`aff4656`/`5932ef9`:

- exact stock `Image` and DTB;
- stock kernel 5.10.198;
- nonblocking FunctionFS adbd;
- one configfs gadget bind;
- no dependency on charger `online` for ADB startup;
- no DWC2 platform unbind/bind in the normal ADB path;
- no MMIO force-mode helper;
- no transport monitor required for cold-boot enumeration.

Before another implementation, recover the latest failed `8f3e5f5` persistent
log from SD. The next controlled build should restore that historical ADB
lifecycle byte-for-byte while retaining only the current ADB-priority guards
that prevent Wi-Fi services from touching DWC2. Wi-Fi host behavior must be
tested only after plain ADB cold boot and one real shell pass again.
