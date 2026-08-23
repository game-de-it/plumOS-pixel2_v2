# SDカードとフォルダ

[English](storage.md)

## パーティション構成

配布イメージは小さなseedとして作られ、初回起動時に最終構成を自動作成します。

| ボリューム | 形式 | 用途 |
| --- | --- | --- |
| `PLUMOS_BOOT` | FAT32 | bootファイルとA/B System。通常は編集しない |
| `PLUMOS_SYS` | ext4 | plumOS RuntimeとLinux状態。コンテンツ用ではない |
| `PLUMOS_USER` | FAT32 | ROM、BIOS、メディア、セーブ、アップデート |

macOSやWindowsから通常操作するのは`PLUMOS_USER`だけです。

## ユーザー用フォルダ

| フォルダ | 用途 |
| --- | --- |
| `roms/` | システム別のゲームフォルダ |
| `bios/` | 利用者が用意するエミュレータBIOS・firmware |
| `Images/` | 取得または手動配置した画像 |
| `Themes/` | ユーザーテーマと関連ファイル |
| `Screenshots/` | フロントエンド・エミュレータの画面画像 |
| `Music/` | 音楽プレイヤー用ファイル |
| `updates/` | 署名済みplumOSアップデート |
| `imports/`、`exports/` | ファイルマネージャーの入出力領域 |
| `plumos-logs/` | サポート用に出力される永続ログ |

`roms/`のシステムフォルダはplumOSが作成した名前を使用します。例は`FC`、`SFC`、
`GB`、`GBA`、`PS`、`NEOGEO`、`PSP`です。arcade ROMなどのzipは、各エミュレータで
展開が必要と明記されていない限りzipのまま配置してください。

## 安全な取り扱いとバックアップ

- SDカードを抜く前にシャットダウンします。
- PCからカードを抜く前にボリュームを取り出します。
- `PLUMOS_BOOT`や`PLUMOS_SYS`をrename・formatしないでください。
- カード交換やrecovery image適用前に、セーブ、ステート、画像、設定をバックアップします。
- ホスト側のfactory treeをカード全体へ上書きしないでください。セーブ、Wi-Fi認証情報、
  PortMasterゲーム、ユーザー設定が失われる可能性があります。
