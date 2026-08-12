// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_CARD_INDEX 31
#define ROUTE_PROBE_INTERVAL 32
#define VOLUME_MAX 20
#define VOLUME_DEFAULT 8
#define VOLUME_PROBE_INTERVAL 4
#define INTERNAL_CARD_ID "rockchiprk817"

typedef struct {
    snd_pcm_ioplug_t io;
    snd_pcm_t *physical;
    int physical_card;
    int physical_is_usb;
    int physical_uses_plug;
    snd_pcm_uframes_t physical_buffer_size;
    snd_pcm_uframes_t submitted_ptr;
    int16_t *output_buffer;
    size_t output_capacity;
    unsigned int probe_countdown;
    unsigned int volume_probe_countdown;
    int volume_level;
    int allow_fast_drop;
    char fast_forward_state[PATH_MAX];
    int fast_forward_active;
    int physical_resync_pending;
    int poll_proxy_fd;
    int debug_enabled;
    unsigned int debug_count;
} plumos_pcm_t;

static void debug_event(plumos_pcm_t *pcm, const char *event,
                        snd_pcm_sframes_t value, snd_pcm_sframes_t detail)
{
    int recovery_event = strstr(event, "recover") || strstr(event, "resync");

    if (!pcm->debug_enabled || (!recovery_event && pcm->debug_count >= 256))
        return;
    if (!recovery_event)
        pcm->debug_count++;
    fprintf(stderr,
            "plumos-hotplug-debug: event=%s value=%ld detail=%ld "
            "io_state=%d io_appl=%lu io_hw=%lu submitted=%lu physical=%d\n",
            event, (long)value, (long)detail, pcm->io.state,
            (unsigned long)pcm->io.appl_ptr, (unsigned long)pcm->io.hw_ptr,
            (unsigned long)pcm->submitted_ptr,
            pcm->physical ? (int)snd_pcm_state(pcm->physical) : -1);
}

static int clamp_volume(int volume)
{
    if (volume < 0)
        return 0;
    if (volume > VOLUME_MAX)
        return VOLUME_MAX;
    return volume;
}

static int read_volume_file(const char *path, int json)
{
    char buffer[1024];
    char *cursor;
    char *end;
    FILE *fp = fopen(path, "r");
    long value;
    size_t length;

    if (!fp)
        return -1;
    length = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    buffer[length] = '\0';
    cursor = buffer;
    if (json) {
        cursor = strstr(buffer, "\"volume\"");
        if (!cursor)
            return -1;
        cursor = strchr(cursor, ':');
        if (!cursor)
            return -1;
        cursor++;
    }
    errno = 0;
    value = strtol(cursor, &end, 10);
    if (errno || end == cursor)
        return -1;
    return clamp_volume((int)value);
}

static int read_system_volume(void)
{
    const char *root;
    char settings[PATH_MAX];
    int volume = read_volume_file("/run/plumos/volume/current", 0);

    if (volume >= 0)
        return volume;
    root = getenv("PLUMOS_ROOT");
    if (!root || !root[0])
        root = "/mnt/plumos";
    if (snprintf(settings, sizeof(settings), "%s/config/system/settings.json",
                 root) >= (int)sizeof(settings))
        return VOLUME_DEFAULT;
    volume = read_volume_file(settings, 1);
    return volume >= 0 ? volume : VOLUME_DEFAULT;
}

static int16_t apply_software_volume(int16_t sample, int volume)
{
    int32_t scaled;

    volume = clamp_volume(volume);
    if (volume == VOLUME_MAX)
        return sample;
    scaled = (int32_t)sample * volume;
    return (int16_t)(scaled / VOLUME_MAX);
}

static int fast_forward_is_active(const plumos_pcm_t *pcm)
{
    return pcm->allow_fast_drop && pcm->fast_forward_state[0] &&
           access(pcm->fast_forward_state, F_OK) == 0;
}

static int card_has_usb_id(int card)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/asound/card%d/usbid", card);
    return access(path, R_OK) == 0;
}

static int card_has_playback(int card)
{
    char path[64];
    snprintf(path, sizeof(path), "/dev/snd/pcmC%dD0p", card);
    return access(path, R_OK | W_OK) == 0;
}

static int read_card_id(int card, char *buffer, size_t buffer_size)
{
    char path[64];
    FILE *fp;
    size_t length;

    if (!buffer || buffer_size < 2)
        return -EINVAL;
    snprintf(path, sizeof(path), "/proc/asound/card%d/id", card);
    fp = fopen(path, "r");
    if (!fp)
        return -errno;
    length = fread(buffer, 1, buffer_size - 1, fp);
    fclose(fp);
    buffer[length] = '\0';
    while (length > 0 &&
           (buffer[length - 1] == '\n' || buffer[length - 1] == '\r' ||
            buffer[length - 1] == ' ' || buffer[length - 1] == '\t'))
        buffer[--length] = '\0';
    return length > 0 ? 0 : -ENODEV;
}

