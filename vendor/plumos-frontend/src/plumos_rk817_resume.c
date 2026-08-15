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

static int reapply_route_switch(snd_mixer_t *mixer, const char *name,
                                int nudge) {
  snd_mixer_elem_t *element = find_element(mixer, name);
  int saved[SND_MIXER_SCHN_LAST + 1];
  int present[SND_MIXER_SCHN_LAST + 1];
  int channel;

  if (!element || !snd_mixer_selem_has_playback_switch(element)) {
    return -ENOENT;
  }
  memset(present, 0, sizeof(present));
  for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
    snd_mixer_selem_channel_id_t id =
        (snd_mixer_selem_channel_id_t)channel;
    if (!snd_mixer_selem_has_playback_channel(element, id) ||
        snd_mixer_selem_get_playback_switch(element, id, &saved[channel]) < 0) {
      continue;
    }
    present[channel] = 1;
    if (nudge && saved[channel] &&
        snd_mixer_selem_set_playback_switch(element, id, 0) < 0) {
      return -EIO;
    }
  }
  for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
    if (present[channel] &&
        snd_mixer_selem_set_playback_switch(
            element, (snd_mixer_selem_channel_id_t)channel,
            saved[channel]) < 0) {
      return -EIO;
    }
  }
  return 0;
}

static int reapply_pixel2_routes(snd_mixer_t *mixer, int nudge) {
  const char *names[] = {"Speaker", "Headphone"};
  int found = 0;
  size_t index;

  for (index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
    int result = reapply_route_switch(mixer, names[index], nudge);
    if (result == 0) {
      found++;
    } else if (result != -ENOENT) {
      return result;
    }
  }
  if (!found) {
    fprintf(stderr, "rk817-resume: Pixel2 route switches unavailable\n");
    return -ENOENT;
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
    card = "default";
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
  result = reapply_pixel2_routes(mixer, strcmp(action, "rearm") == 0);
  snd_mixer_close(mixer);
  if (result < 0) {
    return 1;
  }
  printf("rk817_resume=result-ok action=%s card=%s routes=Speaker,Headphone\n",
         action, card);
  return 0;
}
