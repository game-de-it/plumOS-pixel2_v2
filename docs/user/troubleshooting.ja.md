# トラブルシューティング

[English](troubleshooting.md)

## フロントエンドが起動しない

- 初回セットアップやアップデート画面が完了するまで待ちます。初回だけ1回自動再起動する
  場合があります。
- `NO SD`が表示された場合は電源を切り、カードを挿し直します。改善しなければ、
  確認済みイメージを信頼できるbrand SDへ書いて試します。
- `NO SD`やkernelのMMC tuning・I/O messageが繰り返される場合は、カードまたは接点の
  問題であり、エミュレータ設定の変更では修復できません。

## ゲームが表示されない・起動しない

- ROMが`roms/`内に作成された対応システムフォルダへ入っているか確認します。
- UI Settingsの`Refresh TOP`を実行するか、正常再起動してROM listを再生成します。
- 必要BIOSが正しい名前とhashで`bios/`にあるか確認します。
- arcadeでは選択coreと互換性のあるROM set revisionを使用します。
- 必要な場合だけゲーム上でSELECTを押し、別の収録coreを試します。

## Wi-Fiへ接続できない

- 実機確認済みアダプターを使用し、coldの初回接続は30秒程度待ちます。
- SSIDとパスワードを確認し、Network InformationでIPを確認します。
- Wi-Fiを一度OFF・ONします。LEDが消えたままなら、アダプターを抜き差しするか、
  挿した状態で通常rebootします。
- 1つのUSBポートを充電器として使用中はWi-Fiを利用できません。

## 操作・画面・音がおかしい

- エミュレータを終了し、通常のフロントエンド導線から起動し直します。
- FUNCTIONでエミュレータメニューを開き、使用profileを確認します。
- 手動設定変更が原因の場合は、System Settingsから対象runtimeのfactory設定を復元します。
  ROMファイルは置換されません。
- 特定システムだけの場合は、system、game、selected core、game画面・menu・input・audioの
  どこに問題があるか記録します。

## ストレージとログ

`START -> System Settings -> Storage Check`は時間制限付きのread-only health checkです。
mount中FAT32のrepairは行いません。サポート用永続ログは、正常shutdown後に
`PLUMOS_USER/plumos-logs`から確認できます。

問題報告にはplumOS version、SDカード識別、正確な操作、画面のerror text、通常再起動後も
再現するかを含めてください。Wi-Fiパスワード、private key、ROM、著作権保護BIOSは
公開しないでください。
