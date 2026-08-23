# システムアップデート

[English](updates.md)

Pixel2は署名済みアップデートを使用します。Runtime updateは`PLUMOS_SYS`上の
管理対象アプリ・サービスを変更し、System updateは`PLUMOS_BOOT`の非active A/B slot
だけへ書き込みます。

## アップデートを適用する

1. release noteを読み、Pixel2用packageであることを確認します。
2. SHA-256が公開されている場合は、downloadしたpackageと一致するか確認します。
3. カードリーダーまたはネットワーク転送で`PLUMOS_USER/updates`へコピーします。
4. 開始前に十分なバッテリー残量または安定した電源を用意します。
5. `START -> System Settings -> System Update`を開きます。
6. packageを確認し、検証、インストール、自動再起動が全て完了するまで待ちます。
7. 通常のフロントエンドが起動し、System Informationが想定versionになったことを確認します。

アップデート画面中にSDカードや電源を抜かないでください。更新engineは署名、device、
version、ABI、payload hash、最終readbackを確認してからgenerationを切り替えます。
正常起動が確認されないgenerationは昇格せず、以前の管理世代を復旧用に保持します。

## ユーザーデータの保持

管理対象アップデートは、ROM、BIOS、セーブ、ステート、画面画像、Wi-Fi認証情報、
SSH host key、PortMaster content、active user設定を意図的に置換しません。
それでもrelease update前には重要データをバックアップしてください。

update archiveを手動で`/mnt/plumos`へ展開しないでください。管理ファイルと
manifest/checksumは、原子的な1つのupdate単位として整合している必要があります。