static int card_id_is(int card, const char *expected)
{
    char id[64];
    return read_card_id(card, id, sizeof(id)) == 0 &&
           strcmp(id, expected) == 0;
}

static int find_internal_card(void)
{
    const char *expected = getenv("PLUMOS_PIXEL2_INTERNAL_CARD_ID");
    int card;

    if (!expected || !expected[0])
        expected = INTERNAL_CARD_ID;
    for (card = 0; card <= MAX_CARD_INDEX; card++) {
        if (card_has_playback(card) && card_id_is(card, expected))
            return card;
    }
    return -1;
}

static int find_usb_card(void)
{
    int card;
    for (card = 0; card <= MAX_CARD_INDEX; card++) {
        if (card_has_playback(card) && card_has_usb_id(card))
            return card;
    }
    return -1;
}

static void write_route_status(int card, int is_usb, int uses_plug)
{
    const char *status = "/run/plumos/audio/output.status";
    char temporary[PATH_MAX];
    char card_id[64] = "unknown";
    FILE *fp;

    read_card_id(card, card_id, sizeof(card_id));
    snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", status, (long)getpid());
    fp = fopen(temporary, "w");
    if (!fp)
        return;
    fprintf(fp,
            "mode=%s\n"
            "router=alsa_ioplug_hotplug\n"
            "card=%d\n"
            "card_id=%s\n"
            "physical_pcm=%s:%d,0\n"
            "pcm=plumos_output\n"
            "alsa_config_path=/run/plumos/audio/asound.conf\n",
            is_usb ? "usb_stereo" : "rk817_stereo", card, card_id,
            uses_plug ? "plughw" : "hw", card);
    if (fclose(fp) == 0)
        rename(temporary, status);
    else
        unlink(temporary);
}

static int configure_physical(snd_pcm_t *pcm, const snd_pcm_ioplug_t *io,
                              snd_pcm_uframes_t *actual_buffer)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *sw;
    snd_pcm_uframes_t period = io->period_size ? io->period_size : 768;
    snd_pcm_uframes_t buffer = io->buffer_size ? io->buffer_size : period * 4;
    unsigned int rate = io->rate;
    int direction = 0;
    int err;

    snd_pcm_hw_params_alloca(&params);
    if ((err = snd_pcm_hw_params_any(pcm, params)) < 0 ||
        (err = snd_pcm_hw_params_set_access(pcm, params,
                                             SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ||
        (err = snd_pcm_hw_params_set_format(pcm, params,
                                             SND_PCM_FORMAT_S16_LE)) < 0 ||
        (err = snd_pcm_hw_params_set_channels(pcm, params, 2)) < 0 ||
        (err = snd_pcm_hw_params_set_rate_near(pcm, params, &rate,
                                                &direction)) < 0 ||
        (err = snd_pcm_hw_params_set_period_size_near(pcm, params, &period,
                                                       &direction)) < 0 ||
        (err = snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer)) < 0 ||
        (err = snd_pcm_hw_params(pcm, params)) < 0)
        return err;
    if (rate != io->rate)
        return -EINVAL;

    snd_pcm_sw_params_alloca(&sw);
    if ((err = snd_pcm_sw_params_current(pcm, sw)) < 0 ||
        (err = snd_pcm_sw_params_set_avail_min(pcm, sw, period)) < 0 ||
        (err = snd_pcm_sw_params_set_start_threshold(pcm, sw, period)) < 0 ||
        (err = snd_pcm_sw_params(pcm, sw)) < 0)
        return err;
    if (actual_buffer)
        *actual_buffer = buffer;
    return snd_pcm_prepare(pcm);
}

static snd_pcm_t *open_physical(const snd_pcm_ioplug_t *io, int card,
                                int *uses_plug,
                                snd_pcm_uframes_t *actual_buffer)
{
    const plumos_pcm_t *owner = io->private_data;
    char name[64];
    snd_pcm_t *pcm = NULL;
    int mode = owner->allow_fast_drop ? SND_PCM_NONBLOCK : 0;
    int attempt;
    int direct_first = owner->allow_fast_drop;
    int plug;
    int err;

    /*
     * RetroArch needs the direct hardware PCM: inserting plughw adds a second
     * buffering domain whose delay destabilizes its rate control. Other
     * clients use the blocking route and retain the original plughw-first
     * contract; SDL audio clients such as PicoArch otherwise spin before their
     * first transfer on RK817. Both paths keep the opposite route as fallback.
     */
    for (attempt = 0; attempt <= 1; attempt++) {
        plug = direct_first ? attempt : 1 - attempt;
        snprintf(name, sizeof(name), "plumos_%s_card%d",
                 plug ? "plughw" : "hw", card);
        err = snd_pcm_open(&pcm, name, SND_PCM_STREAM_PLAYBACK, mode);
        if (err < 0)
            continue;
        err = configure_physical(pcm, io, actual_buffer);
        if (err >= 0) {
            if (uses_plug)
                *uses_plug = plug;
            return pcm;
        }
        snd_pcm_close(pcm);
        pcm = NULL;
    }
    return NULL;
}

