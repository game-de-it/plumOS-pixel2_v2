# Pixel2 強制電源断後の健全性とPicoArch PC-98検証

日付: 2026-08-31

## 強制電源断後の健全性

バッテリー切れ後も実機は通常起動し、managed Runtimeは破損していなかった。
Frontendとhardware-key serviceは稼働し、ext4のapp/state partitionにerrorは無く、
storage警告は`/dev/mmcblk0p3`のFAT dirty bitだけだった。

mount中のread-only検査ではdirectory/allocation破損を検出しなかった。その後、
user partitionをofflineにして同梱`fsck.fat`で検査・修復した。

```text
Dirty bit is set. Fs was not properly unmounted and some data may be corrupt.
 Automatically removing dirty bit.
/dev/mmcblk0p3: 3818 files, 170347/1628977 clusters
```

修復はmetadata修正を表すrc=1、直後のno-write再検査はrc=0だった。plumOSの安全な
再起動後、kernel logから`Volume was not properly unmounted`が消え、ext4、MMC、
I/O、corruption errorも無いことを確認した。

## PicoArch NP2kai入力

NP2kai上流は`np2kai_joymode=OFF`を既定とする。この状態ではPicoArchから
RetroPad入力がcoreへ届いても、十字キーやface buttonをPC-98 keyboard入力へ
変換しない。Pixel2 core buildで英語・日本語双方のcore option既定値を
`Arrows 3button`へ変更し、動作済みのRetroArch factory方針と揃えた。

PicoArchはcore既定値の後にuserが保存したgame/global configを読み込むため、
userが選んだ`Mouse`、`OFF`、その他のmappingは維持される。PC-98のFrontendには
既存のRetroArch 2経路に`picoarch:np2kai`と`picoarch:nekop2`を追加し、既定経路は
従来どおりRetroArch NP2kaiのままとした。

## Neko Project IIフォント

Neko Project IIは従来`system/np2`だけを探索するが、Pixel2の共有PC-98 firmwareは
`system/np2kai`にあるため、font無しで起動していた。MFで実績のあるfallbackを
次の順序で移植した。

1. `np2/font.bmp`
2. `np2/FONT.ROM`
3. `np2kai/font.bmp`
4. `np2kai/FONT.ROM`

BIOSのcopyやrenameは行わず、mutableなuser BIOS directoryの既存fileを唯一の
正本として共有する。

## Build・実機検証

- commit: `b1c1de3`
- Runtime: `0.1.4-dev-b1c1de3`
- AArch64 core SHA-256:
  - NP2kai: `757fd02b9301bac9a05a20e5f1591fcf83d86da587f6eedafc68f92f03308526`
  - Neko Project II: `781c0c8a71657ed3b62193f369b004adc8b49446e25cac85ed733603b941008d`
- Frontend component: 201 / 201 checksum合格
- Libretro core component: 360 / 360 checksum合格
- 完全app layer: 11,334 / 11,334 checksum合格

既存の`Can Can Bunny 5 and half Limited.hdi`を使い、新規・非既定PC-98 3経路を
実際に起動した。

| Profile | 起動 | 非黒画面 | ALSA再生進行 |
|---|---:|---:|---:|
| `retroarch:nekop2` | pass | pass | pass |
| `picoarch:np2kai` | pass | pass | pass |
| `picoarch:nekop2` | pass | pass | pass |

Neko Project IIの両captureで日本語のPC-9800 boot textが表示され、共有fontを
読み込んだことを確認した。機械試験ではPixel2の物理buttonを押せないため、
十字キー・ABXYの最終操作だけはoperator確認項目とする。

deployではmutable設定を変更していない。active RetroArch configのSHA-256は
`6c077932...9258`、`bios/np2kai/np2kai.cfg`は`0fd644e7...dabb`のまま、deploy・
再起動前後で一致した。

