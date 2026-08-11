// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define INPUT_SCAN_LIMIT 32
#define REOPEN_INTERVAL_MS 2000
#define REPEAT_DELAY_MS 450
#define REPEAT_INTERVAL_MS 120
#define PERSIST_DELAY_MS 750

struct input_source {
  const char *name;
  int fd;
};

static volatile sig_atomic_t running = 1;

static void stop_running(int signal_number) {
  (void)signal_number;
  running = 0;
}

static long long monotonic_ms(void) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
    return 0;
  }
  return (long long)now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
}

static int open_named_event(const char *target_name) {
  int index;
  for (index = 0; index < INPUT_SCAN_LIMIT; index++) {
    char path[64];
    char name[256] = "";
    int fd;

    snprintf(path, sizeof(path), "/dev/input/event%d", index);
    fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
      continue;
    }
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
        strcmp(name, target_name) == 0) {
      fprintf(stderr, "hardware-keys: opened=%s name=%s\n", path, name);
      return fd;
    }
    close(fd);
  }
  return -1;
}

static void reopen_source(struct input_source *source) {
  if (source->fd < 0) {
    source->fd = open_named_event(source->name);
  }
}

static int run_helper(const char *helper_name, const char *action) {
  const char *root = getenv("PLUMOS_ROOT");
  char helper[512];
  pid_t child;
  int status;

  if (!root || !root[0]) {
    root = "/mnt/plumos";
  }
  if (snprintf(helper, sizeof(helper), "%s/bin/%s", root, helper_name) >=
      (int)sizeof(helper)) {
    return -ENAMETOOLONG;
  }
  child = fork();
  if (child < 0) {
    return -errno;
  }
  if (child == 0) {
    execl(helper, helper, action, (char *)NULL);
    _exit(127);
  }
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      return -errno;
    }
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -EIO;
}

static int apply_key_action(int direction, int display_action) {
  const char *helper_name =
      display_action ? "plumos-display-control" : "plumos-volume-control";
  const char *action = direction > 0 ? "runtime-up" : "runtime-down";
  int result = run_helper(helper_name, action);

  fprintf(stderr, "hardware-keys: action=%s direction=%s rc=%d\n",
          display_action ? "display-brightness" : "volume",
          direction > 0 ? "up" : "down", result);
  return result;
}

static void persist_pending(int *volume_pending, int *display_pending) {
  if (*volume_pending) {
    int result = run_helper("plumos-volume-control", "persist-runtime");
    fprintf(stderr, "hardware-keys: persist=volume rc=%d\n", result);
    if (result == 0) {
      *volume_pending = 0;
    }
  }
  if (*display_pending) {
    int result = run_helper("plumos-display-control", "persist-runtime");
    fprintf(stderr, "hardware-keys: persist=display-brightness rc=%d\n",
            result);
    if (result == 0) {
      *display_pending = 0;
    }
  }
}

