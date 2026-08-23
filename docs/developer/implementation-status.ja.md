# Pixel2実装一覧

[English](implementation-status.md)

最終更新: 2026-08-23。詳細task ledgerは`TODO.md`、日付付き根拠は
`docs/validation/`にあります。この一覧は実装、機械検査、実機acceptance、release準備を
区別します。

## Status定義

| 状態 | 必要な根拠 |
| --- | --- |
| Implemented | pinned source、build target、runtime、FE route、metadata、host test |
| Host verified | 再現build、artifact audit、app-layer/image route検査 |
| Device verified | 実LCD、操作、音、終了、FE復帰、必要な永続化 |
| Release ready | 実機根拠に加え、update、法務、CI、配布、再取得gate |
| Accepted deferred | Pixel2製品範囲から明示的に保留し、欠落として隠していない |

## 実装済み製品surface

- stock RK3326 boot prefix、stock 5.10.198 kernel/DTB/initramfsと、`/sbin/init`以降を
  plumOSが所有する`SYSTEM` handoff;
- compact 3-volume image、初回`PLUMOS_SYS`拡張、既存p3をformatしないhost-readable
  `PLUMOS_USER`作成;
- FE、ROM scanner、6項目START menu、global POWER、settings、help、scraping、gallery、
  6-system grid;
- RetroArch、109 libretro core、PicoArch、PCSX-ReARMed、DraStic、PPSSPP、OpenBOR、
  Pyxel、利用者提供PICO-8 runtime;
- File Manager、Music Player、RetroArch App、Pyxel Setup、PortMaster、Update PortMaster、
  installed Ports route;
- Pixel2入力map、Function menu、回転/aspect、RA menu/language/assets、save/state/hotkey/OSD;
- RK817/USB ALSA route、global volume/brightness、内蔵speaker最大+15 dB;
- global power menu、software standby、runtime別pause/resume、reboot、未接続shutdown、
  charger接続中のstock充電mode;
- Wi-Fi優先single-port OTG、`r8188eu`、Pixel2 ABI `8821cu`、UGREEN
  `0bda:1a2b -> c811`、hotplug復旧、SSH/SFTP/FTP/Samba;
- signed Runtime transaction、immutable System dispatcher、A/B slot、readback hash、
  health promotion、rollback state machine。

利用者に見えるFE Appsとrouteはimplementation release gateの対象です。ADB、FunctionFS、
USB Mode selectorは配布しません。

## 採用範囲の決定

- SaturnはRK3326性能方針により非対応です。
- 他の無効化8 systemもPixel2製品範囲外です。
- ScummVM、EasyRPG、Flycast、NXEngine-Evoはlibretroを製品経路とし、追加SAは保留です。
- 互換test contentがないsystemと現在入手できないfirmware 2件は、合法な入力が提供される
  まで受理済み保留です。
- Linux 6.12実験はrelease用stock 5.10経路から隔離します。
- RTL8821CU転送速度調整は実測hardware上限として終了し、推測のdriver power変更はしません。

## 残るrelease作業

unchecked taskの正本は`TODO.md`です。現時点では次が残ります。

1. top-level project licenseの決定・追加
2. release payload単位のthird-party noticeとDraStic再配布条件監査
3. test、禁止content/identity、audit、manifest/checksumのCI gate
4. version artifact、`SHA256SUMS`、archive検査、GitHub再download検証
5. `TODO.md`に残るadapter/bandを含むRTL8811CU/RTL8821CU release acceptance
6. 入力が存在するenabled systemのBIOS/firmware inventory確認
7. 正式release imageでFE power経路と複製SD cold-boot hardware acceptanceを再確認

buildまたは`release-image` commandの成功だけで、実機・法務・配布gateを完了扱いにしません。

## 自動gate

```sh
./scripts/docker-build.sh audit --release-gate
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
```

visible FE surfaceにruntime、backend、profile、language/theme asset、manifest、checksumが
不足するとauditは失敗します。画面、操作、音、Wi-Fi、power、永続化の実機試験は代替しません。
