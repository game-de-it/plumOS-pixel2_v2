このファイルは、この repository で作業する Codex/agent 向けのルールです。
適用範囲は repository 全体です。

## 作業開始時

- `git status --short`、`TODO.md`、関連する `docs/`、直近の `git log --oneline` を確認する。
- 不明点はまず既存 docs、scripts、commit history、artifacts を確認する。

## プロジェクト方針

このプロジェクトではGKD pixel2というハンドヘルドで動作するLinuxを構築することが目的になります。
作業履歴、ログはgitを使って進めましょう。

## 実機デプロイ

- `/mnt/plumos` 配下の app-layer 管理ファイルを実機へ更新する場合は、
  バイナリやライブラリだけを単独でデプロイしてはならない。対応する
  `checksums.sha256` を必ずデプロイに含め、`manifest.json`、必要な
  component manifest も同じデプロイ単位で整合させる。
- 再起動前に実機上でデプロイ対象の SHA-256 と app-layer checksum 検証を
  実行し、bootstrap が更新ファイルを `critical checksum failed` として
  拒否しないことを確認する。
- 実機側で変更されたユーザー設定、PortMaster 更新物、セーブデータなどを
  ホスト側の app-layer metadata に合わせる目的で上書きしてはならない。
  ライブデプロイでは変更対象の管理ファイルと、その metadata entry だけを
  原子的に更新する。
