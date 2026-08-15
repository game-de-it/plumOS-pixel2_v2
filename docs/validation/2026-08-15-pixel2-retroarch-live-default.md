# Pixel2 RetroArch live configuration as factory default

## Goal

Pixel2実機で物理操作、hotkey、OSD、save関連の動作確認を終えた
`/mnt/plumos/config/retroarch/retroarch.cfg`を、今後のfactory defaultおよび
RetroArch component build時のdefault cfgとして固定する。

mutableな利用者設定を無条件に上書きする設計には変更しない。初回起動時はfactory
cfgをinstallし、既存cfgには`plumos-retroarch-config-merge`が不足keyだけを補う。
Factory Reset用ABIにも同一cfgを収録する。

## Source and build result

実機cfgをbyte-for-byteで
`package/retroarch-pixel2/retroarch.cfg`へ採用し、commit `72f42e5`で固定した。

- settings: 3,376 pairs / 3,376 unique keys
- SHA-256: `231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280`
- testは設定数、key重複、主要Pixel2 contractおよび上記SHA-256を検証する

同じcommit/versionから次をbuildした。

```text
PLUMOS_PIXEL2_VERSION=0.1.0-dev-72f42e5 ./scripts/docker-build.sh retroarch
retroarch_component=result-ok

PLUMOS_PIXEL2_VERSION=0.1.0-dev-72f42e5 ./scripts/docker-build.sh frontend
frontend_component=result-ok

PLUMOS_PIXEL2_VERSION=0.1.0-dev-72f42e5 ./scripts/docker-build.sh app-layer --strict
app_layer_verify=result-ok
app_layer=result-ok strict=1
```

source、RetroArch build output、strict app-layerの通常factory cfg、Factory Reset用cfgは
すべて上記SHA-256と一致した。launcherがゲーム起動時に生成するsystem別append configは
従来どおりsave/state directoryなどを上書きするため、factory内の最後に保存されたsystem
値が他systemへ固定されることはない。

## Signed device update

deviceの現Runtime `0.1.0-dev-70357bb`をbaseとして署名Runtime updateを生成した。

```text
package=plumos-pixel2-runtime-0.1.0-dev-72f42e5.tar.gz
sha256=604da39ed6da687928d3ed2fdb058a6c028826f20181aef1f6421972c61d647a
base_version=0.1.0-dev-70357bb
new_version=0.1.0-dev-72f42e5
payload_files=12
deleted_files=0
```

更新packageにはmanaged factory defaultsと対応するcomponent/root metadataを含めた。
mutable `/mnt/plumos/config/retroarch/retroarch.cfg`はpayloadに含めていない。
inspect成功後にsafe rebootし、更新結果を確認した。

```text
runtime_version=0.1.0-dev-72f42e5
update_result=runtime_healthy
frontend=/mnt/plumos/bin/plumos-frontend-pixel2 pid=877
adbd_status=running gadget_bound=1 udc_state=configured
```

## Device acceptance

再起動後、実機の3つのcfgは同一だった。

```text
231ee258...  /mnt/plumos/config/retroarch/retroarch.cfg
231ee258...  /mnt/plumos/factory-defaults/retroarch/retroarch.cfg
231ee258...  /mnt/plumos/factory-defaults/ra/config/retroarch/retroarch.cfg
active_pairs=3376 active_unique=3376
```

旧factory markerから新factory markerへの移行はmerge helperで行った。結果は
`result-unchanged added=0`で、active cfgのSHA-256は実行前後とも
`231ee258...`から変化しなかった。

```text
marker=231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280
retroarch_checksums=ok total=59
frontend_checksums=ok total=191
```

この状態を新規install、factory reset、今後のRetroArch buildの既定値とする。
