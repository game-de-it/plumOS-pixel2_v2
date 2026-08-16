# Pixel2 plumOS network-service alignment and device validation

Date: 2026-08-17

Implementation commits:

- `84a9a9a` — align SSH authentication, defaults, persistence, and service ownership
- `fd0fb34` — initialize loopback in System init and the app-layer service manager

Final Runtime: `0.1.0-dev-fd0fb34`

## Initial device state

The frontend had saved all five services as enabled:

```text
ssh_enabled=1
ftp_enabled=1
sftp_enabled=1
samba_enabled=1
adb_enabled=1
```

ADB, FTP, and Samba had live processes, but SSH reported
`waiting_credentials` and SFTP was stopped. Pixel2 required an
`authorized_keys` file before listening and defaulted only ADB to ON. This
diverged from the shared V90S/MF user contract, where SSH has a documented
public initial credential and can also accept public keys.

The Pixel2 host key was also stored under an update-managed SSH payload
directory. It therefore mixed device identity with immutable component files.

## Alignment

Pixel2 now follows the shared contract while retaining hardware-specific
boundaries:

- fresh-image defaults: SSH and ADB ON; FTP, SFTP, and Samba OFF;
- SSH: Dropbear on TCP 22, initial account `root / plumos`;
- password storage: uniquely salted SHA-512 shadow under
  `/mnt/plumos/config/ssh`, initialized only when absent;
- public-key authentication: `/root/.ssh/authorized_keys`;
- host key: persistent device-local state under `/mnt/plumos/config/ssh`;
- SFTP: the same SSH account and port;
- FTP: anonymous writable share rooted at `/mnt/plumos-user`;
- Samba: `SDCARD`, account `plumos / plumos`;
- ADB: Pixel2 FunctionFS gadget whose checkbox change remains reboot-applied.

Explicit service settings and an existing SSH password remain device-local and
are not replaced by Runtime updates.

## Loopback root cause

After the first aligned Runtime was deployed, Dropbear listened on TCP 22 but
ADB forwarding closed before the SSH banner. `strace` proved that Dropbear did
not receive an `accept`; Pixel2's minimal init had left `lo` down with no
`127.0.0.1/8` address. A temporary loopback setup immediately made password
SSH work.

System init now establishes loopback before services. The app-layer manager
also enforces it before start/restart operations, allowing the fix to take
effect on an older System slot and keeping service recovery defensive.

## Signed deployment

The final signed Runtime package was read back with the same SHA-256 on host
and device:

```text
package=plumos-pixel2-runtime-0.1.0-dev-fd0fb34.tar.gz
sha256=632a54713652557f865af3c0f03a246245819d67b6dce758e9470cc8443b6661
source_version=0.1.0-dev-84a9a9a
payload_files=12
deleted_files=0
```

The transactional updater reached:

```text
status=healthy
version=0.1.0-dev-fd0fb34
previous_version=0.1.0-dev-84a9a9a
```

Both `network-services` and `frontend` component checksum sets passed after
deployment. The saved five-service configuration survived both update boots.

## Protocol acceptance

The final Runtime booted with loopback UP and `127.0.0.1/8` assigned without a
manual command. Tests then used ADB TCP forwarding so that the USB Wi-Fi dongle
was not required for backend acceptance.

| Service | Device result |
| --- | --- |
| ADB | connected after both update reboots |
| SSH | `root / plumos` password login executed a remote marker command |
| SFTP | upload, download, delete; downloaded SHA-256 matched source |
| FTP | anonymous upload, download, delete; downloaded SHA-256 matched source |
| Samba | `plumos / plumos` upload, download, delete through SMB2; downloaded SHA-256 matched source |

The SFTP and Samba round-trip source hash was:

```text
35fb4701c05e9491182db3c39a70639ccf22b0928845321485362e26af0f1e04
```

Samba was tested with a temporary RFC 5737 address on loopback because no WLAN
interface was present. The temporary address and all remote test files were
removed. Samba was restored to `enabled=1`, `state=waiting_network`, which is
the expected state until the USB Wi-Fi adapter obtains IPv4.

## Physical USB Wi-Fi LAN acceptance

The supported `0bda:8179` adapter previously associated and obtained
`192.168.10.147`. On the final Runtime, the host connected directly over that
WLAN address and confirmed:

- SSH password login and remote command execution;
- SFTP upload, download, and delete;
- anonymous FTP upload, download, and delete;
- Samba SMB2 upload, download, and delete with `plumos / plumos`.

All three downloaded copies matched the source SHA-256:

```text
ec596f84fa018a5fa302d5a8d7b1facad47f5e6a3d2820009b27b3d92e0bab95
```

The device-side validation files were absent after each protocol test. This
closes protocol reachability and data-integrity acceptance. It does not close
bulk-transfer performance acceptance; the separate throughput investigation
is recorded in
[`2026-08-17-pixel2-usb-wifi-throughput.md`](2026-08-17-pixel2-usb-wifi-throughput.md).