int main(void) {
  struct input_source gamepad = {"pixel2_joypad", -1};
  struct input_source volume_keys = {"gpio-keys", -1};
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char run_dir[512];
  char lock_path[512];
  int lock_fd;
  long long next_reopen = 0;
  long long repeat_due = 0;
  long long persist_due = 0;
  int select_down = 0;
  int held_direction = 0;
  int held_is_display = 0;
  int volume_pending = 0;
  int display_pending = 0;

  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(run_dir, sizeof(run_dir), "%s/hardware-keys", runtime_root) >=
          (int)sizeof(run_dir) ||
      snprintf(lock_path, sizeof(lock_path), "%s/daemon.lock", run_dir) >=
          (int)sizeof(lock_path)) {
    return 1;
  }
  if (mkdir(run_dir, 0755) < 0 && errno != EEXIST) {
    return 1;
  }
  lock_fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0644);
  if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
    if (lock_fd >= 0) {
      close(lock_fd);
    }
    return 1;
  }

  signal(SIGINT, stop_running);
  signal(SIGTERM, stop_running);
  setvbuf(stderr, NULL, _IOLBF, 0);
  fprintf(stderr, "hardware-keys: start owner=plumos device=pixel2\n");
  (void)run_helper("plumos-volume-control", "apply");
  (void)run_helper("plumos-display-control", "apply");

  while (running) {
    struct input_source *sources[2] = {&gamepad, &volume_keys};
    struct pollfd poll_fds[2];
    long long now = monotonic_ms();
    int index;
    int ready;

    if (now >= next_reopen) {
      reopen_source(&gamepad);
      reopen_source(&volume_keys);
      next_reopen = now + REOPEN_INTERVAL_MS;
    }
    for (index = 0; index < 2; index++) {
      poll_fds[index].fd = sources[index]->fd;
      poll_fds[index].events = POLLIN;
      poll_fds[index].revents = 0;
    }
    ready = poll(poll_fds, 2, 100);
    now = monotonic_ms();
    if (ready < 0 && errno != EINTR) {
      break;
    }
    for (index = 0; ready > 0 && index < 2; index++) {
      struct input_source *source = sources[index];

      if (source->fd < 0 ||
          !(poll_fds[index].revents & (POLLIN | POLLERR | POLLHUP))) {
        continue;
      }
      for (;;) {
        struct input_event event;
        ssize_t bytes = read(source->fd, &event, sizeof(event));

        if (bytes == (ssize_t)sizeof(event)) {
          if (source == &gamepad && event.type == EV_KEY &&
              event.code == BTN_SELECT) {
            select_down = event.value != 0;
          } else if (source == &volume_keys && event.type == EV_KEY &&
                     (event.code == KEY_VOLUMEUP ||
                      event.code == KEY_VOLUMEDOWN)) {
            int direction = event.code == KEY_VOLUMEUP ? 1 : -1;

            if (event.value == 1) {
              int result;

              held_direction = direction;
              held_is_display = select_down;
              result = apply_key_action(direction, held_is_display);
              if (result == 0) {
                if (held_is_display) {
                  display_pending = 1;
                } else {
                  volume_pending = 1;
                }
                persist_due = now + PERSIST_DELAY_MS;
              }
              repeat_due = now + REPEAT_DELAY_MS;
            } else if (event.value == 0 && direction == held_direction) {
              held_direction = 0;
              repeat_due = 0;
            }
          }
          continue;
        }
        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
          break;
        }
        if (bytes < 0 && errno == EINTR) {
          continue;
        }
        fprintf(stderr, "hardware-keys: closed name=%s errno=%d\n",
                source->name, bytes < 0 ? errno : 0);
        close(source->fd);
        source->fd = -1;
        if (source == &gamepad) {
          select_down = 0;
        }
        if (source == &volume_keys) {
          held_direction = 0;
          repeat_due = 0;
        }
        next_reopen = 0;
        break;
      }
    }

    now = monotonic_ms();
    if (held_direction && repeat_due > 0 && now >= repeat_due) {
      int result = apply_key_action(held_direction, held_is_display);

      if (result == 0) {
        if (held_is_display) {
          display_pending = 1;
        } else {
          volume_pending = 1;
        }
        persist_due = now + PERSIST_DELAY_MS;
      }
      repeat_due = now + REPEAT_INTERVAL_MS;
    }
    if ((volume_pending || display_pending) && persist_due > 0 &&
        now >= persist_due) {
      persist_pending(&volume_pending, &display_pending);
      persist_due =
          (volume_pending || display_pending) ? now + PERSIST_DELAY_MS : 0;
    }
  }

  persist_pending(&volume_pending, &display_pending);
  if (gamepad.fd >= 0) {
    close(gamepad.fd);
  }
  if (volume_keys.fd >= 0) {
    close(volume_keys.fd);
  }
  close(lock_fd);
  fprintf(stderr, "hardware-keys: stopped\n");
  return 0;
}
