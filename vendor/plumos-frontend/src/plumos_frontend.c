#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FRONTEND_COMMAND_MAX 8192

struct log_ctx {
  FILE *log;
  int stdout_enabled;
};

struct dir_stats {
  int exists;
  size_t files;
  size_t matching_files;
  size_t subdirs;
};

struct scan_totals {
  int configs;
  size_t rom_dirs;
  size_t rom_files;
  size_t rom_matching_files;
  size_t image_dirs;
  size_t image_files;
  size_t gamelists;
  size_t launchers;
};

static void emit(struct log_ctx *ctx, const char *fmt, ...) {
  va_list ap;
  va_list aq;

  va_start(ap, fmt);
  va_copy(aq, ap);
  if (ctx->stdout_enabled) {
    vprintf(fmt, ap);
    printf("\n");
  }
  if (ctx->log) {
    vfprintf(ctx->log, fmt, aq);
    fprintf(ctx->log, "\n");
    fflush(ctx->log);
  }
  va_end(aq);
  va_end(ap);
}

static int is_dir(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static long file_size_or_negative(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    return -1;
  }
  return (long)st.st_size;
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);

  if (a_len + 1 + b_len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  memcpy(out, a, a_len);
  out[a_len] = '/';
  memcpy(out + a_len + 1, b, b_len);
  out[a_len + 1 + b_len] = '\0';
  return 1;
}

static int copy_string(char *out, size_t out_size, const char *in) {
  size_t len = strlen(in);
  if (len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  memcpy(out, in, len + 1);
  return 1;
}

static int append_string(char *out, size_t out_size, size_t *pos, const char *in) {
  size_t len = strlen(in);
  if (*pos + len + 1 > out_size) {
    return 0;
  }
  memcpy(out + *pos, in, len);
  *pos += len;
  out[*pos] = '\0';
  return 1;
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

static int normalize_path(char *out, size_t out_size, const char *in) {
  char tmp[PATH_MAX];
  char *segments[256];
  char *p;
  size_t count = 0;
  size_t pos = 0;
  int absolute;

  if (!in || !in[0]) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  if (!copy_string(tmp, sizeof(tmp), in)) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  absolute = tmp[0] == '/';
  p = tmp;
  while (*p) {
    char *start;
    while (*p == '/') {
      p++;
    }
    if (!*p) {
      break;
    }
    start = p;
    while (*p && *p != '/') {
      p++;
    }
    if (*p == '/') {
      *p++ = '\0';
    }

    if (strcmp(start, ".") == 0) {
      continue;
    }
    if (strcmp(start, "..") == 0) {
      if (count > 0 && strcmp(segments[count - 1], "..") != 0) {
        count--;
      } else if (!absolute && count < sizeof(segments) / sizeof(segments[0])) {
        segments[count++] = start;
      }
      continue;
    }
    if (count < sizeof(segments) / sizeof(segments[0])) {
      segments[count++] = start;
    }
  }

  if (out_size == 0) {
    return 0;
  }
  out[0] = '\0';

  if (absolute) {
    if (!append_string(out, out_size, &pos, "/")) {
      return 0;
    }
  }

  if (count > 0) {
    size_t i;
    for (i = 0; i < count; i++) {
      if (i > 0) {
        if (!append_string(out, out_size, &pos, "/")) {
          return 0;
        }
      }
      if (!append_string(out, out_size, &pos, segments[i])) {
        return 0;
      }
    }
    return 1;
  }

  if (!absolute) {
    return copy_string(out, out_size, ".");
  }
  return 1;
}

static int resolve_path(char *out, size_t out_size, const char *base_dir, const char *path) {
  char joined[PATH_MAX];

  if (!path || !path[0]) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  if (path[0] == '/') {
    return normalize_path(out, out_size, path);
  }

  if (!join_path(joined, sizeof(joined), base_dir, path)) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  return normalize_path(out, out_size, joined);
}

static int ascii_equal_ci(const char *a, size_t a_len, const char *b, size_t b_len) {
  size_t i;
  if (a_len != b_len) {
    return 0;
  }
  for (i = 0; i < a_len; i++) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
      return 0;
    }
  }
  return 1;
}