static int switch_route(plumos_pcm_t *pcm, int force)
{
    snd_pcm_t *next;
    snd_pcm_t *old;
    int next_uses_plug = 0;
    snd_pcm_uframes_t next_buffer_size = 0;
    int usb_card = find_usb_card();
    int internal_card = find_internal_card();
    int target_card = usb_card >= 0 ? usb_card : internal_card;
    int target_is_usb = usb_card >= 0;

    if (target_card < 0)
        return -ENODEV;
    if (!force && pcm->physical && pcm->physical_card == target_card)
        return 0;

    next = open_physical(&pcm->io, target_card, &next_uses_plug,
                         &next_buffer_size);
    if (!next && target_is_usb) {
        target_card = internal_card;
        target_is_usb = 0;
        if (target_card < 0)
            return -ENODEV;
        if (!force && pcm->physical && pcm->physical_card == target_card)
            return 0;
        next = open_physical(&pcm->io, target_card, &next_uses_plug,
                             &next_buffer_size);
    }
    if (!next)
        return -ENODEV;

    old = pcm->physical;
    pcm->physical = next;
    pcm->physical_card = target_card;
    pcm->physical_is_usb = target_is_usb;
    pcm->physical_uses_plug = next_uses_plug;
    pcm->physical_buffer_size = next_buffer_size;
    pcm->probe_countdown = ROUTE_PROBE_INTERVAL;
    /*
     * The old physical queue and the new one have unrelated hardware
     * pointers. Drop the logical backlog at the handoff boundary so SDL
     * clients can immediately submit fresh audio to the replacement PCM.
     */
    if (old && pcm->io.state == SND_PCM_STATE_RUNNING)
        pcm->submitted_ptr = pcm->io.appl_ptr;
    pcm->physical_resync_pending = 0;
    if (old) {
        snd_pcm_drop(old);
        snd_pcm_close(old);
    }
    write_route_status(target_card, target_is_usb, next_uses_plug);
    fprintf(stderr, "plumos-hotplug: route=%s card=%d pcm=%s\n",
            target_is_usb ? "usb_stereo" : "rk817_stereo", target_card,
            next_uses_plug ? "plughw" : "hw");
    return 0;
}

static int is_route_loss_error(int error)
{
    return error == -EIO || error == -ENODEV || error == -ENXIO ||
           error == -EBADFD;
}

static int recover_physical(plumos_pcm_t *pcm, int error);

static int refresh_poll_route(plumos_pcm_t *pcm, struct pollfd *pfds,
                              unsigned int nfds, unsigned short *revents,
                              int reason)
{
    int count;
    int err = switch_route(pcm, 1);

    debug_event(pcm, "route-recover", err, reason);
    if (err < 0)
        return err;

    /*
     * SDL keeps the pollfd array returned by ALSA. A disconnected USB PCM
     * leaves that fd referring to a deleted device, so repopulate the same
     * array with the replacement RK817 descriptors before returning POLLOUT.
     */
    count = snd_pcm_poll_descriptors_count(pcm->physical);
    if (count < 0)
        return count;
    if ((unsigned int)count != nfds)
        return -EIO;
    err = snd_pcm_poll_descriptors(pcm->physical, pfds, nfds);
    if (err < 0)
        return err;
    *revents = POLLOUT;
    return 0;
}

