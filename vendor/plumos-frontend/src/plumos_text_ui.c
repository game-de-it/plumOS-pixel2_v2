#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_TOP_ENTRIES 128
#define ROM_ENTRY_INITIAL_CAPACITY 256
#define MAX_MENU_ENTRIES 64
#define MAX_APP_ENTRIES 128
#define MAX_CORE_PROFILES 32
#define MAX_SYSTEM_CORE_OVERRIDES 256
#define MAX_ROM_CORE_OVERRIDES 1024
#define MAX_FAVORITES 2048
#define MAX_RECENTS 256
#define TEXT_COMMAND_MAX 8192
#define TEXT_PATH_MAX 1024
#define CPU_FREQ_UNSET (-1L)

struct top_entry {
  char id[64];
  char display_name[128];
  char short_name[64];
  char default_launch_profile[128];
  long rom_count;
  long thumbnail_count;
  int pinned;
  int virtual_entry;
};

struct rom_entry {
  char id[64];
  char title[256];
  char file_name[256];
  char relative_path[TEXT_PATH_MAX];
  char path[TEXT_PATH_MAX];
  char thumbnail[TEXT_PATH_MAX];
};

struct menu_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char action[128];
  int confirm;
};

struct app_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char launch_profile[256];
  char menu[64];
  int visible;
};

struct core_system_def {
  char id[64];
  char display_name[128];
  char default_launch_profile[128];
  char default_cpu_policy[32];
  long default_cpu_freq_khz;
  char launch_profiles[MAX_CORE_PROFILES][128];
  size_t launch_profile_count;
};

struct system_core_override {
  char system_id[64];
  char launch_profile[128];
  char cpu_policy[32];
  long cpu_freq_khz;
};

struct rom_core_override {
  char system_id[64];
  char relative_path[TEXT_PATH_MAX];
  char launch_profile[128];
  char cpu_policy[32];
  long cpu_freq_khz;
  char content_suffix[256];
  char audio_driver[16];
  long audio_latency_ms;
  char dosbox_pure_force60fps[16];
  char dosbox_pure_cycles[32];
};

struct core_override_state {
  struct system_core_override system_overrides[MAX_SYSTEM_CORE_OVERRIDES];
  size_t system_override_count;
  struct rom_core_override rom_overrides[MAX_ROM_CORE_OVERRIDES];
  size_t rom_override_count;
};

struct favorite_entry {
  char system_id[64];
  char relative_path[TEXT_PATH_MAX];
  char title[256];
  char file_name[256];
  char path[TEXT_PATH_MAX];
  char thumbnail[TEXT_PATH_MAX];
};

struct favorite_state {
  struct favorite_entry entries[MAX_FAVORITES];
  size_t count;
};

struct recent_entry {
  char system_id[64];
  char relative_path[TEXT_PATH_MAX];
  char title[256];
  char file_name[256];
  char path[TEXT_PATH_MAX];
  char thumbnail[TEXT_PATH_MAX];
  char launch_profile[128];
  char last_played_at[64];
  int resume_available;
};

struct recent_state {
  struct recent_entry entries[MAX_RECENTS];
  size_t count;
};

struct resume_session {
  int pending;
  char reason[64];
  char system_id[64];
  char relative_path[TEXT_PATH_MAX];
  char title[256];
  char file_name[256];
  char path[TEXT_PATH_MAX];
  char thumbnail[TEXT_PATH_MAX];
  char launch_profile[128];
  char updated_at[64];
  int auto_state_load;
};

struct retroarch_runtime_options {
  char audio_driver[16];
  long audio_latency_ms;
  char dosbox_pure_force60fps[16];
  char dosbox_pure_cycles[32];
};

struct launch_plan {
  char kind[32];
  char system_id[64];
  char relative_path[TEXT_PATH_MAX];
  char title[256];
  char rom_path[TEXT_PATH_MAX];
  char launch_profile[128];
  char cpu_policy[32];
  long cpu_freq_khz;
  char retroarch_path[TEXT_PATH_MAX];
  char standalone_launcher_path[TEXT_PATH_MAX];
  char pyxel_launcher_path[TEXT_PATH_MAX];
  char picoarch_launcher_path[TEXT_PATH_MAX];
  char core_path[TEXT_PATH_MAX];
  char config_path[TEXT_PATH_MAX];
  char command[TEXT_COMMAND_MAX];
  struct retroarch_runtime_options retroarch_options;
  int auto_state_load;
  int rom_exists;
  int runtime_exists;
  int core_exists;
  int can_execute;
};

struct frontend_settings {
  int show_favorites_on_top;
  int show_empty_systems;
  char boot_resume_mode[32];
};

struct cpu_preset {
  const char *label;
  const char *policy;
  long freq_khz;
};

static const struct cpu_preset CPU_PRESETS[] = {
    {"Interactive", "interactive", 0},
    {"Performance", "performance", 0},
    {"Ondemand", "ondemand", 0},
    {"Schedutil", "schedutil", 0},
    {"Conservative", "conservative", 0},
};

static const size_t CPU_PRESET_COUNT = sizeof(CPU_PRESETS) / sizeof(CPU_PRESETS[0]);

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

static int reserve_rom_entries(struct rom_entry **entries, size_t *capacity,
                               size_t min_capacity) {
  struct rom_entry *next_entries;
  const size_t max_capacity = ((size_t)-1) / sizeof((*entries)[0]);
  size_t next_capacity;

  if (!entries || !capacity || min_capacity > max_capacity) {
    return 0;
  }
  if (*capacity >= min_capacity) {
    return 1;
  }

  next_capacity = *capacity ? *capacity : ROM_ENTRY_INITIAL_CAPACITY;
  while (next_capacity < min_capacity) {
    if (next_capacity > max_capacity / 2) {
      next_capacity = max_capacity;
      break;
    }
    next_capacity *= 2;
  }

  next_entries = (struct rom_entry *)realloc(*entries, next_capacity * sizeof((*entries)[0]));
  if (!next_entries) {
    return 0;
  }
  *entries = next_entries;
  *capacity = next_capacity;
  return 1;
}

static int append_rom_entry(struct rom_entry **entries, size_t *count, size_t *capacity,
                            const struct rom_entry *entry) {
  if (!entries || !count || !capacity || !entry ||
      !reserve_rom_entries(entries, capacity, *count + 1)) {
    return 0;
  }
  (*entries)[(*count)++] = *entry;
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

static int dirname_path(char *out, size_t out_size, const char *path) {
  const char *slash;
  size_t len;

  if (!out || out_size == 0 || !path || !path[0]) {
    return 0;
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
    return 0;
  }
  memcpy(out, path, len);
  out[len] = '\0';
  return 1;
}

static int build_system_cache_path(char *out, size_t out_size, const char *plumos_root,
                                   const char *system_id) {
  char dir[PATH_MAX];
  char name[128];
  size_t id_len = strlen(system_id);

  if (id_len + 6 > sizeof(name)) {
    return 0;
  }
  memcpy(name, system_id, id_len);
  memcpy(name + id_len, ".json", 6);

  if (!join_path(dir, sizeof(dir), plumos_root, "state/frontend/systems")) {
    return 0;
  }
  return join_path(out, out_size, dir, name);
}

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int split_content_suffix(const char *path, char *base_out, size_t base_out_size,
                                const char **suffix_out);

static int rom_path_exists(const char *path) {
  struct stat st;
  char base_path[TEXT_PATH_MAX];

  if (stat(path, &st) == 0 && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
    return 1;
  }
  if (split_content_suffix(path, base_path, sizeof(base_path), NULL) &&
      strcmp(base_path, path) != 0 && stat(base_path, &st) == 0) {
    return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
  }
  return 0;
}

static int path_exists_any(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int valid_system_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-')) {
      return 0;
    }
  }
  return 1;
}

static int valid_launch_profile_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-' || c == ':' || c == '.')) {
      return 0;
    }
  }
  return 1;
}

static int valid_cpu_policy(const char *s) {
  return s && (strcmp(s, "interactive") == 0 || strcmp(s, "performance") == 0 ||
               strcmp(s, "ondemand") == 0 || strcmp(s, "schedutil") == 0 ||
               strcmp(s, "conservative") == 0);
}

static int valid_retroarch_audio_driver(const char *s) {
  return s && (strcmp(s, "oss") == 0 || strcmp(s, "alsa") == 0 ||
               strcmp(s, "alsathread") == 0 || strcmp(s, "pixel2_compat_ao") == 0);
}

static int parse_audio_latency_ms(const char *s, long *out) {
  char *end = NULL;
  long value;

  if (!s || !s[0] || !out) {
    return 0;
  }
  errno = 0;
  value = strtol(s, &end, 10);
  if (errno != 0 || !end || *end != '\0' || value <= 0 || value > 1000) {
    return 0;
  }
  *out = value;
  return 1;
}

static int valid_dosbox_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

static int valid_dosbox_cycles(const char *s) {
  char *end = NULL;
  long value;

  if (!s || !s[0]) {
    return 0;
  }
  if (strcmp(s, "auto") == 0 || strcmp(s, "max") == 0) {
    return 1;
  }
  errno = 0;
  value = strtol(s, &end, 10);
  return errno == 0 && end && *end == '\0' && value > 0 && value <= 1000000;
}

static int valid_content_suffix(const char *s) {
  const char *body;

  if (!s || s[0] != '#' || !s[1]) {
    return 0;
  }
  body = s + 1;
  if (body[0] == '/' || strstr(body, "../") || strstr(body, "/..") || strcmp(body, "..") == 0) {
    return 0;
  }
  while (*body) {
    unsigned char c = (unsigned char)*body++;
    if (c < 0x20 || c == 0x7f || c == '#') {
      return 0;
    }
  }
  return 1;
}

static int split_content_suffix(const char *path, char *base_out, size_t base_out_size,
                                const char **suffix_out) {
  const char *hash;
  size_t base_len;

  if (!path || !path[0]) {
    return 0;
  }
  hash = strchr(path, '#');
  if (!hash) {
    if (base_out) {
      return copy_string(base_out, base_out_size, path);
    }
    if (suffix_out) {
      *suffix_out = NULL;
    }
    return 1;
  }
  if (!valid_content_suffix(hash)) {
    return 0;
  }
  base_len = (size_t)(hash - path);
  if (base_len == 0 || (base_out && base_len + 1 > base_out_size)) {
    return 0;
  }
  if (base_out) {
    memcpy(base_out, path, base_len);
    base_out[base_len] = '\0';
  }
  if (suffix_out) {
    *suffix_out = hash;
  }
  return 1;
}

static int valid_relative_rom_path(const char *s) {
  if (!s || !s[0] || s[0] == '/') {
    return 0;
  }
  if (strstr(s, "../") || strstr(s, "/..") || strcmp(s, "..") == 0) {
    return 0;
  }
  return 1;
}

static int path_extension_lower(const char *path, char *out, size_t out_size) {
  const char *slash;
  const char *dot;
  size_t i;

  if (!path || !path[0] || !out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  slash = strrchr(path, '/');
  dot = strrchr(path, '.');
  if (!dot || (slash && dot < slash) || !dot[1]) {
    return 0;
  }
  dot++;
  for (i = 0; dot[i]; i++) {
    if (i + 1 >= out_size) {
      out[0] = '\0';
      return 0;
    }
    out[i] = (char)tolower((unsigned char)dot[i]);
  }
  out[i] = '\0';
  return i > 0;
}

static void current_utc_timestamp(char *out, size_t out_size) {
  time_t now;
  struct tm *tm_now;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  now = time(NULL);
  tm_now = gmtime(&now);
  if (!tm_now || strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", tm_now) == 0) {
    copy_string(out, out_size, "unknown");
  }
}

static int append_shell_quoted(char *out, size_t out_size, size_t *pos, const char *in) {
  const char *p;

  if (!append_string(out, out_size, pos, "'")) {
    return 0;
  }
  for (p = in; p && *p; p++) {
    char c[2];
    if (*p == '\'') {
      if (!append_string(out, out_size, pos, "'\\''")) {
        return 0;
      }
    } else {
      c[0] = *p;
      c[1] = '\0';
      if (!append_string(out, out_size, pos, c)) {
        return 0;
      }
    }
  }
  return append_string(out, out_size, pos, "'");
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

static int json_next_string(const char **cursor, const char *end, char *out, size_t out_size) {
  const char *p = *cursor;

  while (p < end && *p != '"') {
    p++;
  }
  if (p >= end) {
    return 0;
  }
  if (!json_decode_string(&p, end, out, out_size)) {
    return 0;
  }
  *cursor = p;
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
  if (value < end && strncmp(value, "null", 4) == 0) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  return json_decode_string(&value, end, out, out_size);
}

static long json_get_long(const char *json, const char *end, const char *key, long default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  return strtol(value, NULL, 10);
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

static void json_write_escaped(FILE *f, const char *s) {
  fputc('"', f);
  while (s && *s) {
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

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  size_t len;
  size_t i;

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

  for (i = 1; i < len; i++) {
    if (tmp[i] == '/') {
      tmp[i] = '\0';
      if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        return 0;
      }
      tmp[i] = '/';
    }
  }
  if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static int ensure_parent_dir(const char *path) {
  char dir[PATH_MAX];
  char *slash;

  if (!copy_string(dir, sizeof(dir), path)) {
    return 0;
  }
  slash = strrchr(dir, '/');
  if (!slash) {
    return 1;
  }
  if (slash == dir) {
    return 1;
  }
  *slash = '\0';
  return mkdir_p(dir);
}

static int sync_parent_dir(const char *path) {
  char dir[PATH_MAX];
  char *slash;
  int fd;
  int rc;
  int sync_errno = 0;

  if (!copy_string(dir, sizeof(dir), path)) {
    return 0;
  }
  slash = strrchr(dir, '/');
  if (!slash) {
    copy_string(dir, sizeof(dir), ".");
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

static int commit_temp_file(FILE *f, const char *tmp_path, const char *path) {
  int fd;
  int ok = 1;

  if (!f || !tmp_path || !path) {
    return 0;
  }
  fd = fileno(f);
  if (fflush(f) != 0 || fd < 0 || fsync(fd) != 0) {
    ok = 0;
  }
  if (fclose(f) != 0) {
    ok = 0;
  }
  if (!ok) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  return sync_parent_dir(path);
}

static __attribute__((noinline)) const char *runtime_shell_path(void) {
  const char *device_id = getenv("PLUMOS_DEVICE_ID");
  const char *configured = getenv("PLUMOS_BUSYBOX");
  const char *plumos_root;
  static char bundled_busybox[PATH_MAX];

  if (configured && configured[0] && access(configured, X_OK) == 0) {
    return configured;
  }
  if (!device_id || strcmp(device_id, "pixel2") != 0) {
    return "/bin/sh";
  }
  if (access("/mnt/SDCARD/.tmp_update/busybox", X_OK) == 0) {
    return "/mnt/SDCARD/.tmp_update/busybox";
  }
  plumos_root = getenv("PLUMOS_ROOT");
  if (plumos_root && plumos_root[0] &&
      snprintf(bundled_busybox, sizeof(bundled_busybox), "%s/bin/busybox",
               plumos_root) < (int)sizeof(bundled_busybox) &&
      access(bundled_busybox, X_OK) == 0) {
    return bundled_busybox;
  }
  return NULL;
}

static int runtime_shell_is_busybox(const char *shell_path) {
  const char *name;

  if (!shell_path || !shell_path[0]) {
    return 0;
  }
  name = strrchr(shell_path, '/');
  name = name ? name + 1 : shell_path;
  return strcmp(name, "busybox") == 0;
}

static int append_runtime_script_invocation(char *out, size_t out_size,
                                            size_t *pos, const char *script) {
  const char *shell_path = runtime_shell_path();

  if (!shell_path || !script || !script[0] ||
      !append_shell_quoted(out, out_size, pos, shell_path)) {
    return 0;
  }
  if (runtime_shell_is_busybox(shell_path) &&
      !append_string(out, out_size, pos, " sh")) {
    return 0;
  }
  return append_string(out, out_size, pos, " ") &&
         append_shell_quoted(out, out_size, pos, script);
}

static int run_runtime_shell_command(const char *cmd) {
  const char *shell_path;
  char *shell_argv[5];
  pid_t pid;
  int status;

  if (!cmd || !cmd[0]) {
    errno = EINVAL;
    return -1;
  }
  shell_path = runtime_shell_path();
  if (!shell_path) {
    errno = ENOENT;
    return -1;
  }
  shell_argv[0] = (char *)shell_path;
  if (runtime_shell_is_busybox(shell_path)) {
    shell_argv[1] = (char *)"sh";
    shell_argv[2] = (char *)"-c";
    shell_argv[3] = (char *)cmd;
    shell_argv[4] = NULL;
  } else {
    shell_argv[1] = (char *)"-c";
    shell_argv[2] = (char *)cmd;
    shell_argv[3] = NULL;
    shell_argv[4] = NULL;
  }
  pid = vfork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    execve(shell_path, shell_argv, environ);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  return status;
}

static int run_scanner(const char *plumos_root, const char *sdcard_root, const char *system_id,
                       int full_refresh) {
  char scanner[PATH_MAX];
  char cmd[PATH_MAX * 3];
  size_t pos = 0;
  int rc;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    fprintf(stderr, "error: scanner path is too long\n");
    return 0;
  }
  if (!file_exists(scanner)) {
    fprintf(stderr, "error: missing scanner: %s\n", scanner);
    return 0;
  }
  if (system_id && !valid_system_id(system_id)) {
    fprintf(stderr, "error: invalid system id: %s\n", system_id);
    return 0;
  }

  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scanner)) {
    fprintf(stderr, "error: scanner command is too long\n");
    return 0;
  }
  if (!full_refresh &&
      (!append_string(cmd, sizeof(cmd), &pos, " --on-enter ") ||
       !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id))) {
    fprintf(stderr, "error: scanner command is too long\n");
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/dev/null")) {
    fprintf(stderr, "error: scanner command is too long\n");
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  if (rc == -1) {
    fprintf(stderr, "error: failed to run scanner\n");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 1;
  }
  fprintf(stderr, "error: scanner failed with status %d\n", rc);
  return 0;
}

static int env_flag_disabled(const char *name) {
  const char *value = getenv(name);

  if (!value || !value[0]) {
    return 0;
  }
  return strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "False") == 0 ||
         strcmp(value, "no") == 0 || strcmp(value, "No") == 0 || strcmp(value, "off") == 0 ||
         strcmp(value, "Off") == 0;
}

static void redirect_stdio_to_devnull(void) {
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

static int pixel2_compat_runtime_owns_hotkeyd(const char *plumos_root,
                                    const struct launch_plan *plan) {
  char path[PATH_MAX];

  if (!plumos_root || !plumos_root[0] || !plan) {
    return 0;
  }
  if (strcmp(plan->kind, "picoarch") == 0) {
    return 1;
  }
  if (strcmp(plan->kind, "retroarch") == 0) {
    if (join_path(path, sizeof(path), plumos_root, "config/enable-pixel2_compat-ra-runtime") &&
        file_exists(path)) {
      return 1;
    }
    if (join_path(path, sizeof(path), plumos_root,
                  "config/enable-pixel2_compat-onion-compatible-ra") &&
        file_exists(path)) {
      return 1;
    }
  }
  return 0;
}

static int launch_plan_uses_safe_hotkeyd(const char *plumos_root,
                                         const struct launch_plan *plan) {
  if (!plan) {
    return 0;
  }
  if (pixel2_compat_runtime_owns_hotkeyd(plumos_root, plan)) {
    return 0;
  }
  return strcmp(plan->kind, "retroarch") == 0 ||
         strcmp(plan->kind, "standalone") == 0 ||
         strcmp(plan->kind, "picoarch") == 0;
}

static pid_t start_safe_hotkeyd(const char *plumos_root, const struct launch_plan *plan) {
  char hotkeyd_path[PATH_MAX];
  pid_t pid;

  if (!launch_plan_uses_safe_hotkeyd(plumos_root, plan) ||
      env_flag_disabled("PLUMOS_SAFE_HOTKEYD_AUTOSTART")) {
    return 0;
  }
  if (!join_path(hotkeyd_path, sizeof(hotkeyd_path), plumos_root, "bin/plumos-safe-hotkeyd")) {
    fprintf(stderr, "warning: safe hotkeyd path is too long\n");
    return 0;
  }
  if (!file_exists(hotkeyd_path)) {
    fprintf(stderr, "warning: missing safe hotkeyd: %s\n", hotkeyd_path);
    return 0;
  }

  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "warning: cannot start safe hotkeyd: %s\n", strerror(errno));
    return 0;
  }
  if (pid == 0) {
    redirect_stdio_to_devnull();
    execl(hotkeyd_path, hotkeyd_path, (char *)NULL);
    _exit(127);
  }

  printf("safe_hotkeyd: started pid=%ld\n", (long)pid);
  return pid;
}

static void stop_safe_hotkeyd(pid_t pid) {
  int status;

  if (pid <= 0) {
    return;
  }
  if (waitpid(pid, &status, WNOHANG) == pid) {
    return;
  }
  if (errno == ECHILD) {
    return;
  }

  if (path_exists_any("/tmp/plumos-power-action.lock") ||
      path_exists_any("/tmp/plumos-safe-shutdown.lock")) {
    for (int i = 0; i < 350; i++) {
      pid_t rc = waitpid(pid, &status, WNOHANG);
      if (rc == pid || (rc < 0 && errno == ECHILD)) {
        return;
      }
      if (!path_exists_any("/tmp/plumos-power-action.lock") &&
          !path_exists_any("/tmp/plumos-safe-shutdown.lock")) {
        break;
      }
      usleep(100000);
    }
    for (int i = 0; i < 20; i++) {
      pid_t rc = waitpid(pid, &status, WNOHANG);
      if (rc == pid || (rc < 0 && errno == ECHILD)) {
        return;
      }
      usleep(100000);
    }
  }

  kill(pid, SIGTERM);
  for (int i = 0; i < 20; i++) {
    pid_t rc = waitpid(pid, &status, WNOHANG);
    if (rc == pid || (rc < 0 && errno == ECHILD)) {
      return;
    }
    usleep(100000);
  }
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);
}

