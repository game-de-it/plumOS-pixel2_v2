# 検証と根拠資料

[English](validation.md)

## 根拠の規則

検証記録は`docs/validation/`へ置きます。source commit、生成hash、実行command、実機上の
根拠、物理的な観測、残存riskを区別して記録します。

host build成功や`output/`にfileが存在するだけではPixel2動作の証明になりません。
process identity、mount所有、checksum、framebuffer/input owner、Wi-Fi/SSH状態、power動作、
物理button、画面、音を必要なacceptanceごとに確認します。自動launch smokeと、利用者が
行う操作・画面・音・saveの目視確認も区別します。

## 現在の主要根拠

- [Host build](../validation/2026-08-11-plumos-host-build.md)
- [Stock boot substrate](../validation/2026-08-12-stock-boot-substrate-image.md)
- [Stock initramfs handoff](../validation/2026-08-12-stock-initramfs-handoff.md)
- [Frontend input](../validation/2026-08-12-pixel2-frontend-input.md)
- [Power management](../validation/2026-08-12-pixel2-power-management.md)
- [Global power menu and sleep](../validation/2026-08-15-pixel2-global-power-menu-sleep.md)
- [Sleep/resume machine matrix](../validation/2026-08-23-pixel2-sleep-machine-matrix.md)
- [START menu](../validation/2026-08-12-pixel2-start-menu.md)
- [Implementation audit](../validation/2026-08-13-pixel2-implementation-audit.md)
- [User BIOS staging](../validation/2026-08-13-pixel2-user-bios.md)
- [PortMaster and Ports](../validation/2026-08-17-pixel2-portmaster-ports.md)
- [PortMaster共通互換レイヤー](../validation/2026-08-27-pixel2-portmaster-compatibility.ja.md)
- [Release candidate image](../validation/2026-08-23-pixel2-release-candidate-image.md)
- [First Wi-Fi connection](../validation/2026-08-23-pixel2-first-wifi-connect.md)
- [Neo Geo repeated launch/exit](../validation/2026-08-23-pixel2-neogeo-loop.md)
- [v0.1.3実機アップデート](../validation/2026-08-29-pixel2-v0.1.3-device-update.ja.md)
- [v0.1.4 release artifact](../validation/2026-08-30-pixel2-v0.1.4-artifacts.ja.md)
- [v0.1.4実機updateとRockbox](../validation/2026-08-30-pixel2-v0.1.4-device-update.ja.md)

日付付き記録には途中の失敗や廃止方式も意図的に残します。現在の契約はsource、test、
`docs/decisions/`、生成manifest、developer guideの順に確認してください。

## Release gate

release前に最低限、次を要求します。

1. cleanなgit status
2. `audit-pixel2-implementation.py --release-gate`でblocker 0
3. strict app-layerとSYSTEM build成功
4. image検証と再現可能hash
5. release payloadにROM、BIOS、credential、stock抽出入力を含めない
6. 実機cold bootからFE起動
7. LCD向き、全入力、音量、明るさ、音声、reboot、shutdown
8. 代表emulatorのlaunch/exitとsave保持
9. 対応USB Wi-Fi dongle 1種類以上でnetwork maintenance path

実装・実機検証inventoryの正本は
[Pixel2実装一覧](implementation-status.ja.md)です。