static int blocking_poll_revents(plumos_pcm_t *pcm,
                                 unsigned short *revents)
{
    snd_pcm_state_t state;
    int err;

    if (!pcm->physical)
        return -ENODEV;

    state = snd_pcm_state(pcm->physical);
    if (state == SND_PCM_STATE_DISCONNECTED) {
        err = switch_route(pcm, 1);
        debug_event(pcm, "blocking-route-recover", err, -ENODEV);
        if (err < 0)
            return err;
    } else if (state == SND_PCM_STATE_XRUN ||
               state == SND_PCM_STATE_SUSPENDED) {
        err = recover_physical(pcm, state == SND_PCM_STATE_XRUN
                                        ? -EPIPE
                                        : -ESTRPIPE);
        if (err < 0)
            return err;
    }

    /*
     * Blocking SDL clients keep the descriptor returned by ALSA for the
     * lifetime of the logical stream. Wait on the current physical PCM here
     * instead of exposing its replaceable fd to the client. The eventfd
     * remains stable across USB/RK817 handoffs, while this bounded wait keeps
     * normal audio pacing tied to the selected hardware queue.
     */
    err = snd_pcm_wait(pcm->physical, 20);
    if (err < 0) {
        int recovered = recover_physical(pcm, err);

        if (recovered < 0 && is_route_loss_error(recovered)) {
            recovered = switch_route(pcm, 1);
            debug_event(pcm, "blocking-route-recover", recovered, err);
        }
        if (recovered < 0)
            return recovered;
        *revents = POLLOUT;
        return 0;
    }
    *revents = err > 0 ? POLLOUT : 0;
    return 0;
}

static int ensure_output_buffer(plumos_pcm_t *pcm, size_t frames)
{
    int16_t *resized;
    if (frames <= pcm->output_capacity)
        return 0;
    resized = realloc(pcm->output_buffer, frames * 2 * sizeof(*resized));
    if (!resized)
        return -ENOMEM;
    pcm->output_buffer = resized;
    pcm->output_capacity = frames;
    return 0;
}

static int16_t float_to_s16(float value)
{
    if (value >= 1.0f)
        return INT16_MAX;
    if (value <= -1.0f)
        return INT16_MIN;
    return (int16_t)(value * 32767.0f);
}

static int16_t read_input_sample(snd_pcm_format_t format, const void *input,
                                 size_t index)
{
    switch (format) {
    case SND_PCM_FORMAT_S8:
        return (int16_t)((int16_t)((const int8_t *)input)[index] * 256);
    case SND_PCM_FORMAT_U8:
        return (int16_t)(((int)((const uint8_t *)input)[index] - 128) * 256);
    case SND_PCM_FORMAT_S16_LE: {
        int16_t value;
        memcpy(&value, (const uint8_t *)input + index * sizeof(value),
               sizeof(value));
        return value;
    }
    case SND_PCM_FORMAT_U16_LE: {
        uint16_t value;
        memcpy(&value, (const uint8_t *)input + index * sizeof(value),
               sizeof(value));
        return (int16_t)((int32_t)value - 32768);
    }
    case SND_PCM_FORMAT_S24_LE: {
        uint32_t packed;
        int32_t value;
        memcpy(&packed, (const uint8_t *)input + index * sizeof(packed),
               sizeof(packed));
        value = (int32_t)(packed & 0x00ffffffU);
        if (value & 0x00800000)
            value -= 0x01000000;
        return (int16_t)(value / 256);
    }
    case SND_PCM_FORMAT_S24_3LE: {
        const uint8_t *sample = (const uint8_t *)input + index * 3;
        int32_t value = (int32_t)sample[0] |
                        ((int32_t)sample[1] << 8) |
                        ((int32_t)sample[2] << 16);
        if (value & 0x00800000)
            value -= 0x01000000;
        return (int16_t)(value / 256);
    }
    case SND_PCM_FORMAT_S32_LE: {
        int32_t value;
        memcpy(&value, (const uint8_t *)input + index * sizeof(value),
               sizeof(value));
        return (int16_t)(value / 65536);
    }
    case SND_PCM_FORMAT_FLOAT_LE: {
        float value;
        memcpy(&value, (const uint8_t *)input + index * sizeof(value),
               sizeof(value));
        return float_to_s16(value);
    }
    default:
        return 0;
    }
}

static void read_input_frame(const snd_pcm_ioplug_t *io, const void *input,
                             snd_pcm_uframes_t frame, int16_t *left,
                             int16_t *right)
{
    size_t index = (size_t)frame * io->channels;

    *left = read_input_sample(io->format, input, index);
    *right = io->channels == 1
                 ? *left
                 : read_input_sample(io->format, input, index + 1);
}

static int recover_physical(plumos_pcm_t *pcm, int error)
{
    snd_pcm_state_t state;
    int recovery_error = error;
    int result;

    if (!pcm->physical)
        return -ENODEV;
    state = snd_pcm_state(pcm->physical);
    if (state == SND_PCM_STATE_XRUN)
        recovery_error = -EPIPE;
    else if (state == SND_PCM_STATE_SUSPENDED)
        recovery_error = -ESTRPIPE;
    else if (error != -EPIPE && error != -ESTRPIPE)
        return error < 0 ? error : 0;
    else if (error >= 0)
        return 0;

    result = snd_pcm_recover(pcm->physical, recovery_error, 1);
    if (result < 0 && recovery_error == -ESTRPIPE)
        result = snd_pcm_prepare(pcm->physical);
    if (result >= 0)
        pcm->physical_resync_pending = 1;
    debug_event(pcm, "physical-recover", result, recovery_error);
    return result;
}

