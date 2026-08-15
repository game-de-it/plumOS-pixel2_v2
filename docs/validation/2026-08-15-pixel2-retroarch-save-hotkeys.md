# Pixel2 RetroArch save and hotkey completion

Date: 2026-08-15

## Finding

The earlier 3,374-key Pixel2 factory port covered only the main RetroArch cfg.
It retained the old Pixel2 fallback-only saving policy, copied trigger-axis
hotkeys that do not match Pixel2's udev buttons, and omitted the factory core
options and Parallel-N64 remap. The settings UI and save/load menu entries were
visible, but the complete behavior contract had not been ported.

## Implementation

The Pixel2 factory bundle now contains:

- `retroarch.cfg` with content-local sorted saves/states and Pixel2 hotkeys;
- `retroarch-core-options.cfg` with shipped Flycast and Parallel-N64 defaults;
- `remaps/ParaLLEl N64/ParaLLEl N64.rmp` with the core remap.

The active cfg migration is generation-aware. An exact untouched incomplete
factory is replaced atomically. A cfg re-saved by RetroArch is changed only for
the twelve settings that still equal the incomplete factory values and only
when its marker names that generation. Other explicit user values are kept.
Auxiliary cfg files are installed when absent; existing values win and missing
factory keys are appended.

The BusyBox fixture covered fresh install, auxiliary-file merge with user-value
preservation, and the re-saved legacy cfg case. It migrated exactly 12 global
settings while retaining an unrelated custom save-state button.

## Signed device deployment

The strict app-layer `0.1.0-dev-68abe6c` was built from commits:

```text
68abe6c fix: migrate previously saved RetroArch defaults
03540f2 test: align RetroArch migration verification
d3cb602 fix: complete Pixel2 RetroArch factory settings
```

A signed Runtime delta from `0.1.0-dev-aa3a3ab` contained 18 managed files and
zero deletions. Host and device package SHA-256 matched:

```text
46293762565b4d442bf316df3a22b30e8b07ffa463ebfd98002f2e946577b45e
```

After the safe reboot:

```text
runtime=0.1.0-dev-68abe6c
transaction_status=healthy
runtime_pending=absent
runtime_verify=result-ok
frontend_checksums=191/191
retroarch_checksums=59/59
root_checksums=4245/4245
```

The live helper reported `result-migrated-legacy added=12`, installed 13 core
option keys and seven Parallel-N64 remap keys, and advanced the active marker to
the new factory SHA-256. The two pre-existing fallback state files remained at
the same paths with the same hashes before and after update:

```text
7d3b67db7e7b76303795e5e8ff8782a17f69737e1a4c999078a45cf375a4a764
31df8ca8a74c8fb7ec2e9774d0f76b1f1ad8099e55548a38b5d873ba59306f46
```

Physical gameplay save/load, menu operation, and post-reboot loading of a newly
written state remain operator acceptance gates; configuration presence and old
state preservation are not treated as substitutes for those checks.

## START+SELECT regression correction

The `68abe6c` factory incorrectly replaced Pixel2's previously working direct
exit chord with another device's START+SELECT menu combo. This was a regression,
not a required part of the save-layout port. Commit `612cc19` restored:

```text
input_enable_hotkey_btn = "8"
input_exit_emulator_btn = "9"
input_menu_toggle_btn = "14"
input_menu_toggle_gamepad_combo = "0"
```

The old incomplete-factory migration now changes ten save and trigger keys and
leaves Pixel2 exit behavior intact. A separate generation-aware migration
repairs only the two regressed exit/menu values in an active cfg saved by
RetroArch. Its BusyBox fixture reports `result-migrated-regression added=2` and
preserves unrelated user values.

Signed Runtime `0.1.0-dev-612cc19` was applied from
`0.1.0-dev-68abe6c`. The 13-file delta had zero deletions and package SHA-256:

```text
b55c440fa64b3717c91c79c14508b74b405ae2f375c18098293acfa335afd981
```

The device reached `runtime_healthy`; the live helper migrated exactly two
values. All six existing fallback/content-local manual and automatic state
files retained their pre-update SHA-256 hashes. Physical button confirmation
remains required after this configuration and deployment proof.
