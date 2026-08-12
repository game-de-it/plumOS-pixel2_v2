# Pixel2 Audio Routing

Pixel2 routes emulator and app audio through the plumOS ALSA logical PCM:

```text
client -> ALSA default / plumos_output -> Pixel2 audio router
                                           |-- USB playback card, if present
                                           `-- RK817 by card ID rockchiprk817
```

The router is an ALSA ioplug component derived from the existing plumOS
`plumos_output` design and packaged as `components/audio-router`. It runs in
the client process only while a playback stream is open; there is no PulseAudio,
PipeWire, or resident routing daemon.

Runtime files:

```text
/run/plumos/audio/asound.conf
/run/plumos/audio/output.status
```

Managed app-layer files:

```text
/mnt/plumos/bin/plumos-audio-output
/mnt/plumos/lib/alsa-lib/libasound_module_pcm_plumos_hotplug.so
/mnt/plumos/components/audio-router/manifest.json
/mnt/plumos/components/audio-router/checksums.sha256
```

RetroArch launches with:

```text
ALSA_CONFIG_PATH=/run/plumos/audio/asound.conf
ALSA_PLUGIN_DIR=/mnt/plumos/lib/alsa-lib
AUDIODEV=plumos_output
audio_device = "plumos_output"
```

The helper resolves the internal route from ALSA card ID `rockchiprk817`
instead of assuming card 0. A USB playback card is preferred when present.
HDMI-like cards are not selected automatically.

The ioplug keeps RetroArch away from raw hardware delay quirks. Its logical
delay is bounded by the physical PCM ring occupancy from
`snd_pcm_avail_update()`, matching the existing plumOS fix for audio-rate-control
instability. Pixel2 must try this routing layer before using CPU performance
policy as a workaround for audio dropouts or unstable frame pacing.
