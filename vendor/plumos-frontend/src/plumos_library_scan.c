#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_SYSTEMS 128
#define MAX_ALIASES 16
#define MAX_EXTENSIONS 64
#define MAX_ARTWORK_LOOKUPS 16
#define MAX_PROFILES 16
#define MAX_SCANNED_DIRS 1024

struct alias_def {
  char name[64];
  int shared;
};

struct artwork_lookup {
  char root[32];
  char path[256];
};

struct system_def {
  char id[64];
  char display_name[128];
  char short_name[64];
  char hardware[64];
  int sort_order;
  int enabled;
  int pinned;
  int scan_directories;
  struct alias_def aliases[MAX_ALIASES];
  size_t alias_count;
  char extensions[MAX_EXTENSIONS][24];
  size_t extension_count;
  struct artwork_lookup artwork[MAX_ARTWORK_LOOKUPS];
  size_t artwork_count;
  char launch_profiles[MAX_PROFILES][128];
  size_t launch_profile_count;
  char default_launch_profile[128];
};

struct rom_entry {
  size_t system_index;
  char path[PATH_MAX];
  char relative_path[PATH_MAX];
  char file_name[256];
  char title[256];
  char extension[24];
  char directory_alias[64];
  char thumbnail[PATH_MAX];
  uint64_t id_hash;
  int has_identity;
  dev_t device;
  ino_t inode;
};

struct rom_vector {
  struct rom_entry *items;
  size_t count;
  size_t cap;
};

struct scanned_dir {
  size_t system_index;
  char path[PATH_MAX];
  int has_identity;
  dev_t device;
  ino_t inode;
};

static int sync_parent_dir(const char *path) {
  char dir[PATH_MAX];
  char *slash;
  int fd;
  int rc;
  int sync_errno = 0;

  if (!path || snprintf(dir, sizeof(dir), "%s", path) >= (int)sizeof(dir)) {
    return 0;
  }
  slash = strrchr(dir, '/');
  if (!slash) {
    snprintf(dir, sizeof(dir), ".");
  } else if (slash == dir) {
    dir[1] = '\0';
  } else {
    *slash = '\0';
  }
  fd = open(dir, O_RDONLY);
  if (fd < 0) {
    return 0;
  }
  rc = fsync(fd);
  if (rc != 0) {
    sync_errno = errno;
  }
  if (close(fd) != 0) {
    return 0;
  }
  if (rc != 0 && sync_errno != EINVAL) {
    return 0;
  }
  if (rc != 0) {
    sync();
  }
  return 1;
}

struct scan_ctx {
  const char *sdcard_root;
  const char *plumos_root;
  const char *system_filter;
  int resolve_thumbnails;
  struct system_def *systems;
  size_t system_count;
  struct rom_vector roms;
  size_t alias_dirs_found;
  size_t alias_dirs_scanned;
  size_t files_seen;
  size_t files_matched;
  size_t thumbnails_found;
  long long load_ms;
  long long scan_ms;
  long long sort_ms;
  long long ready_ms;
  struct scanned_dir scanned_dirs[MAX_SCANNED_DIRS];
  size_t scanned_dir_count;
};

static int copy_string(char *out, size_t out_size, const char *in) {
  size_t len;
  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!in) {
    return 0;
  }
  len = strlen(in);
  if (len + 1 > out_size) {
    return 0;
  }
  memcpy(out, in, len + 1);
  return 1;
}

static int append_string(char *out, size_t out_size, size_t *pos, const char *in) {
  size_t len;
  if (!out || !pos || !in) {
    return 0;
  }
  len = strlen(in);
  if (*pos + len + 1 > out_size) {
    return 0;
  }
  memcpy(out + *pos, in, len);
  *pos += len;
  out[*pos] = '\0';
  return 1;
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t len_a;
  size_t len_b;
  size_t pos = 0;

  if (!out || out_size == 0 || !a || !b) {
    return 0;
  }

  out[0] = '\0';
  if (b[0] == '/') {
    return copy_string(out, out_size, b);
  }

  len_a = strlen(a);
  len_b = strlen(b);
  if (len_a + (len_a && a[len_a - 1] != '/' ? 1 : 0) + len_b + 1 > out_size) {
    return 0;
  }

  if (!append_string(out, out_size, &pos, a)) {
    return 0;
  }
  if (pos > 0 && out[pos - 1] != '/') {
    if (!append_string(out, out_size, &pos, "/")) {
      return 0;
    }
  }
  return append_string(out, out_size, &pos, b);
}