static void resync_prepared_physical(plumos_pcm_t *pcm)
{
    if (!pcm->physical_resync_pending || !pcm->physical ||
        pcm->io.state != SND_PCM_STATE_RUNNING ||
        snd_pcm_state(pcm->physical) != SND_PCM_STATE_PREPARED)
        return;

    /* The physical queue is empty after an XRUN recovery. Drop the matching
     * logical backlog so callback-driven clients can submit fresh audio. */
    pcm->submitted_ptr = pcm->io.appl_ptr;
    pcm->physical_resync_pending = 0;
    debug_event(pcm, "physical-resync", 0,
                (snd_pcm_sframes_t)pcm->submitted_ptr);
}

static snd_pcm_sframes_t write_physical(plumos_pcm_t *pcm,
                                        const int16_t *samples,
                                        snd_pcm_uframes_t frames)
{
    snd_pcm_uframes_t completed = 0;
    while (completed < frames) {
        snd_pcm_sframes_t written = snd_pcm_writei(
            pcm->physical, samples + completed * 2, frames - completed);
        if (written < 0) {
            int recovered = recover_physical(pcm, (int)written);
            if (recovered >= 0) {
                continue;
            }
            written = recovered;
        }
        if (written == -EAGAIN) {
            if (pcm->allow_fast_drop && pcm->fast_forward_active) {
                /*
                 * RetroArch waits for POLLOUT before every normal blocking
                 * chunk, but skips that wait after its fast-forward action
                 * switches the ALSA driver to nonblocking writes. The
                 * launcher enables this policy only for RetroArch, so EAGAIN
                 * here is the explicit backpressure boundary: accept the
                 * remaining emulated frames instead of pacing the runloop to
                 * the physical audio clock.
                 */
                return (snd_pcm_sframes_t)frames;
            }
            snd_pcm_wait(pcm->physical, 20);
            continue;
        }
        if (written < 0)
            return written;
        if (written == 0)
            return -EIO;
        completed += (snd_pcm_uframes_t)written;
    }
    return (snd_pcm_sframes_t)completed;
}

static snd_pcm_sframes_t plumos_transfer(snd_pcm_ioplug_t *io,
                                         const snd_pcm_channel_area_t *areas,
                                         snd_pcm_uframes_t offset,
                                         snd_pcm_uframes_t size)
{
    plumos_pcm_t *pcm = io->private_data;
    const void *input;
    const int16_t *output;
    snd_pcm_sframes_t result;
    snd_pcm_uframes_t frame;
    int fast_forward_active;
    int physical_width;
    int err;

    physical_width = snd_pcm_format_physical_width(io->format);
    if (!areas || io->channels < 1 || io->channels > 2 ||
        physical_width <= 0 ||
        areas[0].step != (unsigned int)physical_width * io->channels)
        return -EINVAL;
    input = (const unsigned char *)areas[0].addr + areas[0].first / 8 +
            offset * areas[0].step / 8;

    fast_forward_active = fast_forward_is_active(pcm);
    /*
     * Do not reset the physical PCM when RetroArch leaves fast-forward.
     * Unlike the standalone PCSX path, ALSA ioplug still owns a logical
     * stream whose pointer must remain continuous. A drop/prepare here makes
     * the logical hardware pointer jump, so RetroArch's audio rate control
     * progressively lowers pitch and emulation speed. The bounded physical
     * queue drains naturally while normal writes resume.
     */
    if (pcm->fast_forward_active && !fast_forward_active)
        debug_event(pcm, "fast-forward-release-continuous", 0, 0);
    pcm->fast_forward_active = fast_forward_active;

    if (pcm->volume_probe_countdown == 0) {
        pcm->volume_level = read_system_volume();
        pcm->volume_probe_countdown = VOLUME_PROBE_INTERVAL;
    } else {
        pcm->volume_probe_countdown--;
    }

    if (pcm->probe_countdown == 0) {
        err = switch_route(pcm, 0);
        if (err < 0)
            return err;
        pcm->probe_countdown = ROUTE_PROBE_INTERVAL;
    } else {
        pcm->probe_countdown--;
    }

    if (!pcm->physical) {
        err = switch_route(pcm, 1);
        if (err < 0)
            return err;
    }

    output = input;
    if (io->channels == 1 || io->format != SND_PCM_FORMAT_S16_LE ||
        (pcm->physical_is_usb && pcm->volume_level < VOLUME_MAX)) {
        err = ensure_output_buffer(pcm, size);
        if (err < 0)
            return err;
        for (frame = 0; frame < size; frame++) {
            int16_t left;
            int16_t right;
            read_input_frame(io, input, frame, &left, &right);
            if (pcm->physical_is_usb) {
                left = apply_software_volume(left, pcm->volume_level);
                right = apply_software_volume(right, pcm->volume_level);
            }
            pcm->output_buffer[frame * 2] = left;
            pcm->output_buffer[frame * 2 + 1] = right;
        }
        output = pcm->output_buffer;
    }

    result = write_physical(pcm, output, size);
    if (result < 0 && result != -EPIPE && result != -ESTRPIPE) {
        if (switch_route(pcm, 1) == 0) {
            if (io->channels == 1 ||
                io->format != SND_PCM_FORMAT_S16_LE ||
                (pcm->physical_is_usb &&
                 pcm->volume_level < VOLUME_MAX)) {
                err = ensure_output_buffer(pcm, size);
                if (err < 0)
                    return err;
                for (frame = 0; frame < size; frame++) {
                    int16_t left;
                    int16_t right;
                    read_input_frame(io, input, frame, &left, &right);
                    if (pcm->physical_is_usb) {
                        left = apply_software_volume(
                            left, pcm->volume_level);
                        right = apply_software_volume(
                            right, pcm->volume_level);
                    }
                    pcm->output_buffer[frame * 2] = left;
                    pcm->output_buffer[frame * 2 + 1] = right;
                }
                output = pcm->output_buffer;
            } else {
                output = input;
            }
            result = write_physical(pcm, output, size);
        }
    }
    if (result > 0 && io->buffer_size) {
        pcm->submitted_ptr =
            (pcm->submitted_ptr + (snd_pcm_uframes_t)result) % io->buffer_size;
    }
    debug_event(pcm, "transfer", result, (snd_pcm_sframes_t)size);
    return result;
}

