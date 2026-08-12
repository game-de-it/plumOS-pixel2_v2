# Pixel2 implementation audit

Date: 2026-08-13
Baseline source: `4b5ad01`
Scope: repository, generated app-layer, frontend surface, build targets,
runtime helpers, emulator routes, storage/update design, and release readiness

## Commands

```sh
./scripts/audit-pixel2-implementation.py \
  --json output/validation/pixel2-implementation-audit.json \
  --markdown output/validation/pixel2-implementation-audit.md

./scripts/docker-build.sh audit \
  --json output/validation/pixel2-implementation-audit-docker.json \
  --markdown output/validation/pixel2-implementation-audit-docker.md

./tests/test-implementation-audit.sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
./tests/test-stock-capture-scripts.sh
./tests/test-kernel-scripts.sh
git diff --check
```

## Result

```text
systems_total=97
systems_enabled=88
systems_disabled=9
enabled_policy_pending=33
required_components_present=7/7
standalone_built=3
standalone_pending=7
visible_apps=1
release_blockers=22
findings=77
```

The host and Docker informational audits both completed with
`implementation_audit=result-ok release_blockers=22`. The audit unit test
proved that `--release-gate` rejects the same tree. Existing shell contract
tests and `git diff --check` passed.

The non-zero blocker count is intentional evidence that the repository is not
release-ready. It is not converted into a passing result by hiding it from the
work list. `release-image` now executes the release form of the audit after
strict app-layer assembly and stops before building `SYSTEM` or an SD image
while these blockers remain.

## Highest-risk findings

- selectable System Update, Storage Check, Factory Reset, Time Settings,
  Audio Output, and Lid Suspend surfaces do not all have Pixel2 backends;
- FTP, SFTP, and Samba are selectable but explicitly not packaged;
- ADB still starts in unauthenticated development mode;
- PSX and Saturn advertise pending standalone alternatives;
- four selectable languages and four enabled-system logos are absent;
- shared Apps parity and seven standalone binaries remain implementation work;
- 33 enabled systems still need a content-layout or frontend-policy decision.

The complete ordered work list is maintained in
[Pixel2 Implementation Inventory](../developer/implementation-status.md) and
the repository [TODO](../../TODO.md).