static int path_parent(char *out, size_t out_size, const char *path) {
  const char *slash;
  size_t len;

  if (!path || !path[0]) {
    return copy_string(out, out_size, ".");
  }

  slash = strrchr(path, '/');
  if (!slash) {
    return copy_string(out, out_size, ".");
  }
  if (slash == path) {
    return copy_string(out, out_size, "/");
  }

  len = (size_t)(slash - path);
  if (len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  memcpy(out, path, len);
  out[len] = '\0';
  return 1;
}

static const char *path_basename(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static int is_regular_file(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_directory(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int canonical_path(char *out, size_t out_size, const char *path) {
  char resolved[PATH_MAX];
  if (realpath(path, resolved)) {
    return copy_string(out, out_size, resolved);
  }
  return copy_string(out, out_size, path);
}

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  char *p;
  size_t len;

  if (!path || !path[0]) {
    return 0;
  }
  if (!copy_string(tmp, sizeof(tmp), path)) {
    return 0;
  }

  len = strlen(tmp);
  while (len > 1 && tmp[len - 1] == '/') {
    tmp[--len] = '\0';
  }

  for (p = tmp + 1; *p; p++) {
    if (*p != '/') {
      continue;
    }
    *p = '\0';
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
      *p = '/';
      return 0;
    }
    *p = '/';
  }

  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static char *read_file(const char *path, size_t *size_out) {
  FILE *f;
  char *buf;
  long size;

  f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  buf = (char *)calloc((size_t)size + 1, 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
    free(buf);
    fclose(f);
    return NULL;
  }
  fclose(f);
  if (size_out) {
    *size_out = (size_t)size;
  }
  return buf;
}

static const char *skip_ws_range(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int ascii_equal_ci(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static const char *file_extension(const char *name) {
  const char *dot = strrchr(name, '.');
  const char *slash = strrchr(name, '/');
  if (!dot || (slash && dot < slash) || dot[1] == '\0') {
    return "";
  }
  return dot + 1;
}

static int ignored_sidecar_name(const char *name) {
  if (!name || !name[0]) {
    return 1;
  }
  if (strncmp(name, "._", 2) == 0) {
    return 1;
  }
  if (strcmp(name, ".DS_Store") == 0 || strcmp(name, "Thumbs.db") == 0 ||
      strcmp(name, "desktop.ini") == 0 || strcmp(name, "__MACOSX") == 0) {
    return 1;
  }
  return 0;
}

static void stem_from_name(char *out, size_t out_size, const char *name) {
  const char *base = path_basename(name);
  const char *dot = strrchr(base, '.');
  size_t len = dot ? (size_t)(dot - base) : strlen(base);
  if (out_size == 0) {
    return;
  }
  if (len + 1 > out_size) {
    len = out_size - 1;
  }
  memcpy(out, base, len);
  out[len] = '\0';
}

static void stem_from_relative_path(char *out, size_t out_size, const char *relative_path) {
  const char *dot = strrchr(relative_path, '.');
  size_t len = dot ? (size_t)(dot - relative_path) : strlen(relative_path);
  if (out_size == 0) {
    return;
  }
  if (len + 1 > out_size) {
    len = out_size - 1;
  }
  memcpy(out, relative_path, len);
  out[len] = '\0';
}

static int build_stem_ext_name(char *out, size_t out_size, const char *stem, const char *ext) {
  size_t stem_len = strlen(stem);
  size_t ext_len = strlen(ext);

  if (stem_len + 1 + ext_len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  memcpy(out, stem, stem_len);
  out[stem_len] = '.';
  memcpy(out + stem_len + 1, ext, ext_len);
  out[stem_len + 1 + ext_len] = '\0';
  return 1;
}

static int build_system_cache_path(char *out, size_t out_size, const char *plumos_root,
                                   const char *system_id) {
  char dir[PATH_MAX];
  char name[128];

  if (!join_path(dir, sizeof(dir), plumos_root, "state/frontend/systems")) {
    return 0;
  }
  if (!build_stem_ext_name(name, sizeof(name), system_id, "json")) {
    return 0;
  }
  return join_path(out, out_size, dir, name);
}

static uint64_t fnv1a64_update(uint64_t hash, const char *s) {
  while (*s) {
    hash ^= (unsigned char)*s++;
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static uint64_t rom_id_hash(const char *system_id, const char *path) {
  uint64_t hash = UINT64_C(1469598103934665603);
  hash = fnv1a64_update(hash, system_id);
  hash ^= 0xff;
  hash *= UINT64_C(1099511628211);
  hash = fnv1a64_update(hash, path);
  return hash;
}

static int json_decode_string(const char **cursor, const char *end, char *out, size_t out_size) {
  const char *p = *cursor;
  size_t n = 0;

  if (p >= end || *p != '"' || out_size == 0) {
    return 0;
  }
  p++;

  while (p < end && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < end) {
      c = *p++;
      switch (c) {
      case '"':
      case '\\':
      case '/':
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      default:
        break;
      }
    }
    if (n + 1 < out_size) {
      out[n++] = c;
    }
  }
  if (p >= end || *p != '"') {
    out[0] = '\0';
    return 0;
  }
  out[n] = '\0';
  *cursor = p + 1;
  return 1;
}

static int json_find_key_value(const char *json, const char *end, const char *key,
                               const char **value_out) {
  char pattern[128];
  const char *p = json;

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  while (p < end && (p = strstr(p, pattern)) != NULL && p < end) {
    const char *q = p + strlen(pattern);
    q = skip_ws_range(q, end);
    if (q < end && *q == ':') {
      q++;
      *value_out = skip_ws_range(q, end);
      return 1;
    }
    p++;
  }
  return 0;
}

static int json_match_container(const char *start, const char *end, char open_ch,
                                char close_ch, const char **body_start,
                                const char **body_end, const char **after_out) {
  const char *p = start;
  int depth = 0;
  int in_string = 0;
  int escape = 0;

  if (p >= end || *p != open_ch) {
    return 0;
  }

  while (p < end) {
    if (escape) {
      escape = 0;
      p++;
      continue;
    }
    if (in_string && *p == '\\') {
      escape = 1;
      p++;
      continue;
    }
    if (*p == '"') {
      in_string = !in_string;
      p++;
      continue;
    }
    if (!in_string && *p == open_ch) {
      if (depth == 0 && body_start) {
        *body_start = p + 1;
      }
      depth++;
    } else if (!in_string && *p == close_ch) {
      depth--;
      if (depth == 0) {
        if (body_end) {
          *body_end = p;
        }
        if (after_out) {
          *after_out = p + 1;
        }
        return 1;
      }
    }
    p++;
  }
  return 0;
}

static int json_find_array(const char *json, const char *end, const char *key,
                           const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '[', ']', body_start, body_end, NULL);
}

static int json_find_object(const char *json, const char *end, const char *key,
                            const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '{', '}', body_start, body_end, NULL);
}

static int json_next_object(const char **cursor, const char *end,
                            const char **object_start, const char **object_end) {
  const char *p = *cursor;
  const char *body_start;
  const char *body_end;
  const char *after;

  while (p < end && *p != '{') {
    p++;
  }
  if (p >= end) {
    return 0;
  }
  if (!json_match_container(p, end, '{', '}', &body_start, &body_end, &after)) {
    return 0;
  }
  (void)body_start;
  *object_start = p;
  *object_end = after;
  *cursor = after;
  return 1;
}

static char *range_dup(const char *start, const char *end) {
  size_t len = (size_t)(end - start);
  char *s = (char *)calloc(len + 1, 1);
  if (!s) {
    return NULL;
  }
  memcpy(s, start, len);
  s[len] = '\0';
  return s;
}

static int json_get_string(const char *json, const char *end, const char *key,
                           char *out, size_t out_size) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  return json_decode_string(&value, end, out, out_size);
}

static int json_get_int(const char *json, const char *end, const char *key, int default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  return atoi(value);
}

static int json_get_bool(const char *json, const char *end, const char *key, int default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  if ((size_t)(end - value) >= 4 && strncmp(value, "true", 4) == 0) {
    return 1;
  }
  if ((size_t)(end - value) >= 5 && strncmp(value, "false", 5) == 0) {
    return 0;
  }
  return default_value;
}

static size_t parse_string_array(const char *json, const char *end, const char *key,
                                 char values[][128], size_t max_values) {
  const char *start;
  const char *stop;
  const char *p;
  size_t count = 0;

  if (!json_find_array(json, end, key, &start, &stop)) {
    return 0;
  }

  p = start;
  while (p < stop && count < max_values) {
    p = skip_ws_range(p, stop);
    if (p >= stop) {
      break;
    }
    if (*p == '"') {
      if (json_decode_string(&p, stop, values[count], 128)) {
        count++;
      } else {
        break;
      }
    } else {
      p++;
    }
  }
  return count;
}

static void lower_string(char *s) {
  while (*s) {
    *s = (char)tolower((unsigned char)*s);
    s++;
  }
}

static size_t parse_extensions(const char *json, const char *end, struct system_def *system) {
  char values[MAX_EXTENSIONS][128];
  size_t count;
  size_t i;

  count = parse_string_array(json, end, "extensions", values, MAX_EXTENSIONS);
  for (i = 0; i < count; i++) {
    char *ext = values[i];
    if (ext[0] == '.') {
      ext++;
    }
    copy_string(system->extensions[system->extension_count],
                sizeof(system->extensions[system->extension_count]), ext);
    lower_string(system->extensions[system->extension_count]);
    system->extension_count++;
  }
  return system->extension_count;
}

static size_t parse_launch_profiles(const char *json, const char *end, struct system_def *system) {
  char values[MAX_PROFILES][128];
  size_t count;
  size_t i;

  count = parse_string_array(json, end, "launch_profiles", values, MAX_PROFILES);
  for (i = 0; i < count; i++) {
    copy_string(system->launch_profiles[system->launch_profile_count],
                sizeof(system->launch_profiles[system->launch_profile_count]), values[i]);
    system->launch_profile_count++;
  }
  return system->launch_profile_count;
}

static void parse_aliases(const char *json, const char *end, struct system_def *system) {
  const char *start;
  const char *stop;
  const char *cursor;

  if (!json_find_array(json, end, "directory_aliases", &start, &stop)) {
    return;
  }

  cursor = start;
  while (system->alias_count < MAX_ALIASES) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    char name[64];

    if (!json_next_object(&cursor, stop, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      break;
    }
    if (json_get_string(obj, obj + strlen(obj), "name", name, sizeof(name))) {
      copy_string(system->aliases[system->alias_count].name,
                  sizeof(system->aliases[system->alias_count].name), name);
      system->aliases[system->alias_count].shared =
          json_get_bool(obj, obj + strlen(obj), "shared", 0);
      system->alias_count++;
    }
    free(obj);
  }
}

static void parse_artwork(const char *json, const char *end, struct system_def *system) {
  const char *art_start;
  const char *art_end;
  const char *lookup_start;
  const char *lookup_end;
  const char *cursor;
  char *art_obj;

  if (!json_find_object(json, end, "artwork", &art_start, &art_end)) {
    return;
  }
  art_obj = range_dup(art_start, art_end);
  if (!art_obj) {
    return;
  }
  if (!json_find_array(art_obj, art_obj + strlen(art_obj), "lookup", &lookup_start, &lookup_end)) {
    free(art_obj);
    return;
  }

  cursor = lookup_start;
  while (system->artwork_count < MAX_ARTWORK_LOOKUPS) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    char root[32];
    char path[256];

    if (!json_next_object(&cursor, lookup_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      break;
    }
    if (json_get_string(obj, obj + strlen(obj), "root", root, sizeof(root)) &&
        json_get_string(obj, obj + strlen(obj), "path", path, sizeof(path))) {
      copy_string(system->artwork[system->artwork_count].root,
                  sizeof(system->artwork[system->artwork_count].root), root);
      copy_string(system->artwork[system->artwork_count].path,
                  sizeof(system->artwork[system->artwork_count].path), path);
      system->artwork_count++;
    }
    free(obj);
  }

  free(art_obj);
}

static int cmp_systems(const void *a, const void *b) {
  const struct system_def *aa = (const struct system_def *)a;
  const struct system_def *bb = (const struct system_def *)b;
  if (aa->sort_order != bb->sort_order) {
    return aa->sort_order - bb->sort_order;
  }
  return strcmp(aa->id, bb->id);
}

static int load_systems(const char *path, struct system_def *systems, size_t *count_out) {
  char *json;
  const char *start;
  const char *end;
  const char *cursor;
  size_t json_size;
  size_t count = 0;

  *count_out = 0;
  json = read_file(path, &json_size);
  if (!json) {
    fprintf(stderr, "error: cannot read systems file: %s\n", path);
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &start, &end)) {
    fprintf(stderr, "error: systems array not found: %s\n", path);
    free(json);
    return 0;
  }

  cursor = start;
  while (count < MAX_SYSTEMS) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct system_def system;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }

    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&system, 0, sizeof(system));
    system.enabled = 1;
    system.sort_order = 10000;
    json_get_string(obj, obj + strlen(obj), "id", system.id, sizeof(system.id));
    json_get_string(obj, obj + strlen(obj), "display_name", system.display_name,
                    sizeof(system.display_name));
    json_get_string(obj, obj + strlen(obj), "short_name", system.short_name,
                    sizeof(system.short_name));
    json_get_string(obj, obj + strlen(obj), "hardware", system.hardware, sizeof(system.hardware));
    json_get_string(obj, obj + strlen(obj), "default_launch_profile",
                    system.default_launch_profile, sizeof(system.default_launch_profile));
    system.sort_order = json_get_int(obj, obj + strlen(obj), "sort_order", system.sort_order);
    system.enabled = json_get_bool(obj, obj + strlen(obj), "enabled", 1);
    system.pinned = json_get_bool(obj, obj + strlen(obj), "pinned", 0);
    system.scan_directories = json_get_bool(obj, obj + strlen(obj), "scan_directories", 0);
    parse_aliases(obj, obj + strlen(obj), &system);
    parse_extensions(obj, obj + strlen(obj), &system);
    parse_artwork(obj, obj + strlen(obj), &system);
    parse_launch_profiles(obj, obj + strlen(obj), &system);

    if (system.id[0] &&
        ((system.alias_count > 0 && system.extension_count > 0) ||
         (system.launch_profile_count > 0 && system.default_launch_profile[0] &&
          system.extension_count == 0))) {
      if (!system.display_name[0]) {
        copy_string(system.display_name, sizeof(system.display_name), system.id);
      }
      if (!system.short_name[0]) {
        copy_string(system.short_name, sizeof(system.short_name), system.display_name);
      }
      systems[count++] = system;
    }
    free(obj);
  }

  qsort(systems, count, sizeof(systems[0]), cmp_systems);
  *count_out = count;
  free(json);
  return 1;
}

