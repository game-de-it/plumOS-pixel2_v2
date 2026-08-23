# Pixel2 RC1 SD dropout investigation

## Incident

After exiting Neo Geo through RetroArch/FBNeo, the frontend became very slow.
START reported `menu entry is empty`, SELECT could still show the core list,
and ROM launch stopped reacting. The global power menu remained visible, but
reboot and shutdown did not execute. Sleep briefly blanked the display and
returned. At the same time the UGREEN LED and Wi-Fi transport disappeared.

The device was force-powered off only after the ordinary power paths had become
unusable. The next power-on showed `NO SD`. Removing and reinserting the card
allowed the same RC1 installation to boot and reconnect at `192.168.10.107`.

## Evidence

The persistent RetroArch log ends at the Neo Geo launch and has no matching
exit record:

```text
retroarch=result-start system=neogeo core=fbneo_libretro.so rom=aof.zip
```

Reboot and shutdown requests never reached the persistent power log. This is
consistent with the frontend and already-loaded processes remaining partly
alive while SD-backed configuration, launchers, or content were unavailable.
It is not the signature of an ordinary FBNeo process leak.

The first boot after physically reseating the card recorded a failed initial
SDR104 tuning attempt before the kernel retried successfully:

```text
dwmmc_rockchip ff370000.dwmmc: All phases bad!
mmc0: tuning execution failed: -5
mmc0: error -5 whilst initialising SD card
dwmmc_rockchip ff370000.dwmmc: Successfully tuned phase to 360
mmc0: new ultra high speed SDR104 SDXC card at address 0001
```

The card exposes no identifiable manufacturer or OEM:

```text
name=USD
manfid=0x000000
oemid=0x0000
serial=0x00002567
cid=000000555344000020000025670196e1
capacity=58.2 GiB
```

The combination of `NO SD`, recovery after physical reseating, failed tuning,
and a generic zero-manufacturer CID makes the card or its electrical contact
the primary cause. A slot fault cannot be excluded without repeating the image
on a known-good branded card.

## Read-only recovery checks

After reseating, all three expected partitions mounted at their final geometry.
Read-only checks reported:

```text
PLUMOS_BOOT fsck: no structural error
PLUMOS_USER fsck: live/forced-off dirty bit only; no repair performed
Runtime verify: result-ok
System A squashfs: match=yes
System B squashfs: match=yes
Neo Geo aof.zip SHA-256 read: pass
new mmc I/O errors during full Runtime read: 0
```

The recovered runtime had exactly one frontend, one hardware-key daemon, no
RetroArch/FBNeo process, no stale power lock, four `ondemand` CPU governors,
and a working scripted START -> POWER path. The incident therefore left no
confirmed persistent plumOS payload corruption.

## Release decision

This physical SD is excluded from final release acceptance. Do not change the
stock DTB or lower the SD clock based on one unidentified marginal card: a
`NO SD` failure before Linux owns the device cannot be repaired by the plumOS
app layer. Reflash the existing RC1 image to a known-good branded SD and repeat
the cold-boot, Neo Geo exit, START/ROM launch, Wi-Fi, and safe power checks.

