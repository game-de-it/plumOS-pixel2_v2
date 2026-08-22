# Pixel2 sleep/resume machine matrix

## Scope

Pixel2の代表的な4 runtime familyについて、実際のglobal power overlayとstock
5.10 kernelの`mem` suspendを使い、Sleep前後を機械検査した。物理menu選択だけを
test helperへ置換し、launcher、display-owner pause/resume、safe shutdown、RTC wake、
DRM、ALSAはproduction経路を使った。

画面PASSは480x640 panelを覆う非黒・非static DRM planeが存在すること、音声PASSは
物理PCMが`RUNNING`でhardware pointerが進むことを意味する。向き、aspect、flicker、
可聴品質、操作感、物理Power buttonそのものはこの機械判定には含めない。

## Detected regressions and fixes

Music Playerはsleep後の物理PCMが`SUSPENDED`のままになり得た。`a88e192`でALSAを
nonblocking化し、`EAGAIN`をbounded wait、`EPIPE`/`ESTRPIPE`をALSA recoveryへ接続した。
また、Pixel2のFAT32 user volumeに実在する`Music` directoryをscan対象へ追加した。

連続試験ではkernel resume時の一時的なUSB removeを`plumos-wifi-uevent`が物理dongle
抜去と誤認し、OTGへreleaseする回を検出した。`f57f2b7`はsuspend前に専用markerを置き、
その間のremoveを無視する。resume後は既存Systemのhost probeとRuntimeのWi-Fi recoveryを
非同期実行し、markerは復旧workerだけが削除する。Wi-Fi設定、driver、DTBは変更していない。

検証scriptはSleep前にもALSA pointer進行を必須とし、短い効果音の終了をresume失敗と
誤判定しないよう、カード上の長いPyxel BGMを検証専用
`/mnt/plumos-user/Music/plumos-sleep-test.mp3`へcopyする。元音源は変更しない。

## Build and deployment

- code commit: `f57f2b7 fix: recover Pixel2 Wi-Fi after sleep`
- validator follow-up: `ffa1bf0 test: keep Pixel2 sleep audio active`
- device Runtime: `0.1.0-dev-f57f2b7`
- source transition: `0.1.0-dev-a88e192 -> 0.1.0-dev-f57f2b7`
- signed Runtime package SHA-256:
  `511a34083a6c6a41125390fc41e2ca3e2a16b3136161f5f1cb4007626f810296`
- package payload: 10 managed entries, 0 deletes
- signature, exact source, ABI, card readback, frontend checksum,
  Music Player checksum, and full `verify-runtime`: PASS

ROM、BIOS、save、PortMaster更新物、ユーザー設定は更新対象外とした。

## Device results

| family | content / route | pause | kernel sleep | same process | screen pre/post | audio pre/post | result |
|---|---|---:|---:|---:|---:|---:|---:|
| RetroArch | FDS / `retroarch:fceumm` | PASS | PASS | PASS | PASS | PASS | PASS |
| Standalone | PSP / `standalone:ppsspp` | PASS | PASS | PASS | PASS | PASS | PASS |
| PicoArch | FBNeo / `picoarch:fbneo` | PASS | PASS | PASS | PASS | PASS | PASS |
| Apps | Music Player | PASS | PASS | PASS | PASS | PASS | PASS |

RAの全体matrixでは起動5秒時点のpre-captureだけが早すぎてMANUALとなったため、
起動待ち10秒で単独再試験し、同じ全項目をPASSした。

7回の連続kernel sleepで、全回に`usb-guard-armed`、`sleep=resume backend=mem`、
`wifi-resume-recovery-scheduled`、`kernel_sleep=1`を記録した。RTL8821CUはresume後の
最初のWPA接続が15秒上限に達する回があるが、既存uevent recoveryとのbounded retryで
IPv4 `192.168.10.110`とSSHへ自動復帰した。試験後は`wifi=on`、dongle present、
WPA runningで、sleep/probe markerは残っていない。

## Evidence

- `output/validation/2026-08-23-pixel2-sleep-matrix-f57f2b7/report.json`
- `output/validation/2026-08-23-pixel2-sleep-matrix-f57f2b7/report.md`
- `output/validation/2026-08-23-pixel2-sleep-ra-f57f2b7-v2/report.json`
- `output/validation/2026-08-23-pixel2-sleep-ra-f57f2b7-v2/report.md`
- `output/live/2026-08-23-pixel2-sleep-wifi-fix/`

各validation directoryには前後process table、DRM raw capture/PNG、ALSA status、
power log、overlay log、launch logを保持する。これらは実機証拠でありrelease imageには
含めない。
