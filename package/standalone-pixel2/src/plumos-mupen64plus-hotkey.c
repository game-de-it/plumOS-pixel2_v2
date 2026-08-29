// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FUNCTION_EXIT_HOLD_MS 1500L

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number) {
  (void)signal_number;
  stop_requested = 1;
}

static int target_matches(pid_t pid, const char *expected_exe) {
  char proc_path[64];
  char actual_exe[4096];
  ssize_t length;

  snprintf(proc_path, sizeof(proc_path), "/proc/%ld/exe", (long)pid);
  length = readlink(proc_path, actual_exe, sizeof(actual_exe) - 1);
  if (length < 0) {
    return 0;
  }
  actual_exe[length] = '\0';
  return strcmp(actual_exe, expected_exe) == 0;
}

static int wait_for_target(pid_t pid, const char *expected_exe) {
  int attempt;

  for (attempt = 0; attempt < 40; attempt++) {
    if (target_matches(pid, expected_exe)) {
      return 1;
    }
    if (kill(pid, 0) != 0 && errno == ESRCH) {
      return 0;
    }
    usleep(50000);
  }
  return 0;
}

static long elapsed_milliseconds(const struct timespec *start,
                                 const struct timespec *end) {
  return (end->tv_sec - start->tv_sec) * 1000L +
         (end->tv_nsec - start->tv_nsec) / 1000000L;
}

static int open_pixel2_joypad(char *device_path, size_t device_path_size) {
  char name_path[128];
  char name[128];
  int index;

  for (index = 0; index < 32; index++) {
    FILE *name_file;
    size_t length;
    int fd;

    snprintf(name_path, sizeof(name_path),
             "/sys/class/input/event%d/device/name", index);
    name_file = fopen(name_path, "r");
    if (!name_file) {
      continue;
    }
    if (!fgets(name, sizeof(name), name_file)) {
      fclose(name_file);
      continue;
    }
    fclose(name_file);
    length = strcspn(name, "\r\n");
    name[length] = '\0';
    if (strcmp(name, "pixel2_joypad") != 0) {
      continue;
    }
    snprintf(device_path, device_path_size, "/dev/input/event%d", index);
    fd = open(device_path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
      return fd;
    }
  }
  return -1;
}

int main(int argc, char **argv) {
  char *pid_end = NULL;
  char device_path[64];
  long parsed_pid;
  pid_t target_pid;
  const char *expected_exe;
  struct pollfd input_poll;
  struct timespec function_pressed_at = {0, 0};
  int function_down = 0;
  int input_fd;

  if (argc != 3) {
    fprintf(stderr, "usage: %s PID EXPECTED_EXE\n", argv[0]);
    return 2;
  }
  errno = 0;
  parsed_pid = strtol(argv[1], &pid_end, 10);
  if (errno != 0 || !pid_end || *pid_end != '\0' || parsed_pid <= 1) {
    fprintf(stderr, "mupen64plus-hotkey: invalid pid: %s\n", argv[1]);
    return 2;
  }
  target_pid = (pid_t)parsed_pid;
  expected_exe = argv[2];
  if (expected_exe[0] != '/' || !wait_for_target(target_pid, expected_exe)) {
    fprintf(stderr, "mupen64plus-hotkey: target ownership mismatch\n");
    return 3;
  }

  input_fd = open_pixel2_joypad(device_path, sizeof(device_path));
  if (input_fd < 0) {
    fprintf(stderr, "mupen64plus-hotkey: pixel2_joypad input not found\n");
    return 4;
  }
  signal(SIGINT, request_stop);
  signal(SIGTERM, request_stop);
  input_poll.fd = input_fd;
  input_poll.events = POLLIN;
  fprintf(stderr,
          "mupen64plus-hotkey: monitoring %s BTN_TRIGGER_HAPPY1\n",
          device_path);

  while (!stop_requested && target_matches(target_pid, expected_exe)) {
    struct input_event events[16];
    ssize_t bytes_read;
    size_t event_count;
    size_t index;
    int poll_result;

    input_poll.revents = 0;
    poll_result = poll(&input_poll, 1, 250);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (poll_result > 0 && (input_poll.revents & POLLIN)) {
      bytes_read = read(input_fd, events, sizeof(events));
      if (bytes_read < 0) {
        if (errno == EINTR || errno == EAGAIN) {
          continue;
        }
        break;
      }
      event_count = (size_t)bytes_read / sizeof(events[0]);
      for (index = 0; index < event_count; index++) {
        if (events[index].type == EV_KEY &&
            events[index].code == BTN_TRIGGER_HAPPY1) {
          if (events[index].value == 1) {
            function_down = 1;
            if (clock_gettime(CLOCK_MONOTONIC, &function_pressed_at) != 0) {
              perror("mupen64plus-hotkey: clock_gettime");
              close(input_fd);
              return 5;
            }
            fprintf(stderr,
                    "mupen64plus-hotkey: Function pressed; hold %ld ms to exit\n",
                    FUNCTION_EXIT_HOLD_MS);
          } else if (events[index].value == 0) {
            function_down = 0;
            fprintf(stderr,
                    "mupen64plus-hotkey: Function released; game continues\n");
          }
        }
      }
    }

    if (function_down) {
      struct timespec now;
      if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        perror("mupen64plus-hotkey: clock_gettime");
        close(input_fd);
        return 5;
      }
      if (elapsed_milliseconds(&function_pressed_at, &now) >=
          FUNCTION_EXIT_HOLD_MS) {
        if (!target_matches(target_pid, expected_exe)) {
          close(input_fd);
          return 0;
        }
        fprintf(stderr,
                "mupen64plus-hotkey: Function held; stopping pid=%ld\n",
                (long)target_pid);
        if (kill(target_pid, SIGTERM) != 0 && errno != ESRCH) {
          perror("mupen64plus-hotkey: kill");
          close(input_fd);
          return 5;
        }
        close(input_fd);
        return 0;
      }
    }
  }

  close(input_fd);
  return 0;
}
