# ブートとランタイムサービス

[English](runtime.md)

## ブートフロー

1. stock RK3326 chainがstock kernel、DTB、initramfsをloadします。
2. stock initramfsがboot volumeをmountし、`/boot/SYSTEM`へhandoffします。
3. plumOS `/sbin/init`がproc/sys/dev、`/mnt/plumos`、`/mnt/plumos-user`、
   `/state`、`/roms`をmountし、loopbackを起動します。
4. retired ADB設定を除去し、upstream chargerがない場合だけ単一USB portをprobeして、
   networkとfrontend serviceを開始します。

## Service順序

```text
15-usb-host-reenumerate  bounded Wi-Fi host probeとOTG charge解放
20-usb-wifi              保存済みUSB Wi-Fi設定
30-ssh                   compatibility slot
35-network-services      SSH/FTP/SFTP/Samba保存状態
40-frontend              app-layer選択、hardware key、ROM scan、FE
```

fresh imageではSSHが初期ONです。USB downstreamがある間だけhost modeを保持し、
dongle抜去、空probe、upstream chargerでstock OTGへ戻します。ADB、FunctionFS、
USB Mode selector、recovery markerは配布しません。旧ADB markerはWi-Fi認証を
変更せずmigration時に削除します。

通常bootではapp-layer全hashを実行しません。signed updateとlive deployが再起動前に
完全Runtimeを検証し、通常bootはconstant-time metadataでhealthy generationを選びます。
明示監査は次を使用します。

```sh
/usr/sbin/plumos-system-update verify-runtime
```

## Foreground lifecycle

通常はFEが`/dev/fb0`と`/dev/input/event2`を所有します。launcherはchild起動前に
FE ownershipを停止・退避し、終了後にFEを1つだけ復帰します。

## Global hardware key

`plumos-hardware-keys-service`はPixel2 joypadのSELECTと`gpio-keys`の音量キーを読みます。
音量単独でglobal volume、SELECT+音量でbrightnessを変更し、idle後に永続化します。
物理PowerはFE以外でもglobal power overlayを起動します。

## ログ

| 領域 | path |
| --- | --- |
| init | `/mnt/plumos/logs/init.log` |
| frontend | `/mnt/plumos/logs/frontend.log` |
| USB Wi-Fi | `/state/plumos/logs/usb-wifi.log` |
| hardware keys | `/run/plumos/hardware-keys/service.log` |
| power | `/state/plumos/logs/power.log` |

## 安全な電源操作

`plumos-safe-shutdown`はhardware-key serviceとFEを停止し、可能なmountをunmountして
sync後にreboot、charge mode、RK817 `DEV_OFF`を選びます。charger接続中Shutdownは
Linux `RESTART2("charge")`からstock charging UIへ入り、未接続では完全OFFします。

Power overlayは現在のdisplay ownerだけをpauseし、Cancel・Sleepで元processを復帰します。
Reboot/ShutdownはTERM前にownerをresumeし、通常saveを許可します。kernel `mem`が
拒否された場合はsoftware standbyへfallbackし、panelを消灯したまま次のPowerで復帰します。