static int plumos_start(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;
    int err = pcm->physical ? 0 : switch_route(pcm, 1);

    debug_event(pcm, "start", err, 0);
    return err;
}

static int plumos_stop(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;

    if (pcm->physical)
        snd_pcm_drop(pcm->physical);
    pcm->physical_resync_pending = 0;
    debug_event(pcm, "stop", 0, 0);
    return 0;
}

static int physical_queued_frames(plumos_pcm_t *pcm,
                                  snd_pcm_sframes_t *queued)
{
    snd_pcm_sframes_t avail;
    int recovered;

    if (!pcm->physical || !pcm->physical_buffer_size)
        return -ENODEV;
    avail = snd_pcm_avail_update(pcm->physical);
    if (avail < 0) {
        recovered = recover_physical(pcm, (int)avail);
        if (recovered < 0 && is_route_loss_error(recovered)) {
            recovered = switch_route(pcm, 1);
            debug_event(pcm, "route-recover-pointer", recovered, (int)avail);
        }
        if (recovered < 0)
            return recovered;
        *queued = 0;
        return 0;
    }
    if ((snd_pcm_uframes_t)avail > pcm->physical_buffer_size)
        avail = (snd_pcm_sframes_t)pcm->physical_buffer_size;
    *queued = (snd_pcm_sframes_t)pcm->physical_buffer_size - avail;
    return 0;
}

static snd_pcm_sframes_t plumos_pointer(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;
    snd_pcm_sframes_t queued;
    int err;

    if (!pcm->physical || !io->buffer_size)
        return 0;
    resync_prepared_physical(pcm);
    err = physical_queued_frames(pcm, &queued);
    if (err < 0) {
        debug_event(pcm, "pointer-error", err, 0);
        return err;
    }
    if ((snd_pcm_uframes_t)queued > io->buffer_size)
        queued = (snd_pcm_sframes_t)io->buffer_size;
    err = (int)(
        (pcm->submitted_ptr + io->buffer_size -
         (snd_pcm_uframes_t)queued) % io->buffer_size);
    debug_event(pcm, "pointer", err, queued);
    return err;
}

static int plumos_prepare(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;
    int err = switch_route(pcm, 0);
    if (err < 0)
        return err;
    pcm->submitted_ptr = 0;
    pcm->physical_resync_pending = 0;
    err = snd_pcm_prepare(pcm->physical);
    if (err < 0)
        return err;
    debug_event(pcm, "prepare", err, 0);
    return 0;
}

static int plumos_drain(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;
    return pcm->physical ? snd_pcm_drain(pcm->physical) : 0;
}