static int system_ext_matches(const struct system_def *system, const char *ext) {
  char lowered[24];
  size_t i;
  if (!ext || !ext[0]) {
    return 0;
  }
  copy_string(lowered, sizeof(lowered), ext);
  lower_string(lowered);
  for (i = 0; i < system->extension_count; i++) {
    if (strcmp(system->extensions[i], lowered) == 0) {
      return 1;
    }
  }
  return 0;
}

static int archive_extension_is_ambiguous(const char *ext) {
  return ext && (strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "7z") == 0);
}

static int scan_ext_matches(const struct system_def *system, const char *ext,
                            int filter_shared_archives) {
  if (filter_shared_archives && archive_extension_is_ambiguous(ext)) {
    return 0;
  }
  return system_ext_matches(system, ext);
}

static int append_rom(struct rom_vector *roms, const struct rom_entry *entry) {
  struct rom_entry *next;

  if (roms->count == roms->cap) {
    size_t next_cap = roms->cap ? roms->cap * 2 : 128;
    next = (struct rom_entry *)realloc(roms->items, next_cap * sizeof(roms->items[0]));
    if (!next) {
      return 0;
    }
    roms->items = next;
    roms->cap = next_cap;
  }
  roms->items[roms->count++] = *entry;
  return 1;
}