static void persist_runtime_volume(const char *plumos_root) {
  char helper[PATH_MAX];
  char cmd[TEXT_COMMAND_MAX];
  size_t pos = 0;

  if (!join_path(helper, sizeof(helper), plumos_root, "bin/plumos-volume-control") ||
      !file_exists(helper)) {
    return;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, helper) ||
      !append_string(cmd, sizeof(cmd), &pos, " persist-runtime >/tmp/.plumos_volume_set 2>&1")) {
    return;
  }
  (void)run_runtime_shell_command(cmd);
}

static void init_frontend_settings(struct frontend_settings *settings) {
  memset(settings, 0, sizeof(*settings));
  copy_string(settings->boot_resume_mode, sizeof(settings->boot_resume_mode), "off");
}

static void normalize_boot_resume_mode(char *mode, size_t mode_size) {
  if (!mode || mode_size == 0) {
    return;
  }
  if (strcmp(mode, "last") == 0 || strcmp(mode, "Last") == 0 ||
      strcmp(mode, "on") == 0 || strcmp(mode, "ON") == 0 ||
      strcmp(mode, "true") == 0 || strcmp(mode, "1") == 0) {
    copy_string(mode, mode_size, "on");
  } else if (strcmp(mode, "picker") == 0 || strcmp(mode, "Picker") == 0 ||
             strcmp(mode, "recent") == 0 || strcmp(mode, "Recent") == 0) {
    copy_string(mode, mode_size, "recent");
  } else {
    copy_string(mode, mode_size, "off");
  }
}

static int load_frontend_settings(const char *path, struct frontend_settings *settings) {
  char *json;
  size_t json_size;

  init_frontend_settings(settings);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  settings->show_favorites_on_top =
      json_get_bool(json, json + json_size, "show_favorites_on_top", 0);
  settings->show_empty_systems = json_get_bool(json, json + json_size, "show_empty_systems", 0);
  json_get_string(json, json + json_size, "boot_resume_mode", settings->boot_resume_mode,
                  sizeof(settings->boot_resume_mode));
  normalize_boot_resume_mode(settings->boot_resume_mode, sizeof(settings->boot_resume_mode));
  free(json);
  return 1;
}

static int core_profile_index(const struct core_system_def *system, const char *profile) {
  size_t i;
  for (i = 0; i < system->launch_profile_count; i++) {
    if (strcmp(system->launch_profiles[i], profile) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int core_profile_is_listed(const struct core_system_def *system, const char *profile) {
  return profile && profile[0] && core_profile_index(system, profile) >= 0;
}

static int picoarch_core_id_allowed(const char *core_id) {
  if (!core_id || !core_id[0]) {
    return 0;
  }
  /*
   * The Pixel2 PicoArch component is a software framebuffer host. Cores packaged
   * with the hardware-GLES rendering contract must stay on RetroArch, which
   * can create the required graphics context.
   */
  if (strcmp(core_id, "flycast") == 0 || strcmp(core_id, "flycast_xtreme") == 0 ||
      strcmp(core_id, "km_duckswanstation_xtreme_amped") == 0 ||
      strcmp(core_id, "parallel_n64") == 0 ||
      strcmp(core_id, "yabasanshiro") == 0) {
    return 0;
  }
  /*
   * Frodo needs RetroArch's Mapped Port = 2 path for common C64 joystick-port-2
   * titles. PicoArch does not expose that mapping and is too slow on Pixel2.
   */
  if (strcmp(core_id, "frodo") == 0) {
    return 0;
  }
  /*
   * ChaiLove embeds its own SDL video runtime. It cannot acquire a second
   * second video device below PicoArch and terminates with a segmentation fault.
   */
  if (strcmp(core_id, "chailove") == 0) {
    return 0;
  }
  return 1;
}

static int launch_profile_allowed_on_pixel2(const char *profile) {
  if (!profile || !profile[0]) {
    return 0;
  }
  if (strcmp(profile, "picoarch:frodo") == 0) {
    return 0;
  }
  if (strncmp(profile, "picoarch:", 9) == 0) {
    return picoarch_core_id_allowed(profile + 9);
  }
  return 1;
}

static int add_core_profile(struct core_system_def *system, const char *profile) {
  if (!valid_launch_profile_id(profile) || !launch_profile_allowed_on_pixel2(profile)) {
    return 0;
  }
  if (core_profile_index(system, profile) >= 0) {
    return 1;
  }
  if (system->launch_profile_count >= MAX_CORE_PROFILES) {
    return 0;
  }
  copy_string(system->launch_profiles[system->launch_profile_count],
              sizeof(system->launch_profiles[system->launch_profile_count]), profile);
  system->launch_profile_count++;
  return 1;
}

static int picoarch_companion_core_allowed(const char *system_id, const char *core_id) {
  /* N64 hardware-render cores are not supported by the Pixel2 PicoArch host. */
  if (system_id && strcmp(system_id, "n64") == 0) {
    return 0;
  }
  /* No Saturn core reaches a usable launch path through PicoArch on Pixel2. */
  if (system_id && strcmp(system_id, "saturn") == 0) {
    return 0;
  }
  return picoarch_core_id_allowed(core_id);
}

static void add_picoarch_companion_profiles(struct core_system_def *system) {
  size_t original_count;
  size_t i;

  if (!system) {
    return;
  }
  original_count = system->launch_profile_count;
  for (i = 0; i < original_count && system->launch_profile_count < MAX_CORE_PROFILES; i++) {
    const char *profile = system->launch_profiles[i];
    const char *core_id;
    char pico_profile[128];

    if (strncmp(profile, "retroarch:", 10) != 0) {
      continue;
    }
    core_id = profile + 10;
    if (!core_id[0]) {
      continue;
    }
    if (!picoarch_companion_core_allowed(system->id, core_id)) {
      continue;
    }
    if (snprintf(pico_profile, sizeof(pico_profile), "picoarch:%s", core_id) >=
        (int)sizeof(pico_profile)) {
      continue;
    }
    if (!launch_profile_allowed_on_pixel2(pico_profile)) {
      continue;
    }
    add_core_profile(system, pico_profile);
  }
}

static int resolve_device_launch_profile(const struct core_system_def *system, const char *profile,
                                         char *out, size_t out_size) {
  (void)system;
  return copy_string(out, out_size, profile);
}

static void format_launch_profile_display(const char *profile, char *out, size_t out_size) {
  const char *label = NULL;
  const char *value = NULL;

  if (!out || out_size == 0) {
    return;
  }
  if (!profile || !profile[0] || strcmp(profile, "auto") == 0) {
    copy_string(out, out_size, "auto");
    return;
  }
  if (strncmp(profile, "retroarch:", 10) == 0) {
    label = "RA";
    value = profile + 10;
  } else if (strncmp(profile, "picoarch:", 9) == 0) {
    label = "PICO";
    value = profile + 9;
  } else if (strncmp(profile, "standalone:", 11) == 0) {
    label = "SA";
    value = profile + 11;
  }
  if (label && value && value[0]) {
    snprintf(out, out_size, "%s: %s", label, value);
    out[out_size - 1] = '\0';
    return;
  }
  copy_string(out, out_size, profile);
}

static int parse_launch_profile_array(const char *json, const char *end,
                                      struct core_system_def *system) {
  const char *profiles_start;
  const char *profiles_end;
  const char *cursor;
  char profile[128];

  if (!json_find_array(json, end, "launch_profiles", &profiles_start, &profiles_end)) {
    return 1;
  }
  cursor = profiles_start;
  while (system->launch_profile_count < MAX_CORE_PROFILES &&
         json_next_string(&cursor, profiles_end, profile, sizeof(profile))) {
    add_core_profile(system, profile);
  }
  return 1;
}

static int load_core_system_def(const char *path, const char *system_id,
                                struct core_system_def *system_out) {
  char *json;
  size_t json_size;
  const char *systems_start;
  const char *systems_end;
  const char *cursor;

  memset(system_out, 0, sizeof(*system_out));
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    free(json);
    return 0;
  }

  cursor = systems_start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct core_system_def system;

    if (!json_next_object(&cursor, systems_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&system, 0, sizeof(system));
    json_get_string(obj, obj + strlen(obj), "id", system.id, sizeof(system.id));
    if (strcmp(system.id, system_id) != 0) {
      free(obj);
      continue;
    }
    json_get_string(obj, obj + strlen(obj), "display_name", system.display_name,
                    sizeof(system.display_name));
    json_get_string(obj, obj + strlen(obj), "default_launch_profile",
                    system.default_launch_profile, sizeof(system.default_launch_profile));
    json_get_string(obj, obj + strlen(obj), "default_cpu_policy", system.default_cpu_policy,
                    sizeof(system.default_cpu_policy));
    system.default_cpu_freq_khz =
        json_get_long(obj, obj + strlen(obj), "default_cpu_freq_khz", 0);
    if (system.default_cpu_policy[0] && !valid_cpu_policy(system.default_cpu_policy)) {
      system.default_cpu_policy[0] = '\0';
    }
    system.default_cpu_freq_khz = 0;
    parse_launch_profile_array(obj, obj + strlen(obj), &system);
    if (system.default_launch_profile[0]) {
      add_core_profile(&system, system.default_launch_profile);
    }
    add_picoarch_companion_profiles(&system);
    if (!system.display_name[0]) {
      copy_string(system.display_name, sizeof(system.display_name), system.id);
    }
    *system_out = system;
    free(obj);
    free(json);
    return 1;
  }

  free(json);
  return 0;
}

static void init_core_override_state(struct core_override_state *state) {
  memset(state, 0, sizeof(*state));
}

static int load_core_overrides(const char *path, struct core_override_state *state) {
  char *json;
  size_t json_size;
  const char *array_start;
  const char *array_end;
  const char *cursor;

  init_core_override_state(state);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }

  if (json_find_array(json, json + json_size, "system_overrides", &array_start, &array_end)) {
    cursor = array_start;
    while (state->system_override_count < MAX_SYSTEM_CORE_OVERRIDES) {
      const char *obj_start;
      const char *obj_end;
      char *obj;
      struct system_core_override entry;

      if (!json_next_object(&cursor, array_end, &obj_start, &obj_end)) {
        break;
      }
      obj = range_dup(obj_start, obj_end);
      if (!obj) {
        free(json);
        return 0;
      }
      memset(&entry, 0, sizeof(entry));
      json_get_string(obj, obj + strlen(obj), "system_id", entry.system_id,
                      sizeof(entry.system_id));
      json_get_string(obj, obj + strlen(obj), "launch_profile", entry.launch_profile,
                      sizeof(entry.launch_profile));
      json_get_string(obj, obj + strlen(obj), "cpu_policy", entry.cpu_policy,
                      sizeof(entry.cpu_policy));
      entry.cpu_freq_khz = json_get_long(obj, obj + strlen(obj), "cpu_freq_khz", 0);
      entry.cpu_freq_khz = 0;
      if (valid_system_id(entry.system_id) &&
          (!entry.launch_profile[0] || valid_launch_profile_id(entry.launch_profile)) &&
          (!entry.cpu_policy[0] || valid_cpu_policy(entry.cpu_policy))) {
        state->system_overrides[state->system_override_count++] = entry;
      }
      free(obj);
    }
  }

  if (json_find_array(json, json + json_size, "rom_overrides", &array_start, &array_end)) {
    cursor = array_start;
    while (state->rom_override_count < MAX_ROM_CORE_OVERRIDES) {
      const char *obj_start;
      const char *obj_end;
      char *obj;
      struct rom_core_override entry;

      if (!json_next_object(&cursor, array_end, &obj_start, &obj_end)) {
        break;
      }
      obj = range_dup(obj_start, obj_end);
      if (!obj) {
        free(json);
        return 0;
      }
      memset(&entry, 0, sizeof(entry));
      json_get_string(obj, obj + strlen(obj), "system_id", entry.system_id,
                      sizeof(entry.system_id));
      json_get_string(obj, obj + strlen(obj), "relative_path", entry.relative_path,
                      sizeof(entry.relative_path));
      json_get_string(obj, obj + strlen(obj), "launch_profile", entry.launch_profile,
                      sizeof(entry.launch_profile));
      json_get_string(obj, obj + strlen(obj), "cpu_policy", entry.cpu_policy,
                      sizeof(entry.cpu_policy));
      json_get_string(obj, obj + strlen(obj), "content_suffix", entry.content_suffix,
                      sizeof(entry.content_suffix));
      json_get_string(obj, obj + strlen(obj), "audio_driver", entry.audio_driver,
                      sizeof(entry.audio_driver));
      json_get_string(obj, obj + strlen(obj), "dosbox_pure_force60fps",
                      entry.dosbox_pure_force60fps,
                      sizeof(entry.dosbox_pure_force60fps));
      json_get_string(obj, obj + strlen(obj), "dosbox_pure_cycles",
                      entry.dosbox_pure_cycles, sizeof(entry.dosbox_pure_cycles));
      entry.cpu_freq_khz = json_get_long(obj, obj + strlen(obj), "cpu_freq_khz", 0);
      entry.audio_latency_ms = json_get_long(obj, obj + strlen(obj), "audio_latency_ms", 0);
      entry.cpu_freq_khz = 0;
      if (entry.content_suffix[0] && !valid_content_suffix(entry.content_suffix)) {
        entry.content_suffix[0] = '\0';
      }
      if (entry.audio_driver[0] && !valid_retroarch_audio_driver(entry.audio_driver)) {
        entry.audio_driver[0] = '\0';
      }
      if (entry.audio_latency_ms <= 0 || entry.audio_latency_ms > 1000) {
        entry.audio_latency_ms = 0;
      }
      if (entry.dosbox_pure_force60fps[0] &&
          !valid_dosbox_bool(entry.dosbox_pure_force60fps)) {
        entry.dosbox_pure_force60fps[0] = '\0';
      }
      if (entry.dosbox_pure_cycles[0] && !valid_dosbox_cycles(entry.dosbox_pure_cycles)) {
        entry.dosbox_pure_cycles[0] = '\0';
      }
      if (valid_system_id(entry.system_id) && valid_relative_rom_path(entry.relative_path) &&
          (!entry.launch_profile[0] || valid_launch_profile_id(entry.launch_profile)) &&
          (!entry.cpu_policy[0] || valid_cpu_policy(entry.cpu_policy))) {
        state->rom_overrides[state->rom_override_count++] = entry;
      }
      free(obj);
    }
  }

  free(json);
  return 1;
}

static int save_core_overrides(const char *path, const struct core_override_state *state) {
  char tmp_path[PATH_MAX];
  FILE *f;
  size_t i;

  if (!ensure_parent_dir(path)) {
    return 0;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"system_overrides\": [\n");
  for (i = 0; i < state->system_override_count; i++) {
    fprintf(f, "    { \"system_id\": ");
    json_write_escaped(f, state->system_overrides[i].system_id);
    if (state->system_overrides[i].launch_profile[0]) {
      fprintf(f, ", \"launch_profile\": ");
      json_write_escaped(f, state->system_overrides[i].launch_profile);
    }
    if (state->system_overrides[i].cpu_policy[0]) {
      fprintf(f, ", \"cpu_policy\": ");
      json_write_escaped(f, state->system_overrides[i].cpu_policy);
      if (state->system_overrides[i].cpu_freq_khz > 0) {
        fprintf(f, ", \"cpu_freq_khz\": %ld", state->system_overrides[i].cpu_freq_khz);
      }
    }
    fprintf(f, " }%s\n", i + 1 < state->system_override_count ? "," : "");
  }
  fprintf(f, "  ],\n");
  fprintf(f, "  \"rom_overrides\": [\n");
  for (i = 0; i < state->rom_override_count; i++) {
    fprintf(f, "    { \"system_id\": ");
    json_write_escaped(f, state->rom_overrides[i].system_id);
    fprintf(f, ", \"relative_path\": ");
    json_write_escaped(f, state->rom_overrides[i].relative_path);
    if (state->rom_overrides[i].launch_profile[0]) {
      fprintf(f, ", \"launch_profile\": ");
      json_write_escaped(f, state->rom_overrides[i].launch_profile);
    }
    if (state->rom_overrides[i].cpu_policy[0]) {
      fprintf(f, ", \"cpu_policy\": ");
      json_write_escaped(f, state->rom_overrides[i].cpu_policy);
      if (state->rom_overrides[i].cpu_freq_khz > 0) {
        fprintf(f, ", \"cpu_freq_khz\": %ld", state->rom_overrides[i].cpu_freq_khz);
      }
    }
    if (state->rom_overrides[i].content_suffix[0]) {
      fprintf(f, ", \"content_suffix\": ");
      json_write_escaped(f, state->rom_overrides[i].content_suffix);
    }
    if (state->rom_overrides[i].audio_driver[0]) {
      fprintf(f, ", \"audio_driver\": ");
      json_write_escaped(f, state->rom_overrides[i].audio_driver);
    }
    if (state->rom_overrides[i].audio_latency_ms > 0) {
      fprintf(f, ", \"audio_latency_ms\": %ld", state->rom_overrides[i].audio_latency_ms);
    }
    if (state->rom_overrides[i].dosbox_pure_force60fps[0]) {
      fprintf(f, ", \"dosbox_pure_force60fps\": ");
      json_write_escaped(f, state->rom_overrides[i].dosbox_pure_force60fps);
    }
    if (state->rom_overrides[i].dosbox_pure_cycles[0]) {
      fprintf(f, ", \"dosbox_pure_cycles\": ");
      json_write_escaped(f, state->rom_overrides[i].dosbox_pure_cycles);
    }
    fprintf(f, " }%s\n", i + 1 < state->rom_override_count ? "," : "");
  }
  fprintf(f, "  ]\n");
  fprintf(f, "}\n");

  return commit_temp_file(f, tmp_path, path);
}

static int find_system_core_override(const struct core_override_state *state,
                                     const char *system_id) {
  size_t i;
  for (i = 0; i < state->system_override_count; i++) {
    if (strcmp(state->system_overrides[i].system_id, system_id) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int find_rom_core_override(const struct core_override_state *state, const char *system_id,
                                  const char *relative_path) {
  size_t i;
  char base_relative_path[TEXT_PATH_MAX];
  const char *suffix = NULL;

  for (i = 0; i < state->rom_override_count; i++) {
    if (strcmp(state->rom_overrides[i].system_id, system_id) == 0 &&
        strcmp(state->rom_overrides[i].relative_path, relative_path) == 0) {
      return (int)i;
    }
  }
  if (system_id && strcmp(system_id, "dos") == 0 &&
      split_content_suffix(relative_path, base_relative_path, sizeof(base_relative_path),
                           &suffix) &&
      suffix && valid_relative_rom_path(base_relative_path)) {
    for (i = 0; i < state->rom_override_count; i++) {
      if (strcmp(state->rom_overrides[i].system_id, system_id) == 0 &&
          strcmp(state->rom_overrides[i].relative_path, base_relative_path) == 0) {
        return (int)i;
      }
    }
  }
  return -1;
}

static struct rom_core_override *ensure_rom_core_override(struct core_override_state *state,
                                                          const char *system_id,
                                                          const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  struct rom_core_override *entry;

  if (idx >= 0) {
    return &state->rom_overrides[idx];
  }
  if (state->rom_override_count >= MAX_ROM_CORE_OVERRIDES) {
    return NULL;
  }
  entry = &state->rom_overrides[state->rom_override_count++];
  memset(entry, 0, sizeof(*entry));
  copy_string(entry->system_id, sizeof(entry->system_id), system_id);
  copy_string(entry->relative_path, sizeof(entry->relative_path), relative_path);
  return entry;
}

static int set_system_core_override(struct core_override_state *state, const char *system_id,
                                    const char *launch_profile) {
  int idx = find_system_core_override(state, system_id);
  if (idx >= 0) {
    return copy_string(state->system_overrides[idx].launch_profile,
                       sizeof(state->system_overrides[idx].launch_profile), launch_profile);
  }
  if (state->system_override_count >= MAX_SYSTEM_CORE_OVERRIDES) {
    return 0;
  }
  copy_string(state->system_overrides[state->system_override_count].system_id,
              sizeof(state->system_overrides[state->system_override_count].system_id),
              system_id);
  copy_string(state->system_overrides[state->system_override_count].launch_profile,
              sizeof(state->system_overrides[state->system_override_count].launch_profile),
              launch_profile);
  state->system_override_count++;
  return 1;
}

static int system_core_override_is_empty(const struct system_core_override *entry) {
  return !entry->launch_profile[0] && !entry->cpu_policy[0];
}

static int rom_core_override_is_empty(const struct rom_core_override *entry) {
  return !entry->launch_profile[0] && !entry->cpu_policy[0] &&
         !entry->content_suffix[0] && !entry->audio_driver[0] &&
         entry->audio_latency_ms <= 0 && !entry->dosbox_pure_force60fps[0] &&
         !entry->dosbox_pure_cycles[0];
}

static int clear_system_core_override(struct core_override_state *state, const char *system_id) {
  int idx = find_system_core_override(state, system_id);
  if (idx < 0) {
    return 1;
  }
  if ((size_t)idx + 1 < state->system_override_count) {
    memmove(&state->system_overrides[idx], &state->system_overrides[idx + 1],
            (state->system_override_count - (size_t)idx - 1) *
                sizeof(state->system_overrides[0]));
  }
  state->system_override_count--;
  return 1;
}

static int clear_system_launch_profile_override(struct core_override_state *state,
                                                const char *system_id) {
  int idx = find_system_core_override(state, system_id);
  if (idx < 0) {
    return 1;
  }
  state->system_overrides[idx].launch_profile[0] = '\0';
  if (system_core_override_is_empty(&state->system_overrides[idx])) {
    return clear_system_core_override(state, system_id);
  }
  return 1;
}

static int set_system_cpu_override(struct core_override_state *state, const char *system_id,
                                   const char *cpu_policy, long cpu_freq_khz) {
  int idx = find_system_core_override(state, system_id);
  struct system_core_override *entry;

  if (!valid_cpu_policy(cpu_policy)) {
    return 0;
  }
  (void)cpu_freq_khz;

  if (idx >= 0) {
    entry = &state->system_overrides[idx];
  } else {
    if (state->system_override_count >= MAX_SYSTEM_CORE_OVERRIDES) {
      return 0;
    }
    entry = &state->system_overrides[state->system_override_count++];
    memset(entry, 0, sizeof(*entry));
    copy_string(entry->system_id, sizeof(entry->system_id), system_id);
  }
  copy_string(entry->cpu_policy, sizeof(entry->cpu_policy), cpu_policy);
  entry->cpu_freq_khz = 0;
  return 1;
}

static int clear_system_cpu_override(struct core_override_state *state, const char *system_id) {
  int idx = find_system_core_override(state, system_id);
  if (idx < 0) {
    return 1;
  }
  state->system_overrides[idx].cpu_policy[0] = '\0';
  state->system_overrides[idx].cpu_freq_khz = 0;
  if (system_core_override_is_empty(&state->system_overrides[idx])) {
    return clear_system_core_override(state, system_id);
  }
  return 1;
}

static int set_rom_core_override(struct core_override_state *state, const char *system_id,
                                 const char *relative_path, const char *launch_profile) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  if (idx >= 0) {
    return copy_string(state->rom_overrides[idx].launch_profile,
                       sizeof(state->rom_overrides[idx].launch_profile), launch_profile);
  }
  if (state->rom_override_count >= MAX_ROM_CORE_OVERRIDES) {
    return 0;
  }
  copy_string(state->rom_overrides[state->rom_override_count].system_id,
              sizeof(state->rom_overrides[state->rom_override_count].system_id), system_id);
  copy_string(state->rom_overrides[state->rom_override_count].relative_path,
              sizeof(state->rom_overrides[state->rom_override_count].relative_path),
              relative_path);
  copy_string(state->rom_overrides[state->rom_override_count].launch_profile,
              sizeof(state->rom_overrides[state->rom_override_count].launch_profile),
              launch_profile);
  state->rom_override_count++;
  return 1;
}

static int clear_rom_core_override(struct core_override_state *state, const char *system_id,
                                   const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  if (idx < 0) {
    return 1;
  }
  if ((size_t)idx + 1 < state->rom_override_count) {
    memmove(&state->rom_overrides[idx], &state->rom_overrides[idx + 1],
            (state->rom_override_count - (size_t)idx - 1) * sizeof(state->rom_overrides[0]));
  }
  state->rom_override_count--;
  return 1;
}

static int clear_rom_launch_profile_override(struct core_override_state *state,
                                             const char *system_id,
                                             const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  if (idx < 0) {
    return 1;
  }
  state->rom_overrides[idx].launch_profile[0] = '\0';
  if (rom_core_override_is_empty(&state->rom_overrides[idx])) {
    return clear_rom_core_override(state, system_id, relative_path);
  }
  return 1;
}

static int set_rom_cpu_override(struct core_override_state *state, const char *system_id,
                                const char *relative_path, const char *cpu_policy,
                                long cpu_freq_khz) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  struct rom_core_override *entry;

  if (!valid_cpu_policy(cpu_policy)) {
    return 0;
  }
  (void)cpu_freq_khz;

  if (idx >= 0) {
    entry = &state->rom_overrides[idx];
  } else {
    if (state->rom_override_count >= MAX_ROM_CORE_OVERRIDES) {
      return 0;
    }
    entry = &state->rom_overrides[state->rom_override_count++];
    memset(entry, 0, sizeof(*entry));
    copy_string(entry->system_id, sizeof(entry->system_id), system_id);
    copy_string(entry->relative_path, sizeof(entry->relative_path), relative_path);
  }
  copy_string(entry->cpu_policy, sizeof(entry->cpu_policy), cpu_policy);
  entry->cpu_freq_khz = 0;
  return 1;
}

static int clear_rom_cpu_override(struct core_override_state *state, const char *system_id,
                                  const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);
  if (idx < 0) {
    return 1;
  }
  state->rom_overrides[idx].cpu_policy[0] = '\0';
  state->rom_overrides[idx].cpu_freq_khz = 0;
  if (rom_core_override_is_empty(&state->rom_overrides[idx])) {
    return clear_rom_core_override(state, system_id, relative_path);
  }
  return 1;
}

static int set_rom_content_suffix_override(struct core_override_state *state,
                                           const char *system_id,
                                           const char *relative_path,
                                           const char *content_suffix) {
  struct rom_core_override *entry;

  if (!valid_content_suffix(content_suffix)) {
    return 0;
  }
  entry = ensure_rom_core_override(state, system_id, relative_path);
  if (!entry) {
    return 0;
  }
  return copy_string(entry->content_suffix, sizeof(entry->content_suffix), content_suffix);
}

static int clear_rom_content_suffix_override(struct core_override_state *state,
                                             const char *system_id,
                                             const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);

  if (idx < 0) {
    return 1;
  }
  state->rom_overrides[idx].content_suffix[0] = '\0';
  if (rom_core_override_is_empty(&state->rom_overrides[idx])) {
    return clear_rom_core_override(state, system_id, relative_path);
  }
  return 1;
}

