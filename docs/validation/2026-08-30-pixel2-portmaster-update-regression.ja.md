# Pixel2 v0.1.3 PortMaster更新regression

日付: 2026-08-30  
実装commit: `1d1195e`

## 報告内容

v0.1.3更新後にRockboxが黒画面になり、それまで動作していたBlaze of Stormも起動しない
という利用者報告を受けた。公開v0.1.3 updateを再取得してlocal release artifactとの
完全一致を確認した。payloadにはRockbox表示library、共通PortMaster launcher、FE環境
隔離、対応するapp-layer metadataが収録されている。更新済み実機からも公開v0.1.3と
同一hashをreadbackできたため、release payloadの収録漏れではない。

## 原因

v0.1.3は全portの画面取得前に共通互換監査を同期実行していた。Rockboxでは203 ELFと共通
library rootの走査に約6秒かかり、その間は黒い遷移frameだけが見える。規模の大きい商用
portではさらに長くなる可能性がある。

この起動を途中で中断すると、従来cleanupはunmount失敗時にも
`/run/plumos/portmaster/port.mounts`を削除していた。patcher、config、library、theme、
compatibility、Rockboxのbind mountが追跡fileなしで残り、以後の全portをstale mountとして
拒否する。無関係な2タイトルが更新後に同時に動かなくなる報告と一致する。

## 対策

adapter 50ではタイトル個別patchではなくPortMaster共通境界を修正した。

- compatibility auditより先にport processを開始する
- auditはport終了後のdetached advisory taskとして実行する
- 通常unmountを5回まで再試行する
- 追跡を失っても既知のPortMaster private mountを回収する
- bounded retry後もbusyなprivate mountだけをlazy detachする
- mountが消えたことを確認してから追跡fileを削除する

回収対象はPixel2 PortMaster private mountとsession固有home mountのallowlistに限定し、
任意のhost pathをdetachできないようにした。

## Host試験

`1d1195e`からPortMaster componentと全app layerを再生成した。component checksum、
app-layer verifier、shell syntaxと次の試験に合格した。

```text
portmaster_pixel2_mount_cleanup=result-ok
portmaster_pixel2_audit=result-ok
portmaster_pixel2_session_cleanup=result-ok
portmaster_pixel2_runtime=result-ok
```

署名付きv0.1.3差分は次の通り。

```text
source_version=0.1.3
version=0.1.3-dev-1d1195e
payload_files=15
deleted_files=0
package_sha256=a6c1e278f4a9e5322fb14d32404ff52009fee9c87ffe8ec7b988a851f91b5c3b
```

実機転送前後のpackage hash、production updaterによる署名と更新chain受理、safe reboot後の
`runtime_healthy`昇格、全managed Runtime verifyに合格した。

## 実機試験

Rockboxは旧監査時間の約6秒より前、最初の1秒観測内にlauncher PID、GPTokeYB2、
`rockbox` processを開始した。DRM captureは正立640x480で、黒画面ではなくRockbox main
menuを表示している。

```text
8eb01bc74a04064a17303828378b4662a40af7b69f8d6eaa78fcc96bf4e18a95  rockbox-hotfix-logical.png
```

終了後の非同期監査は203 ELF、error 0、既知のprivate `LD_PRELOAD` warning 1で完了した。
FEは1 processへ復帰し、Rockbox、PortMaster launcher、GPTokeYB、compatibility mount、
mount追跡fileの残留は0だった。追跡fileなしで意図的に作ったpatcher bind-mount fixtureも
新helperが回収した。

検証機にはBlaze of Storm本体がない。install済みGameMaker/
`gmloadernext.aarch64`代表としてTiny Rallyを使い、同期監査に待たされずruntimeが開始し、
終了時にprocess group、mount、FEが正常回収されることを確認した。ただし自動DRM captureは
黒のままだったため、これはprocess/lifecycleの証拠に限定し、同タイトルの表示合格とは
しない。Blaze of Storm本体での画面・入力・音は利用者側再試験を残す。

## 保持範囲

transactionはmanaged app-layer fileと対応checksum/manifestだけを更新した。install済み
PortMaster content、game data、設定、save、ROM、BIOS、credential、Wi-Fi設定は上書きして
いない。