static int rom_already_seen(const struct rom_vector *roms, size_t system_index, const char *path,
                            int has_identity, dev_t device, ino_t inode) {
  size_t i;
  for (i = 0; i < roms->count; i++) {
    if (roms->items[i].system_index != system_index) {
      continue;
    }
    if (has_identity && roms->items[i].has_identity && roms->items[i].device == device &&
        roms->items[i].inode == inode) {
      return 1;
    }
    if (roms->items[i].system_index == system_index && strcmp(roms->items[i].path, path) == 0) {
      return 1;
    }
  }
  return 0;
}

static int scanned_dir_already_seen(const struct scan_ctx *ctx, size_t system_index,
                                    const char *path, int has_identity, dev_t device,
                                    ino_t inode) {
  size_t i;
  for (i = 0; i < ctx->scanned_dir_count; i++) {
    if (ctx->scanned_dirs[i].system_index != system_index) {
      continue;
    }
    if (has_identity && ctx->scanned_dirs[i].has_identity &&
        ctx->scanned_dirs[i].device == device && ctx->scanned_dirs[i].inode == inode) {
      return 1;
    }
    if (strcmp(ctx->scanned_dirs[i].path, path) == 0) {
      return 1;
    }
  }
  return 0;
}

static void remember_scanned_dir(struct scan_ctx *ctx, size_t system_index, const char *path,
                                 int has_identity, dev_t device, ino_t inode) {
  if (ctx->scanned_dir_count >= MAX_SCANNED_DIRS) {
    return;
  }
  ctx->scanned_dirs[ctx->scanned_dir_count].system_index = system_index;
  copy_string(ctx->scanned_dirs[ctx->scanned_dir_count].path,
              sizeof(ctx->scanned_dirs[ctx->scanned_dir_count].path), path);
  ctx->scanned_dirs[ctx->scanned_dir_count].has_identity = has_identity;
  ctx->scanned_dirs[ctx->scanned_dir_count].device = device;
  ctx->scanned_dirs[ctx->scanned_dir_count].inode = inode;
  ctx->scanned_dir_count++;
}

static int find_case_insensitive_file(const char *candidate, char *out, size_t out_size) {
  char dir_path[PATH_MAX];
  const char *base_name;
  DIR *dir;
  struct dirent *ent;

  if (is_regular_file(candidate)) {
    return copy_string(out, out_size, candidate);
  }

  if (!path_parent(dir_path, sizeof(dir_path), candidate)) {
    return 0;
  }
  base_name = path_basename(candidate);
  dir = opendir(dir_path);
  if (!dir) {
    return 0;
  }
  while ((ent = readdir(dir)) != NULL) {
    char found[PATH_MAX];
    if (ignored_sidecar_name(ent->d_name)) {
      continue;
    }
    if (!ascii_equal_ci(ent->d_name, base_name)) {
      continue;
    }
    if (!join_path(found, sizeof(found), dir_path, ent->d_name)) {
      continue;
    }
    if (is_regular_file(found)) {
      closedir(dir);
      return copy_string(out, out_size, found);
    }
  }
  closedir(dir);
  return 0;
}

static int resolve_artwork_dir(const struct scan_ctx *ctx, const struct artwork_lookup *lookup,
                               char *out, size_t out_size) {
  if (lookup->path[0] == '/') {
    return copy_string(out, out_size, lookup->path);
  }
  if (strcmp(lookup->root, "plumos") == 0) {
    return join_path(out, out_size, ctx->plumos_root, lookup->path);
  }
  if (strcmp(lookup->root, "sdcard") == 0) {
    return join_path(out, out_size, ctx->sdcard_root, lookup->path);
  }
  return 0;
}

static int find_thumbnail_in_dir(const char *art_dir, const char *relative_stem,
                                 const char *flat_stem, char *out, size_t out_size) {
  static const char *image_exts[] = {"png", "jpg", "jpeg", "webp"};
  size_t e;

  for (e = 0; e < sizeof(image_exts) / sizeof(image_exts[0]); e++) {
    char stem_path[PATH_MAX];
    char candidate[PATH_MAX];
    char name[PATH_MAX];
    if (build_stem_ext_name(name, sizeof(name), relative_stem, image_exts[e]) &&
        join_path(stem_path, sizeof(stem_path), art_dir, name) &&
        find_case_insensitive_file(stem_path, candidate, sizeof(candidate))) {
      return copy_string(out, out_size, candidate);
    }
  }

  for (e = 0; e < sizeof(image_exts) / sizeof(image_exts[0]); e++) {
    char stem_path[PATH_MAX];
    char candidate[PATH_MAX];
    char name[PATH_MAX];
    if (build_stem_ext_name(name, sizeof(name), flat_stem, image_exts[e]) &&
        join_path(stem_path, sizeof(stem_path), art_dir, name) &&
        find_case_insensitive_file(stem_path, candidate, sizeof(candidate))) {
      return copy_string(out, out_size, candidate);
    }
  }

  return 0;
}

