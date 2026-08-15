// SPDX-License-Identifier: MIT

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_pidfd_getfd
#define SYS_pidfd_getfd 438
#endif

#define INPUT_SCAN_LIMIT 32
#define REOPEN_INTERVAL_MS 2000
#define REPEAT_DELAY_MS 450
#define REPEAT_INTERVAL_MS 120
#define PERSIST_DELAY_MS 750
#define POWER_MENU_DEBOUNCE_MS 800
#define USB_POWER_EVENT_GUARD_MS 1500
#define ADB_USB_RESTART_DELAY_MS 2000
#define DRM_PLANE_SNAPSHOT_LIMIT 16

struct input_source {
  const char *name;
  int fd;
};

struct drm_plane_snapshot {
  uint32_t plane_id;
  uint32_t crtc_id;
  uint32_t fb_id;
  uint32_t crtc_x;
  uint32_t crtc_y;
  uint32_t crtc_w;
  uint32_t crtc_h;
  uint32_t src_x;
  uint32_t src_y;
  uint32_t src_w;
  uint32_t src_h;
};

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t power_menu_requested = 0;

static void stop_running(int signal_number) {
  (void)signal_number;
  running = 0;
}

static void request_power_menu(int signal_number) {
  (void)signal_number;
  power_menu_requested = 1;
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

static void reap_children(void) {
  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }
}

static int target_is_display_device(const char *target) {
  return target &&
         (strcmp(target, "/dev/fb0") == 0 ||
          strncmp(target, "/dev/dri/", 9) == 0 ||
          strncmp(target, "/dev/mali", 9) == 0 ||
          strcmp(target, "/dev/disp") == 0);
}

static int process_owns_display(pid_t pid) {
  char directory_path[64];
  DIR *directory;
  struct dirent *entry;
  int owns_display = 0;

  if (pid <= 1 || snprintf(directory_path, sizeof(directory_path),
                           "/proc/%ld/fd", (long)pid) >=
                      (int)sizeof(directory_path)) {
    return 0;
  }
  directory = opendir(directory_path);
  if (!directory) {
    return 0;
  }
  while ((entry = readdir(directory)) != NULL) {
    char link_path[128];
    char target[256];
    ssize_t length;

    if (entry->d_name[0] == '.') {
      continue;
    }
    if (snprintf(link_path, sizeof(link_path), "%s/%s", directory_path,
                 entry->d_name) >= (int)sizeof(link_path)) {
      continue;
    }
    length = readlink(link_path, target, sizeof(target) - 1);
    if (length < 0) {
      continue;
    }
    target[length] = '\0';
    if (target_is_display_device(target)) {
      owns_display = 1;
      break;
    }
  }
  closedir(directory);
  return owns_display;
}

static int process_drm_fd(pid_t pid) {
  char directory_path[64];
  DIR *directory;
  struct dirent *entry;
  int result = -1;

  if (pid <= 1 || snprintf(directory_path, sizeof(directory_path),
                           "/proc/%ld/fd", (long)pid) >=
                      (int)sizeof(directory_path)) {
    return -1;
  }
  directory = opendir(directory_path);
  if (!directory) {
    return -1;
  }
  while ((entry = readdir(directory)) != NULL) {
    char link_path[128];
    char target[256];
    char *end = NULL;
    long fd_number;
    ssize_t length;

    if (entry->d_name[0] == '.') {
      continue;
    }
    errno = 0;
    fd_number = strtol(entry->d_name, &end, 10);
    if (errno || !end || *end || fd_number < 0 || fd_number > 1048576) {
      continue;
    }
    if (snprintf(link_path, sizeof(link_path), "%s/%s", directory_path,
                 entry->d_name) >= (int)sizeof(link_path)) {
      continue;
    }
    length = readlink(link_path, target, sizeof(target) - 1);
    if (length < 0) {
      continue;
    }
    target[length] = '\0';
    if (strcmp(target, "/dev/dri/card0") == 0) {
      result = (int)fd_number;
      break;
    }
  }
  closedir(directory);
  return result;
}

