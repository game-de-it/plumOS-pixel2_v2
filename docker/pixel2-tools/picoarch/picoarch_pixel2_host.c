#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L

#include "picoarch_pixel2_host.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "libretro-common/include/libretro.h"

struct retro_vfs_file_handle {
  FILE *file;
  char *path;
};

struct retro_vfs_dir_handle {
  DIR *dir;
  struct dirent *entry;
  char *path;
  int include_hidden;
};

static retro_time_t pixel2_time_usec(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (retro_time_t)ts.tv_sec * INT64_C(1000000) + ts.tv_nsec / 1000;
}

static retro_perf_tick_t pixel2_perf_counter(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (retro_perf_tick_t)ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;
}

static uint64_t pixel2_cpu_features(void) {
  return RETRO_SIMD_NEON | RETRO_SIMD_ASIMD;
}

static void pixel2_perf_register(struct retro_perf_counter *counter) {
  if (counter)
    counter->registered = true;
}

static void pixel2_perf_start(struct retro_perf_counter *counter) {
  if (counter)
    counter->start = pixel2_perf_counter();
}

static void pixel2_perf_stop(struct retro_perf_counter *counter) {
  retro_perf_tick_t now;

  if (!counter)
    return;
  now = pixel2_perf_counter();
  counter->total += now - counter->start;
  counter->call_cnt++;
}

static void pixel2_perf_log(void) {}

static struct retro_perf_callback perf_interface = {
    pixel2_time_usec,    pixel2_cpu_features, pixel2_perf_counter, pixel2_perf_register,
    pixel2_perf_start,   pixel2_perf_stop,    pixel2_perf_log,
};

bool pixel2_get_perf_interface(void *data) {
  struct retro_perf_callback *out = data;

  if (!out)
    return false;
  *out = perf_interface;
  return true;
}

static const char *pixel2_vfs_get_path(struct retro_vfs_file_handle *stream) {
  return stream ? stream->path : NULL;
}

static struct retro_vfs_file_handle *pixel2_vfs_open(const char *path, unsigned mode,
                                                    unsigned hints) {
  struct retro_vfs_file_handle *stream;
  const char *fmode;

  (void)hints;
  if (!path)
    return NULL;
  if ((mode & RETRO_VFS_FILE_ACCESS_READ_WRITE) == RETRO_VFS_FILE_ACCESS_READ_WRITE)
    fmode = mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING ? "r+b" : "w+b";
  else if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
    fmode = mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING ? "r+b" : "wb";
  else
    fmode = "rb";

  stream = calloc(1, sizeof(*stream));
  if (!stream)
    return NULL;
  stream->file = fopen(path, fmode);
  if (!stream->file && (mode & RETRO_VFS_FILE_ACCESS_WRITE) &&
      (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING))
    stream->file = fopen(path, "w+b");
  stream->path = strdup(path);
  if (!stream->file || !stream->path) {
    if (stream->file)
      fclose(stream->file);
    free(stream->path);
    free(stream);
    return NULL;
  }
  return stream;
}

static int pixel2_vfs_close(struct retro_vfs_file_handle *stream) {
  int rc;

  if (!stream)
    return -1;
  rc = stream->file ? fclose(stream->file) : -1;
  free(stream->path);
  free(stream);
  return rc;
}

static int64_t pixel2_vfs_tell(struct retro_vfs_file_handle *stream) {
  off_t offset;

  if (!stream || !stream->file)
    return -1;
  offset = ftello(stream->file);
  return offset < 0 ? -1 : (int64_t)offset;
}

static int64_t pixel2_vfs_seek(struct retro_vfs_file_handle *stream, int64_t offset,
                             int seek_position) {
  if (!stream || !stream->file || fseeko(stream->file, (off_t)offset, seek_position) != 0)
    return -1;
  return pixel2_vfs_tell(stream);
}

static int64_t pixel2_vfs_size(struct retro_vfs_file_handle *stream) {
  int64_t current;
  int64_t size;

  current = pixel2_vfs_tell(stream);
  if (current < 0 || pixel2_vfs_seek(stream, 0, SEEK_END) < 0)
    return -1;
  size = pixel2_vfs_tell(stream);
  if (pixel2_vfs_seek(stream, current, SEEK_SET) < 0)
    return -1;
  return size;
}

static int64_t pixel2_vfs_read(struct retro_vfs_file_handle *stream, void *buffer,
                             uint64_t length) {
  size_t count;

  if (!stream || !stream->file || !buffer)
    return -1;
  count = fread(buffer, 1, (size_t)length, stream->file);
  if (count == 0 && ferror(stream->file))
    return -1;
  return (int64_t)count;
}

static int64_t pixel2_vfs_write(struct retro_vfs_file_handle *stream, const void *buffer,
                              uint64_t length) {
  size_t count;

  if (!stream || !stream->file || !buffer)
    return -1;
  count = fwrite(buffer, 1, (size_t)length, stream->file);
  return count == 0 && ferror(stream->file) ? -1 : (int64_t)count;
}

