// SPDX-License-Identifier: MIT

#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void configure_alsa(void) {
  const char *root;
  char path[512];

  if (getenv("ALSA_CONFIG_PATH") && getenv("ALSA_CONFIG_PATH")[0]) {
    return;
  }
  root = getenv("PLUMOS_ROOT");
  if (!root || !root[0]) {
    root = "/mnt/plumos";
  }
  snprintf(path, sizeof(path), "%s/config/alsa/alsa.conf", root);
  if (access(path, R_OK) != 0) {
    snprintf(path, sizeof(path), "%s/factory-defaults/alsa/alsa.conf", root);
  }
  if (access(path, R_OK) == 0) {
    setenv("ALSA_CONFIG_PATH", path, 0);
  }
}

static snd_mixer_elem_t *find_element(snd_mixer_t *mixer, const char *name) {
  snd_mixer_selem_id_t *id;

  snd_mixer_selem_id_alloca(&id);
  snd_mixer_selem_id_set_index(id, 0);
  snd_mixer_selem_id_set_name(id, name);
  return snd_mixer_find_selem(mixer, id);
}

static int set_resume_path(snd_mixer_t *mixer) {
  snd_mixer_elem_t *element = find_element(mixer, "Resume Path");
  unsigned int count;
  unsigned int index;

  if (!element || !snd_mixer_selem_is_enumerated(element)) {
    fprintf(stderr, "rk817-resume: Resume Path control unavailable\n");
    return -ENOENT;
  }
  count = snd_mixer_selem_get_enum_items(element);
  for (index = 0; index < count; index++) {
    char item[128] = "";
    if (snd_mixer_selem_get_enum_item_name(element, index, sizeof(item),
                                            item) == 0 &&
        strcmp(item, "ON") == 0) {
      int channel;
      int result = 0;
      for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
        if (!snd_mixer_selem_has_playback_channel(
                element, (snd_mixer_selem_channel_id_t)channel)) {
          continue;
        }
        if (snd_mixer_selem_set_enum_item(
                element, (snd_mixer_selem_channel_id_t)channel, index) < 0) {
          result = -EIO;
        }
      }
      return result;
    }
  }
  fprintf(stderr, "rk817-resume: Resume Path ON item unavailable\n");
  return -ENOENT;
}

static int rearm_speaker_gain(snd_mixer_t *mixer) {
  snd_mixer_elem_t *element = find_element(mixer, "SPK");
  long minimum;
  long maximum;
  long saved[SND_MIXER_SCHN_LAST + 1];
  int present[SND_MIXER_SCHN_LAST + 1];
  int channel;

  if (!element || !snd_mixer_selem_has_playback_volume(element)) {
    fprintf(stderr, "rk817-resume: SPK control unavailable; path only\n");
    return 0;
  }
  if (snd_mixer_selem_get_playback_volume_range(element, &minimum, &maximum) <
      0) {
    return -EIO;
  }
  memset(present, 0, sizeof(present));
  for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
    snd_mixer_selem_channel_id_t id =
        (snd_mixer_selem_channel_id_t)channel;
    long nudge;
    if (!snd_mixer_selem_has_playback_channel(element, id) ||
        snd_mixer_selem_get_playback_volume(element, id, &saved[channel]) < 0) {
      continue;
    }
    present[channel] = 1;
    nudge = saved[channel] > minimum ? saved[channel] - 1
                                      : saved[channel] < maximum
                                            ? saved[channel] + 1
                                            : saved[channel];
    if (nudge != saved[channel] &&
        snd_mixer_selem_set_playback_volume(element, id, nudge) < 0) {
      return -EIO;
    }
  }
  for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
    if (present[channel] &&
        snd_mixer_selem_set_playback_volume(
            element, (snd_mixer_selem_channel_id_t)channel,
            saved[channel]) < 0) {
      return -EIO;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *action = argc > 1 ? argv[1] : "arm";
  const char *card = getenv("PLUMOS_RK817_ALSA_CARD");
  snd_mixer_t *mixer = NULL;
  int result;

  if (strcmp(action, "arm") != 0 && strcmp(action, "rearm") != 0) {
    fprintf(stderr, "usage: %s [arm|rearm]\n", argv[0]);
    return 2;
  }
  if (!card || !card[0]) {
    card = "hw:0";
  }
  configure_alsa();
  result = snd_mixer_open(&mixer, 0);
  if (result >= 0) {
    result = snd_mixer_attach(mixer, card);
  }
  if (result >= 0) {
    result = snd_mixer_selem_register(mixer, NULL, NULL);
  }
  if (result >= 0) {
    result = snd_mixer_load(mixer);
  }
  if (result < 0) {
    fprintf(stderr, "rk817-resume: mixer open failed card=%s error=%s\n", card,
            snd_strerror(result));
    if (mixer) {
      snd_mixer_close(mixer);
    }
    return 1;
  }
  result = set_resume_path(mixer);
  if (result == 0 && strcmp(action, "rearm") == 0) {
    result = rearm_speaker_gain(mixer);
  }
  snd_mixer_close(mixer);
  if (result < 0) {
    return 1;
  }
  printf("rk817_resume=result-ok action=%s card=%s\n", action, card);
  return 0;
}