static int find_thumbnail(const struct scan_ctx *ctx, const struct system_def *system,
                          const char *directory_alias, const char *relative_stem,
                          const char *flat_stem, char *out, size_t out_size) {
  size_t i;

  if (out_size > 0) {
    out[0] = '\0';
  }

  if (directory_alias && directory_alias[0]) {
    char images_dir[PATH_MAX];
    char alias_art_dir[PATH_MAX];
    if (join_path(images_dir, sizeof(images_dir), ctx->sdcard_root, "Images") &&
        join_path(alias_art_dir, sizeof(alias_art_dir), images_dir, directory_alias) &&
        find_thumbnail_in_dir(alias_art_dir, relative_stem, flat_stem, out, out_size)) {
      return 1;
    }
  }

  for (i = 0; i < system->artwork_count; i++) {
    char art_dir[PATH_MAX];
    if (!resolve_artwork_dir(ctx, &system->artwork[i], art_dir, sizeof(art_dir))) {
      continue;
    }
    if (find_thumbnail_in_dir(art_dir, relative_stem, flat_stem, out, out_size)) {
      return 1;
    }
  }

  return 0;
}

static void add_rom_entry(struct scan_ctx *ctx, size_t system_index, const char *alias_name,
                          const char *abs_path, const char *rel_path) {
  const struct system_def *system = &ctx->systems[system_index];
  struct rom_entry entry;
  char canonical_abs[PATH_MAX];
  char rel_stem[PATH_MAX];
  char flat_stem[256];
  char relative_path[PATH_MAX];
  struct stat st;
  int has_identity;
  int is_dir_entry;
  size_t pos = 0;

  memset(&st, 0, sizeof(st));
  canonical_path(canonical_abs, sizeof(canonical_abs), abs_path);
  has_identity = stat(abs_path, &st) == 0;
  is_dir_entry = has_identity && S_ISDIR(st.st_mode);
  if (rom_already_seen(&ctx->roms, system_index, canonical_abs, has_identity, st.st_dev,
                       st.st_ino)) {
    return;
  }

  memset(&entry, 0, sizeof(entry));
  entry.system_index = system_index;
  entry.has_identity = has_identity;
  if (has_identity) {
    entry.device = st.st_dev;
    entry.inode = st.st_ino;
  }
  copy_string(entry.path, sizeof(entry.path), canonical_abs);
  copy_string(entry.file_name, sizeof(entry.file_name), path_basename(abs_path));
  if (is_dir_entry) {
    copy_string(entry.extension, sizeof(entry.extension), "dir");
    copy_string(entry.title, sizeof(entry.title), entry.file_name);
  } else {
    copy_string(entry.extension, sizeof(entry.extension), file_extension(entry.file_name));
    lower_string(entry.extension);
    stem_from_name(entry.title, sizeof(entry.title), entry.file_name);
  }
  copy_string(entry.directory_alias, sizeof(entry.directory_alias), alias_name);

  relative_path[0] = '\0';
  append_string(relative_path, sizeof(relative_path), &pos, alias_name);
  append_string(relative_path, sizeof(relative_path), &pos, "/");
  append_string(relative_path, sizeof(relative_path), &pos, rel_path);
  copy_string(entry.relative_path, sizeof(entry.relative_path), relative_path);

  if (is_dir_entry) {
    copy_string(rel_stem, sizeof(rel_stem), rel_path);
    copy_string(flat_stem, sizeof(flat_stem), entry.file_name);
  } else {
    stem_from_relative_path(rel_stem, sizeof(rel_stem), rel_path);
    stem_from_name(flat_stem, sizeof(flat_stem), entry.file_name);
  }
  if (ctx->resolve_thumbnails &&
      find_thumbnail(ctx, system, alias_name, rel_stem, flat_stem, entry.thumbnail,
                     sizeof(entry.thumbnail))) {
    ctx->thumbnails_found++;
  }

  entry.id_hash = rom_id_hash(system->id, canonical_abs);
  append_rom(&ctx->roms, &entry);
}

static void scan_dir_recursive(struct scan_ctx *ctx, size_t system_index, const char *alias_name,
                               int filter_shared_archives, const char *dir_path,
                               const char *rel_dir) {
  const struct system_def *system = &ctx->systems[system_index];
  DIR *dir = opendir(dir_path);
  struct dirent *ent;

  if (!dir) {
    return;
  }

  while ((ent = readdir(dir)) != NULL) {
    char child_path[PATH_MAX];
    char child_rel[PATH_MAX];
    const char *name = ent->d_name;

    if (name[0] == '.' || ignored_sidecar_name(name)) {
      continue;
    }
    if (!join_path(child_path, sizeof(child_path), dir_path, name)) {
      continue;
    }
    if (rel_dir[0]) {
      char tmp[PATH_MAX];
      if (!join_path(tmp, sizeof(tmp), rel_dir, name)) {
        continue;
      }
      copy_string(child_rel, sizeof(child_rel), tmp);
    } else {
      copy_string(child_rel, sizeof(child_rel), name);
    }

    if (is_directory(child_path)) {
      if (system->scan_directories && !rel_dir[0]) {
        add_rom_entry(ctx, system_index, alias_name, child_path, child_rel);
        continue;
      }
      scan_dir_recursive(ctx, system_index, alias_name, filter_shared_archives, child_path,
                         child_rel);
      continue;
    }
    if (!is_regular_file(child_path)) {
      continue;
    }

    ctx->files_seen++;
    if (!scan_ext_matches(system, file_extension(name), filter_shared_archives)) {
      continue;
    }
    ctx->files_matched++;
    add_rom_entry(ctx, system_index, alias_name, child_path, child_rel);
  }

  closedir(dir);
}

static void scan_alias_dir(struct scan_ctx *ctx, size_t system_index, const char *rom_root,
                           const struct alias_def *alias, int filter_shared_archives) {
  char alias_path[PATH_MAX];
  char canonical_alias_path[PATH_MAX];
  struct stat st;
  int has_identity;

  if (!alias || !alias->name[0]) {
    return;
  }
  memset(&st, 0, sizeof(st));
  if (!join_path(alias_path, sizeof(alias_path), rom_root, alias->name)) {
    return;
  }
  if (!is_directory(alias_path)) {
    return;
  }
  has_identity = stat(alias_path, &st) == 0;
  canonical_path(canonical_alias_path, sizeof(canonical_alias_path), alias_path);
  if (scanned_dir_already_seen(ctx, system_index, canonical_alias_path, has_identity, st.st_dev,
                               st.st_ino)) {
    return;
  }
  remember_scanned_dir(ctx, system_index, canonical_alias_path, has_identity, st.st_dev, st.st_ino);
  ctx->alias_dirs_found++;
  ctx->alias_dirs_scanned++;
  scan_dir_recursive(ctx, system_index, alias->name, filter_shared_archives,
                     canonical_alias_path, "");
}

