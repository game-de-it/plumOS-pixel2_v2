# plumOS Pixel2 開発者向けガイド

このガイドは、plumOS Pixel2のビルド、変更、デプロイ、検証を行う開発者向けの
技術的な入口です。MFとV90SはplumOS契約の参考ですが、Pixel2はstock RK3326
boot substrateと固有hardware runtimeを使用します。通常の導入・操作は
[ユーザー向け取扱説明書](../user/README.ja.md)を参照してください。

## 最初に読む文書

1. [アーキテクチャと所有範囲](architecture.ja.md)
2. [ビルドガイド](build.ja.md)
3. [ブートとランタイムサービス](runtime.ja.md)
4. [ストレージとアップデート](storage-and-updates.ja.md)
5. [フロントエンドとエミュレータ統合](frontend-emulators.ja.md)
6. [ハードウェアサービス](hardware-services.ja.md)
7. [オーディオルーティング](audio-routing.ja.md)
8. [検証・根拠資料](validation.ja.md)
9. [実装一覧とrelease blocker](implementation-status.ja.md)

9文書すべてで英語版と日本語版を対にしています。日付付き`docs/validation/`、
採用済みdecision、実装inventoryは成功・失敗・旧方式を残す技術根拠であり、
英語のみの場合があります。現在の利用者向け操作説明ではありません。

互換性資料: [USB Wi-Fiとネットワークサービス](../configuration/connectivity.ja.md)

## リポジトリ構成

```text
artifacts/                 git管理外のstock SD capture・調査入力
docs/decisions/            採用済みアーキテクチャ判断
docs/developer/            現行の開発者向け技術契約
docs/user/                 利用者向け取扱説明書
docs/validation/           日付付きhost・実機根拠
docker/                    container build入力
package/                   Pixel2 app-layer、FE、RetroArch、初期設定
rootfs/pixel2/             SYSTEM用plumOS init・rootfs payload
scripts/                   build、image、capture、deploy、verify tool
tests/                     host契約・回帰テスト
vendor/plumos-frontend/    Pixel2 FE、text UI、scanner、helper source
output/                    git管理外の生成物
```

## 変更してはいけない契約

- 対象はPixel2専用です。MF/V90Sの実装は参考にしても同一hardware動作を仮定しません。
- stock Pixel2資産がRockchip boot prefix、vendor kernel、登録済みruntime DTB、
  `/boot/SYSTEM`を開くstock initramfs handoffを所有します。
- handoff後はplumOSが`SYSTEM`内`/sbin/init`、app-layer、FE、emulator、service、
  設定、power policyを所有します。
- `/mnt/plumos`は書き込み可能なapp/runtime ABI、`/mnt/plumos-user`はPCから読める
  user/content volume、`/boot`は通常read-onlyです。
- display/inputを所有するforeground processは1つだけです。launcherはchild前にFEを
  解放し、終了後にFEを1つだけ復帰します。
- 実機app-layer更新は、管理ファイルと一致する`checksums.sha256`、`manifest.json`、
  component metadataを含む原子的な単位で行い、再起動前に実機SHA-256を確認します。
- active設定、ROM、BIOS、save/state、log、認証情報、SSH状態、user app dataを
  host metadataへ合わせる目的で上書きしません。
- 実体のないAppsやemulator profileをFEへ公開しません。
- release payloadへROM、利用者BIOS、秘密情報、未追跡stock抽出入力を含めません。

## 正とする情報

矛盾がある場合は次の順序で判断します。

1. 現在のPixel2 sourceとtest
2. `docs/decisions/`の採用済みPixel2 decision
3. 現在生成されたmanifestとchecksum
4. このdeveloper guide
5. 日付付きvalidation記録
6. MF/V90Sの文書と履歴

## English

- [English developer guide](README.md)
