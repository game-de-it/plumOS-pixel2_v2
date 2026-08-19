# plumOS Pixel2

[English](README.md)

GKD Pixel2（RK3326S）向けplumOSを、再現可能なbuildと検証可能な
component metadataから構築するプロジェクトです。

現在採用しているboot境界は、Pixel2 stockのRockchip boot prefix、kernel
5.10.198、内蔵initramfs、checksum登録済みruntime DTBをbyte-for-byteで
boot substrateとして保持し、stock initramfsが
`/boot/SYSTEM`へhandoffした後の`/sbin/init`以降をplumOSが所有する構成です。
stockの通常userland、frontend、service、設定、テーマは実行時に使用しません。

## 現在の実装

- plumOS `SYSTEM` SquashFS、init、USB Wi-Fi、SSH、persistent logs;
- Pixel2 frontend、START menu、ROM scanner、physical input contract;
- RetroArchとPixel2対応112 libretro core、PicoArch;
- OpenBOR、DraStic、PPSSPP standalone;
- Python 3.11、Pyxel、pygame runtime;
- RK817/USB向けplumOS audio routing、音量・輝度service;
- component manifest/checksum付きapp-layerと4 GiB SD image builder。

実装済みcomponentと実機合格は同じ意味ではありません。公開済みだがbackendが
不足するFE項目、未実装Apps、standalone、update、全system実機試験は
[Pixel2 Implementation Inventory](docs/developer/implementation-status.md)と
[TODO](TODO.md)で追跡します。

## Build

Docker Desktopまたは互換Docker engineと、stock Pixel2から読み取り専用で
採取したboot artifactが必要です。private stock captureはGitへ追加しません。

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh retroarch
./scripts/docker-build.sh core-catalog --filter all --concurrency 4
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone
./scripts/docker-build.sh pyxel-runtime
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh audit
./scripts/docker-build.sh system-rootfs
./scripts/docker-build.sh sd-image
```

成果物は`output/`へ生成されます。SD imageは約2.5 GiBのcompact seedで、
stock-compatible boot prefix、512 MiB `PLUMOS_BOOT`、2048 MiB ext4
`PLUMOS_SYS`だけを収録します。16 GB以上のSDで初回起動すると、`PLUMOS_SYS`を
8192 MiBへ拡張し、残り全域にFAT32 `PLUMOS_USER`を作成します。kernelがmount中の
partition更新を受け付けない場合は、初回setup中に一度だけ自動再起動して処理を
再開します。ROMやBIOSをmacOSから配置するのは、この初回起動完了後です。

Pixel2の単一USB portはUSB Wi-Fi dongle専用です。plumOS Pixel2はADBを搭載せず、
Wi-Fi接続後のSSH/SFTPを保守経路にします。

`release-image`は実装監査のrelease blockerが0になるまで失敗します。開発中の
実機試験には`sd-image`を使い、書き込みにはstock SDとは別のカードを使用します。

## Validation

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./scripts/audit-pixel2-implementation.py --release-gate
```

host上のbuild/checksum成功だけでは、Pixel2のLCD回転、入力、音声、frame pacing、
終了、save、power動作を保証しません。実機証跡は`docs/validation/`へ記録します。

開発者向けの入口は[Developer Guide](docs/developer/README.md)です。
