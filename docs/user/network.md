# Network connections and services

[日本語](network.ja.md)

Pixel2 has no built-in Wi-Fi. Connect a supported USB Wi-Fi adapter to use
network services. ADB is not included.

## Connect to Wi-Fi

1. Insert the Wi-Fi adapter.
2. Open `START -> Network Settings` and turn Wi-Fi on.
3. Open `Connect Wi-Fi`, scan, and select the SSID.
4. Enter the password and confirm.
5. Wait for association and IPv4 acquisition. A cold Realtek connection can
   take about 20 seconds.
6. Confirm the address under `Network Settings -> Information`.

Validated adapter paths include:

- UGREEN AC650 / RTL8811CU, which may appear first as `0bda:1a2b` and is
  switched automatically to `0bda:c811`;
- RTL8811CU/RTL8821CU adapters enumerating as `0bda:c811` or `0bda:c820`;
- legacy `0bda:8179` adapters using the stock-compatible driver.

The SSID and password are saved only after Wi-Fi association and an IP address
both succeed. Removing and reinserting a validated adapter automatically
restarts the saved connection. Because Pixel2 has one USB port, unplug Wi-Fi
before connecting a charger.

## Network services

Open `START -> Network Settings -> NW Service`. SSH defaults to ON on a fresh
image; optional transfer services default to OFF. Replace `PIXEL2_IP` with the
address shown on the Information screen.

| Service | Connection | User | Initial password |
| --- | --- | --- | --- |
| SSH | `ssh root@PIXEL2_IP` | `root` | `plumos` |
| SFTP | `sftp root@PIXEL2_IP` | `root` | `plumos` |
| FTP | `ftp://PIXEL2_IP` | anonymous | none |
| Samba | `smb://PIXEL2_IP/SDCARD` | `plumos` | `plumos` |

SSH and SFTP share one device-local password. To change it from SSH, run:

```sh
/mnt/plumos/bin/plumos-ssh-password set
```

Public-key authentication uses `/root/.ssh/authorized_keys`. Passwords, host
keys, and service choices persist across managed OS updates.

The initial credentials are public. Use network services only on a trusted LAN,
change the SSH password, and disable services that are not needed.