static void scan_systems(struct scan_ctx *ctx) {
  static const char *rom_root_names[] = {"Roms", "roms"};
  char rom_root[PATH_MAX];
  size_t s;
  size_t r;

  for (s = 0; s < ctx->system_count; s++) {
    const struct system_def *system = &ctx->systems[s];
    size_t a;

    if (!system->enabled) {
      continue;
    }
    if (ctx->system_filter && strcmp(ctx->system_filter, system->id) != 0) {
      continue;
    }

    for (r = 0; r < sizeof(rom_root_names) / sizeof(rom_root_names[0]); r++) {
      if (!join_path(rom_root, sizeof(rom_root), ctx->sdcard_root, rom_root_names[r])) {
        continue;
      }
      for (a = 0; a < system->alias_count; a++) {
        scan_alias_dir(ctx, s, rom_root, &system->aliases[a],
                       a > 0 && system->aliases[a].shared);
      }
    }
  }
}

static int cmp_rom_entries(const void *a, const void *b) {
  const struct rom_entry *aa = (const struct rom_entry *)a;
  const struct rom_entry *bb = (const struct rom_entry *)b;
  int c;

  if (aa->system_index != bb->system_index) {
    return aa->system_index < bb->system_index ? -1 : 1;
  }
  c = strcasecmp(aa->title, bb->title);
  if (c != 0) {
    return c;
  }
  return strcasecmp(aa->relative_path, bb->relative_path);
}

static void json_write_escaped(FILE *f, const char *s) {
  fputc('"', f);
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    switch (c) {
    case '"':
      fputs("\\\"", f);
      break;
    case '\\':
      fputs("\\\\", f);
      break;
    case '\b':
      fputs("\\b", f);
      break;
    case '\f':
      fputs("\\f", f);
      break;
    case '\n':
      fputs("\\n", f);
      break;
    case '\r':
      fputs("\\r", f);
      break;
    case '\t':
      fputs("\\t", f);
      break;
    default:
      if (c < 0x20) {
        fprintf(f, "\\u%04x", c);
      } else {
        fputc(c, f);
      }
      break;
    }
  }
  fputc('"', f);
}

static size_t count_roms_for_system(const struct scan_ctx *ctx, size_t system_index) {
  size_t i;
  size_t count = 0;
  for (i = 0; i < ctx->roms.count; i++) {
    if (ctx->roms.items[i].system_index == system_index) {
      count++;
    }
  }
  return count;
}

static size_t count_thumbnails_for_system(const struct scan_ctx *ctx, size_t system_index) {
  size_t i;
  size_t count = 0;
  for (i = 0; i < ctx->roms.count; i++) {
    if (ctx->roms.items[i].system_index == system_index && ctx->roms.items[i].thumbnail[0]) {
      count++;
    }
  }
  return count;
}