static int set_rom_audio_driver_override(struct core_override_state *state,
                                         const char *system_id,
                                         const char *relative_path,
                                         const char *audio_driver) {
  struct rom_core_override *entry;

  if (!valid_retroarch_audio_driver(audio_driver)) {
    return 0;
  }
  entry = ensure_rom_core_override(state, system_id, relative_path);
  if (!entry) {
    return 0;
  }
  return copy_string(entry->audio_driver, sizeof(entry->audio_driver), audio_driver);
}

static int set_rom_audio_latency_override(struct core_override_state *state,
                                          const char *system_id,
                                          const char *relative_path,
                                          long audio_latency_ms) {
  struct rom_core_override *entry;

  if (audio_latency_ms <= 0 || audio_latency_ms > 1000) {
    return 0;
  }
  entry = ensure_rom_core_override(state, system_id, relative_path);
  if (!entry) {
    return 0;
  }
  entry->audio_latency_ms = audio_latency_ms;
  return 1;
}

static int clear_rom_audio_override(struct core_override_state *state, const char *system_id,
                                    const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);

  if (idx < 0) {
    return 1;
  }
  state->rom_overrides[idx].audio_driver[0] = '\0';
  state->rom_overrides[idx].audio_latency_ms = 0;
  if (rom_core_override_is_empty(&state->rom_overrides[idx])) {
    return clear_rom_core_override(state, system_id, relative_path);
  }
  return 1;
}

static int set_rom_dosbox_force60fps_override(struct core_override_state *state,
                                              const char *system_id,
                                              const char *relative_path,
                                              const char *force60fps) {
  struct rom_core_override *entry;

  if (!valid_dosbox_bool(force60fps)) {
    return 0;
  }
  entry = ensure_rom_core_override(state, system_id, relative_path);
  if (!entry) {
    return 0;
  }
  return copy_string(entry->dosbox_pure_force60fps,
                     sizeof(entry->dosbox_pure_force60fps), force60fps);
}

static int set_rom_dosbox_cycles_override(struct core_override_state *state,
                                          const char *system_id,
                                          const char *relative_path,
                                          const char *cycles) {
  struct rom_core_override *entry;

  if (!valid_dosbox_cycles(cycles)) {
    return 0;
  }
  entry = ensure_rom_core_override(state, system_id, relative_path);
  if (!entry) {
    return 0;
  }
  return copy_string(entry->dosbox_pure_cycles, sizeof(entry->dosbox_pure_cycles), cycles);
}

static int clear_rom_dosbox_override(struct core_override_state *state, const char *system_id,
                                     const char *relative_path) {
  int idx = find_rom_core_override(state, system_id, relative_path);

  if (idx < 0) {
    return 1;
  }
  state->rom_overrides[idx].dosbox_pure_force60fps[0] = '\0';
  state->rom_overrides[idx].dosbox_pure_cycles[0] = '\0';
  if (rom_core_override_is_empty(&state->rom_overrides[idx])) {
    return clear_rom_core_override(state, system_id, relative_path);
  }
  return 1;
}

