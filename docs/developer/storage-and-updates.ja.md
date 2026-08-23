# ストレージとアップデート

[English](storage-and-updates.md)

## 現在のパーティション契約

Pixel2はstock互換Rockchip prefixと次の3 volumeを使用します。

| mount | label | 用途 |
| --- | --- | --- |
| `/boot` | `PLUMOS_BOOT` | stock互換boot fileと`SYSTEM` |
| `/mnt/plumos` | `PLUMOS_SYS` | app-layer runtimeとLinux state |
| `/mnt/plumos-user` | `PLUMOS_USER` | ROM、BIOS、media、screenshot、update |

`/boot`は通常read-only、`PLUMOS_SYS`はPOSIX permissionを保持するext4、
`PLUMOS_USER`はmacOS/Windowsから読めるFAT32です。

## Compact seedと初回起動

配布imageは2,701,131,776 byteで、最初は2つのMBR partitionを持ちます。

| partition | seed geometry | 初回起動後 |
| --- | --- | --- |
| p1 `PLUMOS_BOOT` | sector 32768、512 MiB FAT32 | 変更なし |
| p2 `PLUMOS_SYS` | sector 1081344、2048 MiB ext4 | 8192 MiBへ拡張 |
| p3 `PLUMOS_USER` | なし | sector 17858560からcard末尾、FAT32 |

対応cardは16 GB以上（読み取り可能容量15,000,000,000 byte以上）です。stock initramfsが
p2をmountした後、provisionerはp1/p2境界を検証し、`/storage/provision`へjournalを
記録してMBRを更新します。その後online `resize2fs`を行い、新しく所有したp3だけを
`PLUMOS_USER`としてformatし、次を作成します。

```text
roms/  bios/  Images/  Themes/  Screenshots/  Music/
updates/  imports/  exports/  plumos-logs/
```

running kernelからp2を拡張できない場合だけ、initは同期したearly rebootを1回実行し、
観測済みtableとjournalから再開します。`complete` markerと最終geometryが一致した後は、
通常bootでresize、format、seedを再実行しません。既存p3はformatせず、互換性のない
legacy geometryもlogへ記録して手動移行用に保存します。

## 管理対象と実機所有data

`/mnt/plumos/checksums.sha256`の対象は置換可能な管理fileです。active設定、ROM、BIOS、
save/state、log、認証情報、SSH state、app dataは実機所有のmutable dataです。
live deployは管理fileとmetadataを同時に更新し、実機所有dataを上書きしません。

## 実装済みupdate契約

- stock initramfsが常に`/SYSTEM`を開くため、同fileは小さなimmutable dispatcherです。
- System generationは`PLUMOS_BOOT`の`/system-slots/system-{a,b}.squashfs`へ保存し、
  inactive slotだけを書き換えます。
- pending slotをcommitする前に書き戻し後のSHA-256を検証します。
- signed Runtime updateは`PLUMOS_SYS`の管理pathだけを更新し、rollback generationと
  write-ahead transaction journalを保持します。
- Ed25519署名、device ID、vendor runtime、System/Runtime ABI、source version、
  payload path、package hashをinstall前に検証します。
- FEが最初のrender後に`/tmp/plumos-fe-ready`を作成して初めてgenerationをhealthyへ
  昇格します。
- 未昇格Systemはactive slotへ戻り、中断または未昇格Runtimeは直前generationを復元します。

FAT32では`/System/`が`/SYSTEM`と衝突するためslot保存には使いません。packageは
`/mnt/plumos-user/updates`へ置き、request journalはsafe reboot前にext4の
`/mnt/plumos/update-state`へcommitします。

host fixtureは署名拒否、互換性gate、中断Runtime復旧、inactive-slot readback、rollback
transitionを検証します。署名済みRuntime/System、FEからのupdate操作、health昇格は実機
検証済みです。意図的な実機failure injectionは、明示的に計画した場合だけ行う破壊的
試験として分離します。