static int write_library_index(const struct scan_ctx *ctx, const char *output_path) {
  char parent[PATH_MAX];
  char tmp_path[PATH_MAX];
  FILE *f;
  int fd;
  int write_ok = 1;
  size_t s;
  int first_system = 1;

  if (!path_parent(parent, sizeof(parent), output_path) || !mkdir_p(parent)) {
    fprintf(stderr, "error: cannot create output directory for %s\n", output_path);
    return 0;
  }
  if (!copy_string(tmp_path, sizeof(tmp_path), output_path)) {
    fprintf(stderr, "error: output path is too long: %s\n", output_path);
    return 0;
  }
  {
    size_t pos = strlen(tmp_path);
    if (!append_string(tmp_path, sizeof(tmp_path), &pos, ".tmp")) {
      fprintf(stderr, "error: output path is too long: %s.tmp\n", output_path);
      return 0;
    }
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    fprintf(stderr, "error: cannot write %s: %s\n", tmp_path, strerror(errno));
    return 0;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"generated_by\": \"plumos-library-scan\",\n");
  fprintf(f, "  \"generated_at_unix\": %ld,\n", (long)time(NULL));
  fprintf(f, "  \"sdcard_root\": ");
  json_write_escaped(f, ctx->sdcard_root);
  fprintf(f, ",\n");
  fprintf(f, "  \"plumos_root\": ");
  json_write_escaped(f, ctx->plumos_root);
  fprintf(f, ",\n");
  fprintf(f, "  \"systems\": [\n");

  for (s = 0; s < ctx->system_count; s++) {
    const struct system_def *system = &ctx->systems[s];
    size_t i;
    size_t rom_count;
    size_t thumb_count;
    int first_rom = 1;

    if (!system->enabled) {
      continue;
    }
    if (ctx->system_filter && strcmp(ctx->system_filter, system->id) != 0) {
      continue;
    }

    rom_count = count_roms_for_system(ctx, s);
    thumb_count = count_thumbnails_for_system(ctx, s);
    if (!first_system) {
      fprintf(f, ",\n");
    }
    first_system = 0;

    fprintf(f, "    {\n");
    fprintf(f, "      \"id\": ");
    json_write_escaped(f, system->id);
    fprintf(f, ",\n");
    fprintf(f, "      \"display_name\": ");
    json_write_escaped(f, system->display_name);
    fprintf(f, ",\n");
    fprintf(f, "      \"short_name\": ");
    json_write_escaped(f, system->short_name);
    fprintf(f, ",\n");
    fprintf(f, "      \"sort_order\": %d,\n", system->sort_order);
    fprintf(f, "      \"rom_count\": %zu,\n", rom_count);
    fprintf(f, "      \"thumbnail_count\": %zu,\n", thumb_count);
    fprintf(f, "      \"pinned\": %s,\n", system->pinned ? "true" : "false");
    fprintf(f, "      \"default_launch_profile\": ");
    json_write_escaped(f, system->default_launch_profile);
    fprintf(f, ",\n");
    fprintf(f, "      \"roms\": [\n");

    for (i = 0; i < ctx->roms.count; i++) {
      const struct rom_entry *rom = &ctx->roms.items[i];
      if (rom->system_index != s) {
        continue;
      }
      if (!first_rom) {
        fprintf(f, ",\n");
      }
      first_rom = 0;
      fprintf(f, "        {\n");
      fprintf(f, "          \"id\": \"path64:%016llx\",\n", (unsigned long long)rom->id_hash);
      fprintf(f, "          \"system_id\": ");
      json_write_escaped(f, system->id);
      fprintf(f, ",\n");
      fprintf(f, "          \"path\": ");
      json_write_escaped(f, rom->path);
      fprintf(f, ",\n");
      fprintf(f, "          \"relative_path\": ");
      json_write_escaped(f, rom->relative_path);
      fprintf(f, ",\n");
      fprintf(f, "          \"file_name\": ");
      json_write_escaped(f, rom->file_name);
      fprintf(f, ",\n");
      fprintf(f, "          \"title\": ");
      json_write_escaped(f, rom->title);
      fprintf(f, ",\n");
      fprintf(f, "          \"extension\": ");
      json_write_escaped(f, rom->extension);
      fprintf(f, ",\n");
      fprintf(f, "          \"directory_alias\": ");
      json_write_escaped(f, rom->directory_alias);
      fprintf(f, ",\n");
      fprintf(f, "          \"media\": { \"thumbnail\": ");
      if (rom->thumbnail[0]) {
        json_write_escaped(f, rom->thumbnail);
      } else {
        fputs("null", f);
      }
      fprintf(f, " },\n");
      fprintf(f, "          \"metadata\": { \"source\": \"scan\" }\n");
      fprintf(f, "        }");
    }

    fprintf(f, "\n      ]\n");
    fprintf(f, "    }");
  }

  fprintf(f, "\n  ],\n");
  fprintf(f, "  \"summary\": {\n");
  fprintf(f, "    \"system_count\": %zu,\n", ctx->system_count);
  fprintf(f, "    \"alias_dirs_found\": %zu,\n", ctx->alias_dirs_found);
  fprintf(f, "    \"files_seen\": %zu,\n", ctx->files_seen);
  fprintf(f, "    \"files_matched\": %zu,\n", ctx->files_matched);
  fprintf(f, "    \"rom_count\": %zu,\n", ctx->roms.count);
  fprintf(f, "    \"thumbnail_count\": %zu,\n", ctx->thumbnails_found);
  fprintf(f, "    \"thumbnail_lookup_deferred\": %s,\n",
          ctx->resolve_thumbnails ? "false" : "true");
  fprintf(f, "    \"load_ms\": %lld,\n", ctx->load_ms);
  fprintf(f, "    \"scan_ms\": %lld,\n", ctx->scan_ms);
  fprintf(f, "    \"sort_ms\": %lld,\n", ctx->sort_ms);
  fprintf(f, "    \"ready_ms\": %lld\n", ctx->ready_ms);
  fprintf(f, "  }\n");
  fprintf(f, "}\n");

  fd = fileno(f);
  if (fflush(f) != 0 || fd < 0 || fsync(fd) != 0) {
    write_ok = 0;
  }
  if (fclose(f) != 0) {
    write_ok = 0;
  }
  if (!write_ok) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, output_path) != 0) {
    fprintf(stderr, "error: cannot rename %s to %s: %s\n", tmp_path, output_path, strerror(errno));
    unlink(tmp_path);
    return 0;
  }
  return sync_parent_dir(output_path);
}

static long long now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

static void redirect_child_stdio_to_devnull(void) {
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    return;
  }
  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  if (fd > STDERR_FILENO) {
    close(fd);
  }
}

static void start_sdcard_cleanup_process(const char *plumos_root, const char *sdcard_root) {
  char cleanup[PATH_MAX];
  pid_t pid;

  if (!join_path(cleanup, sizeof(cleanup), plumos_root, "bin/plumos-sdcard-cleanup")) {
    return;
  }
  if (access(cleanup, X_OK) != 0) {
    return;
  }

  pid = fork();
  if (pid != 0) {
    return;
  }

  setenv("PLUMOS_ROOT", plumos_root, 1);
  setenv("PLUMOS_SDCARD_ROOT", sdcard_root, 1);
  redirect_child_stdio_to_devnull();
  execl(cleanup, cleanup, "--no-cache-invalidate", (char *)NULL);
  _exit(127);
}

static void print_system_summary(const struct scan_ctx *ctx) {
  size_t s;
  for (s = 0; s < ctx->system_count; s++) {
    const struct system_def *system = &ctx->systems[s];
    size_t roms;
    size_t thumbnails;

    if (!system->enabled) {
      continue;
    }
    if (ctx->system_filter && strcmp(ctx->system_filter, system->id) != 0) {
      continue;
    }

    roms = count_roms_for_system(ctx, s);
    thumbnails = count_thumbnails_for_system(ctx, s);
    if (roms > 0 || system->pinned || ctx->system_filter) {
      printf("system %-18s roms=%zu thumbnails=%zu\n", system->id, roms, thumbnails);
    }
  }
}

static void usage(const char *argv0) {
  printf("Usage: %s [--systems PATH] [--output PATH] [--system ID|--on-enter ID]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  --system ID    Scan one system. Default output is state/frontend/systems/ID.json.\n");
  printf("  --on-enter ID  Alias for --system ID, intended for ROM-list entry refresh.\n");
  printf("  --defer-thumbnails  Skip thumbnail lookup for fast first text display.\n");
  printf("  --with-thumbnails   Resolve thumbnails during the scan.\n");
  printf("\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT   Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT          Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_SYSTEMS_JSON  Default: $PLUMOS_ROOT/config/frontend/systems.json\n");
  printf("  PLUMOS_LIBRARY_INDEX Default: full scan uses state/frontend/library-index.json;\n");
  printf("                       system scan uses state/frontend/systems/ID.json\n");
}

