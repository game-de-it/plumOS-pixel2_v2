# セーブ・ステート・スクリーンショット

[English](save-data.md)

## RetroArch

RetroArchは通常、使用中ROMの近くへROMフォルダとcore別にセーブを保存します。
セーブRAMは定期的に書き出され、START + SELECTの通常終了では、最後のセーブと
終了時自動ステートの書き込み完了を待ってからフロントエンドへ戻ります。

| 物理ボタン | 動作 |
| --- | --- |
| FUNCTION | RetroArchメニューを開く |
| START + SELECT | フロントエンドへ終了 |
| SELECT + L | ステートをロード |
| SELECT + R | ステートをセーブ |
| SELECT + 十字キー左右 | 前・次のステートslot |
| SELECT + X | スクリーンショット |
| SELECT + Y | FPS表示切り替え |
| SELECT + L2 | スローモーション切り替え |
| SELECT + R2 | 早送り切り替え |

ステートslotは複数世代とthumbnailを保持します。終了時に自動ステートを保存しますが、
次回起動時には自動ロードしません。

正確なfallback pathとフォルダ例は
[RetroArchのセーブとhotkey](retroarch-saves-and-hotkeys.ja.md)を参照してください。

## その他の実行環境

スタンドアロン、PICO-8、Pyxel、PortMasterは、`PLUMOS_USER`内またはコンテンツ付近の
専用フォルダを使用する場合があります。ファイルをコピーする前に、エミュレータメニューや
所定のhotkeyから終了してください。書き込み中のセーブをコピーすると不完全になる可能性があります。

フロントエンドとエミュレータの画面画像は`Screenshots/`またはruntime固有の
content-local screenshotフォルダへ保存されます。

## バックアップと復元

1. POWERメニューからPixel2をシャットダウンします。
2. SDカードをPCへ接続します。
3. セーブ、ステート、画面画像、重要なapp dataを名前を変えず別ディスクへコピーします。
4. 復元時は同じフォルダとcore/profile構成へ戻します。

古いRuntime tree全体を新しいイメージへ上書きしないでください。復元するのは
セーブ、ステート、画像、コンテンツなどのdevice-owned dataだけにします。
