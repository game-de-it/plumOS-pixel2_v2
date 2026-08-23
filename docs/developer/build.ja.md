# ビルドガイド

[English](build.md)

## 必要環境

- Git
- Docker Desktopまたは互換Docker engine
- ARM64 build出力とSD image用の空き容量
- `artifacts/`へread-only採取済みのstock Pixel2 boot artifact

生成物は`output/`へ置き、stock captureとprivate入力はgitへ追加しません。

## 主なコマンド

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh kernel-modules
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

host契約検査:

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./scripts/audit-pixel2-implementation.py --release-gate
```

通常の`audit`は開発用report、`--release-gate`は利用者に見える項目のbackend、App、
launch profile、language、theme assetが不足すると失敗します。`release-image`は同gateを
自動実行し、既知作業中は`sd-image`で実機試験できます。

## Kernel境界

通常targetは登録済みstock Pixel2 5.10.198 boot substrateを使用します。
Linux 6.12実験は`experiments/linux-6.12/`へ隔離し、通常target・release入力へ
入りません。`kernel-modules`はstock ABI照合後にPixel2用`8821cu`をbuildします。

## App layer

rootは`output/app-layer/pixel2/plumos`です。root `manifest.json`、
`checksums.sha256`、`VERSION`、compatibility/ABIと、各componentのmanifest/checksumを
収録します。strict assemblyは`complete=true`、`missing_components=[]`が必要です。

libretro全体はmonolithic buildより`core-catalog --concurrency 4`を使用し、coreごとの
成果を再利用して1つのcomponentへ集約します。`standalone --filter`は反復用なので、
最終app-layer前にfilterなしで再buildします。FEでPyxelを公開する場合はlauncher、
Python/Pyxel runtime、manifest/checksumが全て必須です。

## SYSTEMと実機deploy

`system-rootfs`は次を生成します。

```text
output/system-rootfs/pixel2/payload/SYSTEM
output/system-rootfs/pixel2/payload/SYSTEM.manifest
```

live app-layerではbinaryだけを単独deployしません。変更管理ファイルとroot/componentの
checksum・manifestを同一単位で反映し、再起動前に実機SHA-256とRuntime verifyを
実行します。active設定、ROM、BIOS、save、user app dataは上書きしません。