static const char *file_extension(const char *name) {
  const char *dot = strrchr(name, '.');
  const char *slash = strrchr(name, '/');
  if (!dot || (slash && dot < slash) || dot[1] == '\0') {
    return "";
  }
  return dot + 1;
}

static int extlist_contains(const char *extlist, const char *ext) {
  const char *p = extlist;
  size_t ext_len = strlen(ext);

  if (!extlist || !extlist[0]) {
    return 1;
  }

  while (*p) {
    const char *start;
    const char *end;
    while (*p == '|' || isspace((unsigned char)*p)) {
      p++;
    }
    start = p;
    while (*p && *p != '|') {
      p++;
    }
    end = p;
    while (end > start && isspace((unsigned char)end[-1])) {
      end--;
    }
    if (end > start && ascii_equal_ci(start, (size_t)(end - start), ext, ext_len)) {
      return 1;
    }
  }

  return 0;
}

static struct dir_stats scan_regular_files(const char *path, const char *extlist) {
  struct dir_stats stats;
  DIR *dir;
  struct dirent *ent;

  memset(&stats, 0, sizeof(stats));
  dir = opendir(path);
  if (!dir) {
    return stats;
  }
  stats.exists = 1;

  while ((ent = readdir(dir)) != NULL) {
    char child[PATH_MAX];

    if (ent->d_name[0] == '.') {
      continue;
    }
    if (!join_path(child, sizeof(child), path, ent->d_name)) {
      continue;
    }
    if (is_dir(child)) {
      stats.subdirs++;
      continue;
    }
    if (!is_file(child)) {
      continue;
    }
    stats.files++;
    if (extlist_contains(extlist, file_extension(ent->d_name))) {
      stats.matching_files++;
    }
  }

  closedir(dir);
  return stats;
}

