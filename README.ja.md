# plumOS Pixel2

[English](README.md) | [ユーザー向け取扱説明書](docs/user/README.ja.md) | [開発者向けガイド](docs/developer/README.ja.md)

plumOS Pixel2は、RK3326Sを搭載したGKD Pixel2向けのLinux
ディストリビューションです。ハードウェア互換性に必要なstockのboot substrateを
保持し、stock initramfsが`SYSTEM`へ引き渡した後は、plumOS独自のsystem、
フロントエンド、サービス、エミュレータ環境を起動します。

## 主な機能

- 6個のシステムアイコンとText・Graphic・Gallery表示を備えたゲーム画面
- RetroArch、PicoArch、スタンドアロンエミュレータ、PICO-8、Pyxel、PortMaster
- ROM、BIOS、画像、セーブ、アップデート用のFAT32 `PLUMOS_USER`
- 共通の音量・明るさ操作、電源メニュー、スリープ、再起動、シャットダウン
- 対応USB Wi-FiアダプターとSSH、SFTP、FTP、Samba
- rollback検査付きの署名済みRuntime更新とA/B System更新

ROMおよび利用者が用意するBIOSファイルは同梱されません。合法的に利用できる
コンテンツだけを使用してください。

## Pixel2固有の重要事項

- このイメージはGKD Pixel2専用です。
- 16 GB以上の別SDカードを使用し、stockOSカードへ上書きしないでください。
- 初回起動ではSystem領域の拡張と`PLUMOS_USER`作成を行い、1回だけ自動再起動する
  場合があります。セットアップ中は電源を切らないでください。
- Pixel2のUSBポートは1つです。外部分岐機器がなければWi-Fiアダプターと充電器を
  同時に接続できません。Wi-Fiを抜くとUSB充電に戻ります。
- ADBは搭載しません。リモート保守にはWi-FiとSSH/SFTPを使用します。
- SDカードを抜く前には、必ずPOWERメニューからシャットダウンしてください。

## ドキュメント

インストール、操作、ストレージ、ネットワーク、エミュレータ、更新、問題対応は
[ユーザー向け取扱説明書](docs/user/README.ja.md)から読んでください。

ビルド、boot所有範囲、Runtime/System構成、app-layerデプロイ、ハードウェア統合、
検証については[開発者向けガイド](docs/developer/README.ja.md)を参照してください。

日英の全体索引は[`docs/`](docs/README.ja.md)にあります。
`docs/validation/`の日付付き文書は、旧方式を含む開発上の根拠資料であり、
利用者向けの操作手順ではありません。

## プロジェクト状況

Pixel2版はリリース候補の実機検証中です。ホスト上のビルドやchecksum成功だけでは、
実機の画面方向、入力、音声、スリープ、充電、ストレージ、エミュレータ動作を保証しません。
現在の実装とrelease blockerは[TODO](TODO.md)および
[実装一覧](docs/developer/implementation-status.md)で管理します。
