# Pixel2 PortMaster共通互換レイヤー

日付: 2026-08-27  
実装commit: `e56af80`、`8ad0606`、`6d7335b`

## 対象

第三者portごとにPixel2専用patchを増やさず、互換性を次の共通境界で扱います。

1. install済みpackageを実行する前の静的監査
2. portが子ELFを起動する瞬間の環境修復
3. foreground launcherの終了・失敗後のsession回収

未知の描画engineや利用者が追加する商用dataまで、機械試験だけで実機合格とはしません。
それらは実行方式ごとの代表portとして、画面・入力・音・終了を実機確認します。

## 設計の根拠

Moonlight NewにはV90S由来のFFmpeg 4.4、libevdev、子processのlibrary path対策が継承
されていました。しかしPixel2ではAvahiとnghttp2をnetwork/scraper componentへ分離して
いるため、通常のPortMaster経路では`libavahi-common.so.3`、次に
`libnghttp2.so.14`が不足しました。Sambaやscraperのlibrary directory全体は公開せず、
検証したSONAMEだけを`/run/plumos/portmaster/lib`へ投影します。

Rockbox launcherは独自SDL scalerを指定する際に`LD_PRELOAD`全体を置換し、Pixel2の
SDL/OpenGL表示補正を破棄していました。これはRockbox固有ではなく、環境変数を置換する
portすべてに起こり得る実行方式の問題として扱います。

## 静的監査

`plumos_portmaster_audit.py`は`readelf`や外部Python moduleを使わず、ELF32/ELF64の
program headerを解析します。次をJSONへ記録します。

- AArch64以外のELF、`PORT_32BIT=Y`
- launcherから参照される実行fileの再帰的`DT_NEEDED`
- 不足SONAME
- inherited `LD_LIBRARY_PATH` / `LD_PRELOAD`を保持しない代入
- direct `sudo`、`service`、`modprobe`、Pixel2非対応`systemctl`などのhost依存

標準`GAMEDIR`からinstall先を特定し、policy versionとpackage metadataのhashで結果を
cacheします。OS boot時には走らず、install後・内容変更後の初回port起動時だけ監査します。
参照される実行fileが非対応architectureまたは依存未解決なら、画面や入力を取得する前に
起動を拒否します。

## 子process実行guard

`libplumos-portmaster-exec-guard.so`を管理下のport shellへ読み込み、`execve`、
`execveat`、`posix_spawn`、`posix_spawnp`の直前で次を復元します。

- Pixel2の隔離PortMaster library path
- execution guardとSDL/OpenGL表示補正のpreload chain
- 変更不可のPortMaster session identity

port固有のpathとpreloadは指定順の先頭に残し、不足しているPixel2要素だけを末尾へ追加
します。独自scalerとPixel2補正をchainでき、install済みlauncherは書き換えません。

## 失敗sessionの回収

foreground process groupを停止した後、`/proc/<pid>/environ`のsession identity完全一致で
background化・親変更された子processも回収します。その後GPTokeYB、session mountを解放し、
FEを一つだけ復帰します。guardがexecごとにidentityを戻すため、上流scriptの環境置換で
所有権を偶発的に失いません。signal直前にもidentityを再確認してPID再利用を避けます。

## Host試験

```text
portmaster_pixel2_audit=result-ok
portmaster_pixel2_exec_guard=result-ok
portmaster_pixel2_session_cleanup=result-ok
portmaster_pixel2_runtime=result-ok
```

Moonlight型のAvahi/nghttp2依存と、Rockbox型の独自`LD_PRELOAD`置換を名前付きfixtureで
検証しています。独自scalerを先頭に残しながらPixel2必須chainが戻ること、session IDが
完全一致するprocessだけを停止することを確認しました。

## 実機deploy

source `b0706c8`からadapter 48をstrict app layerへ組み込みました。署名差分をhidden
temporary fileとしてPixel2へ転送し、atomic rename前後のSHA-256、実機updater inspect、
safe reboot applyを確認しました。

```text
runtime=0.1.2-dev-b0706c8
adapter_version=48
package_sha256=cace82b43a12347bd6675d7340c573f840c4d7df238b4d699c6e3a8b598b39bf
payload_files=12
deleted_files=0
update_result=runtime_healthy
runtime_verify=result-ok
app_layer_verify=result-ok strict=1
```

install済みport、PortMaster設定、save、ROM、credentialはtransaction対象外です。

## 実機結果

Moonlight Newは最終管理launcherの起動・停止経路に合格しました。cache済みauditは
AArch64 ELF 18件、error 0、warning 0です。LÖVE GUIはPixel2 GL回転経路を初期化し、
終了は`result=clean`、FE復帰、GPTokeYB残留0、compatibility mount残留0でした。同じ
実装系列で取得した正立640x480 GUI captureは次のSHA-256です。

```text
c0ae5693601b924a31ebd716fd319dbab5532a04bec2dbb1dcb744902d4fcdaa  moonlight-logical.png
```

Rockboxはstatic closureとprocess所有に合格しています。AArch64 ELF 203件、error 0、
private `LD_PRELOAD`の想定warning 1、Rockbox/GPTokeYB2起動、session clean終了、mount
残留0、FE一つへの復帰を確認しました。一方、adapter 48のDRM scanoutは黒1色で、表示
合格ではありません。

```text
724319c36b0ea551aa309b59afed2e46bb1255b254b235f4dcfc77d96f1c6931  rockbox-logical.png
```

Pixel2 SDL回転無効、private scaler除外、preload両順序、panel unblank、input注入でも
黒のままでした。残件をgeneric ELF closure、環境修復、preload chain、session cleanup
から切り離し、Rockbox runtimeの描画開始として継続します。その後に物理入力・音を確認
します。generic audit合格だけで任意の第三者portを物理合格とは扱いません。