static int plumos_delay(snd_pcm_ioplug_t *io, snd_pcm_sframes_t *delayp)
{
    plumos_pcm_t *pcm = io->private_data;
    int err;

    if (!pcm->physical) {
        *delayp = 0;
        return 0;
    }
    resync_prepared_physical(pcm);
    err = physical_queued_frames(pcm, delayp);
    if (err < 0)
        return err;
    if (io->buffer_size &&
        (snd_pcm_uframes_t)*delayp > io->buffer_size)
        *delayp = (snd_pcm_sframes_t)io->buffer_size;
    return 0;
}

static int plumos_poll_descriptors_count(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;

    if (pcm->poll_proxy_fd >= 0)
        return 1;
    return pcm->physical ? snd_pcm_poll_descriptors_count(pcm->physical) : 0;
}

static int plumos_poll_descriptors(snd_pcm_ioplug_t *io, struct pollfd *pfds,
                                   unsigned int space)
{
    plumos_pcm_t *pcm = io->private_data;

    if (pcm->poll_proxy_fd >= 0) {
        if (space < 1)
            return -EINVAL;
        pfds[0].fd = pcm->poll_proxy_fd;
        pfds[0].events = POLLOUT;
        pfds[0].revents = 0;
        return 1;
    }
    if (!pcm->physical)
        return -ENODEV;
    return snd_pcm_poll_descriptors(pcm->physical, pfds, space);
}

static int plumos_poll_revents(snd_pcm_ioplug_t *io, struct pollfd *pfds,
                               unsigned int nfds, unsigned short *revents)
{
    plumos_pcm_t *pcm = io->private_data;
    snd_pcm_state_t state;
    unsigned int index;
    int err;

    if (!pcm->physical)
        return -ENODEV;
    if (pcm->poll_proxy_fd >= 0)
        return blocking_poll_revents(pcm, revents);
    state = snd_pcm_state(pcm->physical);
    if (state == SND_PCM_STATE_DISCONNECTED)
        return refresh_poll_route(pcm, pfds, nfds, revents, -ENODEV);
    for (index = 0; index < nfds; index++) {
        if (pfds[index].revents & (POLLHUP | POLLNVAL))
            return refresh_poll_route(pcm, pfds, nfds, revents, -ENODEV);
    }
    if (pcm->physical_resync_pending && state == SND_PCM_STATE_PREPARED &&
        io->state == SND_PCM_STATE_RUNNING) {
        resync_prepared_physical(pcm);
        *revents = POLLOUT;
        debug_event(pcm, "poll-resync", 0, *revents);
        return 0;
    }
    if (state == SND_PCM_STATE_XRUN || state == SND_PCM_STATE_SUSPENDED) {
        err = recover_physical(pcm, state == SND_PCM_STATE_XRUN
                                        ? -EPIPE
                                        : -ESTRPIPE);
        if (err < 0)
            return err;
        *revents = POLLOUT;
        debug_event(pcm, "poll-recover", 0, *revents);
        return 0;
    }
    err = snd_pcm_poll_descriptors_revents(
        pcm->physical, pfds, nfds, revents);
    if (err < 0 && is_route_loss_error(err))
        return refresh_poll_route(pcm, pfds, nfds, revents, err);
    if (err >= 0 && (*revents & POLLERR)) {
        state = snd_pcm_state(pcm->physical);
        if (state == SND_PCM_STATE_XRUN || state == SND_PCM_STATE_SUSPENDED) {
            err = recover_physical(pcm, state == SND_PCM_STATE_XRUN
                                            ? -EPIPE
                                            : -ESTRPIPE);
            if (err >= 0)
                *revents = POLLOUT;
        } else {
            return refresh_poll_route(pcm, pfds, nfds, revents, -EIO);
        }
    }
    debug_event(pcm, "poll", err, err < 0 ? 0 : *revents);
    return err;
}

static int plumos_close(snd_pcm_ioplug_t *io)
{
    plumos_pcm_t *pcm = io->private_data;
    if (pcm->physical)
        snd_pcm_close(pcm->physical);
    if (pcm->poll_proxy_fd >= 0)
        close(pcm->poll_proxy_fd);
    free(pcm->output_buffer);
    free(pcm);
    return 0;
}

static const snd_pcm_ioplug_callback_t plumos_callbacks = {
    .start = plumos_start,
    .stop = plumos_stop,
    .pointer = plumos_pointer,
    .transfer = plumos_transfer,
    .close = plumos_close,
    .prepare = plumos_prepare,
    .drain = plumos_drain,
    .delay = plumos_delay,
    .poll_descriptors_count = plumos_poll_descriptors_count,
    .poll_descriptors = plumos_poll_descriptors,
    .poll_revents = plumos_poll_revents,
};

