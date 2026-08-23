# ハードウェアサービス

[English](hardware-services.md)

## 入力

Pixel2 joypadは`pixel2_joypad`です。共通mapは次にあります。

```text
/mnt/plumos/config/system/input-map.env
/mnt/plumos/config/system/input-map.json
```

| 物理button | evdev code | RetroArch udev button |
| --- | ---: | ---: |
| A | 305 | 1 |
| B | 304 | 0 |
| X | 307 | 2 |
| Y | 308 | 3 |

`PLUMOS_INPUT_AB_LAYOUT=east-confirm`によりFEでは物理Aを決定にし、RetroArchとSAも
共通契約へ合わせます。十字は`BTN_DPAD_UP/DOWN/LEFT/RIGHT`、RAでは10/11/12/13です。
kernelは`ABS_X/Y`もadvertiseしますが、実機captureでは十字は`EV_KEY`だったため、
RAのanalog-to-digital D-padは無効にします。

Pixel2はudevdではなくBusyBox `mdev`を使用します。libudevでcontrollerを探すruntimeは、
起動前に`plumos-ensure-udev-input-db`を実行し、`/run/udev/data/cMAJ:MIN`へ最小限の
`ID_INPUT_JOYSTICK=1` propertyを作成します。

## 音量と明るさ

- 音量key単独: global volume
- SELECT + 音量key: backlight brightness
- 長押し: 制限付きrepeat
- 操作終了後: 最終値を永続化

Pixel2のvolumeはRK817 mixer値ではなくstateを持ち、`plumos_output` routerがemulator
音声へsoftware gainとして適用します。brightnessは次のPWM backlightを使用します。

```text
/sys/class/backlight/backlight/brightness
```

## 画面

FEは`/dev/fb0`を`PLUMOS_FBDEV_ROTATION=ccw`で使用します。brightnessは20段階を
hardware 0..255へ対応させます。

## 音声

RK817 card IDは`rockchiprk817`です。plumOS管理runtimeは`plumos_output`を通し、
volume keyの現在値を即時反映します。内蔵speakerはlogical volume適用後に0..+15 dBを
0.5 dB単位で追加できます。実機で+15 dB、無音volume 0、歪み・異常音・観測可能な発熱
なしをacceptance済みです。16-bit範囲を超えるsampleはsaturationし、USB audioには
speaker boostを適用しません。詳細は[オーディオルーティング](audio-routing.ja.md)を
参照してください。

## 電源・充電

Rebootはstock kernelのsysrq経路を使用します。charger未接続ShutdownはRK817 PMICの
`DEV_OFF`（I2C bus 0、addr `0x20`、reg `0xf4` bit 0）を使用します。

charger接続中のShutdownは制限付き`plumos-reboot-mode charge` helperからLinux
`RESTART2`を呼び、stock `mode-charge`を`BOOT_CHARGING=0x5242c30b`へ変換させます。
失敗時は通常rebootへ進まず`DEV_OFF`へfallbackします。charger接続中のstock充電画面と、
未接続時の完全power-offはいずれも実機検証済みです。

Power inputはstock `rk805 pwrkey`です。hardware-key serviceはFE停止後も動作し、FE、
RetroArch、PicoArch、SA、Appsの上に共通power menuを表示します。対象display ownerだけを
pauseし、cancelまたはsleep復帰時に同じprocessだけをresumeします。

DraSticはAArch64 DRM runnerとarmhf coreがsiblingなので、共通parentと実行fileを確認して
両方をpauseし、runnerから順にresumeします。stale PIDで無関係processを止めません。

stock kernelの`freeze mem`が`EBUSY`になる個体ではsoftware standbyへfallbackします。
display ownerをpauseしてbacklightを消し、次のPowerをwake専用eventとして扱います。
FEはwake eventを捨て、復帰直後にpower menuが再表示されることを防ぎます。