int main(int argc, char **argv) {
  struct system_def systems[MAX_SYSTEMS];
  struct scan_ctx ctx;
  const char *sdcard_root;
  const char *plumos_root_env;
  const char *systems_path;
  const char *output_path;
  const char *output_env;
  const char *system_filter = NULL;
  char default_plumos_root[PATH_MAX];
  char default_systems_path[PATH_MAX];
  char default_output_path[PATH_MAX];
  char selected_output_path[PATH_MAX];
  long long started_ms;
  long long loaded_ms;
  long long scanned_ms;
  long long sorted_ms;
  long long elapsed_ms;
  int output_explicit = 0;
  int on_enter_mode = 0;
  int resolve_thumbnails = 1;
  int thumbnail_policy_explicit = 0;
  int i;

  sdcard_root = getenv("PLUMOS_SDCARD_ROOT");
  if (!sdcard_root || !sdcard_root[0]) {
    sdcard_root = "/mnt/SDCARD";
  }
  plumos_root_env = getenv("PLUMOS_ROOT");
  if (plumos_root_env && plumos_root_env[0]) {
    copy_string(default_plumos_root, sizeof(default_plumos_root), plumos_root_env);
  } else {
    join_path(default_plumos_root, sizeof(default_plumos_root), sdcard_root, "plumos");
  }

  systems_path = getenv("PLUMOS_SYSTEMS_JSON");
  if (!systems_path || !systems_path[0]) {
    join_path(default_systems_path, sizeof(default_systems_path), default_plumos_root,
              "config/frontend/systems.json");
    systems_path = default_systems_path;
  }

  output_env = getenv("PLUMOS_LIBRARY_INDEX");
  if (output_env && output_env[0]) {
    output_path = output_env;
    output_explicit = 1;
  } else {
    output_path = NULL;
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    }
    if (strcmp(argv[i], "--systems") == 0 && i + 1 < argc) {
      systems_path = argv[++i];
      continue;
    }
    if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
      output_path = argv[++i];
      output_explicit = 1;
      continue;
    }
    if (strcmp(argv[i], "--system") == 0 && i + 1 < argc) {
      system_filter = argv[++i];
      continue;
    }
    if (strcmp(argv[i], "--on-enter") == 0 && i + 1 < argc) {
      system_filter = argv[++i];
      on_enter_mode = 1;
      continue;
    }
    if (strcmp(argv[i], "--defer-thumbnails") == 0) {
      resolve_thumbnails = 0;
      thumbnail_policy_explicit = 1;
      continue;
    }
    if (strcmp(argv[i], "--with-thumbnails") == 0) {
      resolve_thumbnails = 1;
      thumbnail_policy_explicit = 1;
      continue;
    }
    fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
    usage(argv[0]);
    return 2;
  }

  if (!output_explicit) {
    if (system_filter) {
      if (!build_system_cache_path(selected_output_path, sizeof(selected_output_path),
                                   default_plumos_root, system_filter)) {
        fprintf(stderr, "error: cannot build system cache path for %s\n", system_filter);
        return 1;
      }
      output_path = selected_output_path;
    } else {
      join_path(default_output_path, sizeof(default_output_path), default_plumos_root,
                "state/frontend/library-index.json");
      output_path = default_output_path;
    }
  }
  if (on_enter_mode && !thumbnail_policy_explicit) {
    resolve_thumbnails = 0;
  }

  memset(&ctx, 0, sizeof(ctx));
  memset(systems, 0, sizeof(systems));
  ctx.sdcard_root = sdcard_root;
  ctx.plumos_root = default_plumos_root;
  ctx.system_filter = system_filter;
  ctx.resolve_thumbnails = resolve_thumbnails;
  ctx.systems = systems;

  start_sdcard_cleanup_process(default_plumos_root, sdcard_root);

  started_ms = now_ms();
  if (!load_systems(systems_path, systems, &ctx.system_count)) {
    return 1;
  }
  loaded_ms = now_ms();

  printf("plumOS library scan\n");
  printf("systems: %s loaded=%zu\n", systems_path, ctx.system_count);
  printf("sdcard_root: %s\n", ctx.sdcard_root);
  printf("plumos_root: %s\n", ctx.plumos_root);
  if (system_filter) {
    printf("mode: %s\n", on_enter_mode ? "on-enter" : "system");
    printf("filter: system=%s\n", system_filter);
  } else {
    printf("mode: full\n");
  }
  printf("thumbnail_lookup: %s\n", ctx.resolve_thumbnails ? "resolved" : "deferred");
  printf("output: %s\n", output_path);

  scan_systems(&ctx);
  scanned_ms = now_ms();
  qsort(ctx.roms.items, ctx.roms.count, sizeof(ctx.roms.items[0]), cmp_rom_entries);
  sorted_ms = now_ms();
  ctx.load_ms = loaded_ms - started_ms;
  ctx.scan_ms = scanned_ms - loaded_ms;
  ctx.sort_ms = sorted_ms - scanned_ms;
  ctx.ready_ms = sorted_ms - started_ms;

  if (!write_library_index(&ctx, output_path)) {
    free(ctx.roms.items);
    return 1;
  }

  /* A full boot scan is also the authoritative refresh boundary for the
   * per-system caches consumed when the user enters a system.  Keeping only
   * library-index.json fresh leaves old extensions and removed ROMs visible
   * indefinitely when scan-on-enter is disabled. */
  if (!system_filter) {
    size_t system_index;
    size_t system_cache_count = 0;
    for (system_index = 0; system_index < ctx.system_count; system_index++) {
      char system_cache_path[PATH_MAX];
      if (!ctx.systems[system_index].enabled) {
        continue;
      }
      ctx.system_filter = ctx.systems[system_index].id;
      if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path),
                                   default_plumos_root, ctx.system_filter) ||
          !write_library_index(&ctx, system_cache_path)) {
        fprintf(stderr, "error: cannot refresh system cache for %s\n",
                ctx.system_filter);
        free(ctx.roms.items);
        return 1;
      }
      system_cache_count++;
    }
    ctx.system_filter = NULL;
    printf("wrote_system_caches=%zu\n", system_cache_count);
  }

  elapsed_ms = now_ms() - started_ms;
  print_system_summary(&ctx);
  printf("timing load_ms=%lld scan_ms=%lld sort_ms=%lld ready_ms=%lld write_ms=%lld total_ms=%lld\n",
         ctx.load_ms, ctx.scan_ms, ctx.sort_ms, ctx.ready_ms, elapsed_ms - ctx.ready_ms,
         elapsed_ms);
  printf("summary alias_dirs=%zu files_seen=%zu matched=%zu roms=%zu thumbnails=%zu elapsed_ms=%lld\n",
         ctx.alias_dirs_found, ctx.files_seen, ctx.files_matched, ctx.roms.count,
         ctx.thumbnails_found, elapsed_ms);
  printf("wrote: %s\n", output_path);

  free(ctx.roms.items);
  return 0;
}