static int pixel2_vfs_flush(struct retro_vfs_file_handle *stream) {
  return stream && stream->file ? fflush(stream->file) : -1;
}

static int pixel2_vfs_remove(const char *path) { return path ? remove(path) : -1; }

static int pixel2_vfs_rename(const char *old_path, const char *new_path) {
  return old_path && new_path ? rename(old_path, new_path) : -1;
}

static int64_t pixel2_vfs_truncate(struct retro_vfs_file_handle *stream, int64_t length) {
  if (!stream || !stream->file)
    return -1;
  return ftruncate(fileno(stream->file), (off_t)length);
}

static int pixel2_vfs_stat(const char *path, int32_t *size) {
  struct stat st;
  int flags = 0;

  if (!path || stat(path, &st) != 0)
    return 0;
  flags |= RETRO_VFS_STAT_IS_VALID;
  if (S_ISDIR(st.st_mode))
    flags |= RETRO_VFS_STAT_IS_DIRECTORY;
  if (S_ISCHR(st.st_mode))
    flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
  if (size)
    *size = st.st_size > INT32_MAX ? INT32_MAX : (int32_t)st.st_size;
  return flags;
}

static int pixel2_vfs_mkdir(const char *path) {
  if (!path)
    return -1;
  if (mkdir(path, 0755) == 0)
    return 0;
  return errno == EEXIST ? -2 : -1;
}

static struct retro_vfs_dir_handle *pixel2_vfs_opendir(const char *path,
                                                      bool include_hidden) {
  struct retro_vfs_dir_handle *stream;

  if (!path)
    return NULL;
  stream = calloc(1, sizeof(*stream));
  if (!stream)
    return NULL;
  stream->dir = opendir(path);
  stream->path = strdup(path);
  stream->include_hidden = include_hidden;
  if (!stream->dir || !stream->path) {
    if (stream->dir)
      closedir(stream->dir);
    free(stream->path);
    free(stream);
    return NULL;
  }
  return stream;
}

static bool pixel2_vfs_readdir(struct retro_vfs_dir_handle *stream) {
  if (!stream || !stream->dir)
    return false;
  do {
    stream->entry = readdir(stream->dir);
  } while (stream->entry && !stream->include_hidden && stream->entry->d_name[0] == '.');
  return stream->entry != NULL;
}

static const char *pixel2_vfs_dirent_get_name(struct retro_vfs_dir_handle *stream) {
  return stream && stream->entry ? stream->entry->d_name : NULL;
}

static bool pixel2_vfs_dirent_is_dir(struct retro_vfs_dir_handle *stream) {
  struct stat st;
  char *full_path;
  size_t length;
  bool result = false;

  if (!stream || !stream->entry)
    return false;
#ifdef DT_DIR
  if (stream->entry->d_type == DT_DIR)
    return true;
  if (stream->entry->d_type != DT_UNKNOWN)
    return false;
#endif
  length = strlen(stream->path) + strlen(stream->entry->d_name) + 2;
  full_path = malloc(length);
  if (!full_path)
    return false;
  snprintf(full_path, length, "%s/%s", stream->path, stream->entry->d_name);
  if (stat(full_path, &st) == 0)
    result = S_ISDIR(st.st_mode);
  free(full_path);
  return result;
}

static int pixel2_vfs_closedir(struct retro_vfs_dir_handle *stream) {
  int rc;

  if (!stream)
    return -1;
  rc = stream->dir ? closedir(stream->dir) : -1;
  free(stream->path);
  free(stream);
  return rc;
}

static struct retro_vfs_interface vfs_interface = {
    pixel2_vfs_get_path,       pixel2_vfs_open,          pixel2_vfs_close,
    pixel2_vfs_size,           pixel2_vfs_tell,          pixel2_vfs_seek,
    pixel2_vfs_read,           pixel2_vfs_write,         pixel2_vfs_flush,
    pixel2_vfs_remove,         pixel2_vfs_rename,        pixel2_vfs_truncate,
    pixel2_vfs_stat,           pixel2_vfs_mkdir,         pixel2_vfs_opendir,
    pixel2_vfs_readdir,        pixel2_vfs_dirent_get_name,
    pixel2_vfs_dirent_is_dir,  pixel2_vfs_closedir,
};

bool pixel2_get_vfs_interface(void *data, const char *core_path) {
  struct retro_vfs_interface_info *info = data;

  /* PCSX-ReARMed needs its bundled VFS to retain CD-ROM sector semantics. */
  if (core_path && strstr(core_path, "/pcsx_rearmed_libretro.so"))
    return false;
  if (!info || info->required_interface_version > 3)
    return false;
  info->required_interface_version = 3;
  info->iface = &vfs_interface;
  return true;
}
