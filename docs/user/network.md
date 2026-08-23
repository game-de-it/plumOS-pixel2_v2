# Network and USB Connections

Pixel2 has no built-in Wi-Fi. Connect the supported USB Wi-Fi adapter to use
LAN services. Pixel2 dedicates its single USB data role to Wi-Fi; ADB is not
included in the current plumOS architecture.

## Connect to Wi-Fi

1. Open `START -> Network Settings`.
2. Turn Wi-Fi on.
3. Open `Connect Wi-Fi`, scan, and select the SSID.
4. Enter the password and confirm.
5. Check the IP address under `Network Settings -> Information`.

The currently validated legacy adapter is USB ID `0bda:8179`, using the
`r8188eu` driver. plumOS also includes the V90S-proven UGREEN AC650 / RTL8811CU
path. This adapter can first appear as `0bda:1a2b Realtek DISK`; turning Wi-Fi
on automatically ejects only that virtual driver disk, switches it to
`0bda:c811`, and loads the Pixel2-built `8821cu` driver.
Adapters that enumerate directly as `0bda:c811` or `0bda:c820` use the same
driver. The configuration is saved only after association and IPv4 acquisition
succeed. Pixel2 physical acceptance of this second adapter remains required
before release.

## Network services

Open `START -> Network Settings -> NW Service`. A checkbox changes the live
service and saves the same state for later boots. On a fresh image, SSH
defaults to ON; the other transfer services default to OFF.

Replace `PIXEL2_IP` below with the address shown on the information screen.

| Service | Address | User | Initial password |
| --- | --- | --- | --- |
| SSH | `ssh root@PIXEL2_IP` (port 22) | `root` | `plumos` |
| SFTP | `sftp root@PIXEL2_IP` (port 22) | `root` | Same as SSH |
| FTP | `ftp://PIXEL2_IP` (port 21) | Anonymous | None |
| Samba | `smb://PIXEL2_IP/SDCARD` | `plumos` | `plumos` |

SSH and SFTP share one device-local password. To change it from an SSH
shell, run `/mnt/plumos/bin/plumos-ssh-password set` and enter one password
line. A changed password and the SSH host key persist across OS updates.
Public-key authentication uses `/root/.ssh/authorized_keys`.

The initial credentials are public. Use LAN services only on a trusted home
network and turn off services that are not needed.