static int set_constraints(snd_pcm_ioplug_t *io)
{
    static const unsigned int access[] = { SND_PCM_ACCESS_RW_INTERLEAVED };
    /*
     * The physical handle is plughw, so it converts values not accepted
     * directly by a discrete-rate USB DAC. Keep the logical range bounded to
     * the rates needed by Pixel2 applications and accepted by the RK817 path.
     */
    static const unsigned int format[] = {
        SND_PCM_FORMAT_S8,
        SND_PCM_FORMAT_U8,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_U16_LE,
        SND_PCM_FORMAT_S24_LE,
        SND_PCM_FORMAT_S24_3LE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_FLOAT_LE,
    };
    static const unsigned int channels[] = { 1, 2 };
    int err;

    if ((err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS,
                                              1, access)) < 0 ||
        (err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT,
                                              8, format)) < 0 ||
        (err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_CHANNELS,
                                              2, channels)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(io,
                                                SND_PCM_IOPLUG_HW_RATE,
                                                8000, 96000)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(io,
                                                SND_PCM_IOPLUG_HW_PERIOD_BYTES,
                                                256, 65536)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(io,
                                                SND_PCM_IOPLUG_HW_BUFFER_BYTES,
                                                1024, 262144)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(io,
                                                SND_PCM_IOPLUG_HW_PERIODS,
                                                2, 8)) < 0)
        return err;
    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(plumos_hotplug)
{
    snd_config_iterator_t i, next;
    plumos_pcm_t *pcm;
    const char *poll_proxy;
    int err;

    if (stream != SND_PCM_STREAM_PLAYBACK)
        return -EINVAL;
    snd_config_for_each(i, next, conf) {
        snd_config_t *node = snd_config_iterator_entry(i);
        const char *id;
        if (snd_config_get_id(node, &id) < 0)
            continue;
        if (!strcmp(id, "comment") || !strcmp(id, "type") || !strcmp(id, "hint"))
            continue;
        SNDERR("Unknown field %s", id);
        return -EINVAL;
    }

    pcm = calloc(1, sizeof(*pcm));
    if (!pcm)
        return -ENOMEM;
    pcm->physical_card = -1;
    pcm->poll_proxy_fd = -1;
    pcm->volume_level = read_system_volume();
    pcm->allow_fast_drop =
        getenv("PLUMOS_AUDIO_FAST_FORWARD_DROP") &&
        !strcmp(getenv("PLUMOS_AUDIO_FAST_FORWARD_DROP"), "1");
    poll_proxy = getenv("PLUMOS_AUDIO_POLL_PROXY");
    if (pcm->allow_fast_drop &&
        getenv("PLUMOS_AUDIO_FAST_FORWARD_STATE") &&
        !strncmp(getenv("PLUMOS_AUDIO_FAST_FORWARD_STATE"),
                 "/run/plumos/audio/", 18) &&
        !strstr(getenv("PLUMOS_AUDIO_FAST_FORWARD_STATE"), ".."))
        snprintf(pcm->fast_forward_state, sizeof(pcm->fast_forward_state),
                 "%s", getenv("PLUMOS_AUDIO_FAST_FORWARD_STATE"));
    /*
     * Most blocking clients need a stable descriptor across a physical-route
     * replacement.  YabaSanshiro's SDL2 ALSA backend, however, waits for the
     * real PCM descriptor before submitting its first buffer.  A permanently
     * writable eventfd makes that startup loop spin while the physical PCM
     * remains PREPARED.  Permit that client to retain the physical ALSA poll
     * contract without changing the default hotplug behaviour.
     */
    if (!pcm->allow_fast_drop &&
        (!poll_proxy || strcmp(poll_proxy, "0"))) {
        pcm->poll_proxy_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (pcm->poll_proxy_fd < 0) {
            err = -errno;
            free(pcm);
            return err;
        }
    }
    pcm->debug_enabled = access("/run/plumos/audio/debug", F_OK) == 0;

    pcm->io.version = SND_PCM_IOPLUG_VERSION;
    pcm->io.name = "plumOS Pixel2 hotplug audio";
    pcm->io.poll_fd = -1;
    pcm->io.poll_events = 0;
    pcm->io.mmap_rw = 0;
    pcm->io.callback = &plumos_callbacks;
    pcm->io.private_data = pcm;

    err = snd_pcm_ioplug_create(&pcm->io, name, stream, mode);
    if (err < 0)
        goto error;
    err = set_constraints(&pcm->io);
    if (err < 0) {
        snd_pcm_ioplug_delete(&pcm->io);
        return err;
    }
    *pcmp = pcm->io.pcm;
    return 0;

error:
    if (pcm->poll_proxy_fd >= 0)
        close(pcm->poll_proxy_fd);
    free(pcm);
    return err;
}

SND_PCM_PLUGIN_SYMBOL(plumos_hotplug);
