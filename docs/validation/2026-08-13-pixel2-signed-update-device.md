# Pixel2 signed update device validation

Date: 2026-08-13
Device transport: explicit-opt-in USB ADB while charging
Target source ref: `e47ce97`

## Starting state

The device had a healthy System A generation with System version
`0.1.0-dev`, Runtime version `0.1.0-dev`, and no pending System state. Runtime
source ref was `f5a3bfc`. System B remained the previous healthy generation and
was not touched until the signed System transaction.

The installed app-layer checksum file was pulled read-only and compared with a
clean `e47ce97` app-layer. Four content hashes differed: the FE binary, FE
component manifest/checksum, and root manifest. A checksum-baseline delta also
included root VERSION/checksums plus two managed symlinks that the file-only
app-layer checksum format cannot inventory.

## Signed Runtime update

Package:

```text
name=plumos-pixel2-runtime-0.1.0-dev-e47ce97.tar.gz
sha256=996ef503836fff884275fadf9e1ce0ca511de9acaea698ff2f35bb32c48d5c7b
payload_files=8
payload_uncompressed_bytes=670462
source_version=0.1.0-dev
version=0.1.0-dev-e47ce97
```

The package was staged under `/mnt/plumos-user/updates`, read back, and passed
the updater's Ed25519 signature, Pixel2 device, vendor, named ABI, and source
version checks. The request file was committed on ext4 before safe reboot.

After early init applied the transaction, ADB returned before FE readiness
with:

```text
VERSION=0.1.0-dev-e47ce97
runtime-pending=present
journal.status=pending_health
request=missing
frontend-ready=missing
```

Only after `/tmp/plumos-fe-ready` appeared did the health service remove
runtime-pending and set the journal to `healthy`. The resulting root manifest
reported `source_ref=e47ce97`; a detached background verification completed
all 3450 `checksums.sha256` entries with zero failures. The previous VERSION
and FE binary existed below the single rollback backup, while frontend/system
settings and mutable state remained in place.

## Signed System update

Package and payload:

```text
name=plumos-pixel2-system-0.1.0-dev-e47ce97.tar.gz
package_sha256=7c997858f8f0cefdde73e88d4f05b0b61ce2191b0761e372161533d9e649b0bb
system_sha256=8ff5ed23fe9895863682ccc9f0425c7335209d86d77989077ca22340bb2d665d
source_version=0.1.0-dev
version=0.1.0-dev-e47ce97
```

The live updater verified the package, wrote only inactive slot B, verified a
complete FAT32 readback, committed its hash/manifest/signature and pending
state last, remounted `/flash` read-only, and returned reboot code 20. Early
init performed the second safe reboot without starting services on the staging
boot.

ADB returned on the pending B boot before renderer readiness with exactly:

```text
system_version=0.1.0-dev-e47ce97
active=a
pending=b
attempted=b
booted=b
frontend-ready=missing
```

Slot B readback still matched the signed payload hash and its package manifest
and signature were present. After FE readiness, state became
`active=b, pending=missing, attempted=missing, booted=b` with
`result=system_healthy`. System A remained intact at
`10a4177ac2a14b2fa6ae40b45f9b97db0085cff8dc45a28f0412114cade3b3ab`
as the rollback generation.

A further safe reboot while USB/charging was connected selected B with
dispatcher `reason=active`. At FE readiness, System and Runtime both reported
`0.1.0-dev-e47ce97`, ADB was configured, no update request/pending marker
remained, and exactly one FE process was running.

## Remaining acceptance

Both signed backends were requested through their on-device updater command so
that state could be sampled around each boot. The compiled FE System Update
menu invokes the same `request-latest` and safe-reboot path, but the physical
menu selection itself remains to be exercised. Deliberate package corruption,
Runtime transaction interruption, and unhealthy System rollback pass the host
fixtures; destructive failure injection on the physical device remains a
separate release gate.