static int find_rom_by_relative_path(const struct rom_entry *entries, size_t count,
                                     const char *relative_path) {
  size_t i;
  for (i = 0; i < count; i++) {
    if (strcmp(entries[i].relative_path, relative_path) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int find_rom_by_relative_path_or_dos_suffix(const struct rom_entry *entries,
                                                   size_t count, const char *system_id,
                                                   const char *relative_path,
                                                   char *base_out, size_t base_out_size,
                                                   const char **suffix_out) {
  int idx;

  if (suffix_out) {
    *suffix_out = NULL;
  }
  idx = find_rom_by_relative_path(entries, count, relative_path);
  if (idx >= 0) {
    if (base_out) {
      copy_string(base_out, base_out_size, relative_path);
    }
    return idx;
  }
  if (!system_id || strcmp(system_id, "dos") != 0 ||
      !split_content_suffix(relative_path, base_out, base_out_size, suffix_out) ||
      !suffix_out || !*suffix_out || !valid_relative_rom_path(base_out)) {
    return -1;
  }
  return find_rom_by_relative_path(entries, count, base_out);
}

static int resolve_launch_profile(const struct core_system_def *system,
                                  const struct core_override_state *overrides,
                                  const char *relative_path, char *out, size_t out_size) {
  int idx;

  if (relative_path) {
    idx = find_rom_core_override(overrides, system->id, relative_path);
    if (idx >= 0 &&
        core_profile_is_listed(system, overrides->rom_overrides[idx].launch_profile)) {
      return resolve_device_launch_profile(system, overrides->rom_overrides[idx].launch_profile,
                                           out, out_size);
    }
  }
  idx = find_system_core_override(overrides, system->id);
  if (idx >= 0 &&
      core_profile_is_listed(system, overrides->system_overrides[idx].launch_profile)) {
    return resolve_device_launch_profile(system, overrides->system_overrides[idx].launch_profile,
                                         out, out_size);
  }
  if (core_profile_is_listed(system, system->default_launch_profile)) {
    return resolve_device_launch_profile(system, system->default_launch_profile, out, out_size);
  }
  return copy_string(out, out_size, "auto");
}

static int cpu_setting_is_valid(const char *policy, long freq_khz) {
  (void)freq_khz;
  return valid_cpu_policy(policy);
}

static int copy_cpu_setting(char *policy_out, size_t policy_size, long *freq_out,
                            const char *policy, long freq_khz) {
  if (!cpu_setting_is_valid(policy, freq_khz)) {
    return 0;
  }
  if (!copy_string(policy_out, policy_size, policy)) {
    return 0;
  }
  *freq_out = 0;
  return 1;
}

static int resolve_cpu_setting(const struct core_system_def *system,
                               const struct core_override_state *overrides,
                               const char *relative_path, char *policy_out,
                               size_t policy_size, long *freq_out,
                               const char **source_out) {
  int idx;

  if (!policy_out || policy_size == 0 || !freq_out || !source_out) {
    return 0;
  }
  policy_out[0] = '\0';
  *freq_out = 0;
  *source_out = "launcher default";

  if (relative_path) {
    idx = find_rom_core_override(overrides, system->id, relative_path);
    if (idx >= 0 && overrides->rom_overrides[idx].cpu_policy[0]) {
      if (copy_cpu_setting(policy_out, policy_size, freq_out,
                           overrides->rom_overrides[idx].cpu_policy,
                           overrides->rom_overrides[idx].cpu_freq_khz)) {
        *source_out = "ROM override";
        return 1;
      }
    }
  }
  idx = find_system_core_override(overrides, system->id);
  if (idx >= 0 && overrides->system_overrides[idx].cpu_policy[0]) {
    if (copy_cpu_setting(policy_out, policy_size, freq_out,
                         overrides->system_overrides[idx].cpu_policy,
                         overrides->system_overrides[idx].cpu_freq_khz)) {
      *source_out = "system override";
      return 1;
    }
  }
  if (system->default_cpu_policy[0]) {
    if (copy_cpu_setting(policy_out, policy_size, freq_out, system->default_cpu_policy,
                         system->default_cpu_freq_khz)) {
      *source_out = "plumOS default";
      return 1;
    }
  }
  return 1;
}

static void format_cpu_setting(char *out, size_t out_size, const char *policy, long freq_khz) {
  (void)freq_khz;
  if (!out || out_size == 0) {
    return;
  }
  if (!policy || !policy[0]) {
    copy_string(out, out_size, "launcher default");
  } else if (strcmp(policy, "interactive") == 0) {
    copy_string(out, out_size, "Interactive");
  } else if (strcmp(policy, "performance") == 0) {
    copy_string(out, out_size, "Performance");
  } else if (strcmp(policy, "ondemand") == 0) {
    copy_string(out, out_size, "Ondemand");
  } else if (strcmp(policy, "schedutil") == 0) {
    copy_string(out, out_size, "Schedutil");
  } else if (strcmp(policy, "conservative") == 0) {
    copy_string(out, out_size, "Conservative");
  } else {
    copy_string(out, out_size, policy);
  }
}

static int cpu_setting_matches(const char *policy, long freq_khz, const char *other_policy,
                               long other_freq_khz) {
  (void)freq_khz;
  (void)other_freq_khz;
  if (!policy || !other_policy || strcmp(policy, other_policy) != 0) {
    return 0;
  }
  return 1;
}

static int picoarch_onion_compat_enabled(const char *plumos_root);

static void apply_picoarch_adapter_cpu_defaults(const char *plumos_root,
                                                const char *launch_profile,
                                                char *cpu_policy,
                                                long *cpu_freq_khz,
                                                const char *cpu_source) {
  if (!plumos_root || !launch_profile ||
      strncmp(launch_profile, "picoarch:", 9) != 0 ||
      !picoarch_onion_compat_enabled(plumos_root)) {
    return;
  }
  if (cpu_policy && cpu_freq_khz && cpu_source &&
      strcmp(cpu_source, "plumOS default") == 0) {
    cpu_policy[0] = '\0';
    *cpu_freq_khz = 0;
  }
}

static void resolve_retroarch_runtime_options(const struct core_system_def *system,
                                              const struct core_override_state *overrides,
                                              const char *relative_path,
                                              struct retroarch_runtime_options *out) {
  int idx;

  memset(out, 0, sizeof(*out));
  if (!relative_path) {
    return;
  }
  idx = find_rom_core_override(overrides, system->id, relative_path);
  if (idx < 0) {
    return;
  }
  if (overrides->rom_overrides[idx].audio_driver[0]) {
    copy_string(out->audio_driver, sizeof(out->audio_driver),
                overrides->rom_overrides[idx].audio_driver);
  }
  out->audio_latency_ms = overrides->rom_overrides[idx].audio_latency_ms;
  if (overrides->rom_overrides[idx].dosbox_pure_force60fps[0]) {
    copy_string(out->dosbox_pure_force60fps, sizeof(out->dosbox_pure_force60fps),
                overrides->rom_overrides[idx].dosbox_pure_force60fps);
  }
  if (overrides->rom_overrides[idx].dosbox_pure_cycles[0]) {
    copy_string(out->dosbox_pure_cycles, sizeof(out->dosbox_pure_cycles),
                overrides->rom_overrides[idx].dosbox_pure_cycles);
  }
}

static int apply_content_suffix_override(const struct core_system_def *system,
                                         const struct core_override_state *overrides,
                                         const char *relative_path, struct rom_entry *rom) {
  int idx;
  size_t pos;

  if (!relative_path || strchr(rom->path, '#')) {
    return 1;
  }
  idx = find_rom_core_override(overrides, system->id, relative_path);
  if (idx < 0 || !overrides->rom_overrides[idx].content_suffix[0]) {
    return 1;
  }
  pos = strlen(rom->path);
  return append_string(rom->path, sizeof(rom->path), &pos,
                       overrides->rom_overrides[idx].content_suffix);
}

static void init_favorite_state(struct favorite_state *state) {
  memset(state, 0, sizeof(*state));
}

static int find_favorite(const struct favorite_state *state, const char *system_id,
                         const char *relative_path) {
  size_t i;
  for (i = 0; i < state->count; i++) {
    if (strcmp(state->entries[i].system_id, system_id) == 0 &&
        strcmp(state->entries[i].relative_path, relative_path) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int load_favorites(const char *path, struct favorite_state *state) {
  char *json;
  size_t json_size;
  const char *array_start;
  const char *array_end;
  const char *cursor;

  init_favorite_state(state);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "favorites", &array_start, &array_end)) {
    free(json);
    return 0;
  }

  cursor = array_start;
  while (state->count < MAX_FAVORITES) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct favorite_entry entry;

    if (!json_next_object(&cursor, array_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    json_get_string(obj, obj + strlen(obj), "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj, obj + strlen(obj), "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj, obj + strlen(obj), "title", entry.title, sizeof(entry.title));
    json_get_string(obj, obj + strlen(obj), "file_name", entry.file_name,
                    sizeof(entry.file_name));
    json_get_string(obj, obj + strlen(obj), "path", entry.path, sizeof(entry.path));
    json_get_string(obj, obj + strlen(obj), "thumbnail", entry.thumbnail,
                    sizeof(entry.thumbnail));

    if (valid_system_id(entry.system_id) && valid_relative_rom_path(entry.relative_path) &&
        find_favorite(state, entry.system_id, entry.relative_path) < 0) {
      if (!entry.title[0]) {
        copy_string(entry.title, sizeof(entry.title),
                    entry.file_name[0] ? entry.file_name : entry.relative_path);
      }
      state->entries[state->count++] = entry;
    }
    free(obj);
  }

  free(json);
  return 1;
}

static int save_favorites(const char *path, const struct favorite_state *state) {
  char tmp_path[PATH_MAX];
  FILE *f;
  size_t i;

  if (!ensure_parent_dir(path)) {
    return 0;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"favorites\": [\n");
  for (i = 0; i < state->count; i++) {
    fprintf(f, "    { \"system_id\": ");
    json_write_escaped(f, state->entries[i].system_id);
    fprintf(f, ", \"relative_path\": ");
    json_write_escaped(f, state->entries[i].relative_path);
    fprintf(f, ", \"title\": ");
    json_write_escaped(f, state->entries[i].title);
    fprintf(f, ", \"file_name\": ");
    json_write_escaped(f, state->entries[i].file_name);
    fprintf(f, ", \"path\": ");
    json_write_escaped(f, state->entries[i].path);
    fprintf(f, ", \"thumbnail\": ");
    json_write_escaped(f, state->entries[i].thumbnail);
    fprintf(f, " }%s\n", i + 1 < state->count ? "," : "");
  }
  fprintf(f, "  ]\n");
  fprintf(f, "}\n");

  return commit_temp_file(f, tmp_path, path);
}

static int set_favorite_from_rom(struct favorite_state *state, const char *system_id,
                                 const struct rom_entry *rom) {
  int idx = find_favorite(state, system_id, rom->relative_path);
  struct favorite_entry *entry;

  if (idx >= 0) {
    entry = &state->entries[idx];
  } else {
    if (state->count >= MAX_FAVORITES) {
      return 0;
    }
    entry = &state->entries[state->count++];
    memset(entry, 0, sizeof(*entry));
  }

  copy_string(entry->system_id, sizeof(entry->system_id), system_id);
  copy_string(entry->relative_path, sizeof(entry->relative_path), rom->relative_path);
  copy_string(entry->title, sizeof(entry->title),
              rom->title[0] ? rom->title : rom->relative_path);
  copy_string(entry->file_name, sizeof(entry->file_name), rom->file_name);
  copy_string(entry->path, sizeof(entry->path), rom->path);
  copy_string(entry->thumbnail, sizeof(entry->thumbnail), rom->thumbnail);
  return 1;
}

static int clear_favorite(struct favorite_state *state, const char *system_id,
                          const char *relative_path) {
  int idx = find_favorite(state, system_id, relative_path);
  if (idx < 0) {
    return 1;
  }
  if ((size_t)idx + 1 < state->count) {
    memmove(&state->entries[idx], &state->entries[idx + 1],
            (state->count - (size_t)idx - 1) * sizeof(state->entries[0]));
  }
  state->count--;
  return 1;
}

static void init_recent_state(struct recent_state *state) {
  memset(state, 0, sizeof(*state));
}

static int find_recent(const struct recent_state *state, const char *system_id,
                       const char *relative_path) {
  size_t i;
  for (i = 0; i < state->count; i++) {
    if (strcmp(state->entries[i].system_id, system_id) == 0 &&
        strcmp(state->entries[i].relative_path, relative_path) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int load_recent(const char *path, struct recent_state *state) {
  char *json;
  size_t json_size;
  const char *array_start;
  const char *array_end;
  const char *cursor;

  init_recent_state(state);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (json_size == 0) {
    free(json);
    return 1;
  }
  if (!json_find_array(json, json + json_size, "recents", &array_start, &array_end)) {
    free(json);
    return 0;
  }

  cursor = array_start;
  while (state->count < MAX_RECENTS) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct recent_entry entry;

    if (!json_next_object(&cursor, array_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    json_get_string(obj, obj + strlen(obj), "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj, obj + strlen(obj), "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj, obj + strlen(obj), "title", entry.title, sizeof(entry.title));
    json_get_string(obj, obj + strlen(obj), "file_name", entry.file_name,
                    sizeof(entry.file_name));
    json_get_string(obj, obj + strlen(obj), "path", entry.path, sizeof(entry.path));
    json_get_string(obj, obj + strlen(obj), "thumbnail", entry.thumbnail,
                    sizeof(entry.thumbnail));
    json_get_string(obj, obj + strlen(obj), "launch_profile", entry.launch_profile,
                    sizeof(entry.launch_profile));
    json_get_string(obj, obj + strlen(obj), "last_played_at", entry.last_played_at,
                    sizeof(entry.last_played_at));
    entry.resume_available = json_get_bool(obj, obj + strlen(obj), "resume_available", 0);

    if (valid_system_id(entry.system_id) && valid_relative_rom_path(entry.relative_path) &&
        entry.launch_profile[0] && find_recent(state, entry.system_id, entry.relative_path) < 0) {
      if (!entry.title[0]) {
        copy_string(entry.title, sizeof(entry.title),
                    entry.file_name[0] ? entry.file_name : entry.relative_path);
      }
      state->entries[state->count++] = entry;
    }
    free(obj);
  }

  free(json);
  return 1;
}

static int save_recent(const char *path, const struct recent_state *state) {
  char tmp_path[PATH_MAX];
  FILE *f;
  size_t i;

  if (!ensure_parent_dir(path)) {
    return 0;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"recents\": [\n");
  for (i = 0; i < state->count; i++) {
    fprintf(f, "    { \"system_id\": ");
    json_write_escaped(f, state->entries[i].system_id);
    fprintf(f, ", \"relative_path\": ");
    json_write_escaped(f, state->entries[i].relative_path);
    fprintf(f, ", \"title\": ");
    json_write_escaped(f, state->entries[i].title);
    fprintf(f, ", \"file_name\": ");
    json_write_escaped(f, state->entries[i].file_name);
    fprintf(f, ", \"path\": ");
    json_write_escaped(f, state->entries[i].path);
    fprintf(f, ", \"thumbnail\": ");
    json_write_escaped(f, state->entries[i].thumbnail);
    fprintf(f, ", \"launch_profile\": ");
    json_write_escaped(f, state->entries[i].launch_profile);
    fprintf(f, ", \"last_played_at\": ");
    json_write_escaped(f, state->entries[i].last_played_at);
    fprintf(f, ", \"resume_available\": %s", state->entries[i].resume_available ? "true" : "false");
    fprintf(f, " }%s\n", i + 1 < state->count ? "," : "");
  }
  fprintf(f, "  ]\n");
  fprintf(f, "}\n");

  return commit_temp_file(f, tmp_path, path);
}

static void recent_entry_from_rom(struct recent_entry *entry, const char *system_id,
                                  const struct rom_entry *rom, const char *launch_profile,
                                  const char *played_at, int resume_available) {
  memset(entry, 0, sizeof(*entry));
  copy_string(entry->system_id, sizeof(entry->system_id), system_id);
  copy_string(entry->relative_path, sizeof(entry->relative_path), rom->relative_path);
  copy_string(entry->title, sizeof(entry->title), rom->title[0] ? rom->title : rom->relative_path);
  copy_string(entry->file_name, sizeof(entry->file_name), rom->file_name);
  copy_string(entry->path, sizeof(entry->path), rom->path);
  copy_string(entry->thumbnail, sizeof(entry->thumbnail), rom->thumbnail);
  copy_string(entry->launch_profile, sizeof(entry->launch_profile), launch_profile);
  copy_string(entry->last_played_at, sizeof(entry->last_played_at), played_at);
  entry->resume_available = resume_available;
}

static int add_recent_entry(struct recent_state *state, const struct recent_entry *entry) {
  int idx = find_recent(state, entry->system_id, entry->relative_path);
  if (idx >= 0) {
    if (idx > 0) {
      memmove(&state->entries[1], &state->entries[0], (size_t)idx * sizeof(state->entries[0]));
    }
  } else {
    if (state->count < MAX_RECENTS) {
      state->count++;
    }
    if (state->count > 1) {
      memmove(&state->entries[1], &state->entries[0],
              (state->count - 1) * sizeof(state->entries[0]));
    }
  }
  state->entries[0] = *entry;
  return 1;
}

static void init_resume_session(struct resume_session *session) {
  memset(session, 0, sizeof(*session));
  copy_string(session->reason, sizeof(session->reason), "none");
}

static int load_resume_session(const char *path, struct resume_session *session) {
  char *json;
  size_t json_size;

  init_resume_session(session);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  session->pending = json_get_bool(json, json + json_size, "pending", 0);
  json_get_string(json, json + json_size, "reason", session->reason, sizeof(session->reason));
  json_get_string(json, json + json_size, "system_id", session->system_id,
                  sizeof(session->system_id));
  json_get_string(json, json + json_size, "relative_path", session->relative_path,
                  sizeof(session->relative_path));
  json_get_string(json, json + json_size, "title", session->title, sizeof(session->title));
  json_get_string(json, json + json_size, "file_name", session->file_name,
                  sizeof(session->file_name));
  json_get_string(json, json + json_size, "path", session->path, sizeof(session->path));
  json_get_string(json, json + json_size, "thumbnail", session->thumbnail,
                  sizeof(session->thumbnail));
  json_get_string(json, json + json_size, "launch_profile", session->launch_profile,
                  sizeof(session->launch_profile));
  json_get_string(json, json + json_size, "updated_at", session->updated_at,
                  sizeof(session->updated_at));
  session->auto_state_load = json_get_bool(json, json + json_size, "auto_state_load", 0);
  if (!session->reason[0]) {
    copy_string(session->reason, sizeof(session->reason), session->pending ? "shutdown" : "none");
  }
  free(json);
  return 1;
}

static int save_resume_session(const char *path, const struct resume_session *session) {
  char tmp_path[PATH_MAX];
  FILE *f;

  if (!ensure_parent_dir(path)) {
    return 0;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": 1,\n");
  fprintf(f, "  \"pending\": %s,\n", session->pending ? "true" : "false");
  fprintf(f, "  \"reason\": ");
  json_write_escaped(f, session->reason);
  fprintf(f, ",\n  \"system_id\": ");
  json_write_escaped(f, session->system_id);
  fprintf(f, ",\n  \"relative_path\": ");
  json_write_escaped(f, session->relative_path);
  fprintf(f, ",\n  \"title\": ");
  json_write_escaped(f, session->title);
  fprintf(f, ",\n  \"file_name\": ");
  json_write_escaped(f, session->file_name);
  fprintf(f, ",\n  \"path\": ");
  json_write_escaped(f, session->path);
  fprintf(f, ",\n  \"thumbnail\": ");
  json_write_escaped(f, session->thumbnail);
  fprintf(f, ",\n  \"launch_profile\": ");
  json_write_escaped(f, session->launch_profile);
  fprintf(f, ",\n  \"updated_at\": ");
  json_write_escaped(f, session->updated_at);
  fprintf(f, ",\n  \"auto_state_load\": %s\n", session->auto_state_load ? "true" : "false");
  fprintf(f, "}\n");

  return commit_temp_file(f, tmp_path, path);
}

static void resume_session_from_rom(struct resume_session *session, const char *system_id,
                                    const struct rom_entry *rom, const char *launch_profile,
                                    const char *updated_at, const char *reason, int pending,
                                    int auto_state_load) {
  init_resume_session(session);
  session->pending = pending;
  copy_string(session->reason, sizeof(session->reason), reason && reason[0] ? reason : "shutdown");
  copy_string(session->system_id, sizeof(session->system_id), system_id);
  copy_string(session->relative_path, sizeof(session->relative_path), rom->relative_path);
  copy_string(session->title, sizeof(session->title), rom->title[0] ? rom->title : rom->relative_path);
  copy_string(session->file_name, sizeof(session->file_name), rom->file_name);
  copy_string(session->path, sizeof(session->path), rom->path);
  copy_string(session->thumbnail, sizeof(session->thumbnail), rom->thumbnail);
  copy_string(session->launch_profile, sizeof(session->launch_profile), launch_profile);
  copy_string(session->updated_at, sizeof(session->updated_at), updated_at);
  session->auto_state_load = auto_state_load;
}

static int build_retroarch_core_path(char *out, size_t out_size, const char *plumos_root,
                                     const char *core_id) {
  char file_name[256];
  char cores_dir[PATH_MAX];

  if (snprintf(file_name, sizeof(file_name), "%s_libretro.so", core_id) >=
      (int)sizeof(file_name)) {
    return 0;
  }
  if (!join_path(cores_dir, sizeof(cores_dir), plumos_root, "cores")) {
    return 0;
  }
  return join_path(out, out_size, cores_dir, file_name);
}

static int retroarch_pixel2_compat_runtime_enabled(const char *plumos_root) {
  char flag_path[PATH_MAX];

  if (join_path(flag_path, sizeof(flag_path), plumos_root,
                "config/enable-pixel2_compat-ra-runtime") &&
      file_exists(flag_path)) {
    return 1;
  }
  return join_path(flag_path, sizeof(flag_path), plumos_root,
                   "config/enable-pixel2_compat-onion-compatible-ra") &&
         file_exists(flag_path);
}

static int retroarch_pixel2_compat_launcher_exists(const char *plumos_root) {
  char launcher_path[PATH_MAX];

  if (join_path(launcher_path, sizeof(launcher_path), plumos_root,
                "bin/plumos-retroarch-pixel2_compat-launch") &&
      file_exists(launcher_path)) {
    return 1;
  }
  return join_path(launcher_path, sizeof(launcher_path), plumos_root,
                   "bin/plumos-retroarch-onion-launch") &&
         file_exists(launcher_path);
}

static int picoarch_onion_compat_enabled(const char *plumos_root) {
  char flag_path[PATH_MAX];

  if (join_path(flag_path, sizeof(flag_path), plumos_root,
                "config/enable-pixel2_compat-emulator-runtime") &&
      file_exists(flag_path)) {
    return 1;
  }
  return join_path(flag_path, sizeof(flag_path), plumos_root,
                   "config/enable-pixel2_compat-onion-compatible-emulators") &&
         file_exists(flag_path);
}

static int picoarch_onion_launcher_exists(const char *plumos_root) {
  char adapter_path[PATH_MAX];
  char original_path[PATH_MAX];

  return join_path(adapter_path, sizeof(adapter_path), plumos_root,
                   "bin/plumos-picoarch-onion-launch") &&
         join_path(original_path, sizeof(original_path), plumos_root,
                   "bin/plumos-picoarch-launch.noncompat") &&
         file_exists(adapter_path) && file_exists(original_path);
}

static int build_picoarch_core_path(char *out, size_t out_size, const char *plumos_root,
                                    const char *core_id) {
  char file_name[256];
  char cores_dir[PATH_MAX];
  char pico_path[PATH_MAX];
  char retro_path[PATH_MAX];

  if (snprintf(file_name, sizeof(file_name), "%s_libretro.so", core_id) >=
      (int)sizeof(file_name)) {
    return 0;
  }
  if (!join_path(cores_dir, sizeof(cores_dir), plumos_root, "picoarch/cores") ||
      !join_path(pico_path, sizeof(pico_path), cores_dir, file_name)) {
    return 0;
  }
  if (file_exists(pico_path)) {
    return copy_string(out, out_size, pico_path);
  }
  if (build_retroarch_core_path(retro_path, sizeof(retro_path), plumos_root, core_id) &&
      file_exists(retro_path)) {
    return copy_string(out, out_size, retro_path);
  }
  return copy_string(out, out_size, pico_path);
}

static const char *standalone_content_path_for_emulator(const char *emulator_id,
                                                        const char *rom_path,
                                                        char *scratch,
                                                        size_t scratch_size) {
  char extension[32];

  if (!emulator_id || !rom_path) {
    return rom_path;
  }
  if (strcmp(emulator_id, "easyrpg") != 0) {
    return rom_path;
  }
  if (path_extension_lower(rom_path, extension, sizeof(extension)) &&
      strcmp(extension, "ldb") == 0 &&
      dirname_path(scratch, scratch_size, rom_path)) {
    return scratch;
  }
  return rom_path;
}

static int build_launch_plan(struct launch_plan *plan, const char *plumos_root,
                             const char *system_id, const char *relative_path,
                             const char *title, const char *rom_path,
                             const char *launch_profile, const char *cpu_policy,
                             long cpu_freq_khz,
                             const struct retroarch_runtime_options *retroarch_options,
                             int auto_state_load) {
  size_t pos = 0;
  (void)cpu_freq_khz;

  memset(plan, 0, sizeof(*plan));
  copy_string(plan->system_id, sizeof(plan->system_id), system_id);
  copy_string(plan->relative_path, sizeof(plan->relative_path), relative_path);
  copy_string(plan->title, sizeof(plan->title), title && title[0] ? title : relative_path);
  copy_string(plan->rom_path, sizeof(plan->rom_path), rom_path);
  copy_string(plan->launch_profile, sizeof(plan->launch_profile), launch_profile);
  if (cpu_policy && cpu_policy[0]) {
    copy_string(plan->cpu_policy, sizeof(plan->cpu_policy), cpu_policy);
    plan->cpu_freq_khz = 0;
  }
  if (retroarch_options) {
    plan->retroarch_options = *retroarch_options;
  }
  plan->auto_state_load = auto_state_load;
  plan->rom_exists = rom_path_exists(plan->rom_path);

  if (strncmp(launch_profile, "retroarch:", 10) == 0) {
    const char *core_id = launch_profile + 10;
    char launcher_dir[PATH_MAX];
    int pixel2_compat_runtime = retroarch_pixel2_compat_runtime_enabled(plumos_root);

    copy_string(plan->kind, sizeof(plan->kind), "retroarch");
    if (!join_path(launcher_dir, sizeof(launcher_dir), plumos_root, "bin") ||
        !join_path(plan->retroarch_path, sizeof(plan->retroarch_path), launcher_dir,
                   "plumos-retroarch-launch") ||
        !join_path(plan->config_path, sizeof(plan->config_path), plumos_root,
                   "config/retroarch/retroarch.cfg")) {
      return 0;
    }
    if (!build_retroarch_core_path(plan->core_path, sizeof(plan->core_path),
                                   plumos_root, core_id)) {
      return 0;
    }
    plan->runtime_exists = file_exists(plan->retroarch_path);
    if (pixel2_compat_runtime) {
      plan->runtime_exists = plan->runtime_exists &&
                             retroarch_pixel2_compat_launcher_exists(plumos_root);
    }
    plan->core_exists = file_exists(plan->core_path);

    if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                          plan->retroarch_path) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " --system ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, system_id) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " --core ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, plan->core_path) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " --rom ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, plan->rom_path)) {
      return 0;
    }
    if (plan->cpu_policy[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos, " --cpu ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos, plan->cpu_policy)) {
        return 0;
      }
    }
    if (plan->retroarch_options.audio_driver[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos, " --audio ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->retroarch_options.audio_driver)) {
        return 0;
      }
    }
    if (plan->retroarch_options.audio_latency_ms > 0) {
      char latency_buf[32];
      snprintf(latency_buf, sizeof(latency_buf), "%ld",
               plan->retroarch_options.audio_latency_ms);
      if (!append_string(plan->command, sizeof(plan->command), &pos, " --audio-latency ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos, latency_buf)) {
        return 0;
      }
    }
    if (plan->retroarch_options.dosbox_pure_force60fps[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos,
                         " --dosbox-pure-force60fps ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->retroarch_options.dosbox_pure_force60fps)) {
        return 0;
      }
    }
    if (plan->retroarch_options.dosbox_pure_cycles[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos,
                         " --dosbox-pure-cycles ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->retroarch_options.dosbox_pure_cycles)) {
        return 0;
      }
    }
    if (!append_string(plan->command, sizeof(plan->command), &pos, " --safe-exit false")) {
      return 0;
    }
    plan->can_execute = plan->runtime_exists && plan->core_exists && plan->rom_exists;
    return 1;
  }

  if (strncmp(launch_profile, "pyxel:", 6) == 0) {
    char launcher_dir[PATH_MAX];
    char extension[32];
    const char *pyxel_profile = launch_profile + 6;
    const char *launcher_name = "plumos-pyxel-pixel2-launch";
    const char *pyxel_command = NULL;

    copy_string(plan->kind, sizeof(plan->kind), "pyxel");
    if (pyxel_profile[0] && strcmp(pyxel_profile, "pixel2") != 0) {
      plan->can_execute = 0;
      return 1;
    }
    if (!join_path(launcher_dir, sizeof(launcher_dir), plumos_root, "bin") ||
        !join_path(plan->pyxel_launcher_path, sizeof(plan->pyxel_launcher_path),
                   launcher_dir, launcher_name)) {
      return 0;
    }
    plan->runtime_exists = file_exists(plan->pyxel_launcher_path);
    plan->core_exists = 1;
    if (path_extension_lower(plan->rom_path, extension, sizeof(extension))) {
      if (strcmp(extension, "pyxapp") == 0) {
        pyxel_command = "play";
      } else if (strcmp(extension, "py") == 0) {
        pyxel_command = "run";
      }
    }
    if (!pyxel_command) {
      plan->can_execute = 0;
      return 1;
    }
    if (plan->cpu_policy[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos,
                         "PLUMOS_PYXEL_CPU_POLICY=") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->cpu_policy) ||
          !append_string(plan->command, sizeof(plan->command), &pos, " ")) {
        return 0;
      }
    }
    if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                          plan->pyxel_launcher_path) ||
        !append_string(plan->command, sizeof(plan->command), &pos,
                       " -m pyxel ") ||
        !append_string(plan->command, sizeof(plan->command), &pos,
                       pyxel_command) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                             plan->rom_path)) {
      return 0;
    }
    plan->can_execute = plan->runtime_exists && plan->rom_exists;
    return 1;
  }

  if (strncmp(launch_profile, "picoarch:", 9) == 0) {
    const char *core_id = launch_profile + 9;
    char launcher_dir[PATH_MAX];
    int onion_compat = picoarch_onion_compat_enabled(plumos_root);

    copy_string(plan->kind, sizeof(plan->kind), "picoarch");
    if (!core_id[0] ||
        !join_path(launcher_dir, sizeof(launcher_dir), plumos_root, "bin") ||
        !join_path(plan->picoarch_launcher_path, sizeof(plan->picoarch_launcher_path),
                   launcher_dir, "plumos-picoarch-launch") ||
        !build_picoarch_core_path(plan->core_path, sizeof(plan->core_path), plumos_root,
                                  core_id)) {
      return 0;
    }
    plan->runtime_exists = file_exists(plan->picoarch_launcher_path);
    if (onion_compat) {
      plan->runtime_exists = plan->runtime_exists &&
                             picoarch_onion_launcher_exists(plumos_root);
    }
    plan->core_exists = file_exists(plan->core_path);
    if (plan->cpu_policy[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos,
                         "PLUMOS_PICOARCH_CPU_POLICY=") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->cpu_policy) ||
          !append_string(plan->command, sizeof(plan->command), &pos, " ")) {
        return 0;
      }
    }
    if (!append_string(plan->command, sizeof(plan->command), &pos,
                       "PLUMOS_PICOARCH_SYSTEM=") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, system_id) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ")) {
      return 0;
    }
    if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                          plan->picoarch_launcher_path) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, core_id) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, plan->rom_path)) {
      return 0;
    }
    plan->can_execute = plan->runtime_exists && plan->core_exists && plan->rom_exists;
    return 1;
  }

  if (strncmp(launch_profile, "standalone:", 11) == 0) {
    const char *emulator_id = launch_profile + 11;
    const char *content_path;
    char launcher_dir[PATH_MAX];
    char standalone_content_path[PATH_MAX];

    copy_string(plan->kind, sizeof(plan->kind), "standalone");
    if (!emulator_id[0] ||
        !join_path(launcher_dir, sizeof(launcher_dir), plumos_root, "bin") ||
        !join_path(plan->standalone_launcher_path, sizeof(plan->standalone_launcher_path),
                   launcher_dir, "plumos-standalone-launch")) {
      return 0;
    }
    plan->runtime_exists = file_exists(plan->standalone_launcher_path);
    plan->core_exists = 1;
    if (plan->cpu_policy[0]) {
      if (!append_string(plan->command, sizeof(plan->command), &pos,
                         "PLUMOS_STANDALONE_CPU_POLICY=") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->cpu_policy) ||
          !append_string(plan->command, sizeof(plan->command), &pos, " ")) {
        return 0;
      }
    }
    if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                          plan->standalone_launcher_path) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ") ||
        !append_shell_quoted(plan->command, sizeof(plan->command), &pos, emulator_id) ||
        !append_string(plan->command, sizeof(plan->command), &pos, " ")) {
      return 0;
    }
    content_path = standalone_content_path_for_emulator(
        emulator_id, plan->rom_path, standalone_content_path, sizeof(standalone_content_path));
    if (!append_shell_quoted(plan->command, sizeof(plan->command), &pos, content_path)) {
      return 0;
    }
    plan->can_execute = plan->runtime_exists && plan->rom_exists;
    return 1;
  }

  if (strncmp(launch_profile, "external:", 9) == 0) {
    char portmaster_launcher[PATH_MAX];

    copy_string(plan->kind, sizeof(plan->kind), "external");
    plan->core_exists = 1;
    if (strcmp(launch_profile, "external:port") == 0) {
      if (!join_path(portmaster_launcher, sizeof(portmaster_launcher), plumos_root,
                     "bin/plumos-portmaster-port-launch")) {
        return 0;
      }
      plan->runtime_exists = file_exists(portmaster_launcher);
      if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                            portmaster_launcher) ||
          !append_string(plan->command, sizeof(plan->command), &pos, " ") ||
          !append_shell_quoted(plan->command, sizeof(plan->command), &pos,
                               plan->rom_path)) {
        return 0;
      }
      plan->can_execute = plan->runtime_exists && plan->rom_exists;
      return 1;
    }

    plan->runtime_exists = 1;
    if (!append_runtime_script_invocation(plan->command, sizeof(plan->command), &pos,
                                          plan->rom_path)) {
      return 0;
    }
    plan->can_execute = plan->rom_exists;
    return 1;
  }

  if (strncmp(launch_profile, "internal:", 9) == 0) {
    copy_string(plan->kind, sizeof(plan->kind), "internal");
  } else {
    copy_string(plan->kind, sizeof(plan->kind), "unknown");
  }
  plan->can_execute = 0;
  return 1;
}