static pid_t external_drm_owner(int *target_fd) {
  DIR *proc;
  struct dirent *entry;
  pid_t result = 0;

  if (target_fd) {
    *target_fd = -1;
  }
  proc = opendir("/proc");
  if (!proc) {
    return 0;
  }
  while ((entry = readdir(proc)) != NULL) {
    char *end = NULL;
    long value;
    int fd;

    if (entry->d_name[0] < '0' || entry->d_name[0] > '9') {
      continue;
    }
    errno = 0;
    value = strtol(entry->d_name, &end, 10);
    if (errno || !end || *end || value <= 1 || value == (long)getpid()) {
      continue;
    }
    fd = process_drm_fd((pid_t)value);
    if (fd >= 0) {
      result = (pid_t)value;
      if (target_fd) {
        *target_fd = fd;
      }
      break;
    }
  }
  closedir(proc);
  return result;
}

static int duplicate_process_fd(pid_t pid, int target_fd) {
  int pidfd;
  int duplicated;

  pidfd = (int)syscall(SYS_pidfd_open, pid, 0U);
  if (pidfd < 0) {
    return -errno;
  }
  duplicated = (int)syscall(SYS_pidfd_getfd, pidfd, target_fd, 0U);
  if (duplicated < 0) {
    duplicated = -errno;
  }
  close(pidfd);
  return duplicated;
}

static int process_is_stopped(pid_t pid) {
  char path[64];
  char line[512];
  char *closing;
  FILE *file;

  if (snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid) >=
      (int)sizeof(path)) {
    return 0;
  }
  file = fopen(path, "r");
  if (!file) {
    return 0;
  }
  line[0] = '\0';
  (void)fgets(line, sizeof(line), file);
  fclose(file);
  closing = strrchr(line, ')');
  return closing && closing[1] == ' ' &&
         (closing[2] == 'T' || closing[2] == 't');
}

static int stop_process_for_overlay(pid_t pid) {
  int attempt;

  if (pid <= 1 || kill(pid, SIGSTOP) < 0) {
    return 0;
  }
  for (attempt = 0; attempt < 20; attempt++) {
    if (process_is_stopped(pid)) {
      return 1;
    }
    usleep(10000);
  }
  return process_is_stopped(pid);
}

static size_t suspend_drm_planes(
    int drm_fd, struct drm_plane_snapshot *snapshots, size_t capacity) {
  drmModePlaneRes *resources;
  size_t count = 0;
  uint32_t index;

  if (drm_fd < 0 || !snapshots || capacity == 0) {
    return 0;
  }
  (void)drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  resources = drmModeGetPlaneResources(drm_fd);
  if (!resources) {
    return 0;
  }
  for (index = 0; index < resources->count_planes && count < capacity;
       index++) {
    drmModePlane *plane = drmModeGetPlane(drm_fd, resources->planes[index]);
    drmModeFB *framebuffer;
    struct drm_plane_snapshot *snapshot;

    if (!plane) {
      continue;
    }
    if (!plane->crtc_id || !plane->fb_id) {
      drmModeFreePlane(plane);
      continue;
    }
    framebuffer = drmModeGetFB(drm_fd, plane->fb_id);
    if (!framebuffer) {
      drmModeFreePlane(plane);
      continue;
    }
    snapshot = &snapshots[count];
    snapshot->plane_id = plane->plane_id;
    snapshot->crtc_id = plane->crtc_id;
    snapshot->fb_id = plane->fb_id;
    snapshot->crtc_x = plane->crtc_x;
    snapshot->crtc_y = plane->crtc_y;
    snapshot->crtc_w = framebuffer->width;
    snapshot->crtc_h = framebuffer->height;
    snapshot->src_x = plane->x;
    snapshot->src_y = plane->y;
    snapshot->src_w = framebuffer->width << 16;
    snapshot->src_h = framebuffer->height << 16;
    if (drmModeSetPlane(drm_fd, snapshot->plane_id, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0) == 0) {
      count++;
    }
    drmModeFreeFB(framebuffer);
    drmModeFreePlane(plane);
  }
  drmModeFreePlaneResources(resources);
  return count;
}

