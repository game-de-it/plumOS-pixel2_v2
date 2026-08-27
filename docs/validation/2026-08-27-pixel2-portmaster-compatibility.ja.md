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

## 残る実機確認

- adapter 46とstrict app layerのbuild
- component metadata/checksumを含む実機deploy
- FEからMoonlight NewとRockboxを起動
- 画面向き、入力、音、終了、FE一つ、GPTokeYB/session process不在、mount不在
- 実機のaudit reportを検証根拠として保存
