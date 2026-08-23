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

対応runtimeではFUNCTIONでエミュレータメニューを開きます。RetroArchは
START + SELECTが通常の終了操作です。セーブとhotkeyは
[セーブ・ステート・スクリーンショット](save-data.ja.md)を参照してください。

## BIOSとコンテンツ

システムによっては`PLUMOS_USER/bios`にBIOSやfirmwareが必要です。plumOSは
著作権で保護されたゲームBIOSを配布しません。必要BIOS、track file、対応ROM setが
不足すると、システムが表示されていてもゲームを起動できない場合があります。

特にarcade coreはROM set revisionの影響を受けます。parent ROMと必要BIOS archiveを
揃え、zip内部のファイル名を変更しないでください。