static int restore_drm_planes(int drm_fd,
                              const struct drm_plane_snapshot *snapshots,
                              size_t count) {
  size_t index;
  int ok = 1;

  for (index = 0; index < count; index++) {
    const struct drm_plane_snapshot *snapshot = &snapshots[index];
    if (drmModeSetPlane(
            drm_fd, snapshot->plane_id, snapshot->crtc_id, snapshot->fb_id, 0,
            snapshot->crtc_x, snapshot->crtc_y, snapshot->crtc_w,
            snapshot->crtc_h, snapshot->src_x, snapshot->src_y,
            snapshot->src_w, snapshot->src_h) != 0) {
      ok = 0;
    }
  }
  return ok;
}

static pid_t frontend_pid(void) {
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char path[512];
  char line[64];
  FILE *file;
  long value;

  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(path, sizeof(path), "%s/frontend.pid", runtime_root) >=
      (int)sizeof(path)) {
    return 0;
  }
  file = fopen(path, "r");
  if (!file) {
    return 0;
  }
  line[0] = '\0';
  if (!fgets(line, sizeof(line), file)) {
    fclose(file);
    return 0;
  }
  fclose(file);
  errno = 0;
  value = strtol(line, NULL, 10);
  if (errno || value <= 1 || kill((pid_t)value, 0) < 0) {
    return 0;
  }
  return (pid_t)value;
}

static int software_sleep_active(void) {
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char sleep_marker[512];

  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(sleep_marker, sizeof(sleep_marker), "%s/software-sleep",
               runtime_root) >= (int)sizeof(sleep_marker)) {
    return 0;
  }
  return access(sleep_marker, F_OK) == 0;
}

static int wake_software_sleep(void) {
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char sleep_marker[512];
  char suppress_marker[512];
  int fd;

  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(sleep_marker, sizeof(sleep_marker), "%s/software-sleep",
               runtime_root) >= (int)sizeof(sleep_marker) ||
      snprintf(suppress_marker, sizeof(suppress_marker),
               "%s/power-wake-suppress", runtime_root) >=
          (int)sizeof(suppress_marker)) {
    return 0;
  }
  if (access(sleep_marker, F_OK) != 0) {
    return 0;
  }
  fd = open(suppress_marker, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
  if (fd >= 0) {
    (void)write(fd, "wake\n", 5);
    close(fd);
  }
  if (unlink(sleep_marker) < 0 && errno != ENOENT) {
    fprintf(stderr, "hardware-keys: action=software-sleep-wake rc=%d\n",
            -errno);
    return 0;
  }
  fprintf(stderr, "hardware-keys: action=software-sleep-wake rc=0\n");
  return 1;
}

static int read_usb_online(void) {
  const char *path = getenv("PLUMOS_PIXEL2_USB_ONLINE");
  FILE *file;
  int value;

  if (!path || !path[0]) {
    path = "/sys/class/power_supply/usb/online";
  }
  file = fopen(path, "r");
  if (!file) {
    return -1;
  }
  value = -1;
  if (fscanf(file, "%d", &value) != 1) {
    value = -1;
  }
  fclose(file);
  return value < 0 ? -1 : value != 0;
}

static int spawn_adbd_restart(void) {
  const char *control = getenv("PLUMOS_ADBD_CONTROL");
  pid_t child;

  if (!control || !control[0]) {
    control = "/usr/lib/plumos/init.d/10-adbd";
  }
  child = fork();
  if (child < 0) {
    return -errno;
  }
  if (child == 0) {
    execl(control, control, "restart", (char *)NULL);
    _exit(127);
  }
  return 0;
}