static void print_launch_plan(const struct launch_plan *plan) {
  printf("plumOS text UI - launch plan\n");
  printf("kind: %s\n", plan->kind);
  printf("system: %s\n", plan->system_id);
  printf("rom: %s\n", plan->relative_path);
  printf("title: %s\n", plan->title);
  printf("path: %s\n", plan->rom_path);
  printf("launch_profile: %s\n", plan->launch_profile);
  if (plan->cpu_policy[0]) {
    printf("cpu: %s\n", plan->cpu_policy);
  } else {
    printf("cpu: launcher default\n");
  }
  printf("cpu_cores: all online\n");
  if (plan->retroarch_options.audio_driver[0]) {
    printf("retroarch_audio: %s\n", plan->retroarch_options.audio_driver);
  }
  if (plan->retroarch_options.audio_latency_ms > 0) {
    printf("retroarch_audio_latency: %ld\n", plan->retroarch_options.audio_latency_ms);
  }
  if (plan->retroarch_options.dosbox_pure_force60fps[0]) {
    printf("dosbox_pure_force60fps: %s\n",
           plan->retroarch_options.dosbox_pure_force60fps);
  }
  if (plan->retroarch_options.dosbox_pure_cycles[0]) {
    printf("dosbox_pure_cycles: %s\n", plan->retroarch_options.dosbox_pure_cycles);
  }
  if (plan->retroarch_path[0]) {
    printf("retroarch: %s (%s)\n", plan->retroarch_path,
           plan->runtime_exists ? "exists" : "missing");
  }
  if (plan->standalone_launcher_path[0]) {
    printf("standalone_launcher: %s (%s)\n", plan->standalone_launcher_path,
           plan->runtime_exists ? "exists" : "missing");
  }
  if (plan->pyxel_launcher_path[0]) {
    printf("pyxel_launcher: %s (%s)\n", plan->pyxel_launcher_path,
           plan->runtime_exists ? "exists" : "missing");
  }
  if (plan->picoarch_launcher_path[0]) {
    printf("picoarch_launcher: %s (%s)\n", plan->picoarch_launcher_path,
           plan->runtime_exists ? "exists" : "missing");
  }
  if (plan->core_path[0]) {
    printf("core: %s (%s)\n", plan->core_path, plan->core_exists ? "exists" : "missing");
  }
  printf("rom_exists: %s\n", plan->rom_exists ? "yes" : "no");
  printf("can_execute: %s\n", plan->can_execute ? "yes" : "no");
  if (plan->command[0]) {
    printf("command: %s\n", plan->command);
  }
}

static int execute_launch_plan(const struct launch_plan *plan, const char *plumos_root) {
  int rc;
  pid_t hotkeyd_pid;

  if (!plan->can_execute || !plan->command[0]) {
    fprintf(stderr, "error: launch plan is not executable\n");
    return 0;
  }
  hotkeyd_pid = start_safe_hotkeyd(plumos_root, plan);
  rc = run_runtime_shell_command(plan->command);
  stop_safe_hotkeyd(hotkeyd_pid);
  persist_runtime_volume(plumos_root);
  if (rc == -1) {
    fprintf(stderr, "error: failed to execute launch command\n");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 1;
  }
  fprintf(stderr, "error: launch command failed with status %d\n", rc);
  return 0;
}

static int append_favorites_top_entry(struct top_entry *entries, size_t max_entries,
                                      size_t *count, const struct favorite_state *favorites) {
  struct top_entry entry;

  if (*count >= max_entries) {
    return 0;
  }
  memset(&entry, 0, sizeof(entry));
  copy_string(entry.id, sizeof(entry.id), "favorites");
  copy_string(entry.display_name, sizeof(entry.display_name), "Favorites");
  copy_string(entry.short_name, sizeof(entry.short_name), "Fav");
  copy_string(entry.default_launch_profile, sizeof(entry.default_launch_profile),
              "internal:favorites");
  entry.rom_count = (long)favorites->count;
  entry.thumbnail_count = 0;
  entry.pinned = 1;
  entry.virtual_entry = 1;
  entries[(*count)++] = entry;
  return 1;
}

static int load_top_entries(const char *path, struct top_entry *entries, size_t max_entries,
                            size_t *count_out, long *ready_ms_out) {
  char *json;
  size_t json_size;
  const char *systems_start;
  const char *systems_end;
  const char *cursor;
  size_t count = 0;

  *count_out = 0;
  *ready_ms_out = -1;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  *ready_ms_out = json_get_long(json, json + json_size, "ready_ms", -1);
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    free(json);
    return 0;
  }

  cursor = systems_start;
  while (count < max_entries) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct top_entry entry;

    if (!json_next_object(&cursor, systems_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    json_get_string(obj, obj + strlen(obj), "id", entry.id, sizeof(entry.id));
    json_get_string(obj, obj + strlen(obj), "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj, obj + strlen(obj), "short_name", entry.short_name,
                    sizeof(entry.short_name));
    json_get_string(obj, obj + strlen(obj), "default_launch_profile",
                    entry.default_launch_profile, sizeof(entry.default_launch_profile));
    entry.rom_count = json_get_long(obj, obj + strlen(obj), "rom_count", 0);
    entry.thumbnail_count = json_get_long(obj, obj + strlen(obj), "thumbnail_count", 0);
    entry.pinned = json_get_bool(obj, obj + strlen(obj), "pinned", 0);

    if (entry.id[0]) {
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      entries[count++] = entry;
    }
    free(obj);
  }

  *count_out = count;
  free(json);
  return 1;
}

static int load_rom_entries(const char *path, const char *system_id,
                            struct rom_entry **entries_out, size_t *count_out,
                            long *ready_ms_out) {
  char *json;
  size_t json_size;
  const char *systems_start;
  const char *systems_end;
  const char *system_cursor;
  struct rom_entry *entries = NULL;
  size_t count = 0;
  size_t capacity = 0;

  *entries_out = NULL;
  *count_out = 0;
  *ready_ms_out = -1;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  *ready_ms_out = json_get_long(json, json + json_size, "ready_ms", -1);
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    free(json);
    return 0;
  }

  system_cursor = systems_start;
  while (1) {
    const char *system_start;
    const char *system_end;
    char *system_obj;
    char id[64];
    const char *roms_start;
    const char *roms_end;
    const char *rom_cursor;

    if (!json_next_object(&system_cursor, systems_end, &system_start, &system_end)) {
      break;
    }
    system_obj = range_dup(system_start, system_end);
    if (!system_obj) {
      free(json);
      free(entries);
      return 0;
    }
    json_get_string(system_obj, system_obj + strlen(system_obj), "id", id, sizeof(id));
    if (strcmp(id, system_id) != 0) {
      free(system_obj);
      continue;
    }

    if (!json_find_array(system_obj, system_obj + strlen(system_obj), "roms", &roms_start,
                         &roms_end)) {
      free(system_obj);
      free(json);
      free(entries);
      return 0;
    }

    rom_cursor = roms_start;
    while (1) {
      const char *rom_start;
      const char *rom_end;
      const char *media_start;
      const char *media_end;
      char *rom_obj;
      struct rom_entry entry;

      if (!json_next_object(&rom_cursor, roms_end, &rom_start, &rom_end)) {
        break;
      }
      rom_obj = range_dup(rom_start, rom_end);
      if (!rom_obj) {
        free(system_obj);
        free(json);
        free(entries);
        return 0;
      }

      memset(&entry, 0, sizeof(entry));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "id", entry.id, sizeof(entry.id));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "title", entry.title,
                      sizeof(entry.title));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "file_name", entry.file_name,
                      sizeof(entry.file_name));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "relative_path", entry.relative_path,
                      sizeof(entry.relative_path));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "path", entry.path, sizeof(entry.path));
      if (json_find_object(rom_obj, rom_obj + strlen(rom_obj), "media", &media_start,
                           &media_end)) {
        char *media_obj = range_dup(media_start, media_end);
        if (media_obj) {
          json_get_string(media_obj, media_obj + strlen(media_obj), "thumbnail", entry.thumbnail,
                          sizeof(entry.thumbnail));
          free(media_obj);
        }
      }
      if (!entry.title[0]) {
        copy_string(entry.title, sizeof(entry.title),
                    entry.file_name[0] ? entry.file_name : entry.relative_path);
      }
      if (!append_rom_entry(&entries, &count, &capacity, &entry)) {
        free(rom_obj);
        free(system_obj);
        free(json);
        free(entries);
        return 0;
      }
      free(rom_obj);
    }

    free(system_obj);
    break;
  }

  *entries_out = entries;
  *count_out = count;
  free(json);
  return 1;
}

static int load_selected_rom(const char *plumos_root, const char *sdcard_root,
                             const char *system_id, const char *relative_path, int scan,
                             struct rom_entry *rom_out) {
  char system_cache_path[PATH_MAX];
  struct rom_entry *entries = NULL;
  size_t count = 0;
  long ready_ms = -1;
  int rom_idx;
  char base_relative_path[TEXT_PATH_MAX];
  const char *content_suffix = NULL;
  int suffix_match = 0;

  if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path), plumos_root,
                               system_id)) {
    fprintf(stderr, "error: system cache path is too long\n");
    return 0;
  }
  if ((scan || !file_exists(system_cache_path)) &&
      !run_scanner(plumos_root, sdcard_root, system_id, 0)) {
    return 0;
  }
  if (!load_rom_entries(system_cache_path, system_id, &entries, &count, &ready_ms)) {
    fprintf(stderr, "error: cannot read ROM cache: %s\n", system_cache_path);
    free(entries);
    return 0;
  }
  (void)ready_ms;
  rom_idx = find_rom_by_relative_path_or_dos_suffix(entries, count, system_id, relative_path,
                                                   base_relative_path,
                                                   sizeof(base_relative_path),
                                                   &content_suffix);
  if (rom_idx < 0) {
    fprintf(stderr, "error: ROM is not in scan cache for %s: %s\n", system_id, relative_path);
    free(entries);
    return 0;
  }
  *rom_out = entries[rom_idx];
  suffix_match = content_suffix && content_suffix[0];
  if (suffix_match) {
    size_t pos;
    copy_string(rom_out->relative_path, sizeof(rom_out->relative_path), relative_path);
    pos = strlen(rom_out->path);
    if (!append_string(rom_out->path, sizeof(rom_out->path), &pos, content_suffix)) {
      fprintf(stderr, "error: DOSBox-Pure content suffix is too long: %s\n", relative_path);
      free(entries);
      return 0;
    }
  }
  free(entries);
  return 1;
}

static int load_menu_entries(const char *path, const char *menu_id, struct menu_entry *entries,
                             size_t max_entries, size_t *count_out) {
  char *json;
  size_t json_size;
  const char *menus_start;
  const char *menus_end;
  const char *menu_cursor;
  size_t count = 0;

  *count_out = 0;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "menus", &menus_start, &menus_end)) {
    free(json);
    return 0;
  }

  menu_cursor = menus_start;
  while (1) {
    const char *menu_start;
    const char *menu_end;
    const char *entries_start;
    const char *entries_end;
    const char *entry_cursor;
    char *menu_obj;
    char id[64];

    if (!json_next_object(&menu_cursor, menus_end, &menu_start, &menu_end)) {
      break;
    }
    menu_obj = range_dup(menu_start, menu_end);
    if (!menu_obj) {
      free(json);
      return 0;
    }
    json_get_string(menu_obj, menu_obj + strlen(menu_obj), "id", id, sizeof(id));
    if (strcmp(id, menu_id) != 0) {
      free(menu_obj);
      continue;
    }

    if (!json_find_array(menu_obj, menu_obj + strlen(menu_obj), "entries", &entries_start,
                         &entries_end)) {
      free(menu_obj);
      free(json);
      return 0;
    }

    entry_cursor = entries_start;
    while (count < max_entries) {
      const char *entry_start;
      const char *entry_end;
      char *entry_obj;
      struct menu_entry entry;

      if (!json_next_object(&entry_cursor, entries_end, &entry_start, &entry_end)) {
        break;
      }
      entry_obj = range_dup(entry_start, entry_end);
      if (!entry_obj) {
        free(menu_obj);
        free(json);
        return 0;
      }

      memset(&entry, 0, sizeof(entry));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "id", entry.id,
                      sizeof(entry.id));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "display_name",
                      entry.display_name, sizeof(entry.display_name));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "kind", entry.kind,
                      sizeof(entry.kind));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "action", entry.action,
                      sizeof(entry.action));
      entry.confirm = json_get_bool(entry_obj, entry_obj + strlen(entry_obj), "confirm", 0);
      if (entry.id[0]) {
        if (!entry.display_name[0]) {
          copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
        }
        entries[count++] = entry;
      }
      free(entry_obj);
    }

    free(menu_obj);
    break;
  }

  *count_out = count;
  free(json);
  return count > 0;
}

