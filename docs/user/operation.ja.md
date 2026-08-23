# 基本操作

[English](operation.md)

## フロントエンド操作

| 物理ボタン | フロントエンドでの動作 |
| --- | --- |
| 十字キー | システム、ゲーム、メニュー内を移動 |
| A | 決定、システムを開く、ゲームを起動 |
| B | 戻る、キャンセル |
| X | ゲーム一覧とGallery表示を切り替える |
| Y | 選択中のゲームをFavoritesへ追加・削除 |
| START | 6項目のSTARTメニューを開く |
| SELECT | 選択中のシステム・ゲームのコア選択を開く |
| FUNCTION | フロントエンドのスクリーンショットを保存 |
| 音量 - / + | 共通音量を変更 |
| SELECT + 音量 - / + | 画面の明るさを変更 |
| 電源 | 共通電源メニューを開く |

STARTメニューには`UI Settings`、`System Settings`、`Network Settings`、
`Apps`、`HELP`、`POWER`があります。`POWER`からSleep、Reboot、Shutdown、
Cancelを選択します。

## ゲームとエミュレータメニュー

- 選択中のゲームはAで起動します。
- RetroArch、PicoArch、対応スタンドアロンではFUNCTIONでメニューを開きます。
- RetroArchはSTART + SELECTで終了し、フロントエンドへ戻ります。
- 電源ボタンはフロントエンド、ゲーム、対応アプリで利用できます。前面のプログラムを
  一時停止してからSleep、Reboot、Shutdownを表示します。

ボタン配置はPixel2向けに統一しています。エミュレータ内部のSDL番号が異なる場合でも、
物理Aが決定、物理Bが戻る動作になります。

## スリープと電源

電源メニューからSleepを選び、復帰時は電源ボタンを1回押します。ゲーム中の場合は
同じゲームプロセスへ復帰します。stock kernelがhardware sleepへ入れない場合は、
plumOSがsoftware standbyへ切り替え、画面を完全に消灯します。

SDカードを抜く前にはShutdownを使用してください。充電器を接続したままShutdownすると
stockの充電画面へ入り、未接続の場合は完全に電源OFFになります。
