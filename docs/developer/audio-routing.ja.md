# Pixel2オーディオルーティング

[English](audio-routing.md)

Pixel2はemulatorとappの音声をplumOS ALSA logical PCMへ接続します。

```text
client -> ALSA default / plumos_output -> Pixel2 audio router
                                           |-- USB playback card
                                           `-- RK817 (rockchiprk817)
```

routerは既存plumOS `plumos_output`設計由来のALSA ioplugで、
`components/audio-router`として収録します。playback stream中だけclient process内で
動作し、PulseAudio、PipeWire、常駐router daemonは使用しません。

一時runtime file:

```text
/run/plumos/audio/asound.conf
/run/plumos/audio/output.status
```

管理対象:

```text
/mnt/plumos/bin/plumos-audio-output
/mnt/plumos/lib/alsa-lib/libasound_module_pcm_plumos_hotplug.so
/mnt/plumos/components/audio-router/manifest.json
/mnt/plumos/components/audio-router/checksums.sha256
```

RetroArchは`ALSA_CONFIG_PATH`、`ALSA_PLUGIN_DIR`、`AUDIODEV=plumos_output`を設定して
起動します。内部出力はcard番号固定ではなくALSA card ID `rockchiprk817`で解決し、
USB playback cardがあれば優先します。HDMI相当cardは自動選択しません。

ioplugはraw hardware delayの癖をclientから分離し、`snd_pcm_avail_update()`で得た
physical ring occupancyからlogical delayを制限します。音飛びやframe pacing問題では、
CPU performance policyへ逃げる前にこのrouteを検証します。

内蔵speakerにはglobal volume適用後、0から+15 dBまで0.5 dB単位のsoftware gainを
適用できます。Pixel2実機で+15 dBとvolume 0の完全muteをacceptance済みです。
USB audio routeにはspeaker boostを適用しません。