static int load_app_entries(const char *path, const char *menu_id, struct app_entry *entries,
                            size_t max_entries, size_t *count_out) {
  char *json;
  size_t json_size;
  const char *apps_start;
  const char *apps_end;
  const char *app_cursor;
  size_t count = 0;

  *count_out = 0;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "apps", &apps_start, &apps_end)) {
    free(json);
    return 0;
  }

  app_cursor = apps_start;
  while (count < max_entries) {
    const char *app_start;
    const char *app_end;
    char *app_obj;
    struct app_entry entry;

    if (!json_next_object(&app_cursor, apps_end, &app_start, &app_end)) {
      break;
    }
    app_obj = range_dup(app_start, app_end);
    if (!app_obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    entry.visible = json_get_bool(app_obj, app_obj + strlen(app_obj), "visible", 1);
    json_get_string(app_obj, app_obj + strlen(app_obj), "id", entry.id, sizeof(entry.id));
    json_get_string(app_obj, app_obj + strlen(app_obj), "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(app_obj, app_obj + strlen(app_obj), "kind", entry.kind, sizeof(entry.kind));
    json_get_string(app_obj, app_obj + strlen(app_obj), "launch_profile", entry.launch_profile,
                    sizeof(entry.launch_profile));
    json_get_string(app_obj, app_obj + strlen(app_obj), "menu", entry.menu, sizeof(entry.menu));

    if (entry.visible && entry.id[0] && strcmp(entry.menu, menu_id) == 0) {
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      entries[count++] = entry;
    }
    free(app_obj);
  }

  *count_out = count;
  free(json);
  return 1;
}

static void print_top(const struct top_entry *entries, size_t count, int show_all, size_t limit,
                      const char *cache_path, long ready_ms) {
  size_t i;
  size_t shown = 0;
  size_t available = 0;

  printf("plumOS text UI - TOP\n");
  printf("cache: %s\n", cache_path);
  if (ready_ms >= 0) {
    printf("cache_ready_ms: %ld\n", ready_ms);
  }
  printf("\n");
  printf("%-4s %-3s %-18s  %s\n", "No.", "V", "System", "Default profile");
  printf("%-4s %-3s %-18s  %s\n", "---", "-", "------", "---------------");

  for (i = 0; i < count; i++) {
    if (!show_all && entries[i].rom_count <= 0 && !entries[i].pinned) {
      continue;
    }
    available++;
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-3s %-18s  %s\n", shown, entries[i].virtual_entry ? "*" : "",
           entries[i].display_name, entries[i].default_launch_profile);
  }
  if (available == 0) {
    printf("(ROMがあるsystemはまだありません。full scan cacheを確認してください。)\n");
  } else if (limit > 0 && available > shown) {
    printf("... %zu more\n", available - shown);
  }
}

static void print_roms(const char *system_id, const struct rom_entry *entries, size_t count,
                       const struct favorite_state *favorites, size_t limit,
                       const char *cache_path, long ready_ms) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - ROM list\n");
  printf("system: %s\n", system_id);
  printf("roms: %zu\n", count);
  printf("cache: %s\n", cache_path);
  if (ready_ms >= 0) {
    printf("ready_ms: %ld\n", ready_ms);
  }
  printf("\n");
  printf("%-4s %-3s %-34s %s\n", "No.", "Fav", "Title", "Path");
  printf("%-4s %-3s %-34s %s\n", "---", "---", "-----", "----");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-3s %-34s %s\n", shown,
           favorites && find_favorite(favorites, system_id, entries[i].relative_path) >= 0 ? "*"
                                                                                          : "",
           entries[i].title, entries[i].relative_path);
  }
  if (count == 0) {
    printf("(このsystemには表示できるROMがありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void print_favorites(const struct favorite_state *favorites, size_t limit,
                            const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - Favorites\n");
  printf("source: %s\n", path);
  printf("count: %zu\n", favorites->count);
  printf("\n");
  printf("%-4s %-12s %-34s %s\n", "No.", "System", "Title", "Path");
  printf("%-4s %-12s %-34s %s\n", "---", "------", "-----", "----");

  for (i = 0; i < favorites->count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-12s %-34s %s\n", shown, favorites->entries[i].system_id,
           favorites->entries[i].title, favorites->entries[i].relative_path);
  }
  if (favorites->count == 0) {
    printf("(favorite はまだありません。)\n");
  } else if (limit > 0 && favorites->count > shown) {
    printf("... %zu more\n", favorites->count - shown);
  }
}

static void print_favorite_result(const char *action, const char *system_id,
                                  const struct rom_entry *rom,
                                  const struct favorite_state *favorites,
                                  const char *path) {
  int idx = find_favorite(favorites, system_id, rom->relative_path);

  printf("plumOS text UI - favorite\n");
  printf("action: %s\n", action);
  printf("system: %s\n", system_id);
  printf("rom: %s\n", rom->relative_path);
  printf("title: %s\n", rom->title);
  printf("source: %s\n", path);
  printf("favorite: %s\n", idx >= 0 ? "yes" : "no");
  printf("count: %zu\n", favorites->count);
}

static void print_recent(const struct recent_state *recent, size_t limit, const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - Recent\n");
  printf("source: %s\n", path);
  printf("count: %zu\n", recent->count);
  printf("\n");
  printf("%-4s %-12s %-4s %-34s %-22s %s\n", "No.", "System", "Resume", "Title",
         "Last played", "Launch profile");
  printf("%-4s %-12s %-4s %-34s %-22s %s\n", "---", "------", "------", "-----",
         "-----------", "--------------");

  for (i = 0; i < recent->count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-12s %-6s %-34s %-22s %s\n", shown, recent->entries[i].system_id,
           recent->entries[i].resume_available ? "yes" : "no", recent->entries[i].title,
           recent->entries[i].last_played_at, recent->entries[i].launch_profile);
  }
  if (recent->count == 0) {
    printf("(recent entry はまだありません。)\n");
  } else if (limit > 0 && recent->count > shown) {
    printf("... %zu more\n", recent->count - shown);
  }
}

static void print_resume_session(const struct resume_session *session, const char *path) {
  printf("plumOS text UI - resume session\n");
  printf("source: %s\n", path);
  printf("pending: %s\n", session->pending ? "yes" : "no");
  printf("reason: %s\n", session->reason);
  printf("system: %s\n", session->system_id);
  printf("rom: %s\n", session->relative_path);
  printf("title: %s\n", session->title);
  printf("launch_profile: %s\n", session->launch_profile);
  printf("updated_at: %s\n", session->updated_at);
  printf("legacy_auto_state_load: %s\n", session->auto_state_load ? "yes" : "no");
}

static void print_resume_result(const char *action, const struct resume_session *session,
                                const char *path) {
  printf("plumOS text UI - resume\n");
  printf("action: %s\n", action);
  printf("source: %s\n", path);
  printf("pending: %s\n", session->pending ? "yes" : "no");
  printf("system: %s\n", session->system_id);
  printf("rom: %s\n", session->relative_path);
  printf("title: %s\n", session->title);
  printf("launch_profile: %s\n", session->launch_profile);
  printf("updated_at: %s\n", session->updated_at);
}

static void print_boot_resume(const struct frontend_settings *settings,
                              const struct resume_session *session,
                              const struct recent_state *recent, size_t limit,
                              const char *resume_path, const char *recent_path) {
  printf("plumOS text UI - boot startup\n");
  printf("mode: %s\n", settings->boot_resume_mode);
  printf("legacy_resume: %s\n", resume_path);
  printf("recent: %s\n", recent_path);
  printf("\n");

  if (strcmp(settings->boot_resume_mode, "off") == 0) {
    printf("decision: show TOP\n");
    return;
  }
  if (strcmp(settings->boot_resume_mode, "on") == 0) {
    if (recent->count > 0 && recent->entries[0].system_id[0] &&
        recent->entries[0].relative_path[0]) {
      printf("decision: launch last ROM\n");
      printf("system: %s\n", recent->entries[0].system_id);
      printf("rom: %s\n", recent->entries[0].relative_path);
      printf("title: %s\n", recent->entries[0].title);
      printf("launch_profile: %s\n", recent->entries[0].launch_profile);
    } else {
      printf("decision: show TOP\n");
      printf("reason: no recent ROM\n");
    }
    return;
  }

  printf("decision: show Recent\n");
  if (session->pending && session->system_id[0] && session->relative_path[0]) {
    printf("\n");
    printf("Legacy pending resume ignored:\n");
    printf("  %s / %s / %s / %s\n", session->system_id, session->title,
           session->relative_path, session->launch_profile);
  }
  printf("\n");
  print_recent(recent, limit, recent_path);
}

static void print_menu(const char *menu_id, const struct menu_entry *entries, size_t count,
                       size_t limit, const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - menu\n");
  printf("menu: %s\n", menu_id);
  printf("source: %s\n", path);
  printf("\n");
  printf("%-4s %-24s %-10s %-24s %s\n", "No.", "Entry", "Kind", "Action", "Confirm");
  printf("%-4s %-24s %-10s %-24s %s\n", "---", "-----", "----", "------", "-------");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-24s %-10s %-24s %s\n", shown, entries[i].display_name,
           entries[i].kind[0] ? entries[i].kind : "-", entries[i].action,
           entries[i].confirm ? "yes" : "no");
  }
  if (count == 0) {
    printf("(menu entry はありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void print_apps_menu(const char *menu_id, const struct app_entry *entries, size_t count,
                            size_t limit, const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - apps menu\n");
  printf("menu: %s\n", menu_id);
  printf("source: %s\n", path);
  printf("\n");
  printf("%-4s %-24s %-10s %s\n", "No.", "App", "Kind", "Launch profile");
  printf("%-4s %-24s %-10s %s\n", "---", "---", "----", "--------------");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-24s %-10s %s\n", shown, entries[i].display_name,
           entries[i].kind[0] ? entries[i].kind : "-", entries[i].launch_profile);
  }
  if (count == 0) {
    printf("(app/tool entry はありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void print_core_selection(const char *scope, const struct core_system_def *system,
                                 const char *rom_relative_path,
                                 const struct core_override_state *state,
                                 const char *systems_path, const char *overrides_path) {
  int system_idx;
  int rom_idx = -1;
  const char *system_profile = NULL;
  const char *rom_profile = NULL;
  const char *effective_profile = NULL;
  const char *effective_source = "auto detect";
  char effective_profile_buf[128];
  char effective_cpu_policy[32];
  long effective_cpu_freq_khz = 0;
  const char *effective_cpu_source = "launcher default";
  char effective_cpu_label[64];
  const char *system_cpu_policy = NULL;
  long system_cpu_freq_khz = 0;
  const char *rom_cpu_policy = NULL;
  long rom_cpu_freq_khz = 0;
  const char *rom_content_suffix = NULL;
  const char *rom_audio_driver = NULL;
  long rom_audio_latency_ms = 0;
  const char *rom_dosbox_force60fps = NULL;
  const char *rom_dosbox_cycles = NULL;
  char rom_audio_latency_label[32];
  size_t i;

  system_idx = find_system_core_override(state, system->id);
  if (system_idx >= 0) {
    system_profile = state->system_overrides[system_idx].launch_profile;
    if (state->system_overrides[system_idx].cpu_policy[0]) {
      system_cpu_policy = state->system_overrides[system_idx].cpu_policy;
      system_cpu_freq_khz = state->system_overrides[system_idx].cpu_freq_khz;
    }
  }
  if (rom_relative_path) {
    rom_idx = find_rom_core_override(state, system->id, rom_relative_path);
    if (rom_idx >= 0) {
      rom_profile = state->rom_overrides[rom_idx].launch_profile;
      if (state->rom_overrides[rom_idx].cpu_policy[0]) {
        rom_cpu_policy = state->rom_overrides[rom_idx].cpu_policy;
        rom_cpu_freq_khz = state->rom_overrides[rom_idx].cpu_freq_khz;
      }
      if (state->rom_overrides[rom_idx].content_suffix[0]) {
        rom_content_suffix = state->rom_overrides[rom_idx].content_suffix;
      }
      if (state->rom_overrides[rom_idx].audio_driver[0]) {
        rom_audio_driver = state->rom_overrides[rom_idx].audio_driver;
      }
      rom_audio_latency_ms = state->rom_overrides[rom_idx].audio_latency_ms;
      if (state->rom_overrides[rom_idx].dosbox_pure_force60fps[0]) {
        rom_dosbox_force60fps = state->rom_overrides[rom_idx].dosbox_pure_force60fps;
      }
      if (state->rom_overrides[rom_idx].dosbox_pure_cycles[0]) {
        rom_dosbox_cycles = state->rom_overrides[rom_idx].dosbox_pure_cycles;
      }
    }
  }

  if (core_profile_is_listed(system, rom_profile) &&
      resolve_device_launch_profile(system, rom_profile, effective_profile_buf,
                                    sizeof(effective_profile_buf))) {
    effective_profile = effective_profile_buf;
    effective_source = "ROM override";
  } else if (core_profile_is_listed(system, system_profile) &&
             resolve_device_launch_profile(system, system_profile, effective_profile_buf,
                                           sizeof(effective_profile_buf))) {
    effective_profile = effective_profile_buf;
    effective_source = "system override";
  } else if (core_profile_is_listed(system, system->default_launch_profile) &&
             resolve_device_launch_profile(system, system->default_launch_profile,
                                           effective_profile_buf,
                                           sizeof(effective_profile_buf))) {
    effective_profile = effective_profile_buf;
    effective_source = "plumOS default";
  } else {
    effective_profile = "auto";
  }
  resolve_cpu_setting(system, state, rom_relative_path, effective_cpu_policy,
                      sizeof(effective_cpu_policy), &effective_cpu_freq_khz,
                      &effective_cpu_source);
  format_cpu_setting(effective_cpu_label, sizeof(effective_cpu_label), effective_cpu_policy,
                     effective_cpu_freq_khz);

  printf("plumOS text UI - core selection\n");
  printf("scope: %s\n", scope);
  printf("system: %s (%s)\n", system->id, system->display_name);
  if (rom_relative_path) {
    printf("rom: %s\n", rom_relative_path);
  }
  printf("systems: %s\n", systems_path);
  printf("overrides: %s\n", overrides_path);
  printf("current_profile: %s (%s)\n", effective_profile, effective_source);
  printf("current_cpu: %s (%s)\n", effective_cpu_label, effective_cpu_source);
  if (rom_relative_path) {
    if (rom_audio_latency_ms > 0) {
      snprintf(rom_audio_latency_label, sizeof(rom_audio_latency_label), "%ld",
               rom_audio_latency_ms);
    } else {
      copy_string(rom_audio_latency_label, sizeof(rom_audio_latency_label), "-");
    }
    printf("rom_content_suffix: %s\n", rom_content_suffix ? rom_content_suffix : "-");
    printf("rom_audio: driver=%s latency=%s\n", rom_audio_driver ? rom_audio_driver : "-",
           rom_audio_latency_label);
    printf("rom_dosbox_pure: force60fps=%s cycles=%s\n",
           rom_dosbox_force60fps ? rom_dosbox_force60fps : "-",
           rom_dosbox_cycles ? rom_dosbox_cycles : "-");
  }
  printf("\n");
  printf("Launch profiles\n");
  printf("%-4s %-30s %-24s %-8s %-8s %-8s %s\n", "No.", "Launch profile",
         "Display", "Default", "System", "ROM", "Effective");
  printf("%-4s %-30s %-24s %-8s %-8s %-8s %s\n", "---", "--------------",
         "-------", "-------", "------", "---", "---------");

  for (i = 0; i < system->launch_profile_count; i++) {
    const char *profile = system->launch_profiles[i];
    char display[128];
    format_launch_profile_display(profile, display, sizeof(display));
    printf("%3zu. %-30s %-24s %-8s %-8s %-8s %s\n", i + 1, profile, display,
           strcmp(profile, system->default_launch_profile) == 0 ? "yes" : "no",
           system_profile && strcmp(profile, system_profile) == 0 ? "yes" : "no",
           rom_profile && strcmp(profile, rom_profile) == 0 ? "yes" : (rom_relative_path ? "no" : "-"),
           strcmp(profile, effective_profile) == 0 ? "*" : "");
  }
  if (system->launch_profile_count == 0) {
    printf("(このsystemには launch_profiles がありません。launcher 側で auto detect します。)\n");
  }

  printf("\n");
  printf("CPU governor presets\n");
  printf("%-4s %-22s %-12s %-8s %-8s %-8s %s\n", "No.", "Preset", "Value", "Default",
         "System", "ROM", "Effective");
  printf("%-4s %-22s %-12s %-8s %-8s %-8s %s\n", "---", "------", "-----", "-------",
         "------", "---", "---------");
  for (i = 0; i < CPU_PRESET_COUNT; i++) {
    const struct cpu_preset *preset = &CPU_PRESETS[i];
    char value[64];
    format_cpu_setting(value, sizeof(value), preset->policy, preset->freq_khz);
    printf("%3zu. %-22s %-12s %-8s %-8s %-8s %s\n", i + 1, preset->label, value,
           cpu_setting_matches(system->default_cpu_policy, system->default_cpu_freq_khz,
                               preset->policy, preset->freq_khz)
               ? "yes"
               : "no",
           system_cpu_policy && cpu_setting_matches(system_cpu_policy, system_cpu_freq_khz,
                                                    preset->policy, preset->freq_khz)
               ? "yes"
               : "no",
           rom_cpu_policy && cpu_setting_matches(rom_cpu_policy, rom_cpu_freq_khz,
                                                 preset->policy, preset->freq_khz)
               ? "yes"
               : (rom_relative_path ? "no" : "-"),
           effective_cpu_policy[0] && cpu_setting_matches(effective_cpu_policy,
                                                          effective_cpu_freq_khz,
                                                          preset->policy, preset->freq_khz)
               ? "*"
               : "");
  }
}

static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s top [--all] [--refresh] [--limit N]\n", argv0);
  printf("  %s roms SYSTEM [--no-scan] [--limit N]\n", argv0);
  printf("  %s menu start|apps [--limit N]\n", argv0);
  printf("  %s core system SYSTEM [--set PROFILE] [--cpu interactive|performance|ondemand|schedutil|conservative] [--clear-profile|--clear|--clear-cpu]\n",
         argv0);
  printf("  %s core rom SYSTEM RELATIVE_PATH [--set PROFILE] [--cpu interactive|performance|ondemand|schedutil|conservative] [--content-suffix '#EXE'] [--audio oss|alsa|alsathread|pixel2_compat_ao] [--latency MS] [--dosbox-force60fps true|false] [--dosbox-cycles auto|max|N] [--clear-profile|--clear|--clear-cpu|--clear-content-suffix|--clear-audio|--clear-dosbox] [--no-scan]\n",
         argv0);
  printf("  %s favorites [--limit N]\n", argv0);
  printf("  %s favorite rom SYSTEM RELATIVE_PATH [--toggle|--set|--clear] [--no-scan]\n",
         argv0);
  printf("  %s recent [--limit N]\n", argv0);
  printf("  %s recent add SYSTEM RELATIVE_PATH [--profile PROFILE] [--resume yes|no] [--no-scan]\n",
         argv0);
  printf("  %s resume show|clear\n", argv0);
  printf("  %s resume set SYSTEM RELATIVE_PATH [--profile PROFILE] [--reason REASON] [--no-scan]\n",
         argv0);
  printf("  %s launch SYSTEM RELATIVE_PATH [--profile PROFILE] [--execute] [--no-scan]\n",
         argv0);
  printf("  %s boot [--limit N] [--execute]\n", argv0);
  printf("\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_TEXT_LIMIT   Default: 50 for ROM lists, unlimited for TOP\n");
}