static char *read_file(const char *path, size_t *size_out) {
  FILE *f = fopen(path, "rb");
  char *buf = NULL;
  long size = 0;

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

static size_t count_nonempty_lines(const char *text) {
  size_t count = 0;
  int in_line = 0;

  if (!text) {
    return 0;
  }

  while (*text) {
    if (*text == '\n' || *text == '\r') {
      if (in_line) {
        count++;
        in_line = 0;
      }
    } else if (!isspace((unsigned char)*text)) {
      in_line = 1;
    }
    text++;
  }
  if (in_line) {
    count++;
  }
  return count;
}

static const char *skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size) {
  char pattern[128];
  const char *p = json;

  if (!json || !key || out_size == 0) {
    return 0;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  out[0] = '\0';

  while ((p = strstr(p, pattern)) != NULL) {
    const char *q = p + strlen(pattern);
    size_t n = 0;

    q = skip_ws(q);
    if (*q != ':') {
      p++;
      continue;
    }
    q++;
    q = skip_ws(q);
    if (*q != '"') {
      p++;
      continue;
    }
    q++;

    while (*q && *q != '"' && n + 1 < out_size) {
      if (*q == '\\' && q[1]) {
        q++;
      }
      out[n++] = *q++;
    }
    out[n] = '\0';
    return 1;
  }

  return 0;
}

static int json_find_array(const char *json, const char *key, const char **start_out, const char **end_out) {
  char pattern[128];
  const char *p;

  if (!json || !key || !start_out || !end_out) {
    return 0;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  p = strstr(json, pattern);
  if (!p) {
    return 0;
  }
  p += strlen(pattern);
  p = skip_ws(p);
  if (*p != ':') {
    return 0;
  }
  p++;
  p = skip_ws(p);
  if (*p != '[') {
    return 0;
  }

  {
    const char *q = p;
    int depth = 0;
    int in_string = 0;
    int escape = 0;

    while (*q) {
      if (escape) {
        escape = 0;
        q++;
        continue;
      }
      if (in_string && *q == '\\') {
        escape = 1;
        q++;
        continue;
      }
      if (*q == '"') {
        in_string = !in_string;
        q++;
        continue;
      }
      if (!in_string && *q == '[') {
        depth++;
      } else if (!in_string && *q == ']') {
        depth--;
        if (depth == 0) {
          *start_out = p + 1;
          *end_out = q;
          return 1;
        }
      }
      q++;
    }
  }

  return 0;
}

static size_t count_key_between(const char *start, const char *end, const char *key) {
  char pattern[128];
  const char *p = start;
  size_t count = 0;

  if (!start || !end || start >= end) {
    return 0;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  while ((p = strstr(p, pattern)) != NULL && p < end) {
    count++;
    p += strlen(pattern);
  }
  return count;
}

static size_t json_count_launchlist_entries(const char *json) {
  const char *start = NULL;
  const char *end = NULL;
  if (!json_find_array(json, "launchlist", &start, &end)) {
    return 0;
  }
  return count_key_between(start, end, "launch");
}

static int cmp_strptr(const void *a, const void *b) {
  const char *aa = *(const char *const *)a;
  const char *bb = *(const char *const *)b;
  return strcmp(aa, bb);
}

static char **list_dirs(const char *path, size_t *count_out) {
  DIR *dir = opendir(path);
  struct dirent *ent;
  char **items = NULL;
  size_t count = 0;
  size_t cap = 0;

  *count_out = 0;
  if (!dir) {
    return NULL;
  }

  while ((ent = readdir(dir)) != NULL) {
    char child[PATH_MAX];
    char *name_copy;

    if (ent->d_name[0] == '.') {
      continue;
    }

    join_path(child, sizeof(child), path, ent->d_name);
    if (!is_dir(child)) {
      continue;
    }

    if (count == cap) {
      size_t next_cap = cap ? cap * 2 : 32;
      char **next = (char **)realloc(items, next_cap * sizeof(char *));
      if (!next) {
        break;
      }
      items = next;
      cap = next_cap;
    }

    name_copy = strdup(ent->d_name);
    if (!name_copy) {
      break;
    }
    items[count++] = name_copy;
  }

  closedir(dir);
  qsort(items, count, sizeof(char *), cmp_strptr);
  *count_out = count;
  return items;
}

static void free_list(char **items, size_t count) {
  size_t i;
  for (i = 0; i < count; i++) {
    free(items[i]);
  }
  free(items);
}

static struct scan_totals scan_category(struct log_ctx *ctx, const char *sd_root,
                                        const char *category, const char *display_key) {
  char base[PATH_MAX];
  char **dirs;
  size_t count = 0;
  size_t i;
  struct scan_totals totals;

  memset(&totals, 0, sizeof(totals));

  join_path(base, sizeof(base), sd_root, category);
  dirs = list_dirs(base, &count);
  emit(ctx, "[%s] path=%s dirs=%zu", category, base, count);

  for (i = 0; i < count; i++) {
    char dir_path[PATH_MAX];
    char config_path[PATH_MAX];
    char label[256] = "";
    char rompath[512] = "";
    char imgpath[512] = "";
    char gamelist[512] = "";
    char extlist[512] = "";
    char launch[256] = "";
    char resolved_rompath[PATH_MAX] = "";
    char resolved_imgpath[PATH_MAX] = "";
    char resolved_gamelist[PATH_MAX] = "";
    long gamelist_size = -1;
    size_t launcher_count = 0;
    struct dir_stats rom_stats;
    struct dir_stats image_stats;
    char *json;

    memset(&rom_stats, 0, sizeof(rom_stats));
    memset(&image_stats, 0, sizeof(image_stats));

    join_path(dir_path, sizeof(dir_path), base, dirs[i]);
    join_path(config_path, sizeof(config_path), dir_path, "config.json");
    if (!is_file(config_path)) {
      emit(ctx, "  - %s config=missing", dirs[i]);
      continue;
    }

    json = read_file(config_path, NULL);
    if (!json) {
      emit(ctx, "  - %s config=unreadable", dirs[i]);
      continue;
    }

    json_get_string(json, display_key, label, sizeof(label));
    if (!label[0] && strcmp(display_key, "label") != 0) {
      json_get_string(json, "label", label, sizeof(label));
    }
    json_get_string(json, "rompath", rompath, sizeof(rompath));
    json_get_string(json, "imgpath", imgpath, sizeof(imgpath));
    json_get_string(json, "gamelist", gamelist, sizeof(gamelist));
    json_get_string(json, "extlist", extlist, sizeof(extlist));
    json_get_string(json, "launch", launch, sizeof(launch));
    launcher_count = json_count_launchlist_entries(json);

    if (rompath[0] && resolve_path(resolved_rompath, sizeof(resolved_rompath), dir_path, rompath)) {
      rom_stats = scan_regular_files(resolved_rompath, extlist);
      if (rom_stats.exists) {
        totals.rom_dirs++;
        totals.rom_files += rom_stats.files;
        totals.rom_matching_files += rom_stats.matching_files;
      }
    }

    if (imgpath[0] && resolve_path(resolved_imgpath, sizeof(resolved_imgpath), dir_path, imgpath)) {
      image_stats = scan_regular_files(resolved_imgpath, "png|jpg|jpeg|bmp");
      if (image_stats.exists) {
        totals.image_dirs++;
        totals.image_files += image_stats.matching_files;
      }
    }

    if (gamelist[0] && resolve_path(resolved_gamelist, sizeof(resolved_gamelist), dir_path, gamelist)) {
      gamelist_size = file_size_or_negative(resolved_gamelist);
      if (gamelist_size >= 0) {
        totals.gamelists++;
      }
    }
    totals.launchers += launcher_count;

    emit(ctx, "  - %s label=\"%s\" launch=\"%s\" rompath=\"%s\" imgpath=\"%s\" extlist=\"%s\"",
         dirs[i], label[0] ? label : "(none)", launch[0] ? launch : "(none)",
         rompath[0] ? rompath : "(none)", imgpath[0] ? imgpath : "(none)",
         extlist[0] ? extlist : "(none)");
    if (rompath[0]) {
      emit(ctx, "    roms path=%s exists=%s files=%zu matched=%zu subdirs=%zu",
           resolved_rompath[0] ? resolved_rompath : "(unresolved)", rom_stats.exists ? "yes" : "no",
           rom_stats.files, rom_stats.matching_files, rom_stats.subdirs);
    }
    if (imgpath[0]) {
      emit(ctx, "    artwork path=%s exists=%s images=%zu subdirs=%zu",
           resolved_imgpath[0] ? resolved_imgpath : "(unresolved)", image_stats.exists ? "yes" : "no",
           image_stats.matching_files, image_stats.subdirs);
    }
    if (gamelist[0]) {
      emit(ctx, "    gamelist path=%s exists=%s size=%ld",
           resolved_gamelist[0] ? resolved_gamelist : "(unresolved)", gamelist_size >= 0 ? "yes" : "no",
           gamelist_size);
    }
    if (launcher_count > 0) {
      emit(ctx, "    launchlist entries=%zu", launcher_count);
    }
    totals.configs++;
    free(json);
  }

  free_list(dirs, count);
  return totals;
}

static void ensure_log_dir(const char *root) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/logs", root);
  mkdir(root, 0755);
  mkdir(path, 0755);
}

static void maybe_run_boot_resume(struct log_ctx *ctx, const char *plumos_root,
                                  const char *sd_root, const char *log_path) {
  char text_ui[PATH_MAX];
  char cmd[FRONTEND_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!join_path(text_ui, sizeof(text_ui), plumos_root, "bin/plumos-text-ui")) {
    emit(ctx, "[boot-resume] skip reason=text-ui-path-too-long");
    return;
  }
  if (!is_file(text_ui)) {
    emit(ctx, "[boot-resume] skip reason=text-ui-missing path=%s", text_ui);
    return;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, sd_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " boot --execute >> ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    emit(ctx, "[boot-resume] skip reason=command-too-long");
    return;
  }

  emit(ctx, "[boot-resume] begin");
  rc = system(cmd);
  if (rc == -1) {
    emit(ctx, "[boot-resume] failed reason=system-call");
    return;
  }
  if (WIFEXITED(rc)) {
    emit(ctx, "[boot-resume] end status=%d exit=%d", rc, WEXITSTATUS(rc));
    return;
  }
  emit(ctx, "[boot-resume] end status=%d", rc);
}

int main(int argc, char **argv) {
  const char *mode = getenv("PLUMOS_FRONTEND_MODE");
  const char *sd_root = getenv("PLUMOS_SDCARD_ROOT");
  const char *plumos_root = getenv("PLUMOS_ROOT");
  char log_path[PATH_MAX];
  struct log_ctx ctx;
  time_t now;
  struct scan_totals emu_totals;
  struct scan_totals rapp_totals;
  struct scan_totals app_totals;
  struct scan_totals theme_totals;
  char recent_path[PATH_MAX];
  long recent_size;
  size_t recent_entries = 0;

  (void)argc;
  (void)argv;

  if (!mode || !mode[0]) {
    mode = "boot";
  }
  if (!sd_root || !sd_root[0]) {
    sd_root = "/mnt/SDCARD";
  }
  if (!plumos_root || !plumos_root[0]) {
    plumos_root = "/mnt/SDCARD/plumos";
  }

  ensure_log_dir(plumos_root);
  snprintf(log_path, sizeof(log_path), "%s/logs/plumos-frontend.log", plumos_root);
  ctx.log = fopen(log_path, "a");
  ctx.stdout_enabled = strcmp(mode, "manual") == 0 || getenv("PLUMOS_FRONTEND_STDOUT") != NULL;

  now = time(NULL);
  emit(&ctx, "===== plumOS frontend prototype =====");
  emit(&ctx, "time=%ld mode=%s sd_root=%s plumos_root=%s", (long)now, mode, sd_root, plumos_root);

  if (strcmp(mode, "manual") != 0) {
    maybe_run_boot_resume(&ctx, plumos_root, sd_root, log_path);
  }

  emu_totals = scan_category(&ctx, sd_root, "Emu", "label");
  rapp_totals = scan_category(&ctx, sd_root, "RApp", "label");
  app_totals = scan_category(&ctx, sd_root, "App", "label");
  theme_totals = scan_category(&ctx, sd_root, "Themes", "name");

  snprintf(recent_path, sizeof(recent_path), "%s/Roms/recentlist.json", sd_root);
  recent_size = file_size_or_negative(recent_path);
  if (recent_size >= 0) {
    char *recent_json = read_file(recent_path, NULL);
    recent_entries = count_nonempty_lines(recent_json);
    free(recent_json);
  }
  emit(&ctx, "[recent] path=%s exists=%s size=%ld entries=%zu", recent_path,
       recent_size >= 0 ? "yes" : "no", recent_size, recent_entries);
  emit(&ctx, "summary configs emu=%d rapp=%d app=%d themes=%d", emu_totals.configs,
       rapp_totals.configs, app_totals.configs, theme_totals.configs);
  emit(&ctx, "summary roms dirs=%zu files=%zu matched=%zu", emu_totals.rom_dirs + rapp_totals.rom_dirs,
       emu_totals.rom_files + rapp_totals.rom_files,
       emu_totals.rom_matching_files + rapp_totals.rom_matching_files);
  emit(&ctx, "summary artwork dirs=%zu images=%zu",
       emu_totals.image_dirs + rapp_totals.image_dirs,
       emu_totals.image_files + rapp_totals.image_files);
  emit(&ctx, "summary metadata gamelists=%zu launchers=%zu",
       emu_totals.gamelists + rapp_totals.gamelists,
       emu_totals.launchers + rapp_totals.launchers);

  if (ctx.log) {
    fclose(ctx.log);
  }

  if (strcmp(mode, "manual") == 0) {
    return 0;
  }

  return 75;
}
