# RetroArchのセーブとhotkey

[English](retroarch-saves-and-hotkeys.md)

## Hotkey

特記がないゲーム中hotkeyはSELECTをmodifierとして使用します。

| 物理ボタン | 動作 |
| --- | --- |
| FUNCTION | RetroArchメニューを開く |
| START + SELECT | RetroArchを終了してフロントエンドへ戻る |
| SELECT + L | ステートをロード |
| SELECT + R | ステートをセーブ |
| SELECT + 十字キー左右 | 前・次のステートslot |
| SELECT + X | スクリーンショット |
| SELECT + Y | FPS表示切り替え |
| SELECT + L2 | スローモーション切り替え |
| SELECT + R2 | 早送り切り替え |

START + SELECTはRetroArchの通常終了経路を使用するため、通常セーブと終了時自動
ステートの書き込み完了を待ちます。RetroArchメニューが必要な場合はFUNCTIONを
使用できます。

## 保存場所

標準では、RetroArchはactive ROMと同じfilesystemへROM folder・core別に保存します。
次のROMをQuickNESで使用する場合:

```text
roms/FC/Akumajou Densetsu.nes
```

おおむね次の構成になります。

```text
roms/FC/FC/QuickNES/
  Akumajou Densetsu.srm
  Akumajou Densetsu.state.auto
  Akumajou Densetsu.state.auto.png
```

通常のsave RAMは10秒ごとにflushします。save-state slotは自動indexされ、最大20世代と
thumbnailを保持します。終了時に自動ステートを保存しますが、次回起動時には自動で
loadしません。

RetroArchでcontent-local saveを無効にした場合のfallbackは次です。

```text
/mnt/plumos/saves/<system>/
/mnt/plumos/states/<system>/
```

アップデートとfactory設定migrationは、既存fallback save/stateを削除しません。
セーブデータをコピーまたは置換する前にゲームを終了してください。