int main(int argc, char **argv) {
  const char *sdcard_root;
  const char *plumos_root_env;
  char plumos_root[PATH_MAX];
  char full_cache_path[PATH_MAX];
  char system_cache_path[PATH_MAX];
  char systems_path[PATH_MAX];
  char settings_path[PATH_MAX];
  char menus_path[PATH_MAX];
  char apps_path[PATH_MAX];
  char core_overrides_path[PATH_MAX];
  char favorites_path[PATH_MAX];
  char recent_path[PATH_MAX];
  char resume_session_path[PATH_MAX];
  char resume_hold_path[PATH_MAX];
  const char *cmd;
  size_t limit = 0;
  int i;

  sdcard_root = getenv("PLUMOS_SDCARD_ROOT");
  if (!sdcard_root || !sdcard_root[0]) {
    sdcard_root = "/mnt/SDCARD";
  }
  plumos_root_env = getenv("PLUMOS_ROOT");
  if (plumos_root_env && plumos_root_env[0]) {
    copy_string(plumos_root, sizeof(plumos_root), plumos_root_env);
  } else {
    if (!join_path(plumos_root, sizeof(plumos_root), sdcard_root, "plumos")) {
      fprintf(stderr, "error: plumOS root path is too long\n");
      return 1;
    }
  }
  if (!join_path(full_cache_path, sizeof(full_cache_path), plumos_root,
                 "state/frontend/library-index.json")) {
    fprintf(stderr, "error: library-index path is too long\n");
    return 1;
  }
  if (!join_path(systems_path, sizeof(systems_path), plumos_root,
                 "config/frontend/systems.json")) {
    fprintf(stderr, "error: systems config path is too long\n");
    return 1;
  }
  if (!join_path(settings_path, sizeof(settings_path), plumos_root,
                 "config/frontend/settings.json")) {
    fprintf(stderr, "error: settings config path is too long\n");
    return 1;
  }
  if (!join_path(menus_path, sizeof(menus_path), plumos_root, "config/frontend/menus.json") ||
      !join_path(apps_path, sizeof(apps_path), plumos_root, "config/frontend/apps.json")) {
    fprintf(stderr, "error: menu config path is too long\n");
    return 1;
  }
  if (!join_path(core_overrides_path, sizeof(core_overrides_path), plumos_root,
                 "state/frontend/core-overrides.json")) {
    fprintf(stderr, "error: core-overrides path is too long\n");
    return 1;
  }
  if (!join_path(favorites_path, sizeof(favorites_path), plumos_root,
                 "state/frontend/favorites.json")) {
    fprintf(stderr, "error: favorites path is too long\n");
    return 1;
  }
  if (!join_path(recent_path, sizeof(recent_path), plumos_root,
                 "state/frontend/recent.json") ||
      !join_path(resume_session_path, sizeof(resume_session_path), plumos_root,
                 "state/frontend/resume-session.json") ||
      !join_path(resume_hold_path, sizeof(resume_hold_path), plumos_root,
                 "state/frontend/resume-hold.json")) {
    fprintf(stderr, "error: recent/resume path is too long\n");
    return 1;
  }

  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(argv[0]);
    return argc < 2 ? 1 : 0;
  }

  cmd = argv[1];
  if (strcmp(cmd, "top") == 0) {
    struct top_entry *entries;
    struct frontend_settings settings;
    static struct favorite_state favorites;
    size_t count = 0;
    long ready_ms = -1;
    int show_all = 0;
    int refresh = 0;

    if (!load_frontend_settings(settings_path, &settings)) {
      fprintf(stderr, "error: cannot read frontend settings: %s\n", settings_path);
      return 1;
    }
    show_all = settings.show_empty_systems;

    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--all") == 0) {
        show_all = 1;
      } else if (strcmp(argv[i], "--refresh") == 0) {
        refresh = 1;
      } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown top option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    entries = (struct top_entry *)calloc(MAX_TOP_ENTRIES, sizeof(entries[0]));
    if (!entries) {
      fprintf(stderr, "error: out of memory\n");
      return 1;
    }
    if ((refresh || !file_exists(full_cache_path)) &&
        !run_scanner(plumos_root, sdcard_root, NULL, 1)) {
      free(entries);
      return 1;
    }
    if (!load_top_entries(full_cache_path, entries, MAX_TOP_ENTRIES, &count, &ready_ms)) {
      fprintf(stderr, "error: cannot read top cache: %s\n", full_cache_path);
      free(entries);
      return 1;
    }
    if (settings.show_favorites_on_top) {
      if (!load_favorites(favorites_path, &favorites)) {
        fprintf(stderr, "error: cannot read favorites: %s\n", favorites_path);
        free(entries);
        return 1;
      }
      if (!append_favorites_top_entry(entries, MAX_TOP_ENTRIES, &count, &favorites)) {
        fprintf(stderr, "error: cannot append Favorites TOP entry\n");
        free(entries);
        return 1;
      }
    }
    print_top(entries, count, show_all, limit, full_cache_path, ready_ms);
    free(entries);
    return 0;
  }

  if (strcmp(cmd, "roms") == 0) {
    struct rom_entry *entries = NULL;
    static struct favorite_state favorites;
    const char *system_id;
    size_t count = 0;
    long ready_ms = -1;
    int scan = 1;
    const char *limit_env = getenv("PLUMOS_TEXT_LIMIT");

    if (argc < 3) {
      fprintf(stderr, "error: roms requires SYSTEM\n");
      usage(argv[0]);
      return 2;
    }
    system_id = argv[2];
    if (!valid_system_id(system_id)) {
      fprintf(stderr, "error: invalid system id: %s\n", system_id);
      return 2;
    }
    limit = limit_env && limit_env[0] ? (size_t)strtoul(limit_env, NULL, 10) : 50;

    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--no-scan") == 0) {
        scan = 0;
      } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown roms option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (strcmp(system_id, "favorites") == 0) {
      if (!load_favorites(favorites_path, &favorites)) {
        fprintf(stderr, "error: cannot read favorites: %s\n", favorites_path);
        return 1;
      }
      print_favorites(&favorites, limit, favorites_path);
      return 0;
    }

    if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path), plumos_root,
                                 system_id)) {
      fprintf(stderr, "error: system cache path is too long\n");
      return 1;
    }
    if ((scan || !file_exists(system_cache_path)) &&
        !run_scanner(plumos_root, sdcard_root, system_id, 0)) {
      return 1;
    }
    if (!load_rom_entries(system_cache_path, system_id, &entries, &count, &ready_ms)) {
      fprintf(stderr, "error: cannot read ROM cache: %s\n", system_cache_path);
      free(entries);
      return 1;
    }
    if (!load_favorites(favorites_path, &favorites)) {
      fprintf(stderr, "error: cannot read favorites: %s\n", favorites_path);
      free(entries);
      return 1;
    }
    print_roms(system_id, entries, count, &favorites, limit, system_cache_path, ready_ms);
    free(entries);
    return 0;
  }

  if (strcmp(cmd, "favorites") == 0) {
    static struct favorite_state favorites;
    const char *limit_env = getenv("PLUMOS_TEXT_LIMIT");

    limit = limit_env && limit_env[0] ? (size_t)strtoul(limit_env, NULL, 10) : 50;
    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown favorites option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (!load_favorites(favorites_path, &favorites)) {
      fprintf(stderr, "error: cannot read favorites: %s\n", favorites_path);
      return 1;
    }
    print_favorites(&favorites, limit, favorites_path);
    return 0;
  }

  if (strcmp(cmd, "favorite") == 0 || strcmp(cmd, "fav") == 0) {
    const char *scope;
    const char *system_id;
    const char *rom_relative_path;
    const char *action = "toggle";
    static struct favorite_state favorites;
    struct rom_entry *entries = NULL;
    struct rom_entry selected_rom;
    size_t count = 0;
    long ready_ms = -1;
    int scan = 1;
    int action_seen = 0;
    int rom_idx;
    int was_favorite;

    if (argc < 5) {
      fprintf(stderr, "error: favorite requires rom SYSTEM RELATIVE_PATH\n");
      usage(argv[0]);
      return 2;
    }
    scope = argv[2];
    if (strcmp(scope, "rom") != 0) {
      fprintf(stderr, "error: favorite currently supports only rom scope\n");
      usage(argv[0]);
      return 2;
    }
    system_id = argv[3];
    rom_relative_path = argv[4];
    if (!valid_system_id(system_id)) {
      fprintf(stderr, "error: invalid system id: %s\n", system_id);
      return 2;
    }
    if (!valid_relative_rom_path(rom_relative_path)) {
      fprintf(stderr, "error: invalid ROM relative path: %s\n", rom_relative_path);
      return 2;
    }

    for (i = 5; i < argc; i++) {
      if (strcmp(argv[i], "--toggle") == 0) {
        if (action_seen) {
          fprintf(stderr, "error: use only one favorite action\n");
          return 2;
        }
        action_seen = 1;
        action = "toggle";
      } else if (strcmp(argv[i], "--set") == 0) {
        if (action_seen) {
          fprintf(stderr, "error: use only one favorite action\n");
          return 2;
        }
        action_seen = 1;
        action = "set";
      } else if (strcmp(argv[i], "--clear") == 0) {
        if (action_seen) {
          fprintf(stderr, "error: use only one favorite action\n");
          return 2;
        }
        action_seen = 1;
        action = "clear";
      } else if (strcmp(argv[i], "--no-scan") == 0) {
        scan = 0;
      } else {
        fprintf(stderr, "error: unknown favorite option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path), plumos_root,
                                 system_id)) {
      fprintf(stderr, "error: system cache path is too long\n");
      return 1;
    }
    if ((scan || !file_exists(system_cache_path)) &&
        !run_scanner(plumos_root, sdcard_root, system_id, 0)) {
      return 1;
    }

    if (!load_rom_entries(system_cache_path, system_id, &entries, &count, &ready_ms)) {
      fprintf(stderr, "error: cannot read ROM cache: %s\n", system_cache_path);
      free(entries);
      return 1;
    }
    (void)ready_ms;
    rom_idx = find_rom_by_relative_path(entries, count, rom_relative_path);
    if (rom_idx < 0) {
      fprintf(stderr, "error: ROM is not in scan cache for %s: %s\n", system_id,
              rom_relative_path);
      free(entries);
      return 2;
    }
    selected_rom = entries[rom_idx];
    free(entries);

    if (!load_favorites(favorites_path, &favorites)) {
      fprintf(stderr, "error: cannot read favorites: %s\n", favorites_path);
      return 1;
    }
    was_favorite = find_favorite(&favorites, system_id, rom_relative_path) >= 0;

    if (strcmp(action, "set") == 0 || (strcmp(action, "toggle") == 0 && !was_favorite)) {
      if (!set_favorite_from_rom(&favorites, system_id, &selected_rom)) {
        fprintf(stderr, "error: cannot set favorite\n");
        return 1;
      }
    } else if (strcmp(action, "clear") == 0 ||
               (strcmp(action, "toggle") == 0 && was_favorite)) {
      if (!clear_favorite(&favorites, system_id, rom_relative_path)) {
        fprintf(stderr, "error: cannot clear favorite\n");
        return 1;
      }
    }

    if (!save_favorites(favorites_path, &favorites)) {
      fprintf(stderr, "error: cannot write favorites: %s\n", favorites_path);
      return 1;
    }
    print_favorite_result(action, system_id, &selected_rom, &favorites, favorites_path);
    return 0;
  }

  if (strcmp(cmd, "recent") == 0 || strcmp(cmd, "recents") == 0) {
    static struct recent_state recent;
    const char *limit_env = getenv("PLUMOS_TEXT_LIMIT");

    limit = limit_env && limit_env[0] ? (size_t)strtoul(limit_env, NULL, 10) : 20;
    if (argc >= 3 && strcmp(argv[2], "add") == 0) {
      const char *system_id;
      const char *rom_relative_path;
      const char *profile_arg = NULL;
      const char *played_at_arg = NULL;
      int resume_available = 1;
      int scan = 1;
      struct rom_entry rom;
      struct core_system_def system;
      static struct core_override_state overrides;
      struct recent_entry entry;
      char launch_profile[128];
      char played_at[64];

      if (argc < 5) {
        fprintf(stderr, "error: recent add requires SYSTEM RELATIVE_PATH\n");
        usage(argv[0]);
        return 2;
      }
      system_id = argv[3];
      rom_relative_path = argv[4];
      if (!valid_system_id(system_id) || !valid_relative_rom_path(rom_relative_path)) {
        fprintf(stderr, "error: invalid recent target\n");
        return 2;
      }
      for (i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
          profile_arg = argv[++i];
          if (!valid_launch_profile_id(profile_arg)) {
            fprintf(stderr, "error: invalid launch profile: %s\n", profile_arg);
            return 2;
          }
        } else if (strcmp(argv[i], "--played-at") == 0 && i + 1 < argc) {
          played_at_arg = argv[++i];
        } else if (strcmp(argv[i], "--resume") == 0 && i + 1 < argc) {
          const char *value = argv[++i];
          if (strcmp(value, "yes") == 0 || strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            resume_available = 1;
          } else if (strcmp(value, "no") == 0 || strcmp(value, "false") == 0 ||
                     strcmp(value, "0") == 0) {
            resume_available = 0;
          } else {
            fprintf(stderr, "error: --resume expects yes or no\n");
            return 2;
          }
        } else if (strcmp(argv[i], "--no-scan") == 0) {
          scan = 0;
        } else {
          fprintf(stderr, "error: unknown recent option: %s\n", argv[i]);
          usage(argv[0]);
          return 2;
        }
      }

      if (!load_selected_rom(plumos_root, sdcard_root, system_id, rom_relative_path, scan, &rom)) {
        return 1;
      }
      if (profile_arg) {
        copy_string(launch_profile, sizeof(launch_profile), profile_arg);
      } else {
        if (!load_core_system_def(systems_path, system_id, &system) ||
            !load_core_overrides(core_overrides_path, &overrides) ||
            !resolve_launch_profile(&system, &overrides, rom_relative_path, launch_profile,
                                    sizeof(launch_profile))) {
          fprintf(stderr, "error: cannot resolve launch profile for %s\n", system_id);
          return 1;
        }
      }
      if (played_at_arg) {
        copy_string(played_at, sizeof(played_at), played_at_arg);
      } else {
        current_utc_timestamp(played_at, sizeof(played_at));
      }
      if (!load_recent(recent_path, &recent)) {
        fprintf(stderr, "error: cannot read recent: %s\n", recent_path);
        return 1;
      }
      recent_entry_from_rom(&entry, system_id, &rom, launch_profile, played_at,
                            resume_available);
      add_recent_entry(&recent, &entry);
      if (!save_recent(recent_path, &recent)) {
        fprintf(stderr, "error: cannot write recent: %s\n", recent_path);
        return 1;
      }
      print_recent(&recent, limit, recent_path);
      return 0;
    }

    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown recent option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }
    if (!load_recent(recent_path, &recent)) {
      fprintf(stderr, "error: cannot read recent: %s\n", recent_path);
      return 1;
    }
    print_recent(&recent, limit, recent_path);
    return 0;
  }

  if (strcmp(cmd, "resume") == 0) {
    const char *subcmd = argc >= 3 ? argv[2] : "show";
    static struct resume_session session;

    if (strcmp(subcmd, "show") == 0) {
      if (!load_resume_session(resume_session_path, &session)) {
        fprintf(stderr, "error: cannot read resume session: %s\n", resume_session_path);
        return 1;
      }
      print_resume_session(&session, resume_session_path);
      return 0;
    }
    if (strcmp(subcmd, "clear") == 0) {
      init_resume_session(&session);
      current_utc_timestamp(session.updated_at, sizeof(session.updated_at));
      if (!save_resume_session(resume_session_path, &session)) {
        fprintf(stderr, "error: cannot write resume session: %s\n", resume_session_path);
        return 1;
      }
      print_resume_result("clear", &session, resume_session_path);
      return 0;
    }
    if (strcmp(subcmd, "set") == 0) {
      const char *system_id;
      const char *rom_relative_path;
      const char *profile_arg = NULL;
      const char *reason = "shutdown";
      int auto_state_load = 0;
      int scan = 1;
      struct rom_entry rom;
      struct core_system_def system;
      static struct core_override_state overrides;
      static struct recent_state recent;
      struct recent_entry recent_entry;
      char launch_profile[128];
      char updated_at[64];

      if (argc < 5) {
        fprintf(stderr, "error: resume set requires SYSTEM RELATIVE_PATH\n");
        usage(argv[0]);
        return 2;
      }
      system_id = argv[3];
      rom_relative_path = argv[4];
      if (!valid_system_id(system_id) || !valid_relative_rom_path(rom_relative_path)) {
        fprintf(stderr, "error: invalid resume target\n");
        return 2;
      }
      for (i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
          profile_arg = argv[++i];
          if (!valid_launch_profile_id(profile_arg)) {
            fprintf(stderr, "error: invalid launch profile: %s\n", profile_arg);
            return 2;
          }
        } else if (strcmp(argv[i], "--reason") == 0 && i + 1 < argc) {
          reason = argv[++i];
        } else if (strcmp(argv[i], "--auto-state-load") == 0 && i + 1 < argc) {
          const char *value = argv[++i];
          if (strcmp(value, "yes") == 0 || strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            auto_state_load = 1;
          } else if (strcmp(value, "no") == 0 || strcmp(value, "false") == 0 ||
                     strcmp(value, "0") == 0) {
            auto_state_load = 0;
          } else {
            fprintf(stderr, "error: --auto-state-load expects yes or no\n");
            return 2;
          }
        } else if (strcmp(argv[i], "--no-scan") == 0) {
          scan = 0;
        } else {
          fprintf(stderr, "error: unknown resume option: %s\n", argv[i]);
          usage(argv[0]);
          return 2;
        }
      }

      if (!load_selected_rom(plumos_root, sdcard_root, system_id, rom_relative_path, scan, &rom)) {
        return 1;
      }
      if (profile_arg) {
        copy_string(launch_profile, sizeof(launch_profile), profile_arg);
      } else {
        if (!load_core_system_def(systems_path, system_id, &system) ||
            !load_core_overrides(core_overrides_path, &overrides) ||
            !resolve_launch_profile(&system, &overrides, rom_relative_path, launch_profile,
                                    sizeof(launch_profile))) {
          fprintf(stderr, "error: cannot resolve launch profile for %s\n", system_id);
          return 1;
        }
      }
      current_utc_timestamp(updated_at, sizeof(updated_at));
      resume_session_from_rom(&session, system_id, &rom, launch_profile, updated_at, reason, 1,
                              auto_state_load);
      if (!save_resume_session(resume_session_path, &session)) {
        fprintf(stderr, "error: cannot write resume session: %s\n", resume_session_path);
        return 1;
      }

      if (!load_recent(recent_path, &recent)) {
        fprintf(stderr, "error: cannot read recent: %s\n", recent_path);
        return 1;
      }
      recent_entry_from_rom(&recent_entry, system_id, &rom, launch_profile, updated_at, 1);
      add_recent_entry(&recent, &recent_entry);
      if (!save_recent(recent_path, &recent)) {
        fprintf(stderr, "error: cannot write recent: %s\n", recent_path);
        return 1;
      }
      print_resume_result("set", &session, resume_session_path);
      return 0;
    }

    fprintf(stderr, "error: unknown resume command: %s\n", subcmd);
    usage(argv[0]);
    return 2;
  }

  if (strcmp(cmd, "launch") == 0) {
    const char *system_id;
    const char *rom_relative_path;
    const char *profile_arg = NULL;
    int scan = 1;
    int execute = 0;
    struct rom_entry rom;
    struct core_system_def system;
    static struct core_override_state overrides;
    static struct launch_plan plan;
    static struct recent_state recent;
    struct recent_entry recent_entry;
    static struct resume_session session;
    char launch_profile[128];
    char cpu_policy[32];
    long cpu_freq_khz = 0;
    const char *cpu_source = NULL;
    struct retroarch_runtime_options retroarch_options;
    char timestamp[64];
    int ok;

    if (argc < 4) {
      fprintf(stderr, "error: launch requires SYSTEM RELATIVE_PATH\n");
      usage(argv[0]);
      return 2;
    }
    system_id = argv[2];
    rom_relative_path = argv[3];
    if (!valid_system_id(system_id) || !valid_relative_rom_path(rom_relative_path)) {
      fprintf(stderr, "error: invalid launch target\n");
      return 2;
    }
    for (i = 4; i < argc; i++) {
      if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
        profile_arg = argv[++i];
        if (!valid_launch_profile_id(profile_arg)) {
          fprintf(stderr, "error: invalid launch profile: %s\n", profile_arg);
          return 2;
        }
      } else if (strcmp(argv[i], "--execute") == 0) {
        execute = 1;
      } else if (strcmp(argv[i], "--no-scan") == 0) {
        scan = 0;
      } else {
        fprintf(stderr, "error: unknown launch option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (!load_selected_rom(plumos_root, sdcard_root, system_id, rom_relative_path, scan, &rom)) {
      return 1;
    }
    if (!load_core_system_def(systems_path, system_id, &system) ||
        !load_core_overrides(core_overrides_path, &overrides)) {
      fprintf(stderr, "error: cannot read launch profile state for %s\n", system_id);
      return 1;
    }
    if (profile_arg) {
      if (core_profile_index(&system, profile_arg) < 0) {
        fprintf(stderr, "error: launch profile is not listed for %s: %s\n", system_id,
                profile_arg);
        return 2;
      }
      if (!resolve_device_launch_profile(&system, profile_arg, launch_profile,
                                         sizeof(launch_profile))) {
        fprintf(stderr, "error: cannot resolve launch profile for %s\n", system_id);
        return 1;
      }
    } else if (!resolve_launch_profile(&system, &overrides, rom_relative_path, launch_profile,
                                       sizeof(launch_profile))) {
      fprintf(stderr, "error: cannot resolve launch profile for %s\n", system_id);
      return 1;
    }
    if (!resolve_cpu_setting(&system, &overrides, rom_relative_path, cpu_policy,
                             sizeof(cpu_policy), &cpu_freq_khz, &cpu_source)) {
      fprintf(stderr, "error: cannot resolve CPU policy for %s\n", system_id);
      return 1;
    }
    if (!apply_content_suffix_override(&system, &overrides, rom_relative_path, &rom)) {
      fprintf(stderr, "error: cannot apply content suffix for %s\n", rom_relative_path);
      return 1;
    }
    resolve_retroarch_runtime_options(&system, &overrides, rom_relative_path,
                                      &retroarch_options);
    apply_picoarch_adapter_cpu_defaults(plumos_root, launch_profile, cpu_policy,
                                        &cpu_freq_khz, cpu_source);
    if (!build_launch_plan(&plan, plumos_root, system_id, rom_relative_path, rom.title,
                           rom.path, launch_profile, cpu_policy, cpu_freq_khz,
                           &retroarch_options, 0)) {
      fprintf(stderr, "error: cannot build launch plan\n");
      return 1;
    }

    print_launch_plan(&plan);
    if (!execute) {
      printf("execute: no (--execute not specified)\n");
      return 0;
    }
    if (!plan.can_execute) {
      fprintf(stderr, "error: launch plan is not executable\n");
      return 1;
    }

    current_utc_timestamp(timestamp, sizeof(timestamp));
    if (!load_recent(recent_path, &recent)) {
      fprintf(stderr, "error: cannot read recent: %s\n", recent_path);
      return 1;
    }
    recent_entry_from_rom(&recent_entry, system_id, &rom, launch_profile, timestamp, 1);
    add_recent_entry(&recent, &recent_entry);
    if (!save_recent(recent_path, &recent)) {
      fprintf(stderr, "error: cannot write recent: %s\n", recent_path);
      return 1;
    }

    resume_session_from_rom(&session, system_id, &rom, launch_profile, timestamp, "launch", 1, 0);
    if (!save_resume_session(resume_session_path, &session)) {
      fprintf(stderr, "error: cannot write resume session: %s\n", resume_session_path);
      return 1;
    }

    ok = execute_launch_plan(&plan, plumos_root);

    current_utc_timestamp(timestamp, sizeof(timestamp));
    unlink(resume_hold_path);
    init_resume_session(&session);
    copy_string(session.updated_at, sizeof(session.updated_at), timestamp);
    if (!save_resume_session(resume_session_path, &session)) {
      fprintf(stderr, "warning: cannot clear resume session: %s\n", resume_session_path);
    }
    printf("execute: %s\n", ok ? "ok" : "failed");
    return ok ? 0 : 1;
  }

  if (strcmp(cmd, "boot") == 0) {
    struct frontend_settings settings;
    static struct resume_session session;
    static struct recent_state recent;
    static struct launch_plan plan;
    struct core_system_def system;
    static struct core_override_state overrides;
    const char *limit_env = getenv("PLUMOS_TEXT_LIMIT");
    char cpu_policy[32];
    long cpu_freq_khz = 0;
    const char *cpu_source = NULL;
    struct retroarch_runtime_options retroarch_options;
    char launch_profile[128];
    char session_launch_path[TEXT_PATH_MAX];
    int execute = 0;
    int has_recent_rom = 0;

    limit = limit_env && limit_env[0] ? (size_t)strtoul(limit_env, NULL, 10) : 10;
    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else if (strcmp(argv[i], "--execute") == 0) {
        execute = 1;
      } else {
        fprintf(stderr, "error: unknown boot option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }
    if (!load_frontend_settings(settings_path, &settings) ||
        !load_resume_session(resume_session_path, &session) ||
        !load_recent(recent_path, &recent)) {
      fprintf(stderr, "error: cannot read boot startup state\n");
      return 1;
    }
    print_boot_resume(&settings, &session, &recent, limit, resume_session_path, recent_path);
    has_recent_rom = recent.count > 0 && recent.entries[0].system_id[0] &&
                     recent.entries[0].relative_path[0];
    if (strcmp(settings.boot_resume_mode, "on") == 0 && has_recent_rom) {
      const struct recent_entry *last = &recent.entries[0];
      if (!load_core_system_def(systems_path, last->system_id, &system) ||
          !load_core_overrides(core_overrides_path, &overrides) ||
          !resolve_cpu_setting(&system, &overrides, last->relative_path, cpu_policy,
                               sizeof(cpu_policy), &cpu_freq_khz, &cpu_source)) {
        fprintf(stderr, "error: cannot resolve CPU setting for %s\n", last->system_id);
        return 1;
      }
      copy_string(session_launch_path, sizeof(session_launch_path), last->path);
      if (!strchr(session_launch_path, '#')) {
        int rom_idx = find_rom_core_override(&overrides, system.id, last->relative_path);
        if (rom_idx >= 0 && overrides.rom_overrides[rom_idx].content_suffix[0]) {
          size_t pos = strlen(session_launch_path);
          if (!append_string(session_launch_path, sizeof(session_launch_path), &pos,
                             overrides.rom_overrides[rom_idx].content_suffix)) {
            fprintf(stderr, "error: cannot apply content suffix for %s\n",
                    last->relative_path);
            return 1;
          }
        }
      }
      resolve_retroarch_runtime_options(&system, &overrides, last->relative_path,
                                        &retroarch_options);
      if (core_profile_is_listed(&system, last->launch_profile)) {
        if (!resolve_device_launch_profile(&system, last->launch_profile, launch_profile,
                                           sizeof(launch_profile))) {
          fprintf(stderr, "error: cannot resolve launch profile for %s\n", last->system_id);
          return 1;
        }
      } else if (!resolve_launch_profile(&system, &overrides, last->relative_path,
                                         launch_profile, sizeof(launch_profile))) {
        fprintf(stderr, "error: cannot resolve launch profile for %s\n", last->system_id);
        return 1;
      }
      apply_picoarch_adapter_cpu_defaults(plumos_root, launch_profile, cpu_policy,
                                          &cpu_freq_khz, cpu_source);
      if (!build_launch_plan(&plan, plumos_root, last->system_id, last->relative_path,
                             last->title, session_launch_path, launch_profile,
                             cpu_policy, cpu_freq_khz, &retroarch_options,
                             0)) {
        fprintf(stderr, "error: cannot build launch plan\n");
        return 1;
      }
      printf("\n");
      print_launch_plan(&plan);
      if (execute) {
        return execute_launch_plan(&plan, plumos_root) ? 0 : 1;
      }
      printf("execute: no (--execute not specified)\n");
      return 0;
    }
    if (execute) {
      printf("\n");
      printf("execute: no action\n");
    }
    return 0;
  }

  if (strcmp(cmd, "menu") == 0) {
    const char *menu_id;

    if (argc < 3) {
      fprintf(stderr, "error: menu requires start or apps\n");
      usage(argv[0]);
      return 2;
    }
    menu_id = argv[2];
    if (!valid_system_id(menu_id)) {
      fprintf(stderr, "error: invalid menu id: %s\n", menu_id);
      return 2;
    }
    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown menu option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (strcmp(menu_id, "apps") == 0) {
      struct app_entry *entries;
      size_t count = 0;

      entries = (struct app_entry *)calloc(MAX_APP_ENTRIES, sizeof(entries[0]));
      if (!entries) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
      }
      if (!load_app_entries(apps_path, menu_id, entries, MAX_APP_ENTRIES, &count)) {
        fprintf(stderr, "error: cannot read apps menu: %s\n", apps_path);
        free(entries);
        return 1;
      }
      print_apps_menu(menu_id, entries, count, limit, apps_path);
      free(entries);
      return 0;
    } else {
      struct menu_entry *entries;
      size_t count = 0;

      entries = (struct menu_entry *)calloc(MAX_MENU_ENTRIES, sizeof(entries[0]));
      if (!entries) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
      }
      if (!load_menu_entries(menus_path, menu_id, entries, MAX_MENU_ENTRIES, &count)) {
        fprintf(stderr, "error: cannot read menu: %s id=%s\n", menus_path, menu_id);
        free(entries);
        return 1;
      }
      print_menu(menu_id, entries, count, limit, menus_path);
      free(entries);
      return 0;
    }
  }

  if (strcmp(cmd, "core") == 0 || strcmp(cmd, "cores") == 0) {
    const char *scope;
    const char *system_id;
    const char *rom_relative_path = NULL;
    const char *set_profile = NULL;
    const char *set_cpu_policy = NULL;
    const char *set_content_suffix = NULL;
    const char *set_audio_driver = NULL;
    const char *set_dosbox_force60fps = NULL;
    const char *set_dosbox_cycles = NULL;
    long set_audio_latency_ms = 0;
    struct core_system_def system;
    static struct core_override_state state;
    int clear = 0;
    int clear_profile = 0;
    int clear_cpu = 0;
    int clear_content_suffix = 0;
    int clear_audio = 0;
    int clear_dosbox = 0;
    int audio_latency_seen = 0;
    int scan = 1;
    int option_start;

    if (argc < 4) {
      fprintf(stderr, "error: core requires system or rom scope\n");
      usage(argv[0]);
      return 2;
    }
    scope = argv[2];
    system_id = argv[3];
    if (!valid_system_id(system_id)) {
      fprintf(stderr, "error: invalid system id: %s\n", system_id);
      return 2;
    }
    if (strcmp(scope, "system") == 0) {
      option_start = 4;
    } else if (strcmp(scope, "rom") == 0) {
      if (argc < 5) {
        fprintf(stderr, "error: core rom requires RELATIVE_PATH\n");
        usage(argv[0]);
        return 2;
      }
      rom_relative_path = argv[4];
      if (!valid_relative_rom_path(rom_relative_path)) {
        fprintf(stderr, "error: invalid ROM relative path: %s\n", rom_relative_path);
        return 2;
      }
      option_start = 5;
    } else {
      fprintf(stderr, "error: unknown core scope: %s\n", scope);
      usage(argv[0]);
      return 2;
    }

    for (i = option_start; i < argc; i++) {
      if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) {
        if (clear || clear_profile || set_profile) {
          fprintf(stderr, "error: use only one profile action\n");
          return 2;
        }
        set_profile = argv[++i];
        if (!valid_launch_profile_id(set_profile)) {
          fprintf(stderr, "error: invalid launch profile: %s\n", set_profile);
          return 2;
        }
      } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
        if (clear || clear_cpu || set_cpu_policy) {
          fprintf(stderr, "error: use only one CPU action\n");
          return 2;
        }
        set_cpu_policy = argv[++i];
        if (!valid_cpu_policy(set_cpu_policy)) {
          fprintf(stderr, "error: invalid CPU policy: %s\n", set_cpu_policy);
          return 2;
        }
      } else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) {
        fprintf(stderr, "error: fixed CPU frequencies are no longer supported\n");
        return 2;
      } else if (strcmp(argv[i], "--content-suffix") == 0 && i + 1 < argc) {
        if (clear || clear_content_suffix || set_content_suffix) {
          fprintf(stderr, "error: use only one content suffix action\n");
          return 2;
        }
        set_content_suffix = argv[++i];
        if (!valid_content_suffix(set_content_suffix)) {
          fprintf(stderr, "error: invalid content suffix: %s\n", set_content_suffix);
          return 2;
        }
      } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
        if (clear || clear_audio || set_audio_driver) {
          fprintf(stderr, "error: use only one audio driver action\n");
          return 2;
        }
        set_audio_driver = argv[++i];
        if (!valid_retroarch_audio_driver(set_audio_driver)) {
          fprintf(stderr, "error: --audio expects oss, alsa, alsathread, or pixel2_compat_ao\n");
          return 2;
        }
      } else if (strcmp(argv[i], "--latency") == 0 && i + 1 < argc) {
        if (clear || clear_audio || audio_latency_seen) {
          fprintf(stderr, "error: use only one audio latency action\n");
          return 2;
        }
        if (!parse_audio_latency_ms(argv[++i], &set_audio_latency_ms)) {
          fprintf(stderr, "error: invalid audio latency: %s\n", argv[i]);
          return 2;
        }
        audio_latency_seen = 1;
      } else if (strcmp(argv[i], "--dosbox-force60fps") == 0 && i + 1 < argc) {
        if (clear || clear_dosbox || set_dosbox_force60fps) {
          fprintf(stderr, "error: use only one DOSBox-Pure force60fps action\n");
          return 2;
        }
        set_dosbox_force60fps = argv[++i];
        if (!valid_dosbox_bool(set_dosbox_force60fps)) {
          fprintf(stderr, "error: --dosbox-force60fps expects true or false\n");
          return 2;
        }
      } else if (strcmp(argv[i], "--dosbox-cycles") == 0 && i + 1 < argc) {
        if (clear || clear_dosbox || set_dosbox_cycles) {
          fprintf(stderr, "error: use only one DOSBox-Pure cycles action\n");
          return 2;
        }
        set_dosbox_cycles = argv[++i];
        if (!valid_dosbox_cycles(set_dosbox_cycles)) {
          fprintf(stderr, "error: invalid DOSBox-Pure cycles: %s\n", set_dosbox_cycles);
          return 2;
        }
      } else if (strcmp(argv[i], "--clear-content-suffix") == 0) {
        if (clear || clear_content_suffix || set_content_suffix) {
          fprintf(stderr, "error: use only one content suffix action\n");
          return 2;
        }
        clear_content_suffix = 1;
      } else if (strcmp(argv[i], "--clear-audio") == 0) {
        if (clear || clear_audio || set_audio_driver || audio_latency_seen) {
          fprintf(stderr, "error: use only one audio action\n");
          return 2;
        }
        clear_audio = 1;
      } else if (strcmp(argv[i], "--clear-dosbox") == 0) {
        if (clear || clear_dosbox || set_dosbox_force60fps || set_dosbox_cycles) {
          fprintf(stderr, "error: use only one DOSBox-Pure action\n");
          return 2;
        }
        clear_dosbox = 1;
      } else if (strcmp(argv[i], "--clear") == 0) {
        if (clear || set_profile || set_cpu_policy || clear_cpu ||
            clear_profile || set_content_suffix || clear_content_suffix || set_audio_driver ||
            audio_latency_seen || clear_audio || set_dosbox_force60fps ||
            set_dosbox_cycles || clear_dosbox) {
          fprintf(stderr, "error: --clear cannot be combined with set actions\n");
          return 2;
        }
        clear = 1;
      } else if (strcmp(argv[i], "--clear-profile") == 0) {
        if (clear || clear_profile || set_profile) {
          fprintf(stderr, "error: use only one profile action\n");
          return 2;
        }
        clear_profile = 1;
      } else if (strcmp(argv[i], "--clear-cpu") == 0) {
        if (clear || set_cpu_policy || clear_cpu) {
          fprintf(stderr, "error: use only one CPU action\n");
          return 2;
        }
        clear_cpu = 1;
      } else if (strcmp(argv[i], "--no-scan") == 0 && rom_relative_path) {
        scan = 0;
      } else {
        fprintf(stderr, "error: unknown core option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (!load_core_system_def(systems_path, system_id, &system)) {
      fprintf(stderr, "error: cannot read system launch profiles: %s id=%s\n", systems_path,
              system_id);
      return 1;
    }
    if (!load_core_overrides(core_overrides_path, &state)) {
      fprintf(stderr, "error: cannot read core overrides: %s\n", core_overrides_path);
      return 1;
    }
    if (set_profile && core_profile_index(&system, set_profile) < 0) {
      fprintf(stderr, "error: launch profile is not listed for %s: %s\n", system_id,
              set_profile);
      return 2;
    }
    if (!rom_relative_path &&
        (set_content_suffix || clear_content_suffix || set_audio_driver ||
         audio_latency_seen || clear_audio || set_dosbox_force60fps ||
         set_dosbox_cycles || clear_dosbox)) {
      fprintf(stderr, "error: content/audio/DOSBox-Pure options require core rom scope\n");
      return 2;
    }

    if (rom_relative_path) {
      struct rom_entry *entries = NULL;
      size_t count = 0;
      long ready_ms = -1;
      int rom_idx;

      if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path), plumos_root,
                                   system_id)) {
        fprintf(stderr, "error: system cache path is too long\n");
        return 1;
      }
      if ((scan || !file_exists(system_cache_path)) &&
          !run_scanner(plumos_root, sdcard_root, system_id, 0)) {
        return 1;
      }
      if (!load_rom_entries(system_cache_path, system_id, &entries, &count, &ready_ms)) {
        fprintf(stderr, "error: cannot read ROM cache: %s\n", system_cache_path);
        free(entries);
        return 1;
      }
      (void)ready_ms;
      rom_idx = find_rom_by_relative_path(entries, count, rom_relative_path);
      free(entries);
      if (rom_idx < 0) {
        fprintf(stderr, "error: ROM is not in scan cache for %s: %s\n", system_id,
                rom_relative_path);
        return 2;
      }
    }

    if (clear) {
      if (rom_relative_path) {
        clear_rom_core_override(&state, system_id, rom_relative_path);
      } else {
        clear_system_core_override(&state, system_id);
      }
      if (!save_core_overrides(core_overrides_path, &state)) {
        fprintf(stderr, "error: cannot write core overrides: %s\n", core_overrides_path);
        return 1;
      }
    } else if (set_profile || clear_profile || set_cpu_policy || clear_cpu ||
               set_content_suffix || clear_content_suffix || set_audio_driver ||
               audio_latency_seen || clear_audio || set_dosbox_force60fps ||
               set_dosbox_cycles || clear_dosbox) {
      int ok = 1;
      if (set_profile) {
        ok = rom_relative_path
                 ? set_rom_core_override(&state, system_id, rom_relative_path, set_profile)
                 : set_system_core_override(&state, system_id, set_profile);
      }
      if (ok && clear_profile) {
        ok = rom_relative_path
                 ? clear_rom_launch_profile_override(&state, system_id, rom_relative_path)
                 : clear_system_launch_profile_override(&state, system_id);
      }
      if (ok && set_cpu_policy) {
        ok = rom_relative_path
                 ? set_rom_cpu_override(&state, system_id, rom_relative_path, set_cpu_policy,
                                        0)
                 : set_system_cpu_override(&state, system_id, set_cpu_policy,
                                           0);
      }
      if (ok && clear_cpu) {
        ok = rom_relative_path ? clear_rom_cpu_override(&state, system_id, rom_relative_path)
                               : clear_system_cpu_override(&state, system_id);
      }
      if (ok && set_content_suffix) {
        ok = set_rom_content_suffix_override(&state, system_id, rom_relative_path,
                                             set_content_suffix);
      }
      if (ok && clear_content_suffix) {
        ok = clear_rom_content_suffix_override(&state, system_id, rom_relative_path);
      }
      if (ok && set_audio_driver) {
        ok = set_rom_audio_driver_override(&state, system_id, rom_relative_path,
                                           set_audio_driver);
      }
      if (ok && audio_latency_seen) {
        ok = set_rom_audio_latency_override(&state, system_id, rom_relative_path,
                                            set_audio_latency_ms);
      }
      if (ok && clear_audio) {
        ok = clear_rom_audio_override(&state, system_id, rom_relative_path);
      }
      if (ok && set_dosbox_force60fps) {
        ok = set_rom_dosbox_force60fps_override(&state, system_id, rom_relative_path,
                                                set_dosbox_force60fps);
      }
      if (ok && set_dosbox_cycles) {
        ok = set_rom_dosbox_cycles_override(&state, system_id, rom_relative_path,
                                            set_dosbox_cycles);
      }
      if (ok && clear_dosbox) {
        ok = clear_rom_dosbox_override(&state, system_id, rom_relative_path);
      }
      if (!ok || !save_core_overrides(core_overrides_path, &state)) {
        fprintf(stderr, "error: cannot write core overrides: %s\n", core_overrides_path);
        return 1;
      }
    }

    print_core_selection(rom_relative_path ? "rom" : "system", &system, rom_relative_path,
                         &state, systems_path, core_overrides_path);
    return 0;
  }

  fprintf(stderr, "error: unknown command: %s\n", cmd);
  usage(argv[0]);
  return 2;
}