static int spawn_power_menu_overlay(void) {
  const char *root = getenv("PLUMOS_ROOT");
  char helper[512];
  pid_t child;

  if (!root || !root[0]) {
    root = "/mnt/plumos";
  }
  if (snprintf(helper, sizeof(helper), "%s/bin/plumos-power-menu-overlay",
               root) >= (int)sizeof(helper)) {
    return -ENAMETOOLONG;
  }
  child = fork();
  if (child < 0) {
    return -errno;
  }
  if (child == 0) {
    int target_fd = -1;
    pid_t owner = external_drm_owner(&target_fd);
    int owner_drm_fd = -1;
    int handed_off = 0;
    int owner_stopped = 0;
    struct drm_plane_snapshot plane_snapshots[DRM_PLANE_SNAPSHOT_LIMIT];
    size_t plane_count = 0;
    pid_t overlay;
    int status = 1 << 8;

    if (owner > 1 && target_fd >= 0) {
      owner_drm_fd = duplicate_process_fd(owner, target_fd);
      if (owner_drm_fd >= 0) {
        owner_stopped = stop_process_for_overlay(owner);
        if (owner_stopped) {
          plane_count = suspend_drm_planes(
              owner_drm_fd, plane_snapshots, DRM_PLANE_SNAPSHOT_LIMIT);
          fprintf(stderr,
                  "hardware-keys: drm-planes=suspend owner=%ld count=%zu\n",
                  (long)owner, plane_count);
        }
        if (drmDropMaster(owner_drm_fd) == 0) {
          handed_off = 1;
          fprintf(stderr,
                  "hardware-keys: drm-master=drop owner=%ld fd=%d rc=0\n",
                  (long)owner, target_fd);
        } else {
          fprintf(stderr,
                  "hardware-keys: drm-master=drop owner=%ld fd=%d rc=%d\n",
                  (long)owner, target_fd, -errno);
        }
      } else {
        fprintf(stderr,
                "hardware-keys: drm-master=duplicate owner=%ld fd=%d rc=%d\n",
                (long)owner, target_fd, owner_drm_fd);
      }
    }
    if (owner_stopped) {
      char owner_text[32];
      snprintf(owner_text, sizeof(owner_text), "%ld", (long)owner);
      (void)setenv("PLUMOS_POWER_MENU_PREPAUSED_PID", owner_text, 1);
    }
    overlay = fork();
    if (overlay == 0) {
      execl(helper, helper, "open", (char *)NULL);
      _exit(127);
    }
    if (overlay > 0) {
      while (waitpid(overlay, &status, 0) < 0 && errno == EINTR) {
      }
    }
    if (handed_off) {
      int attempt;
      int restore_result = -1;

      for (attempt = 0; attempt < 20; attempt++) {
        if (drmSetMaster(owner_drm_fd) == 0) {
          restore_result = 0;
          break;
        }
        usleep(50000);
      }
      fprintf(stderr,
              "hardware-keys: drm-master=restore owner=%ld rc=%d\n",
              (long)owner, restore_result == 0 ? 0 : -errno);
      if (restore_result == 0 && plane_count > 0) {
        int planes_ok =
            restore_drm_planes(owner_drm_fd, plane_snapshots, plane_count);
        fprintf(stderr,
                "hardware-keys: drm-planes=restore owner=%ld count=%zu rc=%d\n",
                (long)owner, plane_count, planes_ok ? 0 : -errno);
      }
    } else if (owner_drm_fd >= 0 && plane_count > 0) {
      int planes_ok =
          restore_drm_planes(owner_drm_fd, plane_snapshots, plane_count);
      fprintf(stderr,
              "hardware-keys: drm-planes=restore owner=%ld count=%zu rc=%d\n",
              (long)owner, plane_count, planes_ok ? 0 : -errno);
    }
    if (owner_drm_fd >= 0) {
      close(owner_drm_fd);
    }
    if (owner_stopped) {
      (void)kill(owner, SIGCONT);
      fprintf(stderr, "hardware-keys: display-owner=resume owner=%ld\n",
              (long)owner);
    }
    _exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
  }
  return 0;
}

