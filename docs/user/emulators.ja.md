# 対応システムとエミュレータ

[English](emulators.md)

plumOSは、Pixel2用パッケージに実行環境が存在する導線だけを表示します。
実際に表示されるシステムは、`roms/`内のフォルダと互換コンテンツにより変わります。

## 実行環境

| 種類 | 主な用途 |
| --- | --- |
| RetroArch | 多くのシステムとlibretro coreを実行する標準環境 |
| PicoArch | 一部システム向けの軽量libretro環境 |
| DraStic | Nintendo DS用スタンドアロン |
| PPSSPP | PSP用スタンドアロン |
| PCSX-ReARMed | PlayStation用スタンドアロン選択肢 |
| Mupen64Plus | Nintendo 64用スタンドアロン選択肢 |
| OpenBOR | OpenBORゲームパッケージ |
| PICO-8 | 利用者が用意する公式ARM64 runtimeとcartridge |
| Pyxel | `.pyxapp`ゲームと同梱Pyxel環境 |
| PortMaster | 対応native portとPortMaster package |

Saturnは、RK3326の性能では必要な体験を満たせないためPixel2の採用対象外です。
その他の高負荷システムもゲームごとの差があり、常にフルスピードになる保証はありません。

## コアを選択する

システムまたはゲームを選択してSELECTを押すとCore Settingsが開きます。
LEFT・RIGHTでprofileを変更し、Bで戻ります。互換性や性能上の理由がない場合は
defaultのまま使用してください。

GB・GBC・GBAでは`RA: mgba`と`RA: mgba_modern`を選択できます。前者は実績のある
従来版、後者はColor CorrectionとInterframe Blendingを備えた新しい固定版です。
新コアが性能を暗黙に変えないよう、既定コアは変更していません。通常のバッテリー
セーブは共有しますが、コア間の互換性が保証されないステートセーブはmGBA Modern
専用領域へ分離します。

Nintendo 64の既定は実績のある`RA: parallel_n64`です。ゲームごとの互換性を比較する場合は
Core Settingsから`SA: mupen64plus`を選択すると、Rice GLES2を使うstandalone版で起動します。
このstandalone版はFUNCTIONで終了してFEへ戻ります。

対応runtimeではFUNCTIONでエミュレータメニューを開きます。RetroArchは
START + SELECTが通常の終了操作です。セーブとhotkeyは
[セーブ・ステート・スクリーンショット](save-data.ja.md)を参照してください。

## PICO-8

PICO-8本体は有償のため、plumOSには同梱されません。正規に入手したRaspberry Pi版の
ARM64実行ファイル`pico8_64`と`pico8.dat`を、次のディレクトリへ配置してください。

```text
PLUMOS_USER/roms/pico-8/aarch64/
```

ファイル名だけではCPU種別を判定しません。launcherはELFを検査し、AArch64版だけを
起動します。現行Sploreのcartridgeを利用する場合はPICO-8 0.2.7以降を推奨します。
古い0.2.6bでは一覧を表示できても、新しいcartridgeをfuture versionとして拒否する
場合があります。

v0.1.2以降は、SploreのHTTPからHTTPSへの遷移をplumOS管理下のcurl・CA証明書へ渡す
Pixel2専用adapterを使用します。取得したcartridge、設定、cdataはmutable user stateへ
保存され、Runtime updateで削除されません。Fake-08とRetro8もalternate coreとして
選択できます。

## BIOSとコンテンツ

システムによっては`PLUMOS_USER/bios`にBIOSやfirmwareが必要です。plumOSは
著作権で保護されたゲームBIOSを配布しません。必要BIOS、track file、対応ROM setが
不足すると、システムが表示されていてもゲームを起動できない場合があります。

特にarcade coreはROM set revisionの影響を受けます。parent ROMと必要BIOS archiveを
揃え、zip内部のファイル名を変更しないでください。