static int open_power_menu(int force_overlay) {
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char lock_path[512];
  pid_t pid = frontend_pid();
  int result;

  if (!force_overlay && pid > 0 && process_owns_display(pid)) {
    fprintf(stderr,
            "hardware-keys: action=power-menu delegated=frontend pid=%ld\n",
            (long)pid);
    return 0;
  }
  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(lock_path, sizeof(lock_path), "%s/power-menu-overlay.lock",
               runtime_root) >= (int)sizeof(lock_path)) {
    return -ENAMETOOLONG;
  }
  if (access(lock_path, F_OK) == 0) {
    fprintf(stderr,
            "hardware-keys: action=power-menu skipped=already-open\n");
    return 0;
  }
  result = spawn_power_menu_overlay();
  fprintf(stderr, "hardware-keys: action=power-menu overlay=1 rc=%d\n",
          result);
  return result;
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
  struct input_source power_key = {"rk805 pwrkey", -1};
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char run_dir[512];
  char lock_path[512];
  int lock_fd;
  long long next_reopen = 0;
  long long repeat_due = 0;
  long long persist_due = 0;
  long long power_menu_debounce_due = 0;
  long long usb_power_event_guard_due = 0;
  long long adb_usb_restart_due = 0;
  int usb_online = -1;
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
  signal(SIGUSR1, request_power_menu);
  setvbuf(stderr, NULL, _IOLBF, 0);
  fprintf(stderr, "hardware-keys: start owner=plumos device=pixel2\n");
  usb_online = read_usb_online();
  (void)run_helper("plumos-volume-control", "apply");
  (void)run_helper("plumos-display-control", "apply");

  while (running) {
    struct input_source *sources[3] = {&gamepad, &volume_keys, &power_key};
    struct pollfd poll_fds[3];
    long long now = monotonic_ms();
    int index;
    int ready;

    if (now >= next_reopen) {
      reopen_source(&gamepad);
      reopen_source(&volume_keys);
      reopen_source(&power_key);
      next_reopen = now + REOPEN_INTERVAL_MS;
    }
    for (index = 0; index < 3; index++) {
      poll_fds[index].fd = sources[index]->fd;
      poll_fds[index].events = POLLIN;
      poll_fds[index].revents = 0;
    }
    ready = poll(poll_fds, 3, 100);
    now = monotonic_ms();
    if (ready < 0 && errno != EINTR) {
      break;
    }
    reap_children();
    {
      int current_usb_online = read_usb_online();
      if (current_usb_online >= 0 && usb_online >= 0 &&
          current_usb_online != usb_online) {
        usb_power_event_guard_due = now + USB_POWER_EVENT_GUARD_MS;
        adb_usb_restart_due = current_usb_online
                                  ? now + ADB_USB_RESTART_DELAY_MS
                                  : 0;
        fprintf(stderr,
                "hardware-keys: event=usb-power-transition online=%d guard_ms=%d adb_restart_ms=%d\n",
                current_usb_online, USB_POWER_EVENT_GUARD_MS,
                current_usb_online ? ADB_USB_RESTART_DELAY_MS : 0);
      }
      if (current_usb_online >= 0) {
        usb_online = current_usb_online;
      }
    }
    if (adb_usb_restart_due > 0 && now >= adb_usb_restart_due &&
        usb_online == 1) {
      int result = spawn_adbd_restart();

      fprintf(stderr,
              "hardware-keys: action=adb-usb-restart online=1 rc=%d\n",
              result);
      adb_usb_restart_due = 0;
    }
    if (power_menu_requested) {
      power_menu_requested = 0;
      (void)open_power_menu(1);
      now = monotonic_ms();
      power_menu_debounce_due = now + POWER_MENU_DEBOUNCE_MS;
    }
    for (index = 0; ready > 0 && index < 3; index++) {
      struct input_source *source = sources[index];

      if (source->fd < 0 ||
          !(poll_fds[index].revents & (POLLIN | POLLERR | POLLHUP))) {
        continue;
      }
      for (;;) {
        struct input_event event;
        ssize_t bytes = read(source->fd, &event, sizeof(event));

        if (bytes == (ssize_t)sizeof(event)) {
          if (source == &power_key && event.type == EV_KEY &&
              event.code == KEY_POWER) {
            if (event.value == 1 && now >= power_menu_debounce_due) {
              if (software_sleep_active() &&
                  now < usb_power_event_guard_due) {
                fprintf(stderr,
                        "hardware-keys: action=software-sleep-wake ignored=usb-guard\n");
              } else if (!wake_software_sleep()) {
                (void)open_power_menu(0);
              }
              now = monotonic_ms();
              power_menu_debounce_due = now + POWER_MENU_DEBOUNCE_MS;
            }
          } else if (source == &gamepad && event.type == EV_KEY &&
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
  if (power_key.fd >= 0) {
    close(power_key.fd);
  }
  reap_children();
  close(lock_fd);
  fprintf(stderr, "hardware-keys: stopped\n");
  return 0;
}
