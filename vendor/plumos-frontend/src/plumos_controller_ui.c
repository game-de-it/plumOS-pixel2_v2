#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <linux/fb.h>

extern char **environ;

#ifdef PLUMOS_ENABLE_MALI_RENDERER
#include "plumos_mali_renderer.h"
#endif

#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
#include "plumos_fbdev_renderer.h"
#endif

#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
#include "plumos_pixel2_compat_gfx_renderer.h"
#endif

#ifndef PLUMOS_MALI_SETTING_FLASH_MARKER
#define PLUMOS_MALI_SETTING_FLASH_MARKER "@{F:"
#endif

#define PLUMOS_VOLUME_MAX 20
#define PLUMOS_VOLUME_DEFAULT 8

static volatile sig_atomic_t g_terminate_requested = 0;
static long long g_frame_stats_window_ms = 0;
static unsigned int g_frame_stats_count = 0;

static long long current_time_ms(void);
static long long current_time_us(void);

static void handle_terminate_signal(int signo) {
  (void)signo;
  g_terminate_requested = 1;
}

static void record_frame_stats(void) {
  const char *path = getenv("PLUMOS_FRAME_STATS_PATH");
  long long now_ms;
  long long elapsed_ms;
  FILE *f;

  if (!path || !path[0]) {
    return;
  }
  now_ms = current_time_ms();
  if (g_frame_stats_window_ms <= 0) {
    g_frame_stats_window_ms = now_ms;
    g_frame_stats_count = 1;
    return;
  }
  g_frame_stats_count++;
  elapsed_ms = now_ms - g_frame_stats_window_ms;
  if (elapsed_ms < 1000) {
    return;
  }
  f = fopen(path, "a");
  if (f) {
    fprintf(f, "frames=%u elapsed_ms=%lld fps=%.3f target_interval_ms=16\n",
            g_frame_stats_count, elapsed_ms,
            (double)g_frame_stats_count * 1000.0 / (double)elapsed_ms);
    fclose(f);
  }
  g_frame_stats_window_ms = now_ms;
  g_frame_stats_count = 0;
}

#if defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_LINUX_INPUT 1
#endif
#endif

#ifndef PLUMOS_HAS_LINUX_INPUT
#define EV_KEY 0x01
struct input_event {
  struct timeval time;
  unsigned short type;
  unsigned short code;
  int value;
};
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef KEY_ESC
#define KEY_ESC 1
#endif
#ifndef KEY_6
#define KEY_6 7
#endif
#ifndef KEY_8
#define KEY_8 9
#endif
#ifndef KEY_9
#define KEY_9 10
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 14
#endif
#ifndef KEY_TAB
#define KEY_TAB 15
#endif
#ifndef KEY_Q
#define KEY_Q 16
#endif
#ifndef KEY_E
#define KEY_E 18
#endif
#ifndef KEY_T
#define KEY_T 20
#endif
#ifndef KEY_R
#define KEY_R 19
#endif
#ifndef KEY_LEFTCTRL
#define KEY_LEFTCTRL 29
#endif
#ifndef KEY_LEFTSHIFT
#define KEY_LEFTSHIFT 42
#endif
#ifndef KEY_LEFTALT
#define KEY_LEFTALT 56
#endif
#ifndef KEY_Z
#define KEY_Z 44
#endif
#ifndef KEY_X
#define KEY_X 45
#endif
#ifndef KEY_Y
#define KEY_Y 21
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 57
#endif
#ifndef KEY_HOME
#define KEY_HOME 102
#endif
#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN 114
#endif
#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP 115
#endif
#ifndef KEY_POWER
#define KEY_POWER 116
#endif
#ifndef KEY_UP
#define KEY_UP 103
#endif
#ifndef KEY_LEFT
#define KEY_LEFT 105
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT 106
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 108
#endif
#ifndef KEY_RIGHTCTRL
#define KEY_RIGHTCTRL 97
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 28
#endif

#ifndef EV_ABS
#define EV_ABS 0x03
#endif
#ifndef ABS_X
#define ABS_X 0x00
#endif
#ifndef ABS_Y
#define ABS_Y 0x01
#endif
#ifndef ABS_HAT0X
#define ABS_HAT0X 0x10
#endif
#ifndef ABS_HAT0Y
#define ABS_HAT0Y 0x11
#endif

#ifndef BTN_SOUTH
#define BTN_SOUTH 304
#endif
#ifndef BTN_EAST
#define BTN_EAST 305
#endif
#ifndef BTN_NORTH
#define BTN_NORTH 307
#endif
#ifndef BTN_WEST
#define BTN_WEST 308
#endif
#ifndef BTN_TL
#define BTN_TL 310
#endif
#ifndef BTN_TR
#define BTN_TR 311
#endif
#ifndef BTN_TL2
#define BTN_TL2 312
#endif
#ifndef BTN_TR2
#define BTN_TR2 313
#endif
#ifndef BTN_SELECT
#define BTN_SELECT 314
#endif
#ifndef BTN_START
#define BTN_START 315
#endif
#ifndef BTN_MODE
#define BTN_MODE 316
#endif
#ifndef BTN_TRIGGER_HAPPY1
#define BTN_TRIGGER_HAPPY1 704
#endif
#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 544
#endif
#ifndef BTN_DPAD_DOWN
#define BTN_DPAD_DOWN 545
#endif
#ifndef BTN_DPAD_LEFT
#define BTN_DPAD_LEFT 546
#endif
#ifndef BTN_DPAD_RIGHT
#define BTN_DPAD_RIGHT 547
#endif
#ifndef KEY_SELECT
#define KEY_SELECT 0x161
#endif
#ifndef KEY_MENU
#define KEY_MENU 139
#endif

#define UI_MAX_TOP 128
#define UI_ROM_INITIAL_CAPACITY 256
#define UI_ROM_CURSOR_MEMORY_MAX 64
#define UI_MAX_MENU 64
#define UI_MAX_SETTINGS 64
#define UI_MAX_SCRAPING_CHOICES 64
#define UI_MAX_CORE_PROFILES 32
#define UI_COMMAND_MAX 8192
#define UI_PATH_MAX 1024
#define UI_RENDER_MAX_LINES 64
#define UI_RENDER_LINE_MAX 512
#define UI_THUMBNAIL_RESULT_MAX_LINES 256
#define UI_ABS_REPEAT_CODE_BASE 0x10000u
#define UI_ABS_AXIS_DEADZONE 512
#define UI_KEY_REPEAT_DELAY_MS 350
#define UI_KEY_REPEAT_INTERVAL_MS 95
#define UI_SETTING_VALUE_REPEAT_INTERVAL_MS 250
#define UI_MAX_WIFI_NETWORKS 64
#define UI_WIFI_SSID_MAX 160
#define UI_WIFI_PASSWORD_MAX 64
#define UI_WIFI_KEYBOARD_ROWS 7
#define UI_WIFI_COMMAND_ROW 6
#define UI_WIFI_COMMAND_COUNT 5
#define UI_TOP_REFRESH_MIN_VISIBLE_MS 1000
#define UI_TOP_STATUS_REFRESH_MS 5000
#define UI_THUMBNAIL_RESULT_WINDOW 11
#define UI_SCRAPING_FIELD_COUNT 3
#define UI_SCRAPING_FIELD_IMAGE 0
#define UI_SCRAPING_FIELD_EXISTING 1
#define UI_SCRAPING_FIELD_SYSTEM 2
#define UI_GALLERY_TRANSITION_MS 360
#define UI_SDCARD_CLEANUP_MIN_INTERVAL_MS 60000
#define UI_ROM_SCAN_REFRESH_MIN_INTERVAL_MS 3000
#define UI_Pixel2_BRIGHTNESS_REAPPLY_DELAY_MS 3000
#define UI_FE_READY_FLAG_PATH "/tmp/plumos-fe-ready"
#define Pixel2_LCD_BACKLIGHT_PATH "/sys/devices/virtual/disp/disp/attr/lcdbl"
#define Pixel2_DISPLAY_ENHANCE_PATH "/sys/devices/virtual/disp/disp/attr/enhance"
#define Pixel2_PWM_ENABLE_PATH "/sys/class/pwm/pwmchip0/pwm0/enable"
#define Pixel2_PWM_DUTY_PATH "/sys/class/pwm/pwmchip0/pwm0/duty_cycle"
#define Pixel2_MI_DISP0_PATH "/proc/mi_modules/mi_disp/mi_disp0"
#define Pixel2_STOCK_SYSTEM_JSON_PATH "/mnt/plumos/system.json"
#define LEGACY_SUNXI_BACKLIGHT_PATH "/sys/class/backlight/sunxi_backlight/brightness"
#define LEGACY_SUNXI_ENHANCE_BRIGHT_PATH "/sys/class/disp/disp/attr/enhance_bright"
#define LEGACY_SUNXI_ENHANCE_CONTRAST_PATH "/sys/class/disp/disp/attr/enhance_contrast"
#define LEGACY_SUNXI_ENHANCE_SATURATION_PATH "/sys/class/disp/disp/attr/enhance_saturation"
#define LEGACY_SUNXI_ENHANCE_MODE_PATH "/sys/class/disp/disp/attr/enhance_mode"
#define LEGACY_SUNXI_COLOR_TEMPERATURE_PATH "/sys/class/disp/disp/attr/color_temperature"
#define PIXEL2_BACKLIGHT_PATH "/sys/class/backlight/backlight/brightness"

static int replace_json_key_value_atomic(const char *path, const char *key,
                                         const char *literal);

struct top_entry {
  char id[64];
  char display_name[128];
  char default_launch_profile[128];
  long rom_count;
  int pinned;
  int virtual_entry;
};

struct scraping_choice {
  char id[64];
  char display_name[128];
  long rom_count;
};

struct scraping_kind_choice {
  const char *display_name;
  const char *scraper_kind;
};

static const struct scraping_kind_choice SCRAPING_KIND_CHOICES[] = {
    {"Box Art", "Named_Boxarts"},
    {"Title Screen", "Named_Titles"},
};

struct core_profile_choice {
  char id[128];
};

struct rom_entry {
  char system_id[64];
  char title[256];
  char relative_path[UI_PATH_MAX];
  char path[UI_PATH_MAX];
  char thumbnail[UI_PATH_MAX];
  char launch_profile[128];
  char detail[256];
  char extension[32];
  int resume_available;
  int is_navigation_directory;
  int is_favorite;
};

struct rom_cursor_memory {
  char system_id[64];
  char directory[UI_PATH_MAX];
  char relative_path[UI_PATH_MAX];
};

static int dirname_path(char *out, size_t out_size, const char *path);
static long clamp_long(long value, long min_value, long max_value);

static int rom_entry_alias_root_path(const struct rom_entry *entry, char *out,
                                     size_t out_size) {
  const char *slash;
  size_t alias_len;
  size_t rel_len;
  size_t path_len;
  size_t prefix_len;

  if (!entry || !out || out_size == 0 || !entry->path[0]) {
    return 0;
  }
  out[0] = '\0';
  if (!entry->relative_path[0]) {
    return dirname_path(out, out_size, entry->path);
  }
  slash = strchr(entry->relative_path, '/');
  if (!slash || slash == entry->relative_path) {
    return dirname_path(out, out_size, entry->path);
  }
  alias_len = (size_t)(slash - entry->relative_path);
  rel_len = strlen(entry->relative_path);
  path_len = strlen(entry->path);
  if (path_len < rel_len ||
      strcmp(entry->path + path_len - rel_len, entry->relative_path) != 0) {
    return dirname_path(out, out_size, entry->path);
  }
  prefix_len = path_len - rel_len;
  if (prefix_len + alias_len + 1 > out_size) {
    return 0;
  }
  memcpy(out, entry->path, prefix_len);
  memcpy(out + prefix_len, entry->relative_path, alias_len);
  out[prefix_len + alias_len] = '\0';
  return 1;
}

struct menu_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char action[256];
  int confirm;
  int background;
  int show_results;
};

struct setting_entry {
  char id[64];
  char display_name[128];
  char value[256];
};

struct setting_choice {
  const char *raw;
  const char *display;
};

#define UI_GRAPHIC_THEME_CHOICE_MAX 32
#define UI_TRANSLATION_MAX 512
#define UI_TRANSLATION_KEY_MAX 96
#define UI_TRANSLATION_VALUE_MAX 256

struct translation_entry {
  char key[UI_TRANSLATION_KEY_MAX];
  char value[UI_TRANSLATION_VALUE_MAX];
};

struct graphic_theme_choice {
  char raw[64];
  char display[128];
};

struct performance_cpu_preset {
  const char *label;
  const char *policy;
  long freq_khz;
};

struct cpu_policy_snapshot {
  int saved;
  char governor[32];
  char min_freq[32];
  char max_freq[32];
  char setspeed[32];
  char cpuinfo_min_freq[32];
  char cpuinfo_max_freq[32];
};

struct power_entry {
  const char *id;
  const char *display_name;
  const char *detail;
};

struct wifi_network_entry {
  char ssid[UI_WIFI_SSID_MAX];
  char security[24];
  char signal[24];
};

struct theme_state {
  int loaded;
  int fallback;
  char id[64];
  char target[32];
  char display_name[128];
  char path[PATH_MAX];
  char layout_preset[64];
  char font_ui[UI_PATH_MAX];
  char font_fallback[64];
  char background[UI_PATH_MAX];
  char gallery_background[UI_PATH_MAX];
  char system_logo_root[UI_PATH_MAX];
  char placeholder_thumbnail[UI_PATH_MAX];
  char graphic_top_layout[32];
  char graphic_transition[32];
  char graphic_transition_axis[32];
  char graphic_transition_easing[32];
  long graphic_transition_ms;
  char color_background[16];
  char color_foreground[16];
  char color_muted[16];
  char color_accent[16];
  char color_panel[16];
  char color_panel_inner[16];
  char color_media_panel[16];
  char color_selection_background[16];
  char color_selection_foreground[16];
  char color_danger[16];
  char status[256];
};

struct device_settings {
  int loaded;
  int wpa_loaded;
  int wifi_enabled;
  int wifi_runtime_enabled;
  int automatic_time_enabled;
  int lid_suspend_enabled;
  long volume;
  long brightness;
  long lumination;
  long contrast;
  long hue;
  long saturation;
  char audio_output[32];
  char language[64];
  char theme[UI_PATH_MAX];
  char timezone[64];
  char model[64];
  char plumos_version[64];
  char vendor_runtime[64];
  char kernel_version[128];
  char sdcard_storage[128];
  char storage_health[128];
  char memory_usage[128];
  char firmware_version[128];
  char gpu_runtime[128];
  char network_status_source[128];
  char network_control_status[128];
  char ssh_status[128];
  int ssh_service_running;
  int ftp_service_running;
  int sftp_service_running;
  int samba_service_running;
  int adb_service_running;
  char ftp_status[128];
  char sftp_status[128];
  char samba_status[128];
  char adb_status[128];
  char brightness_backend[128];
  char volume_backend[128];
  char wifi_state[64];
  char wifi_ip[64];
  char wifi_rssi[64];
  char wifi_linkspeed[64];
  char wifi_frequency[64];
  char status[256];
};

struct frontend_settings {
  int show_empty_systems;
  int show_favorites_on_top;
  int show_recent_on_top;
  int rom_cursor_wrap;
  char boot_resume_mode[32];
  char ui_mode[32];
  char top_mode[32];
  char rom_mode[32];
  char theme_id[64];
  char graphic_theme_id[64];
  char graphic_top_layout[32];
  char graphic_transition[32];
  char graphic_transition_axis[32];
  char graphic_transition_easing[32];
  long graphic_transition_ms;
  char sort_systems[64];
  char sort_roms[64];
  char rom_scan_policy[64];
  long rom_scan_slow_threshold_ms;
  long rom_scan_test_file_count;
  char last_system_id[64];
};

enum ui_screen {
  SCREEN_TOP = 0,
  SCREEN_ROMS = 1,
  SCREEN_START_MENU = 2,
  SCREEN_FAVORITES = 3,
  SCREEN_RECENT = 4,
  SCREEN_SETTINGS = 5,
  SCREEN_POWER_MENU = 6,
  SCREEN_HELP = 7,
  SCREEN_CORE_SELECT = 8,
  SCREEN_NETWORK_RESCUE = 9,
  SCREEN_WIFI_CONNECT = 10,
  SCREEN_THUMBNAIL_RESULTS = 11,
  SCREEN_THUMBNAIL_RUNNING = 12,
  SCREEN_SCRAPING = 13,
  SCREEN_GALLERY = 14,
  SCREEN_TOP_REFRESH_RUNNING = 15,
  SCREEN_POWER_ACTION_RUNNING = 16
};

enum wifi_connect_stage {
  WIFI_CONNECT_SELECT = 0,
  WIFI_CONNECT_PASSWORD,
  WIFI_CONNECT_RESULT
};

enum settings_category {
  SETTINGS_CATEGORY_UI = 0,
  SETTINGS_CATEGORY_UI_THEME,
  SETTINGS_CATEGORY_SYSTEM,
  SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR,
  SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST,
  SETTINGS_CATEGORY_SYSTEM_TIME,
  SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL,
  SETTINGS_CATEGORY_SYSTEM_INFORMATION,
  SETTINGS_CATEGORY_SYSTEM_FACTORY_RESET,
  SETTINGS_CATEGORY_NETWORK,
  SETTINGS_CATEGORY_NETWORK_SERVICE,
  SETTINGS_CATEGORY_NETWORK_INFORMATION,
  SETTINGS_CATEGORY_PERFORMANCE
};

enum ui_action {
  ACTION_NONE = 0,
  ACTION_UP,
  ACTION_DOWN,
  ACTION_LEFT,
  ACTION_RIGHT,
  ACTION_A,
  ACTION_B,
  ACTION_START,
  ACTION_SELECT,
  ACTION_X,
  ACTION_Y,
  ACTION_FUNCTION,
  ACTION_POWER,
  ACTION_VOLUME_DOWN,
  ACTION_VOLUME_UP,
  ACTION_QUIT
};

struct ui_state {
  const char *sdcard_root;
  char plumos_root[PATH_MAX];
  char top_cache_path[PATH_MAX];
  char settings_path[PATH_MAX];
  char system_config_path[PATH_MAX];
  char systems_path[PATH_MAX];
  char wpa_status_path[PATH_MAX];
  char menus_path[PATH_MAX];
  char apps_path[PATH_MAX];
  char favorites_path[PATH_MAX];
  char recent_path[PATH_MAX];
  int show_all;
  int refresh;
  int no_clear;
  int once;
  int power_overlay;
  int exit_requested;
  int renderer_mali;
  int renderer_fbdev;
  int renderer_pixel2_compat_gfx;
  int rescue_network;
  int render_failed;
  int fe_ready_flag_written;
  int timeout_sec;
  enum ui_screen screen;
  enum ui_screen rom_entry_screen;
  enum ui_screen back_screen;
  enum ui_screen power_back_screen;
  enum ui_screen core_back_screen;
  enum ui_screen wifi_back_screen;
  size_t top_cursor;
  size_t top_transition_from_cursor;
  size_t top_transition_from_page;
  size_t top_transition_to_page;
  long long top_transition_start_ms;
  long top_transition_duration_ms;
  int top_transition_direction;
  int top_transition_active;
  size_t gallery_transition_from_cursor;
  size_t gallery_transition_to_cursor;
  long long gallery_transition_start_ms;
  long gallery_transition_duration_ms;
  int gallery_transition_direction;
  int gallery_transition_active;
  size_t gallery_pending_cursor;
  int gallery_pending_direction;
  int gallery_pending_active;
  enum ui_screen gallery_back_screen;
  size_t rom_cursor;
  size_t menu_cursor;
  size_t settings_cursor;
  enum settings_category settings_category;
  size_t settings_blink_cursor;
  int settings_blink_direction;
  long long settings_blink_until_ms;
  size_t power_cursor;
  char power_action[16];
  struct top_entry top_entries[UI_MAX_TOP];
  size_t top_count;
  struct rom_entry *rom_entries;
  size_t rom_count;
  size_t rom_capacity;
  struct rom_cursor_memory rom_cursor_memory[UI_ROM_CURSOR_MEMORY_MAX];
  size_t rom_cursor_memory_count;
  struct menu_entry menu_entries[UI_MAX_MENU];
  size_t menu_count;
  char menu_id[64];
  char menu_title[128];
  char thumbnail_result_lines[UI_THUMBNAIL_RESULT_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t thumbnail_result_count;
  size_t thumbnail_result_cursor;
  char thumbnail_running_title[128];
  char thumbnail_result_return_app_id[64];
  char thumbnail_running_phase[32];
  char thumbnail_running_system[64];
  long thumbnail_progress_current;
  long thumbnail_progress_total;
  long thumbnail_progress_downloaded;
  long thumbnail_progress_no_match;
  long thumbnail_progress_failed;
  struct scraping_choice scraping_choices[UI_MAX_SCRAPING_CHOICES];
  size_t scraping_choice_count;
  size_t scraping_choice_cursor;
  size_t scraping_menu_cursor;
  size_t scraping_kind_cursor;
  int scraping_replace_existing;
  struct setting_entry setting_entries[UI_MAX_SETTINGS];
  size_t setting_count;
  char factory_reset_pending_target[16];
  long long factory_reset_pending_until_ms;
  struct wifi_network_entry wifi_networks[UI_MAX_WIFI_NETWORKS];
  size_t wifi_count;
  size_t wifi_cursor;
  enum wifi_connect_stage wifi_stage;
  long long pixel2_compat_brightness_reapply_due_ms;
  int manual_time_initialized;
  long manual_time_year;
  long manual_time_month;
  long manual_time_day;
  long manual_time_hour;
  long manual_time_minute;
  size_t wifi_key_row;
  size_t wifi_key_col;
  int wifi_key_shift;
  char wifi_password[UI_WIFI_PASSWORD_MAX + 1];
  char wifi_result_title[96];
  char wifi_result_ip[64];
  char wifi_result_gateway[64];
  char wifi_result_gateway_ping[32];
  char wifi_result_stage[64];
  int wifi_result_success;
  struct frontend_settings frontend_settings;
  struct theme_state theme;
  struct device_settings device;
  char input_event_path[PATH_MAX];
  char power_event_path[PATH_MAX];
  int input_event_fd;
  int power_event_fd;
  long long ignore_input_until_ms;
  enum ui_action repeat_action;
  unsigned int repeat_key_code;
  long long repeat_next_ms;
  long long sdcard_cleanup_last_ms;
  pid_t rom_scan_refresh_pid;
  long long rom_scan_refresh_last_ms;
  char rom_scan_refresh_system_id[64];
  int rom_scan_refresh_suppressed;
  int rom_scan_background_started;
  char mali_rotation[16];
  char mali_tty_entry_scale[8];
  char render_lines[UI_RENDER_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t render_line_count;
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  struct plumos_mali_renderer mali_renderer;
#endif
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
  struct plumos_fbdev_renderer fbdev_renderer;
#endif
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  struct plumos_pixel2_compat_gfx_renderer pixel2_compat_gfx_renderer;
#endif
  char current_system_id[64];
  char current_system_name[128];
  char rom_directory[UI_PATH_MAX];
  char fb_path[PATH_MAX];
  char egl_path[PATH_MAX];
  char gles_path[PATH_MAX];
  char fbdev_rotation[16];
  char pixel2_compat_gfx_rotation[16];
  char mali_font_path[PATH_MAX];
  char mali_fallback_font_path[PATH_MAX];
  int renderer_active;
  char core_target_system_id[64];
  char core_target_relative_path[UI_PATH_MAX];
  struct core_profile_choice core_profiles[UI_MAX_CORE_PROFILES];
  size_t core_profile_count;
  size_t core_profile_cursor;
  size_t core_menu_cursor;
  char core_current_profile[128];
  char core_current_source[64];
  char core_cpu_policy[32];
  char core_cpu_label[64];
  char core_cpu_source[64];
  long core_cpu_freq_khz;
  char core_lines[UI_RENDER_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t core_line_count;
  char power_target_system_id[64];
  char power_target_relative_path[UI_PATH_MAX];
  char power_target_launch_profile[128];
  char performance_system_id[64];
  char performance_system_name[128];
  char performance_cpu_policy[32];
  char performance_cpu_label[64];
  long performance_cpu_freq_khz;
  struct translation_entry translations[UI_TRANSLATION_MAX];
  size_t translation_count;
  char translation_language[64];
  char translation_status[128];
  char status[256];
};

static int load_top_entries(struct ui_state *ui);
static void set_status(struct ui_state *ui, const char *text);

static void ui_clear_rom_entries(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->rom_count = 0;
  ui->rom_cursor = 0;
}

static void ui_free_rom_entries(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  free(ui->rom_entries);
  ui->rom_entries = NULL;
  ui->rom_count = 0;
  ui->rom_capacity = 0;
  ui->rom_cursor = 0;
}

static int ui_reserve_rom_entries(struct ui_state *ui, size_t min_capacity) {
  struct rom_entry *next_entries;
  const size_t max_capacity = ((size_t)-1) / sizeof(ui->rom_entries[0]);
  size_t next_capacity;

  if (!ui) {
    return 0;
  }
  if (min_capacity > max_capacity) {
    return 0;
  }
  if (ui->rom_capacity >= min_capacity) {
    return 1;
  }

  next_capacity = ui->rom_capacity ? ui->rom_capacity : UI_ROM_INITIAL_CAPACITY;
  while (next_capacity < min_capacity) {
    if (next_capacity > max_capacity / 2) {
      next_capacity = max_capacity;
      break;
    }
    next_capacity *= 2;
  }
  next_entries = (struct rom_entry *)realloc(
      ui->rom_entries, next_capacity * sizeof(ui->rom_entries[0]));
  if (!next_entries) {
    return 0;
  }
  ui->rom_entries = next_entries;
  ui->rom_capacity = next_capacity;
  return 1;
}

static int ui_append_rom_entry(struct ui_state *ui, const struct rom_entry *entry) {
  if (!ui || !entry || !ui_reserve_rom_entries(ui, ui->rom_count + 1)) {
    return 0;
  }
  ui->rom_entries[ui->rom_count++] = *entry;
  return 1;
}

static const struct power_entry POWER_ENTRIES[] = {
    {"sleep", "Sleep", "sync and enter sleep"},
    {"reboot", "Reboot", "sync and restart OS"},
    {"shutdown", "Shutdown", "sync and power off"},
    {"cancel", "Cancel", "return without changing state"},
};

static const size_t POWER_ENTRY_COUNT = sizeof(POWER_ENTRIES) / sizeof(POWER_ENTRIES[0]);

static const struct performance_cpu_preset PERFORMANCE_CPU_PRESETS[] = {
    {"Interactive", "interactive", 0},
    {"Performance", "performance", 0},
    {"Ondemand", "ondemand", 0},
    {"Schedutil", "schedutil", 0},
    {"Conservative", "conservative", 0},
};

static const size_t PERFORMANCE_CPU_PRESET_COUNT =
    sizeof(PERFORMANCE_CPU_PRESETS) / sizeof(PERFORMANCE_CPU_PRESETS[0]);

enum core_menu_row {
  CORE_MENU_ROW_PROFILE = 0,
  CORE_MENU_ROW_DEFAULT = 1,
  CORE_MENU_ROW_SEPARATOR = 2,
  CORE_MENU_ROW_CPU_FREQ = 3,
  CORE_MENU_ROW_COUNT = 4,
};

static const char *WIFI_KEYBOARD_ROWS_LOWER[UI_WIFI_COMMAND_ROW] = {
    "0123456789",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm",
    "-_.@#$%&!*",
    "?+=:/\\\"'()[]"
};

static const char *WIFI_KEYBOARD_ROWS_UPPER[UI_WIFI_COMMAND_ROW] = {
    "0123456789",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
    "-_.@#$%&!*",
    "?+=:/\\\"'()[]"
};

static const char *WIFI_COMMAND_LABELS[UI_WIFI_COMMAND_COUNT] = {
    "SHIFT", "SPACE", "DEL", "CLEAR", "CONNECT"
};

static long long current_time_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return (long long)time(NULL) * 1000LL;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static long long current_time_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return current_time_ms() * 1000LL;
  }
  return (long long)ts.tv_sec * 1000000LL +
         (long long)(ts.tv_nsec / 1000);
}

static void wait_until_ms(long long deadline_ms) {
  long long now;

  while ((now = current_time_ms()) < deadline_ms) {
    long long remaining = deadline_ms - now;
    if (remaining > 1000) {
      remaining = 1000;
    }
    poll(NULL, 0, (int)remaining);
  }
}

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

static void copy_truncated_string(char *out, size_t out_size, const char *in) {
  size_t len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  len = strlen(in);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, in, len);
  out[len] = '\0';
}

static void copy_prefixed_truncated_string(char *out, size_t out_size,
                                           const char *prefix, const char *value) {
  size_t prefix_len;
  size_t value_len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!prefix) {
    prefix = "";
  }
  if (!value) {
    value = "";
  }
  prefix_len = strlen(prefix);
  if (prefix_len >= out_size) {
    prefix_len = out_size - 1;
  }
  memcpy(out, prefix, prefix_len);
  out[prefix_len] = '\0';
  if (prefix_len + 1 >= out_size) {
    return;
  }
  value_len = strlen(value);
  if (value_len >= out_size - prefix_len) {
    value_len = out_size - prefix_len - 1;
  }
  memcpy(out + prefix_len, value, value_len);
  out[prefix_len + value_len] = '\0';
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

static int append_size_t(char *out, size_t out_size, size_t *pos, size_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%zu", value);
  return append_string(out, out_size, pos, buf);
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

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int ensure_dir_recursive(const char *path) {
  char tmp[PATH_MAX];
  char *p;

  if (!path || !path[0]) {
    return 0;
  }
  if (!copy_string(tmp, sizeof(tmp), path)) {
    return 0;
  }
  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (tmp[0] && mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 0;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static int ensure_parent_dir_for_file(const char *path) {
  char dir[PATH_MAX];

  if (!dirname_path(dir, sizeof(dir), path)) {
    return 0;
  }
  return ensure_dir_recursive(dir);
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

static int valid_relative_rom_path(const char *path) {
  size_t len;

  if (!path || !path[0] || path[0] == '/') {
    return 0;
  }
  len = strlen(path);
  if (strstr(path, "/../") || strncmp(path, "../", 3) == 0 ||
      strcmp(path, "..") == 0 ||
      (len >= 3 && strcmp(path + len - 3, "/..") == 0)) {
    return 0;
  }
  return 1;
}

static int valid_launch_profile_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-' || c == ':')) {
      return 0;
    }
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

static char *trim_ascii_ws(char *s) {
  char *end;

  if (!s) {
    return s;
  }
  while (*s && isspace((unsigned char)*s)) {
    s++;
  }
  end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1])) {
    end--;
  }
  *end = '\0';
  return s;
}

static int valid_language_filename(const char *s) {
  if (!s || !s[0] || strchr(s, '/') || strchr(s, '\\')) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
      return 0;
    }
  }
  return 1;
}

static int translation_set(struct ui_state *ui, const char *key, const char *value) {
  size_t i;

  if (!ui || !key || !key[0] || !value) {
    return 0;
  }
  for (i = 0; i < ui->translation_count; i++) {
    if (strcmp(ui->translations[i].key, key) == 0) {
      return copy_string(ui->translations[i].value,
                         sizeof(ui->translations[i].value), value);
    }
  }
  if (ui->translation_count >= UI_TRANSLATION_MAX) {
    return 0;
  }
  if (!copy_string(ui->translations[ui->translation_count].key,
                   sizeof(ui->translations[ui->translation_count].key), key) ||
      !copy_string(ui->translations[ui->translation_count].value,
                   sizeof(ui->translations[ui->translation_count].value), value)) {
    return 0;
  }
  ui->translation_count++;
  return 1;
}

static int load_translation_file(struct ui_state *ui, const char *path) {
  char *text;
  char *p;
  int loaded = 0;

  if (!ui || !path || !path[0]) {
    return 0;
  }
  text = read_file(path, NULL);
  if (!text) {
    return 0;
  }
  p = text;
  while (p && *p) {
    char *line = p;
    char *next = strchr(p, '\n');
    char *eq;
    char *key;
    char *value;

    if (next) {
      *next = '\0';
      p = next + 1;
    } else {
      p += strlen(p);
    }
    if ((unsigned char)line[0] == 0xef &&
        line[1] && (unsigned char)line[1] == 0xbb &&
        line[2] && (unsigned char)line[2] == 0xbf) {
      line += 3;
    }
    key = trim_ascii_ws(line);
    if (!key[0] || key[0] == '#') {
      continue;
    }
    eq = strchr(key, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    value = trim_ascii_ws(eq + 1);
    key = trim_ascii_ws(key);
    if (!key[0]) {
      continue;
    }
    if (translation_set(ui, key, value)) {
      loaded = 1;
    }
  }
  free(text);
  return loaded;
}

static int load_translations(struct ui_state *ui) {
  char lang_dir[PATH_MAX];
  char lang_path[PATH_MAX];
  const char *language;
  int loaded = 0;

  if (!ui) {
    return 0;
  }
  ui->translation_count = 0;
  ui->translation_language[0] = '\0';
  ui->translation_status[0] = '\0';

  language = valid_language_filename(ui->device.language)
                 ? ui->device.language
                 : "en.lang";
  if (!join_path(lang_dir, sizeof(lang_dir), ui->plumos_root, "share/frontend/lang")) {
    copy_string(ui->translation_status, sizeof(ui->translation_status),
                "language path too long");
    return 0;
  }
  if (join_path(lang_path, sizeof(lang_path), lang_dir, "en.lang")) {
    loaded = load_translation_file(ui, lang_path);
  }
  if (strcmp(language, "en.lang") != 0 &&
      join_path(lang_path, sizeof(lang_path), lang_dir, language)) {
    loaded = load_translation_file(ui, lang_path) || loaded;
  }
  copy_string(ui->translation_language, sizeof(ui->translation_language), language);
  snprintf(ui->translation_status, sizeof(ui->translation_status), "%s %s",
           loaded ? "language loaded" : "language fallback",
           language);
  return loaded;
}

static const char *tr(const struct ui_state *ui, const char *key,
                      const char *fallback) {
  size_t i;

  if (!fallback) {
    fallback = "";
  }
  if (!ui || !key || !key[0]) {
    return fallback;
  }
  for (i = 0; i < ui->translation_count; i++) {
    if (strcmp(ui->translations[i].key, key) == 0 &&
        ui->translations[i].value[0]) {
      return ui->translations[i].value;
    }
  }
  return fallback;
}

static void copy_tr(struct ui_state *ui, const char *key, const char *fallback,
                    char *out, size_t out_size) {
  const char *value = tr(ui, key, fallback);
  if (value == out) {
    return;
  }
  copy_string(out, out_size, value);
}

static int read_key_value_file(const char *path, const char *key, char *out, size_t out_size) {
  char *text;
  char *p;
  size_t key_len;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!path || !key || !key[0]) {
    return 0;
  }
  text = read_file(path, NULL);
  if (!text) {
    return 0;
  }
  key_len = strlen(key);
  p = text;
  while (p && *p) {
    char *line = p;
    char *next = strchr(p, '\n');
    if (next) {
      *next = '\0';
      p = next + 1;
    } else {
      p += strlen(p);
    }
    if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
      int ok = copy_string(out, out_size, line + key_len + 1);
      free(text);
      return ok;
    }
  }
  free(text);
  return 0;
}

static int read_first_line_file(const char *path, char *out, size_t out_size) {
  FILE *f;
  size_t len;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!path || !path[0]) {
    return 0;
  }
  f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  if (!fgets(out, (int)out_size, f)) {
    fclose(f);
    out[0] = '\0';
    return 0;
  }
  fclose(f);
  len = strlen(out);
  while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' ||
                     isspace((unsigned char)out[len - 1]))) {
    out[--len] = '\0';
  }
  return out[0] != '\0';
}

static void trim_line_end(char *s);
static FILE *open_plumos_script_pipe(struct ui_state *ui, const char *script,
                                     const char *arg1, const char *arg2,
                                     pid_t *child_pid);
static int close_plumos_script_pipe(FILE *stream, pid_t child_pid);

static int read_network_service_status(struct ui_state *ui, const char *service,
                                       char *status, size_t status_size,
                                       int *running_out) {
  char script[PATH_MAX];
  char line[256];
  char state[64] = "";
  char summary[128] = "";
  char enabled_value[32] = "";
  FILE *pipe;
  pid_t child_pid;
  int running = 0;
  int installed = 1;
  int enabled = 0;
  int enabled_seen = 0;

  if (status && status_size > 0) {
    status[0] = '\0';
  }
  if (running_out) {
    *running_out = 0;
  }
  if (!ui || !service || !service[0] ||
      !join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-services")) {
    copy_string(status, status_size, "Not Installed");
    return 0;
  }
  if (!file_exists(script)) {
    copy_string(status, status_size, "Not Installed");
    return 0;
  }

  pipe = open_plumos_script_pipe(ui, script, "status", service, &child_pid);
  if (!pipe) {
    copy_string(status, status_size, "Status Error");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    if (strncmp(line, "state=", 6) == 0) {
      copy_truncated_string(state, sizeof(state), line + 6);
    } else if (strncmp(line, "summary=", 8) == 0) {
      copy_truncated_string(summary, sizeof(summary), line + 8);
    } else if (strncmp(line, "enabled=", 8) == 0) {
      copy_truncated_string(enabled_value, sizeof(enabled_value), line + 8);
      enabled_seen = 1;
    }
  }
  close_plumos_script_pipe(pipe, child_pid);

  enabled = strcmp(enabled_value, "1") == 0 ||
            strcmp(enabled_value, "true") == 0 ||
            strcmp(enabled_value, "on") == 0;
  if (strcmp(state, "running") == 0) {
    running = 1;
  } else if (strcmp(state, "not_installed") == 0) {
    installed = 0;
  }
  if (running_out) {
    *running_out = enabled_seen ? enabled : running;
  }
  if (strcmp(state, "running") == 0) {
    copy_string(status, status_size,
                enabled_seen && !enabled ? "Running / Manual"
                                         : "Running / Auto");
  } else if (strcmp(state, "waiting_network") == 0) {
    copy_string(status, status_size, "Waiting for Network");
  } else if (strcmp(state, "stopped") == 0) {
    copy_string(status, status_size,
                enabled_seen && enabled ? "Stopped / Auto" : "Stopped");
  } else if (strcmp(state, "not_installed") == 0) {
    copy_string(status, status_size, "Not Installed");
  } else if (state[0]) {
    copy_string(status, status_size, state);
  } else if (summary[0]) {
    copy_string(status, status_size, summary);
  } else {
    copy_string(status, status_size, installed ? "Stopped" : "Not Installed");
  }
  return installed;
}

static int config_bool_value(const char *value, int default_value) {
  if (!value || !value[0]) {
    return default_value;
  }
  if (strcmp(value, "1") == 0 ||
      strcmp(value, "true") == 0 ||
      strcmp(value, "on") == 0 ||
      strcmp(value, "yes") == 0) {
    return 1;
  }
  if (strcmp(value, "0") == 0 ||
      strcmp(value, "false") == 0 ||
      strcmp(value, "off") == 0 ||
      strcmp(value, "no") == 0) {
    return 0;
  }
  return default_value;
}

static int read_network_service_enabled(struct ui_state *ui, const char *service,
                                        int default_value) {
  char config_path[PATH_MAX];
  char key[64];
  char value[64];

  if (!ui || !service || !service[0] ||
      !join_path(config_path, sizeof(config_path), ui->plumos_root,
                 "config/network/services.conf") ||
      snprintf(key, sizeof(key), "%s_enabled", service) >= (int)sizeof(key)) {
    return default_value;
  }
  if (!read_key_value_file(config_path, key, value, sizeof(value))) {
    return default_value;
  }
  return config_bool_value(value, default_value);
}

static int write_text_file(const char *path, const char *text) {
  int fd;
  size_t len;
  ssize_t written;

  if (!path || !path[0] || !text) {
    return 0;
  }
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    return 0;
  }
  len = strlen(text);
  written = write(fd, text, len);
  if (close(fd) != 0) {
    return 0;
  }
  return written == (ssize_t)len;
}

static int write_text_file_create(const char *path, const char *text) {
  int fd;
  size_t len;
  ssize_t written;

  if (!path || !path[0] || !text) {
    return 0;
  }
  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return 0;
  }
  len = strlen(text);
  written = write(fd, text, len);
  if (close(fd) != 0) {
    return 0;
  }
  return written == (ssize_t)len;
}

static int write_all_string(int fd, const char *text) {
  size_t len;
  size_t off = 0;

  if (fd < 0 || !text) {
    return 0;
  }
  len = strlen(text);
  while (off < len) {
    ssize_t n = write(fd, text + off, len - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    if (n == 0) {
      return 0;
    }
    off += (size_t)n;
  }
  return 1;
}

static int env_flag_enabled_default(const char *name, int default_value) {
  const char *value = getenv(name);

  if (!value || !value[0]) {
    return default_value;
  }
  if (strcmp(value, "0") == 0 || strcmp(value, "no") == 0 ||
      strcmp(value, "NO") == 0 || strcmp(value, "false") == 0 ||
      strcmp(value, "FALSE") == 0 || strcmp(value, "off") == 0 ||
      strcmp(value, "OFF") == 0) {
    return 0;
  }
  return 1;
}

static int parse_unsigned_long_text(char *text, unsigned long *value_out) {
  char *s;
  char *end = NULL;
  unsigned long value;

  if (!text || !value_out) {
    return 0;
  }
  s = trim_ascii_ws(text);
  if (!s || !s[0]) {
    return 0;
  }
  errno = 0;
  value = strtoul(s, &end, 10);
  if (errno != 0 || !end || *trim_ascii_ws(end) != '\0') {
    return 0;
  }
  *value_out = value;
  return 1;
}

static int read_unsigned_long_file(const char *path, unsigned long *value_out) {
  char *text;
  int ok;

  text = read_file(path, NULL);
  if (!text) {
    return 0;
  }
  ok = parse_unsigned_long_text(text, value_out);
  free(text);
  return ok;
}

static int read_fb_virtual_size(unsigned long *width_out, unsigned long *height_out) {
  char *text;
  char *s;
  char *comma;
  int ok = 0;

  text = read_file("/sys/class/graphics/fb0/virtual_size", NULL);
  if (!text) {
    return 0;
  }
  s = trim_ascii_ws(text);
  comma = strchr(s, ',');
  if (comma) {
    *comma = '\0';
    ok = parse_unsigned_long_text(s, width_out) &&
         parse_unsigned_long_text(comma + 1, height_out);
  }
  free(text);
  return ok;
}

static int write_zero_bytes(int fd, unsigned long total_bytes) {
  unsigned char zeros[16384];
  unsigned long remaining = total_bytes;

  if (fd < 0 || total_bytes == 0) {
    return 0;
  }
  memset(zeros, 0, sizeof(zeros));
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(zeros) ? sizeof(zeros) : (size_t)remaining;
    ssize_t n = write(fd, zeros, chunk);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    if (n == 0) {
      return 0;
    }
    remaining -= (unsigned long)n;
  }
  return 1;
}

static int token_list_contains(const char *list, const char *token) {
  const char *p;
  size_t token_len;

  if (!list || !token || !token[0]) {
    return 0;
  }
  token_len = strlen(token);
  p = list;
  while (*p) {
    const char *start;
    size_t len;

    while (*p == ',' || *p == ':' || isspace((unsigned char)*p)) {
      p++;
    }
    start = p;
    while (*p && *p != ',' && *p != ':' && !isspace((unsigned char)*p)) {
      p++;
    }
    len = (size_t)(p - start);
    if (len == token_len && strncmp(start, token, token_len) == 0) {
      return 1;
    }
  }
  return 0;
}

static void clear_pixel2_compat_launch_framebuffer(const struct ui_state *ui,
                                         const char *system_id) {
  const char *fb_path;
  const char *skip_systems;
  unsigned long virtual_w = 0;
  unsigned long virtual_h = 0;
  unsigned long stride = 0;
  unsigned long bpp = 0;
  unsigned long total_bytes;
  int fd;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  int have_var = 0;
  int var_errno = 0;
  const char *pan_status = "skipped";
  int pan_errno = 0;
  char log[256];

  if (!ui || !ui->renderer_pixel2_compat_gfx ||
      !env_flag_enabled_default("PLUMOS_Pixel2_CLEAR_FB_ON_LAUNCH", 1)) {
    return;
  }
  skip_systems = getenv("PLUMOS_Pixel2_CLEAR_FB_ON_LAUNCH_SKIP_SYSTEMS");
  if (!skip_systems) {
    skip_systems = "";
  }
  if (token_list_contains(skip_systems, system_id)) {
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=skipped reason=system system=%.80s\n",
             system_id ? system_id : "");
    write_text_file_create("/tmp/plumos-fe-launch-fb-clear.log", log);
    return;
  }
  fb_path = ui->fb_path[0] ? ui->fb_path : "/dev/fb0";
  fd = open(fb_path, O_RDWR);
  if (fd < 0) {
    fd = open(fb_path, O_WRONLY);
  }
  if (fd < 0) {
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=skipped reason=open_failed fb=%.120s errno=%d\n",
             fb_path, errno);
    write_text_file_create("/tmp/plumos-fe-launch-fb-clear.log", log);
    return;
  }
  memset(&var, 0, sizeof(var));
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0) {
    have_var = 1;
    virtual_w = var.xres_virtual;
    virtual_h = var.yres_virtual;
    bpp = var.bits_per_pixel;
  } else {
    var_errno = errno;
  }
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0 && fix.line_length > 0) {
    stride = fix.line_length;
  }
  if ((virtual_w == 0 || virtual_h == 0) &&
      !read_fb_virtual_size(&virtual_w, &virtual_h)) {
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=skipped reason=bad_virtual_size fb=%.100s ioctl_errno=%d\n",
             fb_path, var_errno);
    write_text_file_create("/tmp/plumos-fe-launch-fb-clear.log", log);
    close(fd);
    return;
  }
  if (stride == 0 &&
      (!read_unsigned_long_file("/sys/class/graphics/fb0/stride", &stride) ||
       stride == 0)) {
    if (bpp == 0) {
      read_unsigned_long_file("/sys/class/graphics/fb0/bits_per_pixel", &bpp);
    }
    if (bpp >= 8) {
      stride = virtual_w * (bpp / 8);
    }
  }
  if (stride == 0 || virtual_h == 0) {
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=skipped reason=bad_size fb=%.100s stride=%lu virtual=%lu,%lu bpp=%lu\n",
             fb_path, stride, virtual_w, virtual_h, bpp);
    write_text_file_create("/tmp/plumos-fe-launch-fb-clear.log", log);
    close(fd);
    return;
  }
  total_bytes = stride * virtual_h;
  if (write_zero_bytes(fd, total_bytes)) {
    if (have_var || ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0) {
      var.xoffset = 0;
      var.yoffset = 0;
      if (ioctl(fd, FBIOPAN_DISPLAY, &var) == 0) {
        pan_status = "front";
      } else {
        pan_status = "failed";
        pan_errno = errno;
      }
    } else {
      pan_status = "query_failed";
      pan_errno = errno;
    }
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=ok fb=%.100s bytes=%lu stride=%lu virtual=%lu,%lu bpp=%lu pan=%s pan_errno=%d\n",
             fb_path, total_bytes, stride, virtual_w, virtual_h, bpp,
             pan_status, pan_errno);
  } else {
    snprintf(log, sizeof(log),
             "frontend_framebuffer_clear=failed fb=%.100s bytes=%lu stride=%lu virtual=%lu,%lu errno=%d pan=%s pan_errno=%d\n",
             fb_path, total_bytes, stride, virtual_w, virtual_h, errno,
             pan_status, pan_errno);
  }
  close(fd);
  write_text_file_create("/tmp/plumos-fe-launch-fb-clear.log", log);
}

static void trim_line_end(char *s) {
  size_t len;

  if (!s) {
    return;
  }
  len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
}

static int write_text_file_line(const char *path, const char *value) {
  char line[64];

  if (!path || !path[0] || !value || !value[0]) {
    return 0;
  }
  snprintf(line, sizeof(line), "%s\n", value);
  return write_text_file(path, line);
}

static void cpu_online_path(char *out, size_t out_size, int cpu) {
  if (!out || out_size == 0) {
    return;
  }
  snprintf(out, out_size, "/sys/devices/system/cpu/cpu%d/online", cpu);
}

static void ensure_all_cpus_online(void) {
  char path[PATH_MAX];
  int cpu;

  for (cpu = 1; cpu <= 3; cpu++) {
    cpu_online_path(path, sizeof(path), cpu);
    write_text_file_line(path, "1");
  }
}

static void save_cpu_policy_snapshot(struct cpu_policy_snapshot *snapshot) {
  if (!snapshot) {
    return;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                       snapshot->governor, sizeof(snapshot->governor));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                       snapshot->min_freq, sizeof(snapshot->min_freq));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                       snapshot->max_freq, sizeof(snapshot->max_freq));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed",
                       snapshot->setspeed, sizeof(snapshot->setspeed));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
                       snapshot->cpuinfo_min_freq, sizeof(snapshot->cpuinfo_min_freq));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                       snapshot->cpuinfo_max_freq, sizeof(snapshot->cpuinfo_max_freq));
  snapshot->saved = 1;
}

static int apply_scraping_cpu_policy(struct cpu_policy_snapshot *snapshot) {
  if (!snapshot) {
    return 0;
  }
  save_cpu_policy_snapshot(snapshot);
  ensure_all_cpus_online();
  if (snapshot->cpuinfo_min_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                         snapshot->cpuinfo_min_freq);
  }
  if (snapshot->cpuinfo_max_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                         snapshot->cpuinfo_max_freq);
  }
  write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                       "ondemand");
  return snapshot->saved;
}

static void restore_cpu_policy_snapshot(const struct cpu_policy_snapshot *snapshot) {
  if (!snapshot || !snapshot->saved) {
    return;
  }
  if (snapshot->cpuinfo_min_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                         snapshot->cpuinfo_min_freq);
  }
  if (snapshot->cpuinfo_max_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                         snapshot->cpuinfo_max_freq);
  }
  if (snapshot->max_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                         snapshot->max_freq);
  }
  if (snapshot->min_freq[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                         snapshot->min_freq);
  }
  if (snapshot->governor[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                         snapshot->governor);
  }
  if (snapshot->setspeed[0] && strcmp(snapshot->governor, "userspace") == 0) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed",
                         snapshot->setspeed);
  }
  ensure_all_cpus_online();
}

static int string_contains_line_break(const char *s) {
  return s && (strchr(s, '\n') || strchr(s, '\r'));
}

static int wifi_network_exists(const struct ui_state *ui, const char *ssid) {
  size_t i;

  if (!ui || !ssid || !ssid[0]) {
    return 0;
  }
  for (i = 0; i < ui->wifi_count; i++) {
    if (strcmp(ui->wifi_networks[i].ssid, ssid) == 0) {
      return 1;
    }
  }
  return 0;
}

static void wifi_add_network(struct ui_state *ui, const char *ssid,
                             const char *security, const char *signal) {
  struct wifi_network_entry *entry;

  if (!ui || !ssid || !ssid[0] || ui->wifi_count >= UI_MAX_WIFI_NETWORKS ||
      wifi_network_exists(ui, ssid)) {
    return;
  }
  entry = &ui->wifi_networks[ui->wifi_count++];
  copy_truncated_string(entry->ssid, sizeof(entry->ssid), ssid);
  copy_truncated_string(entry->security, sizeof(entry->security),
                        security && security[0] ? security : "secured");
  copy_truncated_string(entry->signal, sizeof(entry->signal),
                        signal && signal[0] ? signal : "-");
}

static FILE *open_plumos_script_pipe(struct ui_state *ui, const char *script,
                                     const char *arg1, const char *arg2,
                                     pid_t *child_pid) {
  char busybox[PATH_MAX];
  char hook_root[PATH_MAX];
  const char *configured_busybox;
  char *script_argv[6];
  size_t script_argc = 0;
  int pipe_fds[2];
  pid_t pid;
  FILE *stream;

  if (!ui || !script || !child_pid) {
    return NULL;
  }
  if (!join_path(hook_root, sizeof(hook_root), ui->sdcard_root,
                 ".tmp_update")) {
    return NULL;
  }
  configured_busybox = getenv("PLUMOS_BUSYBOX");
  if (!join_path(busybox, sizeof(busybox), hook_root, "busybox") ||
      access(busybox, X_OK) != 0) {
    if (configured_busybox && configured_busybox[0] &&
        access(configured_busybox, X_OK) == 0) {
      if (!copy_string(busybox, sizeof(busybox), configured_busybox)) {
        return NULL;
      }
    } else {
      if (!join_path(busybox, sizeof(busybox), ui->plumos_root,
                     "bin/busybox") ||
          access(busybox, X_OK) != 0) {
        return NULL;
      }
    }
  }
  if (setenv("PLUMOS_SDCARD_ROOT", ui->sdcard_root, 1) != 0 ||
      setenv("PLUMOS_ROOT", ui->plumos_root, 1) != 0 ||
      setenv("PLUMOS_BUSYBOX", busybox, 1) != 0 ||
      setenv("PLUMOS_HOOK_ROOT", hook_root, 1) != 0 ||
      setenv("PLUMOS_WPA_STATUS", ui->wpa_status_path, 1) != 0) {
    return NULL;
  }
  script_argv[script_argc++] = busybox;
  script_argv[script_argc++] = (char *)"sh";
  script_argv[script_argc++] = (char *)script;
  if (arg1) {
    script_argv[script_argc++] = (char *)arg1;
  }
  if (arg2) {
    script_argv[script_argc++] = (char *)arg2;
  }
  script_argv[script_argc] = NULL;
  if (pipe(pipe_fds) != 0) {
    return NULL;
  }
  pid = vfork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return NULL;
  }
  if (pid == 0) {
    int null_fd;

    close(pipe_fds[0]);
    if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
      _exit(127);
    }
    close(pipe_fds[1]);
    null_fd = open("/dev/null", O_RDWR);
    if (null_fd >= 0) {
      dup2(null_fd, STDIN_FILENO);
      dup2(null_fd, STDERR_FILENO);
      if (null_fd > STDERR_FILENO) {
        close(null_fd);
      }
    }
    execve(busybox, script_argv, environ);
    _exit(127);
  }
  close(pipe_fds[1]);
  stream = fdopen(pipe_fds[0], "r");
  if (!stream) {
    close(pipe_fds[0]);
    kill(pid, SIGTERM);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
    return NULL;
  }
  *child_pid = pid;
  return stream;
}

static int close_plumos_script_pipe(FILE *stream, pid_t child_pid) {
  int status;

  if (!stream || child_pid <= 0) {
    return -1;
  }
  fclose(stream);
  while (waitpid(child_pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  return status;
}

static int run_network_control_quiet(struct ui_state *ui, const char *script,
                                     const char *arg1, const char *arg2) {
  char discard[256];
  pid_t child_pid;
  FILE *stream;
  int rc;

  stream = open_plumos_script_pipe(ui, script, arg1, arg2, &child_pid);
  if (!stream) {
    return -1;
  }
  while (fgets(discard, sizeof(discard), stream)) {
  }
  rc = close_plumos_script_pipe(stream, child_pid);
  return rc;
}

static int wifi_scan_networks(struct ui_state *ui) {
  char script[PATH_MAX];
  char line[512];
  FILE *pipe;
  pid_t child_pid;
  int rc;

  if (!ui) {
    return 0;
  }
  ui->wifi_count = 0;
  ui->wifi_cursor = 0;
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }

  pipe = open_plumos_script_pipe(ui, script, "--scan", NULL, &child_pid);
  if (!pipe) {
    set_status(ui, "Wi-Fi scan failed to start");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    char *security;
    char *signal;
    char *ssid;
    char *tab;

    trim_line_end(line);
    if (strncmp(line, "network\t", 8) != 0) {
      continue;
    }
    security = line + 8;
    tab = strchr(security, '\t');
    if (!tab) {
      continue;
    }
    *tab = '\0';
    signal = tab + 1;
    tab = strchr(signal, '\t');
    if (!tab) {
      continue;
    }
    *tab = '\0';
    ssid = tab + 1;
    if (!ssid[0]) {
      continue;
    }
    wifi_add_network(ui, ssid, security, signal);
  }
  rc = close_plumos_script_pipe(pipe, child_pid);
  if (ui->wifi_count == 0) {
    if (rc == -1 || !(WIFEXITED(rc) && WEXITSTATUS(rc) == 0)) {
      set_status(ui, "Wi-Fi scan failed");
      return 0;
    }
    set_status(ui, "No Wi-Fi networks found");
    return 1;
  }
  snprintf(ui->status, sizeof(ui->status), "Wi-Fi scan found %zu", ui->wifi_count);
  return 1;
}

static const char *wifi_keyboard_row_chars(const struct ui_state *ui, size_t row) {
  if (row >= UI_WIFI_COMMAND_ROW) {
    return "";
  }
  return ui && ui->wifi_key_shift ? WIFI_KEYBOARD_ROWS_UPPER[row]
                                  : WIFI_KEYBOARD_ROWS_LOWER[row];
}

static size_t wifi_keyboard_row_len(const struct ui_state *ui, size_t row) {
  if (row == UI_WIFI_COMMAND_ROW) {
    return UI_WIFI_COMMAND_COUNT;
  }
  if (row >= UI_WIFI_KEYBOARD_ROWS) {
    return 0;
  }
  return strlen(wifi_keyboard_row_chars(ui, row));
}

static void wifi_clamp_key_cursor(struct ui_state *ui) {
  size_t row_len;

  if (!ui) {
    return;
  }
  if (ui->wifi_key_row >= UI_WIFI_KEYBOARD_ROWS) {
    ui->wifi_key_row = UI_WIFI_KEYBOARD_ROWS - 1;
  }
  row_len = wifi_keyboard_row_len(ui, ui->wifi_key_row);
  if (row_len == 0) {
    ui->wifi_key_col = 0;
  } else if (ui->wifi_key_col >= row_len) {
    ui->wifi_key_col = row_len - 1;
  }
}

static void wifi_append_password_char(struct ui_state *ui, char c) {
  size_t len;

  if (!ui) {
    return;
  }
  len = strlen(ui->wifi_password);
  if (len >= UI_WIFI_PASSWORD_MAX) {
    set_status(ui, "Password is already 64 chars");
    return;
  }
  ui->wifi_password[len] = c;
  ui->wifi_password[len + 1] = '\0';
  snprintf(ui->status, sizeof(ui->status), "Password length %zu", len + 1);
}

static void wifi_delete_password_char(struct ui_state *ui) {
  size_t len;

  if (!ui) {
    return;
  }
  len = strlen(ui->wifi_password);
  if (len > 0) {
    ui->wifi_password[len - 1] = '\0';
  }
  snprintf(ui->status, sizeof(ui->status), "Password length %zu",
           strlen(ui->wifi_password));
}

static void wifi_format_keyboard_row(const struct ui_state *ui, size_t row,
                                     char *out, size_t out_size) {
  size_t pos = 0;
  size_t i;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (row == UI_WIFI_COMMAND_ROW) {
    for (i = 0; i < UI_WIFI_COMMAND_COUNT; i++) {
      if (i > 0 && !append_string(out, out_size, &pos, " ")) {
        return;
      }
      if (!append_string(out, out_size, &pos, WIFI_COMMAND_LABELS[i])) {
        return;
      }
    }
    return;
  }
  if (row < UI_WIFI_KEYBOARD_ROWS) {
    const char *keys = wifi_keyboard_row_chars(ui, row);
    for (i = 0; keys[i]; i++) {
      char ch[2];
      if (i > 0 && !append_string(out, out_size, &pos, " ")) {
        return;
      }
      ch[0] = keys[i];
      ch[1] = '\0';
      if (!append_string(out, out_size, &pos, ch)) {
        return;
      }
    }
  }
}

static long scale_setting_to_runtime(long value, long setting_max,
                                     long runtime_max) {
  value = clamp_long(value, 0, setting_max);
  if (setting_max <= 0) {
    return 0;
  }
  return (value * runtime_max + setting_max / 2) / setting_max;
}

static int system_command_succeeded(int rc) {
  return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

static int runtime_requires_busybox_shell(void) {
  const char *device_id = getenv("PLUMOS_DEVICE_ID");
  const char *configured = getenv("PLUMOS_BUSYBOX");

  if (configured && configured[0] && access(configured, X_OK) == 0) {
    return 1;
  }
  return device_id && strcmp(device_id, "pixel2") == 0;
}

static const char *runtime_busybox_shell_path(void) {
  const char *configured = getenv("PLUMOS_BUSYBOX");
  const char *root;
  static char root_busybox[PATH_MAX];

  if (configured && configured[0] && access(configured, X_OK) == 0) {
    return configured;
  }
  if (access("/mnt/SDCARD/.tmp_update/busybox", X_OK) == 0) {
    return "/mnt/SDCARD/.tmp_update/busybox";
  }
  root = getenv("PLUMOS_ROOT");
  if (root && root[0] &&
      snprintf(root_busybox, sizeof(root_busybox), "%s/bin/busybox", root) <
          (int)sizeof(root_busybox) &&
      access(root_busybox, X_OK) == 0) {
    return root_busybox;
  }
  return NULL;
}

static int append_runtime_script_invocation(char *out, size_t out_size,
                                            size_t *pos, const char *script) {
  const char *busybox;

  if (!out || !pos || !script || !script[0]) {
    return 0;
  }
  if (!runtime_requires_busybox_shell()) {
    return append_shell_quoted(out, out_size, pos, script);
  }
  busybox = runtime_busybox_shell_path();
  return busybox &&
         append_shell_quoted(out, out_size, pos, busybox) &&
         append_string(out, out_size, pos, " sh ") &&
         append_shell_quoted(out, out_size, pos, script);
}

static int append_runtime_shell_eval(char *out, size_t out_size, size_t *pos,
                                     const char *command) {
  const char *busybox;

  if (!out || !pos || !command || !command[0]) {
    return 0;
  }
  if (runtime_requires_busybox_shell()) {
    busybox = runtime_busybox_shell_path();
    return busybox &&
           append_shell_quoted(out, out_size, pos, busybox) &&
           append_string(out, out_size, pos, " sh -c ") &&
           append_shell_quoted(out, out_size, pos, command);
  }
  return append_string(out, out_size, pos, "/bin/sh -c ") &&
         append_shell_quoted(out, out_size, pos, command);
}

static int prepare_runtime_shell_command(const char *cmd,
                                         const char **shell_path,
                                         char *shell_argv[5]) {
  const char *busybox;

  if (!cmd || !cmd[0] || !shell_path || !shell_argv) {
    return 0;
  }
  if (runtime_requires_busybox_shell()) {
    busybox = runtime_busybox_shell_path();
    if (!busybox) {
      return 0;
    }
    *shell_path = busybox;
    shell_argv[0] = (char *)busybox;
    shell_argv[1] = (char *)"sh";
    shell_argv[2] = (char *)"-c";
    shell_argv[3] = (char *)cmd;
    shell_argv[4] = NULL;
    return 1;
  }
  *shell_path = "/bin/sh";
  shell_argv[0] = (char *)"sh";
  shell_argv[1] = (char *)"-c";
  shell_argv[2] = (char *)cmd;
  shell_argv[3] = NULL;
  shell_argv[4] = NULL;
  return 1;
}

static int run_runtime_shell_command(const char *cmd) {
  const char *shell_path;
  char *shell_argv[5];
  pid_t pid;
  int status;

  if (!prepare_runtime_shell_command(cmd, &shell_path, shell_argv)) {
    errno = EINVAL;
    return -1;
  }
  pid = vfork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    /*
     * The renderer and input descriptors are opened with CLOEXEC. Closing the
     * duplicated DRM descriptor explicitly in a fork child can spin on Pixel2
     * stock driver, so enter BusyBox immediately and let exec close them.
     */
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

static FILE *open_runtime_shell_pipe(const char *cmd, pid_t *pid_out) {
  const char *shell_path;
  char *shell_argv[5];
  int pipe_fds[2];
  pid_t pid;
  FILE *stream;

  if (!pid_out ||
      !prepare_runtime_shell_command(cmd, &shell_path, shell_argv)) {
    errno = EINVAL;
    return NULL;
  }
  *pid_out = -1;
  if (pipe(pipe_fds) != 0) {
    return NULL;
  }
  pid = vfork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return NULL;
  }
  if (pid == 0) {
    close(pipe_fds[0]);
    if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
      _exit(126);
    }
    close(pipe_fds[1]);
    execve(shell_path, shell_argv, environ);
    _exit(127);
  }
  close(pipe_fds[1]);
  stream = fdopen(pipe_fds[0], "r");
  if (!stream) {
    int ignored_status;
    close(pipe_fds[0]);
    (void)kill(pid, SIGTERM);
    while (waitpid(pid, &ignored_status, 0) < 0 && errno == EINTR) {
    }
    return NULL;
  }
  *pid_out = pid;
  return stream;
}

static int close_runtime_shell_pipe(FILE *stream, pid_t pid) {
  int status;

  if (!stream || pid <= 0) {
    errno = EINVAL;
    return -1;
  }
  if (fclose(stream) != 0) {
    (void)kill(pid, SIGTERM);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  return status;
}

static int command_succeeds_quiet(const char *cmd) {
  if (!cmd || !cmd[0]) {
    return 0;
  }
  return system_command_succeeded(run_runtime_shell_command(cmd));
}

static const char *plumos_default_timezone(void) {
  return "JST-9";
}

static int valid_timezone_value(const char *timezone) {
  const unsigned char *p = (const unsigned char *)timezone;

  if (!timezone || !timezone[0]) {
    return 0;
  }
  while (*p) {
    if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '+' ||
          *p == '/' || *p == ':' || *p == ',' || *p == '.')) {
      return 0;
    }
    p++;
  }
  return 1;
}

static void apply_plumos_timezone_value(const char *timezone) {
  const char *value = valid_timezone_value(timezone) ? timezone : plumos_default_timezone();

  setenv("TZ", value, 1);
  tzset();
}

static void format_current_time_local(char *out, size_t out_size) {
  time_t now;
  struct tm tm_value;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_value)) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  if (strftime(out, out_size, "%Y-%m-%d %H:%M", &tm_value) == 0) {
    copy_string(out, out_size, "unavailable");
  }
}

static int runtime_wifi_enabled(void) {
  char operstate[32];

  if (read_first_line_file("/sys/class/net/wlan0/operstate",
                           operstate, sizeof(operstate)) &&
      strcmp(operstate, "down") != 0) {
    return 1;
  }
  if (command_succeeds_quiet(
          "ifconfig wlan0 2>/dev/null | grep -q 'UP' >/dev/null 2>&1")) {
    return 1;
  }
  if (command_succeeds_quiet(
          "ps w 2>/dev/null | grep '[w]pa_supplicant' >/dev/null 2>&1")) {
    return 1;
  }
  return 0;
}

static const char *runtime_volume_backend_path(void) {
  if (access("/usr/bin/amixer", X_OK) == 0) {
    return "/usr/bin/amixer";
  }
  if (access("/bin/amixer", X_OK) == 0) {
    return "/bin/amixer";
  }
  if (access("/sbin/amixer", X_OK) == 0) {
    return "/sbin/amixer";
  }
  return NULL;
}

static const char *runtime_plumos_root(void) {
  const char *root = getenv("PLUMOS_ROOT");
  return root && root[0] ? root : "/mnt/SDCARD/plumos";
}

static int runtime_device_is_pixel2(void);

static int runtime_volume_control_path(char *out, size_t out_size) {
  return join_path(out, out_size, runtime_plumos_root(), "bin/plumos-volume-control");
}

static int runtime_pixel2_compat_volume_backend_available(void) {
  char helper[PATH_MAX];

  if (!join_path(helper, sizeof(helper), runtime_plumos_root(),
                 "bin/plumos-pixel2_compat-audio-probe")) {
    return 0;
  }
  return access(helper, X_OK) == 0 && access("/config/lib/libmi_ao.so", R_OK) == 0;
}

static int run_volume_control_command(const char *action, long volume,
                                      int include_volume) {
  char helper[PATH_MAX];
  char volume_buf[32];
  char cmd[PATH_MAX + 256];
  size_t pos = 0;

  if (!runtime_volume_control_path(helper, sizeof(helper)) ||
      access(helper, X_OK) != 0) {
    return 0;
  }
  if (include_volume) {
    snprintf(volume_buf, sizeof(volume_buf), "%ld",
             clamp_long(volume, 0, PLUMOS_VOLUME_MAX));
  } else {
    volume_buf[0] = '\0';
  }
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, runtime_plumos_root()) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, helper) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, action)) {
    return 0;
  }
  if (include_volume &&
      (!append_string(cmd, sizeof(cmd), &pos, " ") ||
       !append_shell_quoted(cmd, sizeof(cmd), &pos, volume_buf))) {
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/tmp/.plumos_volume_set 2>&1")) {
    return 0;
  }
  return system_command_succeeded(run_runtime_shell_command(cmd));
}

static int runtime_volume_backend_available(void) {
  const char *amixer_path = runtime_volume_backend_path();
  char cmd[256];

  if (run_volume_control_command("init", 0, 0)) {
    return 1;
  }
  if (!amixer_path) {
    return 0;
  }
  snprintf(cmd, sizeof(cmd),
           "%s cget iface=MIXER,name='Soft Volume Master' >/dev/null 2>&1",
           amixer_path);
  return system_command_succeeded(run_runtime_shell_command(cmd));
}

static int runtime_lcd_backend_available(void) {
  return access(Pixel2_LCD_BACKLIGHT_PATH, W_OK) == 0;
}

static int run_display_control_command(const char *action, long brightness,
                                       int include_brightness) {
  char helper[PATH_MAX];
  char brightness_buf[32];
  char cmd[PATH_MAX + 256];
  size_t pos = 0;

  if (!action ||
      !join_path(helper, sizeof(helper), runtime_plumos_root(),
                 "bin/plumos-display-control") ||
      access(helper, X_OK) != 0) {
    return 0;
  }
  if (include_brightness) {
    long max_brightness = runtime_device_is_pixel2() ? 20 : 6;
    snprintf(brightness_buf, sizeof(brightness_buf), "%ld",
             clamp_long(brightness, 1, max_brightness));
  } else {
    brightness_buf[0] = '\0';
  }
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, runtime_plumos_root()) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, helper) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, action)) {
    return 0;
  }
  if (include_brightness &&
      (!append_string(cmd, sizeof(cmd), &pos, " ") ||
       !append_shell_quoted(cmd, sizeof(cmd), &pos, brightness_buf))) {
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos,
                     " >/tmp/.plumos_brightness_set 2>&1")) {
    return 0;
  }
  return system_command_succeeded(run_runtime_shell_command(cmd));
}

static int runtime_system_backlight_available(void) {
  return access(LEGACY_SUNXI_BACKLIGHT_PATH, W_OK) == 0 ||
         access(PIXEL2_BACKLIGHT_PATH, W_OK) == 0 ||
         run_display_control_command("status", 0, 0);
}

static int runtime_enhance_backend_available(void) {
  return access(Pixel2_DISPLAY_ENHANCE_PATH, W_OK) == 0;
}

static int runtime_device_uses_legacy_sunxi(void) {
  const char *device_id = getenv("PLUMOS_DEVICE_ID");

  if (device_id && device_id[0]) {
    return 0;
  }
  return !runtime_device_is_pixel2();
}

static int runtime_device_is_pixel2_compat(void) {
  const char *device_id = getenv("PLUMOS_DEVICE_ID");

  return device_id && strcmp(device_id, "pixel2_compat") == 0;
}

static int runtime_device_is_pixel2(void) {
  const char *device_id = getenv("PLUMOS_DEVICE_ID");
  char compat_path[PATH_MAX];
  char compat_vendor[64];

  if (device_id && device_id[0]) {
    return strcmp(device_id, "pixel2") == 0;
  }
  if (join_path(compat_path, sizeof(compat_path), runtime_plumos_root(),
                "COMPAT_VENDOR") &&
      read_first_line_file(compat_path, compat_vendor, sizeof(compat_vendor)) &&
      (strcmp(compat_vendor, "pixel2") == 0 ||
       strncmp(compat_vendor, "pixel2-", 7) == 0)) {
    return 1;
  }
  return access("/sys/devices/platform/hall-mh248/hallvalue", R_OK) == 0;
}

static int runtime_pixel2_compat_lcd_backend_available(void) {
  return runtime_device_is_pixel2_compat() && access(Pixel2_PWM_ENABLE_PATH, W_OK) == 0 &&
         access(Pixel2_PWM_DUTY_PATH, W_OK) == 0;
}

static int runtime_pixel2_compat_enhance_backend_available(void) {
  return runtime_device_is_pixel2_compat() && access(Pixel2_MI_DISP0_PATH, W_OK) == 0;
}

static int runtime_legacy_sunxi_enhance_bright_available(void) {
  return access(LEGACY_SUNXI_ENHANCE_BRIGHT_PATH, W_OK) == 0;
}

static int runtime_legacy_sunxi_enhance_contrast_available(void) {
  return access(LEGACY_SUNXI_ENHANCE_CONTRAST_PATH, W_OK) == 0;
}

static int runtime_legacy_sunxi_enhance_saturation_available(void) {
  return access(LEGACY_SUNXI_ENHANCE_SATURATION_PATH, W_OK) == 0;
}

static int runtime_legacy_sunxi_color_temperature_available(void) {
  return access(LEGACY_SUNXI_COLOR_TEMPERATURE_PATH, W_OK) == 0;
}

static int runtime_legacy_sunxi_enhance_backend_available(void) {
  return runtime_legacy_sunxi_enhance_bright_available() ||
         runtime_legacy_sunxi_enhance_contrast_available() ||
         runtime_legacy_sunxi_enhance_saturation_available() ||
         runtime_legacy_sunxi_color_temperature_available();
}

static int system_number_setting_needs_runtime_backend(const char *id) {
  return id && (strcmp(id, "system_volume") == 0 ||
                strcmp(id, "system_brightness") == 0 ||
                strcmp(id, "system_lumination") == 0 ||
                strcmp(id, "system_contrast") == 0 ||
                strcmp(id, "system_hue") == 0 ||
                strcmp(id, "system_saturation") == 0);
}

static int system_number_setting_runtime_available(const char *id) {
  if (!id) {
    return 0;
  }
  if (strcmp(id, "system_volume") == 0) {
    return runtime_volume_backend_available();
  }
  if (strcmp(id, "system_brightness") == 0) {
    return runtime_system_backlight_available() ||
           runtime_lcd_backend_available() || runtime_pixel2_compat_lcd_backend_available();
  }
  if (strcmp(id, "system_lumination") == 0) {
    return runtime_enhance_backend_available() ||
           runtime_pixel2_compat_enhance_backend_available() ||
           runtime_legacy_sunxi_enhance_bright_available();
  }
  if (strcmp(id, "system_contrast") == 0) {
    return runtime_enhance_backend_available() ||
           runtime_pixel2_compat_enhance_backend_available() ||
           runtime_legacy_sunxi_enhance_contrast_available();
  }
  if (strcmp(id, "system_hue") == 0) {
    return runtime_enhance_backend_available() ||
           runtime_pixel2_compat_enhance_backend_available() ||
           runtime_legacy_sunxi_color_temperature_available();
  }
  if (strcmp(id, "system_saturation") == 0) {
    return runtime_enhance_backend_available() ||
           runtime_pixel2_compat_enhance_backend_available() ||
           runtime_legacy_sunxi_enhance_saturation_available();
  }
  return 1;
}

static const char *runtime_pixel2_compat_stock_system_json_path(void) {
  const char *path = getenv("PLUMOS_Pixel2_STOCK_SYSTEM_JSON");

  return path && path[0] ? path : Pixel2_STOCK_SYSTEM_JSON_PATH;
}

static int save_pixel2_compat_stock_system_config_number(const char *key, long value) {
  char literal[64];

  if (!runtime_device_is_pixel2_compat()) {
    return 1;
  }
  snprintf(literal, sizeof(literal), "%ld", value);
  return replace_json_key_value_atomic(runtime_pixel2_compat_stock_system_json_path(), key,
                                       literal);
}

static long pixel2_compat_brightness_pwm_duty(long brightness) {
  static const long pwm_duty_by_brightness[] = {
      3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
      13, 15, 17, 20, 30, 40, 60, 100, 130, 200,
  };

  brightness = clamp_long(brightness, 1, 20);
  return pwm_duty_by_brightness[brightness - 1];
}

static void update_device_backend_status(struct device_settings *device) {
  char volume_helper[PATH_MAX];
  int lcd_available;
  int enhance_available;
  int pixel2_compat_lcd_available;
  int pixel2_compat_enhance_available;
  int system_backlight_available;

  if (!device) {
    return;
  }
  /*
   * The Pixel2 launcher owns and initializes the RK817 volume route.
   * Reporting that managed backend must not execute a helper through
   * system(), because the minimal runtime uses the configured BusyBox shell.
   */
  if (runtime_device_is_pixel2()) {
    if (runtime_volume_control_path(volume_helper, sizeof(volume_helper)) &&
        access(volume_helper, X_OK) == 0) {
      copy_string(device->volume_backend, sizeof(device->volume_backend),
                  "plumOS volume helper");
    } else {
      copy_string(device->volume_backend, sizeof(device->volume_backend),
                  "plumOS volume helper unavailable");
    }
  } else if (runtime_volume_backend_available()) {
    if (runtime_pixel2_compat_volume_backend_available()) {
      copy_string(device->volume_backend, sizeof(device->volume_backend),
                  "Pixel2 MI_AO volume");
    } else if (run_volume_control_command("status", 0, 0)) {
      copy_string(device->volume_backend, sizeof(device->volume_backend),
                  "plumOS volume helper");
    } else {
      copy_string(device->volume_backend, sizeof(device->volume_backend),
                  "ALSA Soft Volume Master");
    }
  } else if (runtime_volume_backend_path()) {
    copy_string(device->volume_backend, sizeof(device->volume_backend),
                "ALSA softvol unavailable");
  } else {
    copy_string(device->volume_backend, sizeof(device->volume_backend),
                "amixer unavailable");
  }

  lcd_available = runtime_lcd_backend_available();
  enhance_available = runtime_enhance_backend_available();
  pixel2_compat_lcd_available = runtime_pixel2_compat_lcd_backend_available();
  pixel2_compat_enhance_available = runtime_pixel2_compat_enhance_backend_available();
  system_backlight_available = runtime_system_backlight_available();
  if (runtime_device_is_pixel2() && system_backlight_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "Pixel2 PWM backlight");
  } else if (system_backlight_available && runtime_legacy_sunxi_enhance_backend_available()) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "sunxi backlight + disp enhance");
  } else if (system_backlight_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "sunxi backlight");
  } else if (runtime_legacy_sunxi_enhance_backend_available()) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                lcd_available ? "disp enhance+lcdbl" : "disp enhance");
  } else if (lcd_available && enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr lcdbl/enhance");
  } else if (pixel2_compat_lcd_available && pixel2_compat_enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "Pixel2 PWM + mi_disp csc");
  } else if (lcd_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr lcdbl only");
  } else if (pixel2_compat_lcd_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "Pixel2 PWM only");
  } else if (enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr enhance only");
  } else if (pixel2_compat_enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "Pixel2 mi_disp csc only");
  } else {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr unavailable");
  }
}

static int apply_runtime_volume(const struct device_settings *device) {
  char cmd[256];
  const char *amixer_path;
  long mixer_value;

  if (!device) {
    return 0;
  }
  if (run_volume_control_command("apply", device->volume, 1)) {
    return 1;
  }
  amixer_path = runtime_volume_backend_path();
  if (!amixer_path) {
    return 0;
  }
  mixer_value = scale_setting_to_runtime(device->volume, PLUMOS_VOLUME_MAX, 255);
  snprintf(cmd, sizeof(cmd),
           "%s cset iface=MIXER,name='Soft Volume Master' %ld "
           ">/tmp/.plumos_volume_set 2>&1",
           amixer_path, mixer_value);
  return system_command_succeeded(run_runtime_shell_command(cmd));
}

static long brightness_raw_value(long brightness) {
  static const long raw_values[] = {
      2, 3, 4, 5, 6, 7, 8, 9, 10, 26,
      43, 59, 75, 92, 108, 125, 141, 157, 174, 190,
  };

  brightness = clamp_long(brightness, 1, 20);
  return raw_values[brightness - 1];
}

static long brightness_setting_from_raw(long raw) {
  long best_setting = 1;
  long best_delta = labs(raw - brightness_raw_value(1));
  long setting;

  for (setting = 2; setting <= 20; setting++) {
    long candidate = brightness_raw_value(setting);
    long delta = labs(raw - candidate);
    if (delta < best_delta) {
      best_setting = setting;
      best_delta = delta;
    }
  }
  return best_setting;
}

static long brightness_setting_from_stored(long stored) {
  if (runtime_device_uses_legacy_sunxi()) {
    return clamp_long(stored, 1, 6);
  }
  if (stored >= 1 && stored <= 20) {
    return stored;
  }
  if (stored < 1) {
    return 1;
  }
  return brightness_setting_from_raw(stored);
}

static int apply_runtime_system_brightness(
    const struct device_settings *device) {
  long maximum;

  if (!device || (!runtime_device_uses_legacy_sunxi() && !runtime_device_is_pixel2())) {
    return 0;
  }
  maximum = runtime_device_is_pixel2() ? 20 : 6;
  return run_display_control_command(
      "apply", clamp_long(device->brightness, 1, maximum), 1);
}

static int apply_runtime_brightness(const struct device_settings *device) {
  char value[64];
  long backlight_value;
  long brightness;

  if (!device || !runtime_lcd_backend_available()) {
    return 0;
  }
  brightness = clamp_long(device->brightness, 1, 20);
  backlight_value = brightness_raw_value(brightness);
  snprintf(value, sizeof(value), "%ld\n", backlight_value);
  return write_text_file(Pixel2_LCD_BACKLIGHT_PATH, value);
}

static int apply_runtime_pixel2_compat_brightness(const struct device_settings *device) {
  char value[64];
  long brightness;
  int ok;

  if (!device || !runtime_pixel2_compat_lcd_backend_available()) {
    return 0;
  }
  brightness = clamp_long(device->brightness, 1, 20);
  snprintf(value, sizeof(value), "%ld\n", pixel2_compat_brightness_pwm_duty(brightness));
  ok = write_text_file(Pixel2_PWM_DUTY_PATH, value);
  if (ok) {
    (void)write_text_file(Pixel2_PWM_ENABLE_PATH, "1\n");
  }
  return ok && save_pixel2_compat_stock_system_config_number("brightness", brightness);
}

static int apply_runtime_display_enhance(const struct device_settings *device) {
  char value[128];
  long lumination_value;
  long contrast_value;
  long hue_value;
  long saturation_value;

  if (!device || !runtime_enhance_backend_available()) {
    return 0;
  }
  lumination_value = scale_setting_to_runtime(device->lumination, 10, 50);
  contrast_value = scale_setting_to_runtime(device->contrast, 20, 100);
  hue_value = scale_setting_to_runtime(device->hue, 20, 100);
  saturation_value = scale_setting_to_runtime(device->saturation, 20, 100);
  snprintf(value, sizeof(value), "1,%ld,%ld,%ld,%ld\n", lumination_value,
           contrast_value, hue_value, saturation_value);
  return write_text_file(Pixel2_DISPLAY_ENHANCE_PATH, value);
}

static int apply_runtime_pixel2_compat_display_enhance(const struct device_settings *device) {
  char value[128];
  long lumination;
  long contrast;
  long hue;
  long saturation;
  long lumination_value;
  long contrast_value;
  long hue_value;
  long saturation_value;
  int ok;

  if (!device || !runtime_pixel2_compat_enhance_backend_available()) {
    return 0;
  }

  lumination = clamp_long(device->lumination, 0, 20);
  contrast = clamp_long(device->contrast, 0, 20);
  hue = clamp_long(device->hue, 0, 20);
  saturation = clamp_long(device->saturation, 0, 20);

  lumination_value = lumination + 17 * 2;
  contrast_value = contrast + 40;
  hue_value = hue * 5;
  saturation_value = saturation * 5;
  snprintf(value, sizeof(value), "csc 0 3 %ld %ld %ld %ld 0 0\n",
           contrast_value, hue_value, lumination_value, saturation_value);

  ok = write_text_file(Pixel2_MI_DISP0_PATH, value);
  ok = save_pixel2_compat_stock_system_config_number("lumination", lumination) && ok;
  ok = save_pixel2_compat_stock_system_config_number("contrast", contrast) && ok;
  ok = save_pixel2_compat_stock_system_config_number("hue", hue) && ok;
  ok = save_pixel2_compat_stock_system_config_number("saturation", saturation) && ok;
  return ok;
}

static long scale_setting_to_runtime_range(long value, long setting_max,
                                           long runtime_min, long runtime_max) {
  long span;

  value = clamp_long(value, 0, setting_max);
  if (setting_max <= 0) {
    return runtime_min;
  }
  span = runtime_max - runtime_min;
  return runtime_min + (value * span + setting_max / 2) / setting_max;
}

static int write_runtime_long_file(const char *path, long value) {
  char text[64];

  snprintf(text, sizeof(text), "%ld\n", value);
  return write_text_file(path, text);
}

static int apply_runtime_legacy_sunxi_display_enhance(const struct device_settings *device,
                                              const char *id) {
  int ok = 1;
  int attempted = 0;
  int enable_enhance = 0;
  long value;

  if (!device) {
    return 0;
  }

  if (!id || strcmp(id, "system_lumination") == 0) {
    if (runtime_legacy_sunxi_enhance_bright_available()) {
      value = scale_setting_to_runtime(device->lumination, 10, 100);
      attempted = 1;
      enable_enhance = 1;
      ok = write_runtime_long_file(LEGACY_SUNXI_ENHANCE_BRIGHT_PATH, value) && ok;
    } else if (id) {
      ok = 0;
    }
  }

  if (!id || strcmp(id, "system_contrast") == 0) {
    if (runtime_legacy_sunxi_enhance_contrast_available()) {
      value = scale_setting_to_runtime(device->contrast, 20, 100);
      attempted = 1;
      enable_enhance = 1;
      ok = write_runtime_long_file(LEGACY_SUNXI_ENHANCE_CONTRAST_PATH, value) && ok;
    } else if (id) {
      ok = 0;
    }
  }

  if (!id || strcmp(id, "system_hue") == 0) {
    if (runtime_legacy_sunxi_color_temperature_available()) {
      value = scale_setting_to_runtime_range(device->hue, 20, -150, 150);
      attempted = 1;
      ok = write_runtime_long_file(LEGACY_SUNXI_COLOR_TEMPERATURE_PATH, value) && ok;
    } else if (id) {
      ok = 0;
    }
  }

  if (!id || strcmp(id, "system_saturation") == 0) {
    if (runtime_legacy_sunxi_enhance_saturation_available()) {
      value = scale_setting_to_runtime(device->saturation, 20, 100);
      attempted = 1;
      enable_enhance = 1;
      ok = write_runtime_long_file(LEGACY_SUNXI_ENHANCE_SATURATION_PATH, value) && ok;
    } else if (id) {
      ok = 0;
    }
  }

  if (enable_enhance && access(LEGACY_SUNXI_ENHANCE_MODE_PATH, W_OK) == 0) {
    (void)write_text_file(LEGACY_SUNXI_ENHANCE_MODE_PATH, "1\n");
  }
  return attempted && ok;
}

static void set_device_setting_number(struct device_settings *device,
                                      const char *id, long value) {
  if (!device || !id) {
    return;
  }
  if (strcmp(id, "system_volume") == 0) {
    device->volume = value;
  } else if (strcmp(id, "system_brightness") == 0) {
    device->brightness = value;
  } else if (strcmp(id, "system_lumination") == 0) {
    device->lumination = value;
  } else if (strcmp(id, "system_contrast") == 0) {
    device->contrast = value;
  } else if (strcmp(id, "system_hue") == 0) {
    device->hue = value;
  } else if (strcmp(id, "system_saturation") == 0) {
    device->saturation = value;
  }
}

static int apply_device_runtime_settings(const struct device_settings *device,
                                         const char *id, char *status,
                                         size_t status_size) {
  int ok = 1;
  int attempted = 0;
  int needs_volume;
  int needs_brightness;
  int needs_enhance;

  if (status && status_size > 0) {
    status[0] = '\0';
  }
  if (!device) {
    if (status && status_size > 0) {
      copy_string(status, status_size, "runtime settings unavailable");
    }
    return 0;
  }

  needs_volume = !id || strcmp(id, "system_volume") == 0;
  needs_brightness = !id || strcmp(id, "system_brightness") == 0;
  needs_enhance = !id || strcmp(id, "system_lumination") == 0 ||
                  strcmp(id, "system_contrast") == 0 ||
                  strcmp(id, "system_hue") == 0 ||
                  strcmp(id, "system_saturation") == 0;

  if (needs_volume) {
    attempted = 1;
    if (!apply_runtime_volume(device)) {
      ok = 0;
    }
  }
  if (needs_brightness && runtime_system_backlight_available()) {
    attempted = 1;
    if (!apply_runtime_system_brightness(device)) {
      ok = 0;
    }
  } else if (needs_brightness && runtime_lcd_backend_available()) {
    attempted = 1;
    if (!apply_runtime_brightness(device)) {
      ok = 0;
    }
  } else if (needs_brightness && runtime_pixel2_compat_lcd_backend_available()) {
    attempted = 1;
    if (!apply_runtime_pixel2_compat_brightness(device)) {
      ok = 0;
    }
  } else if (needs_brightness && id) {
    ok = 0;
  }
  if (needs_enhance && runtime_enhance_backend_available()) {
    attempted = 1;
    if (!apply_runtime_display_enhance(device)) {
      ok = 0;
    }
  } else if (needs_enhance && runtime_pixel2_compat_enhance_backend_available()) {
    attempted = 1;
    if (!apply_runtime_pixel2_compat_display_enhance(device)) {
      ok = 0;
    }
  } else if (needs_enhance && runtime_legacy_sunxi_enhance_backend_available()) {
    attempted = 1;
    if (!apply_runtime_legacy_sunxi_display_enhance(device, id)) {
      ok = 0;
    }
  } else if (needs_enhance && id) {
    ok = 0;
  }

  if (!ok && status && status_size > 0) {
    copy_string(status, status_size,
                attempted ? "runtime apply failed" : "runtime backend unavailable");
  }
  return ok;
}

static void schedule_pixel2_compat_brightness_reapply(struct ui_state *ui) {
  if (!ui || ui->rescue_network || ui->power_overlay || !runtime_device_is_pixel2_compat()) {
    return;
  }
  ui->pixel2_compat_brightness_reapply_due_ms =
      current_time_ms() + UI_Pixel2_BRIGHTNESS_REAPPLY_DELAY_MS;
}

static int run_scheduled_pixel2_compat_brightness_reapply(struct ui_state *ui,
                                                long long now_ms) {
  if (!ui || ui->pixel2_compat_brightness_reapply_due_ms <= 0 ||
      now_ms < ui->pixel2_compat_brightness_reapply_due_ms) {
    return 0;
  }
  ui->pixel2_compat_brightness_reapply_due_ms = 0;
  (void)apply_device_runtime_settings(&ui->device, "system_brightness", NULL, 0);
  return 1;
}

static const char *frontend_cpu_baseline_governor(void) {
  const char *env = getenv("PLUMOS_CPU_BASELINE_GOVERNOR");

  if (!env || !env[0]) {
    return "ondemand";
  }
  if (strcmp(env, "interactive") == 0 || strcmp(env, "performance") == 0 ||
      strcmp(env, "ondemand") == 0 || strcmp(env, "schedutil") == 0 ||
      strcmp(env, "conservative") == 0) {
    return env;
  }
  return "ondemand";
}

static void apply_frontend_cpu_default(void) {
  const char *disabled = getenv("PLUMOS_CONTROLLER_CPU_DEFAULT");
  const char *governor = frontend_cpu_baseline_governor();
  char cpuinfo_min[64];
  char cpuinfo_max[64];

  if (disabled && (strcmp(disabled, "0") == 0 || strcmp(disabled, "off") == 0 ||
                   strcmp(disabled, "false") == 0)) {
    return;
  }

  ensure_all_cpus_online();

  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
                       cpuinfo_min, sizeof(cpuinfo_min));
  read_first_line_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                       cpuinfo_max, sizeof(cpuinfo_max));

  if (cpuinfo_min[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                         cpuinfo_min);
  }
  if (cpuinfo_max[0]) {
    write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                         cpuinfo_max);
  }
  write_text_file_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                       governor);
}

static void format_storage_status(const char *path, char *out, size_t out_size) {
  struct statvfs st;
  unsigned long long total_mb;
  unsigned long long available_blocks;
  unsigned long long used_mb;
  unsigned long long percent;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!path || !path[0] || statvfs(path, &st) != 0 || st.f_frsize == 0) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  total_mb = ((unsigned long long)st.f_blocks * (unsigned long long)st.f_frsize) /
             (1024ULL * 1024ULL);
  available_blocks =
      st.f_bavail > st.f_blocks ? (unsigned long long)st.f_blocks :
                                  (unsigned long long)st.f_bavail;
  used_mb = (((unsigned long long)st.f_blocks - available_blocks) *
             (unsigned long long)st.f_frsize) /
            (1024ULL * 1024ULL);
  if (total_mb == 0) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  percent = (used_mb * 100ULL + total_mb / 2ULL) / total_mb;
  snprintf(out, out_size, "%llu/%llu MB (%llu%%)", used_mb, total_mb, percent);
}

static void load_storage_health_status(struct ui_state *ui) {
  char path[PATH_MAX];
  char result[64];
  char last_check[64];

  if (!ui) {
    return;
  }
  copy_string(ui->device.storage_health, sizeof(ui->device.storage_health),
              "Not checked");
  if (!join_path(path, sizeof(path), ui->plumos_root,
                 "state/storage-health/status") ||
      !read_key_value_file(path, "result", result, sizeof(result))) {
    return;
  }
  if (!read_key_value_file(path, "last_check", last_check,
                           sizeof(last_check))) {
    copy_string(last_check, sizeof(last_check), "unknown");
  }
  if (strcmp(result, "clean") == 0) {
    snprintf(ui->device.storage_health, sizeof(ui->device.storage_health),
             "Clean (%s)", last_check);
  } else if (strcmp(result, "dirty") == 0) {
    copy_string(ui->device.storage_health, sizeof(ui->device.storage_health),
                "Dirty - repair required");
  } else if (strcmp(result, "timeout") == 0) {
    copy_string(ui->device.storage_health, sizeof(ui->device.storage_health),
                "Check timed out");
  } else if (strcmp(result, "inconsistent") == 0) {
    copy_string(ui->device.storage_health, sizeof(ui->device.storage_health),
                "Errors - repair required");
  } else if (strcmp(result, "mounted-rw") == 0) {
    copy_string(ui->device.storage_health, sizeof(ui->device.storage_health),
                "Mounted RW - check after safe eject");
  } else {
    snprintf(ui->device.storage_health, sizeof(ui->device.storage_health),
             "%.48s (%.60s)", result, last_check);
  }
}

static int filesystem_is_read_only(const char *path) {
  struct statvfs st;

  if (!path || !path[0] || statvfs(path, &st) != 0) {
    return 0;
  }
  return (st.f_flag & ST_RDONLY) != 0;
}

static int read_meminfo_kb(const char *text, const char *key,
                           unsigned long long *value_out) {
  const char *p = text;
  size_t key_len;

  if (!text || !key || !value_out) {
    return 0;
  }
  key_len = strlen(key);
  while (*p) {
    if (strncmp(p, key, key_len) == 0 && p[key_len] == ':') {
      const char *value = p + key_len + 1;
      while (*value && isspace((unsigned char)*value)) {
        value++;
      }
      *value_out = strtoull(value, NULL, 10);
      return 1;
    }
    while (*p && *p != '\n') {
      p++;
    }
    if (*p == '\n') {
      p++;
    }
  }
  return 0;
}

static void format_memory_status(char *out, size_t out_size) {
  FILE *f;
  char text[4096];
  size_t text_size;
  unsigned long long total_kb = 0;
  unsigned long long available_kb = 0;
  unsigned long long free_kb = 0;
  unsigned long long buffers_kb = 0;
  unsigned long long cached_kb = 0;
  unsigned long long used_mb;
  unsigned long long total_mb;
  unsigned long long percent;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  f = fopen("/proc/meminfo", "rb");
  if (!f) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  text_size = fread(text, 1, sizeof(text) - 1, f);
  fclose(f);
  text[text_size] = '\0';
  if (text_size == 0 ||
      !read_meminfo_kb(text, "MemTotal", &total_kb) || total_kb == 0) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  if (!read_meminfo_kb(text, "MemAvailable", &available_kb)) {
    if (!read_meminfo_kb(text, "MemFree", &free_kb) ||
        !read_meminfo_kb(text, "Buffers", &buffers_kb) ||
        !read_meminfo_kb(text, "Cached", &cached_kb)) {
      copy_string(out, out_size, "unavailable");
      return;
    }
    available_kb = free_kb + buffers_kb + cached_kb;
  }
  if (available_kb > total_kb) {
    available_kb = total_kb;
  }
  total_mb = total_kb / 1024ULL;
  used_mb = (total_kb - available_kb) / 1024ULL;
  if (total_mb == 0) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  percent = (used_mb * 100ULL + total_mb / 2ULL) / total_mb;
  snprintf(out, out_size, "%llu/%llu MB (%llu%%)", used_mb, total_mb, percent);
}

static int copy_first_yyyymmdd(const char *text, char *out, size_t out_size) {
  const char *p;

  if (!text || !out || out_size < 9) {
    return 0;
  }
  for (p = text; *p; p++) {
    size_t i;
    if (p[0] != '2' || p[1] != '0') {
      continue;
    }
    for (i = 0; i < 8 && p[i] && isdigit((unsigned char)p[i]); i++) {
    }
    if (i == 8) {
      memcpy(out, p, 8);
      out[8] = '\0';
      return 1;
    }
  }
  return 0;
}

static void format_firmware_version_status(char *out, size_t out_size) {
  char *text;
  char version[64];

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  /* Pixel2 version information is owned by plumOS release metadata. */
  if (read_first_line_file("/etc/plumos-release", version, sizeof(version)) &&
      version[0]) {
    copy_string(out, out_size, version);
    return;
  }
  if (read_key_value_file("/etc/os-release", "PRETTY_NAME", version,
                          sizeof(version)) &&
      version[0]) {
    char *trimmed = trim_ascii_ws(version);
    size_t len = strlen(trimmed);
    if (len >= 2 && trimmed[0] == '"' && trimmed[len - 1] == '"') {
      trimmed[len - 1] = '\0';
      trimmed++;
    }
    copy_string(out, out_size, trimmed);
    return;
  }
  text = read_file("/etc/openwrt_release", NULL);
  if (text) {
    if (copy_first_yyyymmdd(text, out, out_size)) {
      free(text);
      return;
    }
    free(text);
  }
  if (read_first_line_file("/etc/openwrt_version", version, sizeof(version)) &&
      version[0]) {
    copy_string(out, out_size, version);
  } else {
    copy_string(out, out_size, "vendor firmware or plumOS");
  }
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

static long json_get_long(const char *json, const char *end, const char *key, long default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  return strtol(value, NULL, 10);
}

static int parse_nonnegative_long(const char *value, long *out) {
  char *endptr = NULL;
  long parsed;

  if (!value || !value[0] || !out) {
    return 0;
  }
  errno = 0;
  parsed = strtol(value, &endptr, 10);
  if (errno != 0 || endptr == value || *endptr != '\0' || parsed < 0) {
    return 0;
  }
  *out = parsed;
  return 1;
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

static int count_json_array_objects(const char *path, const char *array_key) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;
  int count = 0;

  if (!file_exists(path)) {
    return 0;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, array_key, &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (count < 9999) {
    const char *obj_start;
    const char *obj_end;
    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    (void)obj_start;
    (void)obj_end;
    count++;
  }
  free(json);
  return count;
}

static int run_scanner(const char *plumos_root, const char *sdcard_root, const char *system_id,
                       int with_thumbnails) {
  char scanner[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    return 0;
  }
  if (!file_exists(scanner)) {
    return 0;
  }
  if (system_id && !valid_system_id(system_id)) {
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scanner)) {
    return 0;
  }
  if (system_id) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --on-enter ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id)) {
      return 0;
    }
    if (with_thumbnails && !append_string(cmd, sizeof(cmd), &pos, " --with-thumbnails")) {
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/dev/null")) {
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  return rc == 0;
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

static void terminate_foreground_process_group(pid_t pgid) {
  int attempt;

  if (pgid <= 0) {
    return;
  }
  if (kill(-pgid, 0) != 0 && errno == ESRCH) {
    return;
  }
  (void)kill(-pgid, SIGTERM);
  for (attempt = 0; attempt < 10; attempt++) {
    if (kill(-pgid, 0) != 0 && errno == ESRCH) {
      return;
    }
    usleep(50000);
  }
  (void)kill(-pgid, SIGKILL);
}

/*
 * Run an interactive child under one dedicated process group. The renderer is
 * released by the caller before entry, and the frontend's long-lived input
 * descriptors are opened with O_CLOEXEC so the immediate exec closes them.
 * On normal exit as well as frontend termination, descendants left behind by
 * a launcher are cleaned up before the frontend reacquires DRM and resumes
 * input consumption.
 */
static int run_foreground_shell_command(const char *cmd) {
  const char *shell_path;
  char *shell_argv[5];
  pid_t pid;
  int status = 0;

  if (!prepare_runtime_shell_command(cmd, &shell_path, shell_argv)) {
    errno = EINVAL;
    return -1;
  }
  pid = vfork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    if (setpgid(0, 0) != 0) {
      _exit(126);
    }
    execve(shell_path, shell_argv, environ);
    _exit(127);
  }
  if (setpgid(pid, pid) != 0 && errno != EACCES && errno != ESRCH) {
    (void)kill(pid, SIGTERM);
    (void)waitpid(pid, &status, 0);
    return -1;
  }

  for (;;) {
    pid_t waited = waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      terminate_foreground_process_group(pid);
      return status;
    }
    if (waited < 0) {
      if (errno == EINTR) {
        continue;
      }
      terminate_foreground_process_group(pid);
      return -1;
    }
    if (g_terminate_requested) {
      terminate_foreground_process_group(pid);
      while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
      }
      return status;
    }
    usleep(50000);
  }
}

static pid_t start_scanner_process(const char *plumos_root, const char *sdcard_root,
                                   const char *system_id, int with_thumbnails) {
  char scanner[PATH_MAX];
  char *scanner_argv[5];
  size_t scanner_argc = 0;
  pid_t pid;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    return -1;
  }
  if (!file_exists(scanner)) {
    return -1;
  }
  if (system_id && !valid_system_id(system_id)) {
    return -1;
  }
  if (setenv("PLUMOS_ROOT", plumos_root, 1) != 0 ||
      setenv("PLUMOS_SDCARD_ROOT", sdcard_root, 1) != 0) {
    return -1;
  }
  scanner_argv[scanner_argc++] = scanner;
  if (system_id) {
    scanner_argv[scanner_argc++] = (char *)"--on-enter";
    scanner_argv[scanner_argc++] = (char *)system_id;
    if (with_thumbnails) {
      scanner_argv[scanner_argc++] = (char *)"--with-thumbnails";
    }
  }
  scanner_argv[scanner_argc] = NULL;

  pid = vfork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    redirect_child_stdio_to_devnull();
    execve(scanner, scanner_argv, environ);
    _exit(127);
  }
  return pid;
}

static int trigger_rom_scan_refresh(struct ui_state *ui, const char *system_id,
                                    int with_thumbnails) {
  long long now;
  pid_t pid;

  if (!ui || !system_id || !valid_system_id(system_id)) {
    return 0;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 0;
  }
  now = current_time_ms();
  if (ui->rom_scan_refresh_last_ms > 0 &&
      strcmp(ui->rom_scan_refresh_system_id, system_id) == 0 &&
      now - ui->rom_scan_refresh_last_ms < UI_ROM_SCAN_REFRESH_MIN_INTERVAL_MS) {
    return 0;
  }

  pid = start_scanner_process(ui->plumos_root, ui->sdcard_root, system_id,
                              with_thumbnails);
  if (pid <= 0) {
    return 0;
  }
  ui->rom_scan_refresh_pid = pid;
  ui->rom_scan_refresh_last_ms = now;
  copy_string(ui->rom_scan_refresh_system_id, sizeof(ui->rom_scan_refresh_system_id),
              system_id);
  ui->rom_scan_background_started = 1;
  return 1;
}

static int start_sdcard_cleanup_process(const char *plumos_root, const char *sdcard_root,
                                        int force, int invalidate_cache) {
  char cleanup[PATH_MAX];
  const char *busybox;
  char *cleanup_argv[6];
  size_t cleanup_argc = 0;
  pid_t pid;

  if (!join_path(cleanup, sizeof(cleanup), plumos_root, "bin/plumos-sdcard-cleanup")) {
    return 0;
  }
  if (!file_exists(cleanup)) {
    return 0;
  }
  if (setenv("PLUMOS_ROOT", plumos_root, 1) != 0 ||
      setenv("PLUMOS_SDCARD_ROOT", sdcard_root, 1) != 0) {
    return 0;
  }
  busybox = runtime_busybox_shell_path();
  if (!busybox) {
    return 0;
  }
  cleanup_argv[cleanup_argc++] = (char *)busybox;
  cleanup_argv[cleanup_argc++] = (char *)"sh";
  cleanup_argv[cleanup_argc++] = cleanup;
  if (force) {
    cleanup_argv[cleanup_argc++] = (char *)"--force";
  }
  if (!invalidate_cache) {
    cleanup_argv[cleanup_argc++] = (char *)"--no-cache-invalidate";
  }
  cleanup_argv[cleanup_argc] = NULL;

  pid = vfork();
  if (pid < 0) {
    return 0;
  }
  if (pid == 0) {
    redirect_child_stdio_to_devnull();
    execve(busybox, cleanup_argv, environ);
    _exit(127);
  }
  return 1;
}

static void trigger_sdcard_cleanup_from_start_menu(struct ui_state *ui) {
  long long now;

  if (!ui) {
    return;
  }
  now = current_time_ms();
  if (ui->sdcard_cleanup_last_ms > 0 &&
      now - ui->sdcard_cleanup_last_ms < UI_SDCARD_CLEANUP_MIN_INTERVAL_MS) {
    return;
  }
  ui->sdcard_cleanup_last_ms = now;
  start_sdcard_cleanup_process(ui->plumos_root, ui->sdcard_root, 1, 1);
}

static void init_frontend_settings(struct frontend_settings *settings) {
  memset(settings, 0, sizeof(*settings));
  settings->show_favorites_on_top = 1;
  settings->show_recent_on_top = 1;
  settings->rom_cursor_wrap = 1;
  settings->rom_scan_slow_threshold_ms = 500;
  settings->rom_scan_test_file_count = 1000;
  copy_string(settings->boot_resume_mode, sizeof(settings->boot_resume_mode), "off");
  copy_string(settings->ui_mode, sizeof(settings->ui_mode), "text");
  copy_string(settings->top_mode, sizeof(settings->top_mode), "text");
  copy_string(settings->rom_mode, sizeof(settings->rom_mode), "text");
  copy_string(settings->theme_id, sizeof(settings->theme_id), "default");
  copy_string(settings->graphic_theme_id, sizeof(settings->graphic_theme_id), "default");
  settings->graphic_transition_ms = 0;
  copy_string(settings->sort_systems, sizeof(settings->sort_systems), "sort_order");
  copy_string(settings->sort_roms, sizeof(settings->sort_roms), "name");
  copy_string(settings->rom_scan_policy, sizeof(settings->rom_scan_policy), "on_enter");
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

static int load_settings(const char *path, struct frontend_settings *settings) {
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
  settings->show_empty_systems = json_get_bool(json, json + json_size, "show_empty_systems", 0);
  settings->show_favorites_on_top =
      json_get_bool(json, json + json_size, "show_favorites_on_top", 1);
  settings->show_recent_on_top =
      json_get_bool(json, json + json_size, "show_recent_on_top", 1);
  settings->rom_cursor_wrap =
      json_get_bool(json, json + json_size, "rom_cursor_wrap", 1);
  json_get_string(json, json + json_size, "boot_resume_mode", settings->boot_resume_mode,
                  sizeof(settings->boot_resume_mode));
  normalize_boot_resume_mode(settings->boot_resume_mode, sizeof(settings->boot_resume_mode));
  json_get_string(json, json + json_size, "ui_mode", settings->ui_mode,
                  sizeof(settings->ui_mode));
  json_get_string(json, json + json_size, "top_mode", settings->top_mode,
                  sizeof(settings->top_mode));
  json_get_string(json, json + json_size, "rom_mode", settings->rom_mode,
                  sizeof(settings->rom_mode));
  json_get_string(json, json + json_size, "theme_id", settings->theme_id,
                  sizeof(settings->theme_id));
  if (!json_get_string(json, json + json_size, "graphic_theme_id",
                       settings->graphic_theme_id,
                       sizeof(settings->graphic_theme_id)) &&
      settings->theme_id[0]) {
    copy_string(settings->graphic_theme_id, sizeof(settings->graphic_theme_id),
                settings->theme_id);
  }
  json_get_string(json, json + json_size, "graphic_top_layout",
                  settings->graphic_top_layout,
                  sizeof(settings->graphic_top_layout));
  json_get_string(json, json + json_size, "graphic_transition",
                  settings->graphic_transition,
                  sizeof(settings->graphic_transition));
  json_get_string(json, json + json_size, "graphic_transition_axis",
                  settings->graphic_transition_axis,
                  sizeof(settings->graphic_transition_axis));
  json_get_string(json, json + json_size, "graphic_transition_easing",
                  settings->graphic_transition_easing,
                  sizeof(settings->graphic_transition_easing));
  settings->graphic_transition_ms =
      json_get_long(json, json + json_size, "graphic_transition_ms", 0);
  json_get_string(json, json + json_size, "sort_systems", settings->sort_systems,
                  sizeof(settings->sort_systems));
  json_get_string(json, json + json_size, "sort_roms", settings->sort_roms,
                  sizeof(settings->sort_roms));
  json_get_string(json, json + json_size, "rom_scan_policy", settings->rom_scan_policy,
                  sizeof(settings->rom_scan_policy));
  settings->rom_scan_slow_threshold_ms =
      json_get_long(json, json + json_size, "rom_scan_slow_threshold_ms", 500);
  settings->rom_scan_test_file_count =
      json_get_long(json, json + json_size, "rom_scan_test_file_count", 1000);
  json_get_string(json, json + json_size, "last_system_id", settings->last_system_id,
                  sizeof(settings->last_system_id));
  free(json);
  return 1;
}

static void fprint_json_string(FILE *f, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");

  fputc('"', f);
  while (*p) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', f);
      fputc(*p, f);
    } else if (*p == '\n') {
      fputs("\\n", f);
    } else if (*p == '\r') {
      fputs("\\r", f);
    } else if (*p == '\t') {
      fputs("\\t", f);
    } else if (*p < 0x20) {
      fprintf(f, "\\u%04x", (unsigned int)*p);
    } else {
      fputc(*p, f);
    }
    p++;
  }
  fputc('"', f);
}

static int append_json_string_literal(char *out, size_t out_size, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  size_t pos = 0;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!append_string(out, out_size, &pos, "\"")) {
    return 0;
  }
  while (*p) {
    char escaped[8];
    if (*p == '"' || *p == '\\') {
      escaped[0] = '\\';
      escaped[1] = (char)*p;
      escaped[2] = '\0';
      if (!append_string(out, out_size, &pos, escaped)) {
        return 0;
      }
    } else if (*p == '\n') {
      if (!append_string(out, out_size, &pos, "\\n")) {
        return 0;
      }
    } else if (*p == '\r') {
      if (!append_string(out, out_size, &pos, "\\r")) {
        return 0;
      }
    } else if (*p == '\t') {
      if (!append_string(out, out_size, &pos, "\\t")) {
        return 0;
      }
    } else if (*p < 0x20) {
      snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int)*p);
      if (!append_string(out, out_size, &pos, escaped)) {
        return 0;
      }
    } else {
      if (pos + 2 > out_size) {
        return 0;
      }
      out[pos++] = (char)*p;
      out[pos] = '\0';
    }
    p++;
  }
  return append_string(out, out_size, &pos, "\"");
}

static int save_settings(const char *path, const struct frontend_settings *settings) {
  char tmp_path[PATH_MAX];
  FILE *f;
  int fd;

  if (!path || !settings ||
      snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }
  fprintf(f, "{\n  \"version\": 1,\n");
  fprintf(f, "  \"ui_mode\": ");
  fprint_json_string(f, settings->ui_mode);
  fprintf(f, ",\n  \"top_mode\": ");
  fprint_json_string(f, settings->top_mode);
  fprintf(f, ",\n  \"rom_mode\": ");
  fprint_json_string(f, settings->rom_mode);
  fprintf(f, ",\n  \"show_empty_systems\": %s,\n",
          settings->show_empty_systems ? "true" : "false");
  fprintf(f, "  \"show_favorites_on_top\": %s,\n",
          settings->show_favorites_on_top ? "true" : "false");
  fprintf(f, "  \"show_recent_on_top\": %s,\n",
          settings->show_recent_on_top ? "true" : "false");
  fprintf(f, "  \"rom_cursor_wrap\": %s,\n",
          settings->rom_cursor_wrap ? "true" : "false");
  fprintf(f, "  \"boot_resume_mode\": ");
  fprint_json_string(f, settings->boot_resume_mode);
  fprintf(f, ",\n  \"sort_systems\": ");
  fprint_json_string(f, settings->sort_systems);
  fprintf(f, ",\n  \"sort_roms\": ");
  fprint_json_string(f, settings->sort_roms);
  fprintf(f, ",\n  \"rom_scan_policy\": ");
  fprint_json_string(f, settings->rom_scan_policy);
  fprintf(f, ",\n  \"rom_scan_slow_threshold_ms\": %ld,\n",
          settings->rom_scan_slow_threshold_ms);
  fprintf(f, "  \"rom_scan_test_file_count\": %ld,\n",
          settings->rom_scan_test_file_count);
  fprintf(f, "  \"theme_id\": ");
  fprint_json_string(f, settings->graphic_theme_id);
  fprintf(f, ",\n  \"graphic_theme_id\": ");
  fprint_json_string(f, settings->graphic_theme_id);
  fprintf(f, ",\n  \"graphic_top_layout\": ");
  fprint_json_string(f, settings->graphic_top_layout);
  fprintf(f, ",\n  \"graphic_transition\": ");
  fprint_json_string(f, settings->graphic_transition);
  fprintf(f, ",\n  \"graphic_transition_ms\": %ld,\n",
          settings->graphic_transition_ms);
  fprintf(f, "  \"graphic_transition_axis\": ");
  fprint_json_string(f, settings->graphic_transition_axis);
  fprintf(f, ",\n  \"graphic_transition_easing\": ");
  fprint_json_string(f, settings->graphic_transition_easing);
  fprintf(f, ",\n  \"last_system_id\": ");
  fprint_json_string(f, settings->last_system_id);
  fprintf(f, "\n}\n");

  fd = fileno(f);
  if (fflush(f) != 0 || (fd >= 0 && fsync(fd) != 0) || fclose(f) != 0) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  sync();
  return 1;
}

static const char *json_value_end(const char *value, const char *end) {
  const char *p;
  const char *body_start;
  const char *body_end;
  const char *after;
  int escape = 0;

  if (!value || value >= end) {
    return NULL;
  }
  p = skip_ws_range(value, end);
  if (p >= end) {
    return NULL;
  }
  if (*p == '"') {
    p++;
    while (p < end) {
      if (escape) {
        escape = 0;
        p++;
        continue;
      }
      if (*p == '\\') {
        escape = 1;
        p++;
        continue;
      }
      if (*p == '"') {
        return p + 1;
      }
      p++;
    }
    return NULL;
  }
  if (*p == '{') {
    if (json_match_container(p, end, '{', '}', &body_start, &body_end, &after)) {
      (void)body_start;
      (void)body_end;
      return after;
    }
    return NULL;
  }
  if (*p == '[') {
    if (json_match_container(p, end, '[', ']', &body_start, &body_end, &after)) {
      (void)body_start;
      (void)body_end;
      return after;
    }
    return NULL;
  }
  while (p < end && *p != ',' && *p != '}' && *p != ']') {
    p++;
  }
  while (p > value && isspace((unsigned char)p[-1])) {
    p--;
  }
  return p > value ? p : NULL;
}

static int write_buffer_atomic(const char *path, const char *tmp_path,
                               const char *data, size_t data_size) {
  FILE *f;
  int fd;

  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }
  if (data_size > 0 && fwrite(data, 1, data_size, f) != data_size) {
    fclose(f);
    unlink(tmp_path);
    return 0;
  }
  fd = fileno(f);
  if (fflush(f) != 0 || (fd >= 0 && fsync(fd) != 0) || fclose(f) != 0) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  sync();
  return 1;
}

static int ensure_system_config_backup(const char *path, const char *data, size_t data_size) {
  char backup_path[PATH_MAX];
  char backup_tmp_path[PATH_MAX];

  if (!path || !data ||
      snprintf(backup_path, sizeof(backup_path), "%s.plumos.bak", path) >=
          (int)sizeof(backup_path) ||
      snprintf(backup_tmp_path, sizeof(backup_tmp_path), "%s.tmp", backup_path) >=
          (int)sizeof(backup_tmp_path)) {
    return 0;
  }
  if (file_exists(backup_path)) {
    return 1;
  }
  return write_buffer_atomic(backup_path, backup_tmp_path, data, data_size);
}

static int replace_json_key_value_atomic(const char *path, const char *key,
                                         const char *literal) {
  char *json;
  char tmp_path[PATH_MAX];
  size_t json_size;
  const char *json_end;
  const char *value = NULL;
  const char *end_value = NULL;
  const char *insert_at = NULL;
  const char *content_end = NULL;
  const char *object_start = NULL;
  const char *body = NULL;
  int append_key = 0;
  int has_entries = 0;
  FILE *f;
  int fd;
  int write_ok = 0;
  int ok = 0;

  if (!path || !key || !key[0] || !literal ||
      snprintf(tmp_path, sizeof(tmp_path), "%s.plumos.tmp", path) >=
          (int)sizeof(tmp_path)) {
    return 0;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  json_end = json + json_size;
  if (!json_find_key_value(json, json_end, key, &value)) {
    object_start = skip_ws_range(json, json_end);
    if (object_start >= json_end || *object_start != '{') {
      free(json);
      return 0;
    }
    insert_at = json_end;
    while (insert_at > object_start && isspace((unsigned char)insert_at[-1])) {
      insert_at--;
    }
    if (insert_at <= object_start || insert_at[-1] != '}') {
      free(json);
      return 0;
    }
    insert_at--;
    content_end = insert_at;
    while (content_end > object_start + 1 &&
           isspace((unsigned char)content_end[-1])) {
      content_end--;
    }
    body = skip_ws_range(object_start + 1, content_end);
    has_entries = body < content_end;
    append_key = 1;
  } else {
    value = skip_ws_range(value, json_end);
    end_value = json_value_end(value, json_end);
    if (!end_value || end_value < value) {
      free(json);
      return 0;
    }
  }
  if (!ensure_system_config_backup(path, json, json_size)) {
    free(json);
    return 0;
  }

  f = fopen(tmp_path, "wb");
  if (!f) {
    free(json);
    return 0;
  }
  if (append_key) {
    write_ok = fwrite(json, 1, (size_t)(content_end - json), f) ==
               (size_t)(content_end - json);
    if (write_ok && has_entries) {
      write_ok = fputs(",\n", f) >= 0;
    }
    if (write_ok && !has_entries) {
      write_ok = fputs("\n", f) >= 0;
    }
    if (write_ok) {
      write_ok = fputs("  ", f) >= 0;
    }
    if (write_ok) {
      fprint_json_string(f, key);
      write_ok = ferror(f) == 0 && fprintf(f, ": %s\n", literal) >= 0;
    }
    if (write_ok) {
      write_ok = fputc('}', f) != EOF &&
                 fwrite(insert_at + 1, 1, (size_t)(json_end - (insert_at + 1)), f) ==
                     (size_t)(json_end - (insert_at + 1));
    }
  } else {
    write_ok = fwrite(json, 1, (size_t)(value - json), f) ==
                   (size_t)(value - json) &&
               fputs(literal, f) >= 0 &&
               fwrite(end_value, 1, (size_t)(json_end - end_value), f) ==
                   (size_t)(json_end - end_value);
  }
  if (write_ok) {
    fd = fileno(f);
    if (fflush(f) == 0 && (fd < 0 || fsync(fd) == 0)) {
      int close_ok = fclose(f) == 0;
      f = NULL;
      if (close_ok && rename(tmp_path, path) == 0) {
        sync();
        ok = 1;
      }
    }
  }
  if (f) {
    fclose(f);
  }
  if (!ok) {
    unlink(tmp_path);
  }
  free(json);
  return ok;
}

static int save_system_config_number(struct ui_state *ui, const char *key, long value) {
  char literal[64];

  snprintf(literal, sizeof(literal), "%ld", value);
  return replace_json_key_value_atomic(ui->system_config_path, key, literal);
}

static int save_system_config_bool(struct ui_state *ui, const char *key, int value) {
  return replace_json_key_value_atomic(ui->system_config_path, key,
                                       value ? "true" : "false");
}

static int save_system_config_string(struct ui_state *ui, const char *key,
                                     const char *value) {
  char literal[512];

  if (!append_json_string_literal(literal, sizeof(literal), value)) {
    return 0;
  }
  return replace_json_key_value_atomic(ui->system_config_path, key, literal);
}

static int ensure_os_timezone_backup(struct ui_state *ui) {
  char backup_dir[PATH_MAX];
  char backup_system_dir[PATH_MAX];
  char backup_path[PATH_MAX];
  char backup_tmp_path[PATH_MAX];
  char *data;
  size_t data_size = 0;
  int ok;

  if (!ui ||
      !join_path(backup_dir, sizeof(backup_dir), ui->plumos_root, "backups") ||
      !join_path(backup_system_dir, sizeof(backup_system_dir), backup_dir, "system") ||
      !join_path(backup_path, sizeof(backup_path), backup_system_dir, "etc-TZ.stock") ||
      snprintf(backup_tmp_path, sizeof(backup_tmp_path), "%s.tmp", backup_path) >=
          (int)sizeof(backup_tmp_path)) {
    return 0;
  }
  if (file_exists(backup_path)) {
    return 1;
  }
  mkdir(backup_dir, 0755);
  mkdir(backup_system_dir, 0755);
  data = read_file("/etc/TZ", &data_size);
  if (!data) {
    data = strdup("missing\n");
    data_size = strlen(data ? data : "");
  }
  if (!data) {
    return 0;
  }
  ok = write_buffer_atomic(backup_path, backup_tmp_path, data, data_size);
  free(data);
  return ok;
}

static int write_os_timezone_file(struct ui_state *ui, const char *timezone) {
  char value[96];

  if (!valid_timezone_value(timezone)) {
    return 0;
  }
  if (snprintf(value, sizeof(value), "%s\n", timezone) >= (int)sizeof(value)) {
    return 0;
  }
  ensure_os_timezone_backup(ui);
  return write_text_file("/etc/TZ", value);
}

static int apply_system_timezone_runtime(struct ui_state *ui, const char *timezone,
                                         char *status, size_t status_size) {
  const char *value = valid_timezone_value(timezone) ? timezone : plumos_default_timezone();
  int wrote_etc_tz;

  apply_plumos_timezone_value(value);
  wrote_etc_tz = write_os_timezone_file(ui, value);
  if (status && status_size > 0) {
    snprintf(status, status_size, wrote_etc_tz ? "OS TZ=%s" : "TZ env=%s; /etc/TZ unchanged",
             value);
  }
  return wrote_etc_tz;
}

static void init_device_settings(struct device_settings *device) {
  const char *default_gpu;
  const char *default_model;
  const char *device_name;

  memset(device, 0, sizeof(*device));
  device->volume = PLUMOS_VOLUME_DEFAULT;
  device->wifi_enabled = 1;
  device->automatic_time_enabled = 1;
  device->lid_suspend_enabled = 1;
  device->brightness = runtime_device_uses_legacy_sunxi() ? 6 : 10;
  device->lumination = 5;
  device->contrast = 10;
  device->hue = 10;
  device->saturation = 10;
  copy_string(device->audio_output, sizeof(device->audio_output), "speaker");
  copy_string(device->language, sizeof(device->language), "en.lang");
  copy_string(device->theme, sizeof(device->theme), "default");
  copy_string(device->timezone, sizeof(device->timezone), plumos_default_timezone());
  default_model = "GKD Pixel2";
  default_gpu = "Mali-G31 / DRM";
  device_name = getenv("PLUMOS_DEVICE_NAME");
  copy_string(device->model, sizeof(device->model),
              device_name && device_name[0] ? device_name : default_model);
  copy_string(device->plumos_version, sizeof(device->plumos_version), "unknown");
  copy_string(device->vendor_runtime, sizeof(device->vendor_runtime), "unknown");
  copy_string(device->kernel_version, sizeof(device->kernel_version), "unknown");
  copy_string(device->sdcard_storage, sizeof(device->sdcard_storage), "unavailable");
  copy_string(device->storage_health, sizeof(device->storage_health),
              "Not checked");
  copy_string(device->memory_usage, sizeof(device->memory_usage), "unavailable");
  copy_string(device->firmware_version, sizeof(device->firmware_version), "unknown");
  copy_string(device->gpu_runtime, sizeof(device->gpu_runtime), default_gpu);
  copy_string(device->network_status_source, sizeof(device->network_status_source),
              "runtime status missing");
  copy_string(device->network_control_status, sizeof(device->network_control_status),
              "plumOS runtime control");
  copy_string(device->ssh_status, sizeof(device->ssh_status), "Not Installed");
  copy_string(device->ftp_status, sizeof(device->ftp_status), "Not Installed");
  copy_string(device->sftp_status, sizeof(device->sftp_status), "Not Installed");
  copy_string(device->samba_status, sizeof(device->samba_status), "Not Installed");
  copy_string(device->adb_status, sizeof(device->adb_status), "Not Installed");
  copy_string(device->brightness_backend, sizeof(device->brightness_backend),
              "runtime backend unknown");
  copy_string(device->volume_backend, sizeof(device->volume_backend),
              "runtime backend unknown");
  copy_string(device->status, sizeof(device->status), "plumOS defaults");
}

static void load_app_layer_metadata(struct ui_state *ui) {
  char path[PATH_MAX];
  char value[64];

  if (!ui) {
    return;
  }
  if (join_path(path, sizeof(path), ui->plumos_root, "VERSION")) {
    if (read_first_line_file(path, value, sizeof(value)) && value[0]) {
      copy_string(ui->device.plumos_version, sizeof(ui->device.plumos_version),
                  value);
    }
  }
  if (join_path(path, sizeof(path), ui->plumos_root, "COMPAT_VENDOR")) {
    if (read_first_line_file(path, value, sizeof(value)) && value[0]) {
      copy_string(ui->device.vendor_runtime, sizeof(ui->device.vendor_runtime),
                  value);
    }
  }
}

static void refresh_wifi_runtime_status(struct ui_state *ui) {
  char script[PATH_MAX];

  if (!ui ||
      !join_path(script, sizeof(script), ui->plumos_root,
                 "bin/plumos-network-control") ||
      !file_exists(script)) {
    return;
  }
  run_network_control_quiet(ui, script, "--wifi", "status");
}

static void load_wifi_runtime_status(struct ui_state *ui) {
  struct device_settings *device = &ui->device;

  device->wpa_loaded = 0;
  device->wifi_state[0] = '\0';
  device->wifi_ip[0] = '\0';
  device->wifi_rssi[0] = '\0';
  device->wifi_linkspeed[0] = '\0';
  device->wifi_frequency[0] = '\0';
  refresh_wifi_runtime_status(ui);
  device->wifi_runtime_enabled = runtime_wifi_enabled();
  copy_string(device->network_status_source, sizeof(device->network_status_source),
              file_exists(ui->wpa_status_path) ? ui->wpa_status_path : "runtime status missing");
  if (read_key_value_file(ui->wpa_status_path, "wpa_state", device->wifi_state,
                          sizeof(device->wifi_state))) {
    device->wpa_loaded = 1;
  }
  if (read_key_value_file(ui->wpa_status_path, "ip_address", device->wifi_ip,
                          sizeof(device->wifi_ip))) {
    device->wpa_loaded = 1;
  }
  read_key_value_file(ui->wpa_status_path, "RSSI", device->wifi_rssi,
                      sizeof(device->wifi_rssi));
  read_key_value_file(ui->wpa_status_path, "LINKSPEED", device->wifi_linkspeed,
                      sizeof(device->wifi_linkspeed));
  read_key_value_file(ui->wpa_status_path, "FREQUENCY", device->wifi_frequency,
                      sizeof(device->wifi_frequency));
}

static void load_network_service_saved_state(struct ui_state *ui) {
  struct device_settings *device = &ui->device;

  device->ssh_service_running =
      read_network_service_enabled(ui, "ssh", 1);
  device->ftp_service_running =
      read_network_service_enabled(ui, "ftp", 0);
  device->sftp_service_running =
      read_network_service_enabled(ui, "sftp", 0);
  device->samba_service_running =
      read_network_service_enabled(ui, "samba", 0);
  device->adb_service_running =
      read_network_service_enabled(ui, "adb", 1);
}

static void load_device_runtime_status(struct ui_state *ui) {
  struct device_settings *device = &ui->device;

  load_wifi_runtime_status(ui);
  read_network_service_status(ui, "ssh", device->ssh_status,
                              sizeof(device->ssh_status),
                              &device->ssh_service_running);
  read_network_service_status(ui, "ftp", device->ftp_status,
                              sizeof(device->ftp_status),
                              &device->ftp_service_running);
  read_network_service_status(ui, "sftp", device->sftp_status,
                              sizeof(device->sftp_status),
                              &device->sftp_service_running);
  read_network_service_status(ui, "samba", device->samba_status,
                              sizeof(device->samba_status),
                              &device->samba_service_running);
  read_network_service_status(ui, "adb", device->adb_status,
                              sizeof(device->adb_status),
                              &device->adb_service_running);
}

static int load_device_settings(struct ui_state *ui) {
  char rom_storage_path[PATH_MAX];
  char config_value[64];
  char *json;
  size_t json_size;
  const char *json_end;

  init_device_settings(&ui->device);
  load_app_layer_metadata(ui);
  read_first_line_file("/proc/sys/kernel/osrelease", ui->device.kernel_version,
                       sizeof(ui->device.kernel_version));
  if (join_path(rom_storage_path, sizeof(rom_storage_path), ui->sdcard_root,
                "Roms")) {
    format_storage_status(rom_storage_path, ui->device.sdcard_storage,
                          sizeof(ui->device.sdcard_storage));
  } else {
    format_storage_status(ui->sdcard_root, ui->device.sdcard_storage,
                          sizeof(ui->device.sdcard_storage));
  }
  load_storage_health_status(ui);
  format_memory_status(ui->device.memory_usage, sizeof(ui->device.memory_usage));
  format_firmware_version_status(ui->device.firmware_version,
                                 sizeof(ui->device.firmware_version));

  update_device_backend_status(&ui->device);

  load_network_service_saved_state(ui);

  json = read_file(ui->system_config_path, &json_size);
  if (!json) {
    copy_string(ui->device.status, sizeof(ui->device.status), "plumOS config missing; defaults active");
    apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
    return 1;
  }

  json_end = json + json_size;
  ui->device.loaded = 1;
  ui->device.wifi_enabled = json_get_bool(json, json_end, "wifi_enabled",
                                          ui->device.wifi_enabled);
  ui->device.automatic_time_enabled =
      json_get_bool(json, json_end, "automatic_time",
                    ui->device.automatic_time_enabled);
  ui->device.lid_suspend_enabled =
      json_get_bool(json, json_end, "lid_suspend_enabled",
                    ui->device.lid_suspend_enabled);
  {
    long stored_volume = json_get_long(json, json_end, "volume",
                                       ui->device.volume);
    ui->device.volume = clamp_long(stored_volume, 0, PLUMOS_VOLUME_MAX);
    if (stored_volume != ui->device.volume) {
      save_system_config_number(ui, "volume", ui->device.volume);
    }
  }
  {
    long stored_brightness = json_get_long(json, json_end, "brightness",
                                           ui->device.brightness);
    ui->device.brightness = brightness_setting_from_stored(stored_brightness);
    if (stored_brightness != ui->device.brightness) {
      save_system_config_number(ui, "brightness", ui->device.brightness);
    }
  }
  ui->device.lumination = json_get_long(json, json_end, "lumination", ui->device.lumination);
  ui->device.contrast = json_get_long(json, json_end, "contrast", ui->device.contrast);
  ui->device.hue = json_get_long(json, json_end, "hue", ui->device.hue);
  ui->device.saturation = json_get_long(json, json_end, "saturation", ui->device.saturation);
  if (json_get_string(json, json_end, "audio_output", config_value,
                      sizeof(config_value)) &&
      (strcmp(config_value, "speaker") == 0 ||
       strcmp(config_value, "headphone") == 0)) {
    copy_string(ui->device.audio_output, sizeof(ui->device.audio_output),
                config_value);
  }
  if (json_get_string(json, json_end, "language", config_value,
                      sizeof(config_value)) &&
      config_value[0]) {
    copy_string(ui->device.language, sizeof(ui->device.language), config_value);
  }
  if (json_get_string(json, json_end, "theme", config_value,
                      sizeof(config_value)) &&
      config_value[0]) {
    copy_string(ui->device.theme, sizeof(ui->device.theme), config_value);
  }
  if (json_get_string(json, json_end, "timezone", config_value,
                      sizeof(config_value)) &&
      config_value[0]) {
    copy_string(ui->device.timezone, sizeof(ui->device.timezone), config_value);
  }
  if (!valid_timezone_value(ui->device.timezone)) {
    copy_string(ui->device.timezone, sizeof(ui->device.timezone),
                plumos_default_timezone());
  }
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
  copy_string(ui->device.status, sizeof(ui->device.status), "plumOS config loaded");
  free(json);
  return 1;
}

static int build_theme_path(char *out, size_t out_size, const char *plumos_root,
                            const char *theme_id) {
  char dir[PATH_MAX];
  char theme_dir[PATH_MAX];

  if (!valid_system_id(theme_id)) {
    return 0;
  }
  if (!join_path(dir, sizeof(dir), plumos_root, "themes")) {
    return 0;
  }
  if (!join_path(theme_dir, sizeof(theme_dir), dir, theme_id)) {
    return 0;
  }
  return join_path(out, out_size, theme_dir, "theme.json");
}

static void init_theme_state(struct theme_state *theme, const char *theme_id,
                             const char *theme_path) {
  memset(theme, 0, sizeof(*theme));
  theme->fallback = 1;
  copy_string(theme->id, sizeof(theme->id), theme_id && theme_id[0] ? theme_id : "default");
  copy_string(theme->target, sizeof(theme->target), "graphic");
  copy_string(theme->display_name, sizeof(theme->display_name), "Built-in Graphic");
  copy_string(theme->layout_preset, sizeof(theme->layout_preset), "grid_preview");
  copy_string(theme->font_fallback, sizeof(theme->font_fallback), "builtin");
  copy_string(theme->system_logo_root, sizeof(theme->system_logo_root), "logos/systems");
  copy_string(theme->graphic_top_layout, sizeof(theme->graphic_top_layout),
              "tile_grid");
  copy_string(theme->graphic_transition, sizeof(theme->graphic_transition), "none");
  copy_string(theme->graphic_transition_axis, sizeof(theme->graphic_transition_axis),
              "vertical");
  copy_string(theme->graphic_transition_easing, sizeof(theme->graphic_transition_easing),
              "smoothstep");
  theme->graphic_transition_ms = 0;
  copy_string(theme->status, sizeof(theme->status), "builtin graphic fallback");
  if (theme_path) {
    copy_string(theme->path, sizeof(theme->path), theme_path);
  }
}

static int theme_behavior_is_blocked(const char *json, const char *end) {
  const char *policy_start;
  const char *policy_end;

  if (!json_find_object(json, end, "behavior_policy", &policy_start, &policy_end)) {
    return 0;
  }
  if (json_get_bool(policy_start, policy_end, "theme_may_change_input", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_menu_actions", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_launch_profiles", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_rom_scan", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_resume", 0)) {
    return 1;
  }
  return 0;
}

static int valid_graphic_top_layout_value(const char *value) {
  return value && (strcmp(value, "tile_grid") == 0 ||
                   strcmp(value, "tile_strip") == 0);
}

static int valid_graphic_transition_value(const char *value) {
  return value && (strcmp(value, "slide") == 0 ||
                   strcmp(value, "none") == 0);
}

static int valid_graphic_transition_axis_value(const char *value) {
  return value && (strcmp(value, "vertical") == 0 ||
                   strcmp(value, "horizontal") == 0);
}

static int valid_graphic_transition_easing_value(const char *value) {
  return value && (strcmp(value, "smoothstep") == 0 ||
                   strcmp(value, "linear") == 0);
}

static long clamp_graphic_transition_ms(long value) {
  if (value < 80) {
    return 80;
  }
  if (value > 1000) {
    return 1000;
  }
  return value;
}

static void apply_theme_setting_overrides(struct theme_state *theme,
                                          const struct frontend_settings *settings) {
  if (!theme || !settings) {
    return;
  }
  if (valid_graphic_top_layout_value(settings->graphic_top_layout)) {
    copy_string(theme->graphic_top_layout, sizeof(theme->graphic_top_layout),
                settings->graphic_top_layout);
  }
  if (valid_graphic_transition_value(settings->graphic_transition)) {
    copy_string(theme->graphic_transition, sizeof(theme->graphic_transition),
                settings->graphic_transition);
  }
  if (valid_graphic_transition_axis_value(settings->graphic_transition_axis)) {
    copy_string(theme->graphic_transition_axis,
                sizeof(theme->graphic_transition_axis),
                settings->graphic_transition_axis);
  }
  if (valid_graphic_transition_easing_value(settings->graphic_transition_easing)) {
    copy_string(theme->graphic_transition_easing,
                sizeof(theme->graphic_transition_easing),
                settings->graphic_transition_easing);
  }
  if (settings->graphic_transition_ms > 0) {
    theme->graphic_transition_ms =
        clamp_graphic_transition_ms(settings->graphic_transition_ms);
  }
}

static int load_theme_state(struct ui_state *ui, const char *theme_id) {
  char theme_path[PATH_MAX];
  char *json;
  size_t json_size;
  const char *assets_start;
  const char *assets_end;
  const char *colors_start;
  const char *colors_end;
  const char *graphic_start;
  const char *graphic_end;
  const char *json_end;

  if (!theme_id || !theme_id[0]) {
    theme_id = "default";
  }
  if (!build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, theme_id)) {
    init_theme_state(&ui->theme, theme_id, "");
    copy_string(ui->theme.status, sizeof(ui->theme.status), "invalid theme id; builtin fallback");
    return 1;
  }

  init_theme_state(&ui->theme, theme_id, theme_path);
  json = read_file(theme_path, &json_size);
  if (!json) {
    copy_string(ui->theme.status, sizeof(ui->theme.status), "theme missing; builtin fallback");
    return 1;
  }
  json_end = json + json_size;
  ui->theme.loaded = 1;
  ui->theme.fallback = 0;

  json_get_string(json, json_end, "id", ui->theme.id, sizeof(ui->theme.id));
  json_get_string(json, json_end, "target", ui->theme.target,
                  sizeof(ui->theme.target));
  json_get_string(json, json_end, "display_name", ui->theme.display_name,
                  sizeof(ui->theme.display_name));
  json_get_string(json, json_end, "layout_preset", ui->theme.layout_preset,
                  sizeof(ui->theme.layout_preset));
  if (!ui->theme.display_name[0]) {
    copy_string(ui->theme.display_name, sizeof(ui->theme.display_name), ui->theme.id);
  }
  if (!ui->theme.layout_preset[0]) {
    copy_string(ui->theme.layout_preset, sizeof(ui->theme.layout_preset), "grid_preview");
  }

  if (ui->theme.target[0] && strcmp(ui->theme.target, "graphic") != 0) {
    ui->theme.fallback = 1;
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                "theme target is not graphic; builtin fallback");
  }

  if (json_find_object(json, json_end, "assets", &assets_start, &assets_end)) {
    json_get_string(assets_start, assets_end, "font_ui", ui->theme.font_ui,
                    sizeof(ui->theme.font_ui));
    json_get_string(assets_start, assets_end, "font_fallback", ui->theme.font_fallback,
                    sizeof(ui->theme.font_fallback));
    json_get_string(assets_start, assets_end, "background", ui->theme.background,
                    sizeof(ui->theme.background));
    json_get_string(assets_start, assets_end, "gallery_background",
                    ui->theme.gallery_background,
                    sizeof(ui->theme.gallery_background));
    json_get_string(assets_start, assets_end, "system_logo_root",
                    ui->theme.system_logo_root,
                    sizeof(ui->theme.system_logo_root));
    json_get_string(assets_start, assets_end, "placeholder_thumbnail",
                    ui->theme.placeholder_thumbnail,
                    sizeof(ui->theme.placeholder_thumbnail));
  }
  if (!ui->theme.font_fallback[0]) {
    copy_string(ui->theme.font_fallback, sizeof(ui->theme.font_fallback), "builtin");
  }
  if (!ui->theme.system_logo_root[0]) {
    copy_string(ui->theme.system_logo_root, sizeof(ui->theme.system_logo_root),
                "logos/systems");
  }

  if (json_find_object(json, json_end, "colors", &colors_start, &colors_end)) {
    json_get_string(colors_start, colors_end, "background",
                    ui->theme.color_background, sizeof(ui->theme.color_background));
    json_get_string(colors_start, colors_end, "foreground",
                    ui->theme.color_foreground, sizeof(ui->theme.color_foreground));
    json_get_string(colors_start, colors_end, "muted",
                    ui->theme.color_muted, sizeof(ui->theme.color_muted));
    json_get_string(colors_start, colors_end, "accent",
                    ui->theme.color_accent, sizeof(ui->theme.color_accent));
    json_get_string(colors_start, colors_end, "panel",
                    ui->theme.color_panel, sizeof(ui->theme.color_panel));
    json_get_string(colors_start, colors_end, "panel_inner",
                    ui->theme.color_panel_inner, sizeof(ui->theme.color_panel_inner));
    json_get_string(colors_start, colors_end, "media_panel",
                    ui->theme.color_media_panel, sizeof(ui->theme.color_media_panel));
    json_get_string(colors_start, colors_end, "selection_background",
                    ui->theme.color_selection_background,
                    sizeof(ui->theme.color_selection_background));
    json_get_string(colors_start, colors_end, "selection_foreground",
                    ui->theme.color_selection_foreground,
                    sizeof(ui->theme.color_selection_foreground));
    json_get_string(colors_start, colors_end, "danger",
                    ui->theme.color_danger, sizeof(ui->theme.color_danger));
  }
  if (json_find_object(json, json_end, "graphic_mode", &graphic_start, &graphic_end)) {
    json_get_string(graphic_start, graphic_end, "top_layout",
                    ui->theme.graphic_top_layout,
                    sizeof(ui->theme.graphic_top_layout));
    json_get_string(graphic_start, graphic_end, "transition",
                    ui->theme.graphic_transition,
                    sizeof(ui->theme.graphic_transition));
    json_get_string(graphic_start, graphic_end, "transition_axis",
                    ui->theme.graphic_transition_axis,
                    sizeof(ui->theme.graphic_transition_axis));
    json_get_string(graphic_start, graphic_end, "transition_easing",
                    ui->theme.graphic_transition_easing,
                    sizeof(ui->theme.graphic_transition_easing));
    ui->theme.graphic_transition_ms =
        json_get_long(graphic_start, graphic_end, "transition_ms",
                      ui->theme.graphic_transition_ms);
  }
  if (!ui->theme.graphic_transition[0]) {
    copy_string(ui->theme.graphic_transition,
                sizeof(ui->theme.graphic_transition), "none");
  }
  if (!valid_graphic_transition_value(ui->theme.graphic_transition)) {
    copy_string(ui->theme.graphic_transition,
                sizeof(ui->theme.graphic_transition), "none");
  }
  if (!valid_graphic_top_layout_value(ui->theme.graphic_top_layout)) {
    copy_string(ui->theme.graphic_top_layout,
                sizeof(ui->theme.graphic_top_layout), "tile_grid");
  }
  if (!valid_graphic_transition_axis_value(ui->theme.graphic_transition_axis)) {
    copy_string(ui->theme.graphic_transition_axis,
                sizeof(ui->theme.graphic_transition_axis), "vertical");
  }
  if (!valid_graphic_transition_easing_value(ui->theme.graphic_transition_easing)) {
    copy_string(ui->theme.graphic_transition_easing,
                sizeof(ui->theme.graphic_transition_easing), "smoothstep");
  }
  if (ui->theme.graphic_transition_ms < 0) {
    ui->theme.graphic_transition_ms = 0;
  } else if (ui->theme.graphic_transition_ms > 1000) {
    ui->theme.graphic_transition_ms = 1000;
  }

  if (theme_behavior_is_blocked(json, json_end)) {
    ui->theme.fallback = 1;
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                "theme requested behavior control; blocked");
  } else if (!ui->theme.status[0] || strcmp(ui->theme.status, "builtin graphic fallback") == 0) {
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                ui->theme.font_ui[0] ? "graphic theme loaded; font available"
                                     : "graphic theme loaded; builtin font fallback");
  }

  free(json);
  return 1;
}

static int resolve_theme_font_path(const struct theme_state *theme, char *out,
                                   size_t out_size) {
  char theme_dir[PATH_MAX];
  char candidate[PATH_MAX];

  if (!theme || !theme->font_ui[0] || !out || out_size == 0) {
    return 0;
  }
  if (theme->font_ui[0] == '/') {
    if (file_exists(theme->font_ui)) {
      return copy_string(out, out_size, theme->font_ui);
    }
    return 0;
  }
  if (!dirname_path(theme_dir, sizeof(theme_dir), theme->path)) {
    return 0;
  }
  if (join_path(candidate, sizeof(candidate), theme_dir, theme->font_ui) &&
      file_exists(candidate)) {
    return copy_string(out, out_size, candidate);
  }
  return 0;
}

static int resolve_theme_asset_path(const struct theme_state *theme, const char *asset,
                                    char *out, size_t out_size) {
  char theme_dir[PATH_MAX];
  char candidate[PATH_MAX];

  if (!theme || !asset || !asset[0] || !out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (asset[0] == '/') {
    if (file_exists(asset)) {
      return copy_string(out, out_size, asset);
    }
    return 0;
  }
  if (!dirname_path(theme_dir, sizeof(theme_dir), theme->path)) {
    return 0;
  }
  if (join_path(candidate, sizeof(candidate), theme_dir, asset) &&
      file_exists(candidate)) {
    return copy_string(out, out_size, candidate);
  }
  return 0;
}

static int resolve_theme_system_logo_path(const struct ui_state *ui,
                                          const char *system_id,
                                          char *out, size_t out_size) {
  char asset[UI_PATH_MAX];
  const char *root;

  if (!ui || !system_id || !valid_system_id(system_id) ||
      !out || out_size == 0 || !ui->theme.loaded || ui->theme.fallback) {
    return 0;
  }
  out[0] = '\0';
  root = ui->theme.system_logo_root[0] ? ui->theme.system_logo_root : "logos/systems";
  if (snprintf(asset, sizeof(asset), "%s/%s.png", root, system_id) >=
      (int)sizeof(asset)) {
    return 0;
  }
  return resolve_theme_asset_path(&ui->theme, asset, out, out_size);
}

static int graphic_theme_choice_cmp(const void *a, const void *b) {
  const struct graphic_theme_choice *theme_a = (const struct graphic_theme_choice *)a;
  const struct graphic_theme_choice *theme_b = (const struct graphic_theme_choice *)b;

  if (strcmp(theme_a->raw, "default") == 0 && strcmp(theme_b->raw, "default") != 0) {
    return -1;
  }
  if (strcmp(theme_b->raw, "default") == 0 && strcmp(theme_a->raw, "default") != 0) {
    return 1;
  }
  return strcmp(theme_a->display, theme_b->display);
}

static int graphic_theme_choice_exists(const struct graphic_theme_choice *choices,
                                       size_t count, const char *id) {
  size_t i;

  if (!choices || !id) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    if (strcmp(choices[i].raw, id) == 0) {
      return 1;
    }
  }
  return 0;
}

static int read_graphic_theme_display_name(const char *theme_path, const char *theme_id,
                                           int allow_builtin_fallback,
                                           char *display, size_t display_size) {
  char *json;
  size_t json_size;
  char target[32];

  if (!display || display_size == 0 || !theme_id || !theme_id[0]) {
    return 0;
  }
  display[0] = '\0';
  json = read_file(theme_path, &json_size);
  if (!json) {
    if (allow_builtin_fallback && strcmp(theme_id, "default") == 0) {
      return copy_string(display, display_size, "Default Graphic");
    }
    return 0;
  }
  target[0] = '\0';
  json_get_string(json, json + json_size, "target", target, sizeof(target));
  if (target[0] && strcmp(target, "graphic") != 0) {
    free(json);
    return 0;
  }
  if (theme_behavior_is_blocked(json, json + json_size)) {
    free(json);
    return 0;
  }
  json_get_string(json, json + json_size, "display_name", display, display_size);
  free(json);
  if (!display[0]) {
    return copy_string(display, display_size, theme_id);
  }
  return 1;
}

static int append_graphic_theme_choice(struct ui_state *ui,
                                       struct graphic_theme_choice *choices,
                                       size_t capacity, size_t *count,
                                       const char *theme_id,
                                       int allow_builtin_fallback) {
  char theme_path[PATH_MAX];
  char display[128];

  if (!ui || !choices || !count || *count >= capacity ||
      !theme_id || !valid_system_id(theme_id) ||
      graphic_theme_choice_exists(choices, *count, theme_id) ||
      !build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, theme_id)) {
    return 0;
  }
  if (!read_graphic_theme_display_name(theme_path, theme_id,
                                       allow_builtin_fallback,
                                       display, sizeof(display))) {
    return 0;
  }
  copy_string(choices[*count].raw, sizeof(choices[*count].raw), theme_id);
  copy_string(choices[*count].display, sizeof(choices[*count].display), display);
  (*count)++;
  return 1;
}

static size_t load_graphic_theme_choices(struct ui_state *ui,
                                         struct graphic_theme_choice *choices,
                                         size_t capacity) {
  char themes_dir[PATH_MAX];
  DIR *dir;
  struct dirent *entry;
  size_t count = 0;

  if (!ui || !choices || capacity == 0) {
    return 0;
  }
  append_graphic_theme_choice(ui, choices, capacity, &count, "default", 1);
  if (!join_path(themes_dir, sizeof(themes_dir), ui->plumos_root, "themes")) {
    return count;
  }
  dir = opendir(themes_dir);
  if (!dir) {
    return count;
  }
  while ((entry = readdir(dir)) != NULL && count < capacity) {
    const char *name = entry->d_name;
    char theme_path[PATH_MAX];
    struct stat st;

    if (!name || name[0] == '.' || !valid_system_id(name) ||
        strcmp(name, "default") == 0 ||
        !build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, name) ||
        stat(theme_path, &st) != 0 || !S_ISREG(st.st_mode)) {
      continue;
    }
    append_graphic_theme_choice(ui, choices, capacity, &count, name, 0);
  }
  closedir(dir);
  if (count > 1) {
    qsort(choices, count, sizeof(choices[0]), graphic_theme_choice_cmp);
  }
  return count;
}

static void graphic_theme_display_value(struct ui_state *ui, const char *raw_value,
                                        char *out, size_t out_size) {
  struct graphic_theme_choice choices[UI_GRAPHIC_THEME_CHOICE_MAX];
  size_t count;
  size_t i;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!raw_value || !raw_value[0]) {
    raw_value = "default";
  }
  count = load_graphic_theme_choices(ui, choices,
                                     sizeof(choices) / sizeof(choices[0]));
  for (i = 0; i < count; i++) {
    if (strcmp(choices[i].raw, raw_value) == 0) {
      copy_string(out, out_size, choices[i].display);
      return;
    }
  }
  copy_string(out, out_size, raw_value);
}

static int font_path_is_bitmap_only(const char *path) {
  const char *ext;

  if (!path) {
    return 0;
  }
  ext = strrchr(path, '.');
  return ext && (strcmp(ext, ".bdf") == 0 || strcmp(ext, ".pcf") == 0);
}

static int choose_mali_font_path(struct ui_state *ui, const char *requested,
                                 char *out, size_t out_size) {
  static const char *packaged_candidates[] = {
      "fonts/ui.ttf",
      "fonts/default.ttf",
      "fonts/default.otf",
  };
  static const char *rel_candidates[] = {
      "fonts/ui.ttf",
      "fonts/default.ttf",
      "fonts/default.otf",
      "plumos/fonts/ui.ttf",
      "plumos/fonts/default.ttf",
      "plumos/fonts/default.otf",
      "RetroArch/.retroarch/assets/pkg/chinese-fallback-font.ttf",
      "RetroArch/.retroarch/system/msyh.ttf",
      "pixel2/res/wqy-microhei.ttc",
      "pixel2/res/MicrosoftYaHeiGB.ttf",
      "App/commander/res/wqy-microhei.ttc",
      "Themes/MakoVII/wqy-microhei.ttf",
  };
  size_t i;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (requested && requested[0]) {
    if (file_exists(requested)) {
      return copy_string(out, out_size, requested);
    }
    return copy_string(out, out_size, requested);
  }
  if (ui && strcmp(ui->frontend_settings.ui_mode, "graphic") == 0 &&
      resolve_theme_font_path(&ui->theme, out, out_size) &&
      !font_path_is_bitmap_only(out)) {
    return 1;
  }
  out[0] = '\0';
  if (ui) {
    for (i = 0; i < sizeof(packaged_candidates) / sizeof(packaged_candidates[0]);
         i++) {
      char candidate[PATH_MAX];
      if (join_path(candidate, sizeof(candidate), ui->plumos_root,
                    packaged_candidates[i]) &&
          file_exists(candidate)) {
        return copy_string(out, out_size, candidate);
      }
    }
  }
  for (i = 0; i < sizeof(rel_candidates) / sizeof(rel_candidates[0]); i++) {
    char candidate[PATH_MAX];
    if (join_path(candidate, sizeof(candidate), ui->sdcard_root, rel_candidates[i]) &&
        file_exists(candidate)) {
      return copy_string(out, out_size, candidate);
    }
  }
  return 0;
}

static int choose_mali_fallback_font_path(struct ui_state *ui, const char *primary,
                                          char *out, size_t out_size) {
  static const char *packaged_candidates[] = {
      "fonts/cjk-fallback.ttc",
  };
  static const char *rel_candidates[] = {
      "fonts/cjk-fallback.ttc",
      "plumos/fonts/cjk-fallback.ttc",
      "RetroArch/.retroarch/assets/pkg/chinese-fallback-font.ttf",
      "RetroArch/.retroarch/system/msyh.ttf",
      "pixel2/res/wqy-microhei.ttc",
      "pixel2/res/MicrosoftYaHeiGB.ttf",
      "App/commander/res/wqy-microhei.ttc",
      "Themes/MakoVII/wqy-microhei.ttf",
  };
  size_t i;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!ui) {
    return 0;
  }
  for (i = 0; i < sizeof(packaged_candidates) / sizeof(packaged_candidates[0]);
       i++) {
    char candidate[PATH_MAX];
    if (join_path(candidate, sizeof(candidate), ui->plumos_root,
                  packaged_candidates[i]) &&
        file_exists(candidate) &&
        (!primary || strcmp(candidate, primary) != 0)) {
      return copy_string(out, out_size, candidate);
    }
  }
  for (i = 0; i < sizeof(rel_candidates) / sizeof(rel_candidates[0]); i++) {
    char candidate[PATH_MAX];
    if (join_path(candidate, sizeof(candidate), ui->sdcard_root, rel_candidates[i]) &&
        file_exists(candidate) &&
        (!primary || strcmp(candidate, primary) != 0)) {
      return copy_string(out, out_size, candidate);
    }
  }
  return 0;
}

static void add_setting_entry(struct ui_state *ui, const char *id, const char *name,
                              const char *value) {
  struct setting_entry *entry;
  char key[128];
  if (ui->setting_count >= UI_MAX_SETTINGS) {
    return;
  }
  entry = &ui->setting_entries[ui->setting_count++];
  memset(entry, 0, sizeof(*entry));
  copy_string(entry->id, sizeof(entry->id), id);
  if (id && id[0]) {
    snprintf(key, sizeof(key), "settings.item.%s.name", id);
    copy_string(entry->display_name, sizeof(entry->display_name),
                tr(ui, key, name));
  } else {
    copy_string(entry->display_name, sizeof(entry->display_name), name);
  }
  copy_string(entry->value, sizeof(entry->value), value && value[0] ? value : "-");
}

static void add_bool_setting_entry(struct ui_state *ui, const char *id, const char *name,
                                   int value) {
  add_setting_entry(ui, id, name, value ? "true" : "false");
}

static const struct setting_choice UI_MODE_CHOICES[] = {
    {"text", "Text"},
    {"graphic", "Graphic"},
};

static const struct setting_choice BOOT_RESUME_CHOICES[] = {
    {"off", "Off"},
    {"on", "ON"},
    {"recent", "Recent"},
};

static const struct setting_choice SORT_SYSTEMS_CHOICES[] = {
    {"sort_order", "Sort Order"},
    {"name", "Name"},
};

static const struct setting_choice SORT_ROMS_CHOICES[] = {
    {"name", "Name"},
    {"path", "Path"},
};

static const struct setting_choice GRAPHIC_TOP_LAYOUT_CHOICES[] = {
    {"tile_grid", "Grid 3x2"},
    {"tile_strip", "Strip 2x1"},
};

static const struct setting_choice GRAPHIC_TRANSITION_CHOICES[] = {
    {"none", "None"},
    {"slide", "Slide"},
};

static const struct setting_choice GRAPHIC_TRANSITION_AXIS_CHOICES[] = {
    {"vertical", "Vertical"},
    {"horizontal", "Horizontal"},
};

static const struct setting_choice GRAPHIC_TRANSITION_EASING_CHOICES[] = {
    {"smoothstep", "Smooth"},
    {"linear", "Linear"},
};

static const struct setting_choice SYSTEM_LANGUAGE_CHOICES[] = {
    {"en.lang", "English"},
    {"ja.lang", "Japanese"},
    {"ch.lang", "Chinese"},
    {"pt.lang", "Portuguese"},
    {"fr.lang", "French"},
    {"de.lang", "German"},
};

static const struct setting_choice SYSTEM_TIMEZONE_CHOICES[] = {
    {"JST-9", "Japan"},
    {"UTC0", "UTC"},
    {"KST-9", "Korea"},
    {"CST-8", "China"},
    {"PST8PDT,M3.2.0/2,M11.1.0/2", "US Pacific"},
    {"MST7MDT,M3.2.0/2,M11.1.0/2", "US Mountain"},
    {"CST6CDT,M3.2.0/2,M11.1.0/2", "US Central"},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "US Eastern"},
    {"GMT0BST,M3.5.0/1,M10.5.0", "UK"},
    {"CET-1CEST,M3.5.0/2,M10.5.0/3", "Central EU"},
};

static const struct setting_choice SYSTEM_AUDIO_OUTPUT_CHOICES[] = {
    {"speaker", "Speaker"},
    {"headphone", "Headphone"},
};

static const long BRIGHTNESS_TEST_VALUES[] = {
    10, 30, 50, 70, 90, 110, 130, 150, 170, 190, 210, 230, 250, 255,
};
#define BRIGHTNESS_TEST_COUNT \
  (sizeof(BRIGHTNESS_TEST_VALUES) / sizeof(BRIGHTNESS_TEST_VALUES[0]))
#define BRIGHTNESS_TEST_COLUMNS 4

enum setting_control_type {
  SETTING_CONTROL_READONLY = 0,
  SETTING_CONTROL_CHECKBOX,
  SETTING_CONTROL_CHOICE,
  SETTING_CONTROL_NUMBER,
  SETTING_CONTROL_ACTION
};

static const struct setting_choice *setting_choices(const char *id, size_t *count_out) {
  if (count_out) {
    *count_out = 0;
  }
  if (!id) {
    return NULL;
  }
  if (strcmp(id, "ui_mode") == 0) {
    if (count_out) {
      *count_out = sizeof(UI_MODE_CHOICES) / sizeof(UI_MODE_CHOICES[0]);
    }
    return UI_MODE_CHOICES;
  }
  if (strcmp(id, "boot_resume_mode") == 0) {
    if (count_out) {
      *count_out = sizeof(BOOT_RESUME_CHOICES) / sizeof(BOOT_RESUME_CHOICES[0]);
    }
    return BOOT_RESUME_CHOICES;
  }
  if (strcmp(id, "sort_systems") == 0) {
    if (count_out) {
      *count_out = sizeof(SORT_SYSTEMS_CHOICES) / sizeof(SORT_SYSTEMS_CHOICES[0]);
    }
    return SORT_SYSTEMS_CHOICES;
  }
  if (strcmp(id, "sort_roms") == 0) {
    if (count_out) {
      *count_out = sizeof(SORT_ROMS_CHOICES) / sizeof(SORT_ROMS_CHOICES[0]);
    }
    return SORT_ROMS_CHOICES;
  }
  if (strcmp(id, "theme_top_layout") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TOP_LAYOUT_CHOICES) /
                   sizeof(GRAPHIC_TOP_LAYOUT_CHOICES[0]);
    }
    return GRAPHIC_TOP_LAYOUT_CHOICES;
  }
  if (strcmp(id, "theme_transition") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_CHOICES;
  }
  if (strcmp(id, "theme_transition_axis") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_AXIS_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_AXIS_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_AXIS_CHOICES;
  }
  if (strcmp(id, "theme_transition_easing") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_EASING_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_EASING_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_EASING_CHOICES;
  }
  if (strcmp(id, "system_language") == 0) {
    if (count_out) {
      *count_out = sizeof(SYSTEM_LANGUAGE_CHOICES) / sizeof(SYSTEM_LANGUAGE_CHOICES[0]);
    }
    return SYSTEM_LANGUAGE_CHOICES;
  }
  if (strcmp(id, "system_timezone") == 0) {
    if (count_out) {
      *count_out = sizeof(SYSTEM_TIMEZONE_CHOICES) / sizeof(SYSTEM_TIMEZONE_CHOICES[0]);
    }
    return SYSTEM_TIMEZONE_CHOICES;
  }
  if (strcmp(id, "system_audio_output") == 0) {
    if (count_out) {
      *count_out = sizeof(SYSTEM_AUDIO_OUTPUT_CHOICES) /
                   sizeof(SYSTEM_AUDIO_OUTPUT_CHOICES[0]);
    }
    return SYSTEM_AUDIO_OUTPUT_CHOICES;
  }
  return NULL;
}

static const char *setting_choice_display_value(const char *id, const char *raw_value) {
  size_t count = 0;
  size_t i;
  const struct setting_choice *choices = setting_choices(id, &count);

  if (!raw_value || !raw_value[0]) {
    raw_value = "";
  }
  for (i = 0; i < count; i++) {
    if (strcmp(raw_value, choices[i].raw) == 0 ||
        strcmp(raw_value, choices[i].display) == 0) {
      return choices[i].display;
    }
  }
  return raw_value[0] ? raw_value : "-";
}

static int rom_scan_policy_is_on_enter(const char *policy) {
  return !policy || !policy[0] || strcmp(policy, "on_enter") == 0 ||
         strcmp(policy, "On Enter") == 0;
}

static int compare_text_ci(const char *a, const char *b) {
  const unsigned char *pa = (const unsigned char *)(a ? a : "");
  const unsigned char *pb = (const unsigned char *)(b ? b : "");

  while (*pa && *pb) {
    int ca = tolower(*pa);
    int cb = tolower(*pb);
    if (ca != cb) {
      return ca - cb;
    }
    pa++;
    pb++;
  }
  return (int)*pa - (int)*pb;
}

static int cmp_top_entry_name(const void *a, const void *b) {
  const struct top_entry *ea = (const struct top_entry *)a;
  const struct top_entry *eb = (const struct top_entry *)b;
  int cmp = compare_text_ci(ea->display_name, eb->display_name);

  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->id, eb->id);
}

static int cmp_rom_entry_name(const void *a, const void *b) {
  const struct rom_entry *ea = (const struct rom_entry *)a;
  const struct rom_entry *eb = (const struct rom_entry *)b;
  int cmp = compare_text_ci(ea->title, eb->title);

  if (ea->is_navigation_directory != eb->is_navigation_directory) {
    return eb->is_navigation_directory - ea->is_navigation_directory;
  }
  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->relative_path, eb->relative_path);
}

static int cmp_rom_entry_path(const void *a, const void *b) {
  const struct rom_entry *ea = (const struct rom_entry *)a;
  const struct rom_entry *eb = (const struct rom_entry *)b;
  int cmp = compare_text_ci(ea->relative_path, eb->relative_path);

  if (ea->is_navigation_directory != eb->is_navigation_directory) {
    return eb->is_navigation_directory - ea->is_navigation_directory;
  }
  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->title, eb->title);
}

static enum setting_control_type setting_control_type_for_id(const char *id) {
  if (!id) {
    return SETTING_CONTROL_READONLY;
  }
  if (strcmp(id, "network_connect_wifi") == 0 ||
      strcmp(id, "network_services") == 0 ||
      strcmp(id, "network_information") == 0 ||
      strcmp(id, "refresh_top") == 0 ||
      strcmp(id, "ui_theme_settings") == 0 ||
      strcmp(id, "system_display_color") == 0 ||
      strcmp(id, "system_time_settings") == 0 ||
      strcmp(id, "system_storage_check") == 0 ||
      strcmp(id, "system_sync_now") == 0 ||
      strcmp(id, "system_manual_time") == 0 ||
      strcmp(id, "system_manual_time_apply") == 0 ||
      strcmp(id, "system_information") == 0 ||
      strcmp(id, "system_update") == 0 ||
      strcmp(id, "system_factory_reset") == 0 ||
      strcmp(id, "system_factory_reset_all") == 0 ||
      strcmp(id, "system_factory_reset_ra") == 0 ||
      strcmp(id, "system_factory_reset_pico") == 0 ||
      strcmp(id, "system_factory_reset_sa") == 0 ||
      strcmp(id, "performance_clear_cpu_override") == 0 ||
      strcmp(id, "performance_core_details") == 0) {
    return SETTING_CONTROL_ACTION;
  }
  if (strcmp(id, "show_empty_systems") == 0 ||
      strcmp(id, "show_favorites_on_top") == 0 ||
      strcmp(id, "show_recent_on_top") == 0 ||
      strcmp(id, "rom_cursor_wrap") == 0 ||
      strcmp(id, "rom_scan_policy") == 0 ||
      strcmp(id, "system_automatic_time") == 0 ||
      strcmp(id, "system_lid_suspend") == 0 ||
      strcmp(id, "network_wifi_enabled") == 0 ||
      strcmp(id, "network_ssh_enabled") == 0 ||
      strcmp(id, "network_ftp_enabled") == 0 ||
      strcmp(id, "network_sftp_enabled") == 0 ||
      strcmp(id, "network_samba_enabled") == 0 ||
      strcmp(id, "network_adb_enabled") == 0) {
    return SETTING_CONTROL_CHECKBOX;
  }
  if (setting_choices(id, NULL)) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "graphic_theme_id") == 0) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "performance_system") == 0 ||
      strcmp(id, "performance_cpu_policy") == 0) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0 ||
      strcmp(id, "rom_scan_test_file_count") == 0 ||
      strcmp(id, "system_volume") == 0 ||
      strcmp(id, "system_brightness") == 0 ||
      strcmp(id, "system_lumination") == 0 ||
      strcmp(id, "system_contrast") == 0 ||
      strcmp(id, "system_hue") == 0 ||
      strcmp(id, "system_saturation") == 0 ||
      strcmp(id, "system_manual_time_year") == 0 ||
      strcmp(id, "system_manual_time_month") == 0 ||
      strcmp(id, "system_manual_time_day") == 0 ||
      strcmp(id, "system_manual_time_hour") == 0 ||
      strcmp(id, "system_manual_time_minute") == 0 ||
      strcmp(id, "theme_transition_ms") == 0) {
    return SETTING_CONTROL_NUMBER;
  }
  return SETTING_CONTROL_READONLY;
}

static int setting_is_writable(const char *id) {
  return id && (strcmp(id, "ui_mode") == 0 ||
                strcmp(id, "show_empty_systems") == 0 ||
                strcmp(id, "show_favorites_on_top") == 0 ||
                strcmp(id, "show_recent_on_top") == 0 ||
                strcmp(id, "rom_cursor_wrap") == 0 ||
                strcmp(id, "boot_resume_mode") == 0 ||
                strcmp(id, "graphic_theme_id") == 0 ||
                strcmp(id, "theme_top_layout") == 0 ||
                strcmp(id, "theme_transition") == 0 ||
                strcmp(id, "theme_transition_axis") == 0 ||
                strcmp(id, "theme_transition_easing") == 0 ||
                strcmp(id, "theme_transition_ms") == 0 ||
                strcmp(id, "sort_systems") == 0 ||
                strcmp(id, "sort_roms") == 0 ||
                strcmp(id, "rom_scan_policy") == 0 ||
                strcmp(id, "rom_scan_slow_threshold_ms") == 0 ||
                strcmp(id, "rom_scan_test_file_count") == 0 ||
                strcmp(id, "system_volume") == 0 ||
                strcmp(id, "system_audio_output") == 0 ||
                strcmp(id, "system_brightness") == 0 ||
                strcmp(id, "system_lumination") == 0 ||
                strcmp(id, "system_contrast") == 0 ||
                strcmp(id, "system_hue") == 0 ||
                strcmp(id, "system_saturation") == 0 ||
                strcmp(id, "system_language") == 0 ||
                strcmp(id, "system_timezone") == 0 ||
                strcmp(id, "system_automatic_time") == 0 ||
                strcmp(id, "system_lid_suspend") == 0 ||
                strcmp(id, "system_manual_time_year") == 0 ||
                strcmp(id, "system_manual_time_month") == 0 ||
                strcmp(id, "system_manual_time_day") == 0 ||
                strcmp(id, "system_manual_time_hour") == 0 ||
                strcmp(id, "system_manual_time_minute") == 0 ||
                strcmp(id, "network_wifi_enabled") == 0 ||
                strcmp(id, "network_ssh_enabled") == 0 ||
                strcmp(id, "network_ftp_enabled") == 0 ||
                strcmp(id, "network_sftp_enabled") == 0 ||
                strcmp(id, "network_samba_enabled") == 0 ||
                strcmp(id, "network_adb_enabled") == 0 ||
                strcmp(id, "performance_system") == 0 ||
                strcmp(id, "performance_cpu_policy") == 0);
}

static const char *settings_category_title(const struct ui_state *ui,
                                           enum settings_category category) {
  switch (category) {
  case SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR:
    return tr(ui, "settings.category.system_display_color",
              "System Settings - Display Color");
  case SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST:
    return tr(ui, "settings.category.system_brightness_test",
              "System Settings - Brightness Test");
  case SETTINGS_CATEGORY_SYSTEM_TIME:
    return tr(ui, "settings.category.system_time",
              "System Settings - Time Settings");
  case SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL:
    return tr(ui, "settings.category.system_time_manual",
              "System Settings - Manual Time");
  case SETTINGS_CATEGORY_SYSTEM_INFORMATION:
    return tr(ui, "settings.category.system_information",
              "System Settings - INFORMATION");
  case SETTINGS_CATEGORY_SYSTEM_FACTORY_RESET:
    return tr(ui, "settings.category.system_factory_reset",
              "System Settings - Factory Reset");
  case SETTINGS_CATEGORY_SYSTEM:
    return tr(ui, "settings.category.system", "System Settings");
  case SETTINGS_CATEGORY_NETWORK:
    return tr(ui, "settings.category.network", "Network Settings");
  case SETTINGS_CATEGORY_NETWORK_SERVICE:
    return tr(ui, "settings.category.network_service",
              "Network Settings - NW Service");
  case SETTINGS_CATEGORY_NETWORK_INFORMATION:
    return tr(ui, "settings.category.network_information",
              "Network Settings - INFORMATION");
  case SETTINGS_CATEGORY_PERFORMANCE:
    return tr(ui, "settings.category.performance", "Performance Settings");
  case SETTINGS_CATEGORY_UI_THEME:
    return tr(ui, "settings.category.ui_theme", "Theme Settings");
  case SETTINGS_CATEGORY_UI:
  default:
    return tr(ui, "settings.category.ui", "UI Settings");
  }
}

static void add_ui_settings_entries(struct ui_state *ui,
                                    const struct frontend_settings *settings) {
  add_setting_entry(ui, "refresh_top", "Refresh TOP", "");
  add_setting_entry(ui, "ui_mode", "UI Mode",
                    setting_choice_display_value("ui_mode", settings->ui_mode));
  add_bool_setting_entry(ui, "show_empty_systems", "Show Empty Systems",
                         settings->show_empty_systems);
  add_bool_setting_entry(ui, "show_favorites_on_top", "Favorites On TOP",
                         settings->show_favorites_on_top);
  add_bool_setting_entry(ui, "show_recent_on_top", "Recent On TOP",
                         settings->show_recent_on_top);
  add_bool_setting_entry(ui, "rom_cursor_wrap", "ROM Cursor Wrap",
                         settings->rom_cursor_wrap);
  add_setting_entry(ui, "boot_resume_mode", "Open Last ROM At Boot",
                    setting_choice_display_value("boot_resume_mode",
                                                 settings->boot_resume_mode));
  add_setting_entry(ui, "sort_systems", "Sort Systems",
                    setting_choice_display_value("sort_systems", settings->sort_systems));
  add_setting_entry(ui, "sort_roms", "Sort ROMs",
                    setting_choice_display_value("sort_roms", settings->sort_roms));
  add_bool_setting_entry(ui, "rom_scan_policy", "Scan On Enter",
                         rom_scan_policy_is_on_enter(settings->rom_scan_policy));
  add_setting_entry(ui, "ui_theme_settings", "Theme Settings", "");
}

static void add_ui_theme_settings_entries(struct ui_state *ui,
                                          const struct frontend_settings *settings) {
  char graphic_theme_display[128];
  char value[128];

  graphic_theme_display_value(ui, settings->graphic_theme_id,
                              graphic_theme_display,
                              sizeof(graphic_theme_display));
  add_setting_entry(ui, "graphic_theme_id", "Theme", graphic_theme_display);
  add_setting_entry(ui, "theme_name", "Name", ui->theme.display_name);
  add_setting_entry(ui, "theme_status", "Status", ui->theme.status);
  add_setting_entry(ui, "theme_layout", "Layout", ui->theme.layout_preset);
  add_setting_entry(ui, "theme_top_layout", "TOP Layout",
                    setting_choice_display_value("theme_top_layout",
                                                 ui->theme.graphic_top_layout));
  add_setting_entry(ui, "theme_transition", "Transition",
                    setting_choice_display_value("theme_transition",
                                                 ui->theme.graphic_transition));
  snprintf(value, sizeof(value), "%ld ms", ui->theme.graphic_transition_ms);
  add_setting_entry(ui, "theme_transition_ms", "Time", value);
  add_setting_entry(ui, "theme_transition_axis", "Axis",
                    setting_choice_display_value("theme_transition_axis",
                                                 ui->theme.graphic_transition_axis));
  add_setting_entry(ui, "theme_transition_easing", "Easing",
                    setting_choice_display_value("theme_transition_easing",
                                                 ui->theme.graphic_transition_easing));
  add_setting_entry(ui, "theme_font", "Font",
                    ui->theme.font_ui[0] ? ui->theme.font_ui : ui->theme.font_fallback);
}

static int days_in_month(long year, long month) {
  static const int days[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };
  int leap;

  if (month < 1 || month > 12) {
    return 31;
  }
  if (month != 2) {
    return days[month - 1];
  }
  leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
  return leap ? 29 : 28;
}

static void clamp_manual_time_fields(struct ui_state *ui) {
  long max_day;

  if (!ui) {
    return;
  }
  ui->manual_time_year = clamp_long(ui->manual_time_year, 2024, 2037);
  ui->manual_time_month = clamp_long(ui->manual_time_month, 1, 12);
  max_day = days_in_month(ui->manual_time_year, ui->manual_time_month);
  ui->manual_time_day = clamp_long(ui->manual_time_day, 1, max_day);
  ui->manual_time_hour = clamp_long(ui->manual_time_hour, 0, 23);
  ui->manual_time_minute = clamp_long(ui->manual_time_minute, 0, 59);
}

static void init_manual_time_from_current(struct ui_state *ui) {
  time_t now;
  struct tm tm_value;

  if (!ui) {
    return;
  }
  apply_plumos_timezone_value(ui->device.timezone);
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_value)) {
    ui->manual_time_year = 2026;
    ui->manual_time_month = 1;
    ui->manual_time_day = 1;
    ui->manual_time_hour = 0;
    ui->manual_time_minute = 0;
  } else {
    ui->manual_time_year = tm_value.tm_year + 1900;
    ui->manual_time_month = tm_value.tm_mon + 1;
    ui->manual_time_day = tm_value.tm_mday;
    ui->manual_time_hour = tm_value.tm_hour;
    ui->manual_time_minute = tm_value.tm_min;
  }
  clamp_manual_time_fields(ui);
  ui->manual_time_initialized = 1;
}

static void format_manual_time_value(const struct ui_state *ui,
                                     char *out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (!ui || !ui->manual_time_initialized) {
    copy_string(out, out_size, "-");
    return;
  }
  snprintf(out, out_size, "%04ld-%02ld-%02ld %02ld:%02ld",
           ui->manual_time_year, ui->manual_time_month, ui->manual_time_day,
           ui->manual_time_hour, ui->manual_time_minute);
}

static void add_system_time_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;
  time_t now;
  char rtc_epoch_text[64];
  long long rtc_epoch;
  long long delta;
  long long magnitude;
  const char *direction;

  format_current_time_local(value, sizeof(value));
  add_setting_entry(ui, "system_current_time", "Current Time", value);
  add_bool_setting_entry(ui, "system_automatic_time", "Automatic Time",
                         device->automatic_time_enabled);
  add_setting_entry(ui, "system_sync_now", "Sync Now", "");
  value[0] = '\0';
  if (read_first_line_file("/sys/class/rtc/rtc0/since_epoch",
                           rtc_epoch_text, sizeof(rtc_epoch_text))) {
    rtc_epoch = strtoll(rtc_epoch_text, NULL, 10);
    now = time(NULL);
    delta = (long long)now - rtc_epoch;
    if (delta >= -5 && delta <= 5) {
      copy_string(value, sizeof(value), "Synced");
    } else {
      direction = delta > 0 ? "Behind" : "Ahead";
      magnitude = delta > 0 ? delta : -delta;
      if (magnitude < 60) {
        snprintf(value, sizeof(value), "%s %llds", direction, magnitude);
      } else if (magnitude < 3600) {
        snprintf(value, sizeof(value), "%s %lldm", direction, magnitude / 60);
      } else if (magnitude < 86400) {
        snprintf(value, sizeof(value), "%s %lldh", direction, magnitude / 3600);
      } else {
        snprintf(value, sizeof(value), "%s %lldd", direction, magnitude / 86400);
      }
    }
  } else {
    copy_string(value, sizeof(value), "Unavailable");
  }
  add_setting_entry(ui, "system_rtc_status", "RTC Status", value);
  add_setting_entry(ui, "system_timezone", "Timezone",
                    setting_choice_display_value("system_timezone", device->timezone));
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  format_manual_time_value(ui, value, sizeof(value));
  add_setting_entry(ui, "system_manual_time", "Manual Time", value);
}

static void add_system_time_manual_entries(struct ui_state *ui) {
  char value[256];

  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  clamp_manual_time_fields(ui);
  format_current_time_local(value, sizeof(value));
  add_setting_entry(ui, "system_current_time", "Current Time", value);
  add_setting_entry(ui, "system_manual_timezone", "Timezone",
                    setting_choice_display_value("system_timezone",
                                                 ui->device.timezone));
  snprintf(value, sizeof(value), "%ld", ui->manual_time_year);
  add_setting_entry(ui, "system_manual_time_year", "Year", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_month);
  add_setting_entry(ui, "system_manual_time_month", "Month", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_day);
  add_setting_entry(ui, "system_manual_time_day", "Day", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_hour);
  add_setting_entry(ui, "system_manual_time_hour", "Hour", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_minute);
  add_setting_entry(ui, "system_manual_time_minute", "Minute", value);
  format_manual_time_value(ui, value, sizeof(value));
  add_setting_entry(ui, "system_manual_time_apply", "Apply Manual Time", value);
}

static void format_runtime_number_setting_value(const char *id, long value,
                                                char *out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (system_number_setting_needs_runtime_backend(id) &&
      !system_number_setting_runtime_available(id)) {
    copy_string(out, out_size, "N/A");
    return;
  }
  snprintf(out, out_size, "%ld", value);
}

static void add_system_settings_entries(struct ui_state *ui) {
  char value[256];
  char runtime_version_path[PATH_MAX];
  char system_version_path[PATH_MAX];
  char runtime_version[64];
  char system_version[64];
  char contrast_value[32];
  char hue_value[32];
  char saturation_value[32];
  const char *system_root;
  const struct device_settings *device = &ui->device;

  format_runtime_number_setting_value("system_volume", device->volume, value,
                                      sizeof(value));
  add_setting_entry(ui, "system_volume", "Volume", value);
  /* Pixel2 has one RK817 speaker route and no jack-routing backend. */
  if (!runtime_device_is_pixel2()) {
    add_setting_entry(ui, "system_audio_output", "Audio Output",
                      setting_choice_display_value("system_audio_output",
                                                   device->audio_output));
  }

  format_runtime_number_setting_value("system_brightness", device->brightness,
                                      value, sizeof(value));
  add_setting_entry(ui, "system_brightness", "Brightness", value);
  /* A lid setting is not a Pixel2 capability. */
  if (!runtime_device_is_pixel2()) {
    add_bool_setting_entry(ui, "system_lid_suspend", "Lid Suspend",
                           device->lid_suspend_enabled);
  }
  format_runtime_number_setting_value("system_lumination", device->lumination,
                                      value, sizeof(value));
  add_setting_entry(ui, "system_lumination", "Lumination", value);
  format_runtime_number_setting_value("system_contrast", device->contrast,
                                      contrast_value, sizeof(contrast_value));
  format_runtime_number_setting_value("system_hue", device->hue,
                                      hue_value, sizeof(hue_value));
  format_runtime_number_setting_value("system_saturation", device->saturation,
                                      saturation_value, sizeof(saturation_value));
  snprintf(value, sizeof(value), "C=%s T=%s S=%s",
           contrast_value, hue_value, saturation_value);
  add_setting_entry(ui, "system_display_color", "Display Color", value);

  add_setting_entry(ui, "system_time_settings", "Time Settings",
                    setting_choice_display_value("system_timezone", device->timezone));
  add_setting_entry(ui, "system_language", "Language",
                    setting_choice_display_value("system_language", device->language));
  if (!join_path(runtime_version_path, sizeof(runtime_version_path),
                 ui->plumos_root, "VERSION") ||
      !read_first_line_file(runtime_version_path, runtime_version,
                            sizeof(runtime_version))) {
    copy_string(runtime_version, sizeof(runtime_version), "unknown");
  }
  system_root = getenv("PLUMOS_SYSTEM_ROOT");
  if (!system_root || !system_root[0] ||
      !join_path(system_version_path, sizeof(system_version_path), system_root,
                 "etc/plumos-system-version") ||
      !read_first_line_file(system_version_path, system_version,
                            sizeof(system_version))) {
    copy_string(system_version, sizeof(system_version), "unknown");
  }
  snprintf(value, sizeof(value), "Runtime %s / System %s",
           runtime_version, system_version);
  add_setting_entry(ui, "system_update", "System Update", value);
  add_setting_entry(ui, "system_factory_reset", "Factory Reset", "");
  add_setting_entry(ui, "system_storage_check", "Storage Check",
                    device->storage_health);
  add_setting_entry(ui, "system_information", "INFORMATION", "");
}

static void add_system_factory_reset_entries(struct ui_state *ui) {
  char path[PATH_MAX];
  int has_ra = 0;
  int has_pico = 0;
  int has_sa = 0;

  if (join_path(path, sizeof(path), ui->plumos_root, "factory-defaults/ra")) {
    has_ra = dir_exists(path);
  }
  if (join_path(path, sizeof(path), ui->plumos_root, "factory-defaults/pico")) {
    has_pico = dir_exists(path);
  }
  if (join_path(path, sizeof(path), ui->plumos_root, "factory-defaults/sa")) {
    has_sa = dir_exists(path);
  }
  if (has_ra && has_pico && has_sa) {
    add_setting_entry(ui, "system_factory_reset_all", "All Emulator Settings",
                      "RA + PICO + SA");
  }
  if (has_ra) {
    add_setting_entry(ui, "system_factory_reset_ra", "RetroArch Settings", "");
  }
  if (has_pico) {
    add_setting_entry(ui, "system_factory_reset_pico", "PicoArch Settings", "");
  }
  if (has_sa) {
    add_setting_entry(ui, "system_factory_reset_sa", "Standalone Settings", "");
  }
  if (!has_ra && !has_pico && !has_sa) {
    add_setting_entry(ui, "system_factory_reset_none", "No Defaults Installed",
                      "N/A");
  }
}

static void add_system_brightness_test_entries(struct ui_state *ui) {
  size_t i;
  char id[64];
  char value[32];
  const struct device_settings *device = &ui->device;

  for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
    long brightness = BRIGHTNESS_TEST_VALUES[i];
    snprintf(id, sizeof(id), "system_brightness_test_%ld", brightness);
    snprintf(value, sizeof(value), "%ld", brightness);
    add_setting_entry(ui, id, value,
                      brightness_raw_value(device->brightness) == brightness ? "current"
                                                                             : "preset");
  }
}

static void add_system_display_color_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  format_runtime_number_setting_value("system_contrast", device->contrast,
                                      value, sizeof(value));
  add_setting_entry(ui, "system_contrast", "Contrast", value);
  format_runtime_number_setting_value("system_hue", device->hue, value,
                                      sizeof(value));
  add_setting_entry(ui, "system_hue", "Hue", value);
  format_runtime_number_setting_value("system_saturation", device->saturation,
                                      value, sizeof(value));
  add_setting_entry(ui, "system_saturation", "Saturation", value);
}

static void add_system_information_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;
  char battery_capacity[32];
  char battery_status[32];
  char battery_temperature[32];
  char value[128];

  add_setting_entry(ui, "system_model", "Device Model", device->model);
  add_setting_entry(ui, "system_plumos_version", "plumOS", device->plumos_version);
  add_setting_entry(ui, "system_vendor_runtime", "Vendor", device->vendor_runtime);
  add_setting_entry(ui, "system_kernel", "Kernel", device->kernel_version);
  add_setting_entry(ui, "system_gpu_runtime", "GPU", device->gpu_runtime);
  add_setting_entry(ui, "system_display_backend", "Display", device->brightness_backend);
  add_setting_entry(ui, "system_audio_backend", "Audio", device->volume_backend);
  add_setting_entry(ui, "system_sdcard", "Storage", device->sdcard_storage);
  add_setting_entry(ui, "system_storage_health", "Storage Health",
                    device->storage_health);
  add_setting_entry(ui, "system_memory", "Memory", device->memory_usage);
  add_setting_entry(ui, "system_firmware", "Base OS", device->firmware_version);
  if (runtime_device_is_pixel2()) {
    if (!read_first_line_file("/sys/class/power_supply/battery/capacity",
                              battery_capacity, sizeof(battery_capacity))) {
      copy_string(battery_capacity, sizeof(battery_capacity), "N/A");
    }
    if (!read_first_line_file("/sys/class/power_supply/battery/status",
                              battery_status, sizeof(battery_status))) {
      copy_string(battery_status, sizeof(battery_status), "N/A");
    }
    snprintf(value, sizeof(value), "%s%% / %s", battery_capacity,
             battery_status);
    add_setting_entry(ui, "system_battery", "Battery", value);
    if (read_first_line_file("/sys/class/power_supply/battery/temp",
                             battery_temperature,
                             sizeof(battery_temperature))) {
      long raw = strtol(battery_temperature, NULL, 10);
      long absolute = raw < 0 ? -raw : raw;
      snprintf(value, sizeof(value), "%s%ld.%ld C", raw < 0 ? "-" : "",
               absolute / 10, absolute % 10);
    } else {
      copy_string(value, sizeof(value), "N/A");
    }
    add_setting_entry(ui, "system_battery_temperature",
                      "Battery Temperature", value);
  }
}

static void add_network_settings_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;

  add_bool_setting_entry(ui, "network_wifi_enabled", "Wi-Fi",
                         device->wifi_enabled);
  add_setting_entry(ui, "network_connect_wifi", "Connect Wi-Fi",
                    "Scan SSID");
  add_setting_entry(ui, "network_services", "NW Service",
                    "File Transfer");
  add_setting_entry(ui, "network_information", "INFORMATION", "");
}

static void add_network_service_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;

  add_bool_setting_entry(ui, "network_ssh_enabled", "SSH",
                         device->ssh_service_running);
  add_bool_setting_entry(ui, "network_ftp_enabled", "FTP",
                         device->ftp_service_running);
  add_bool_setting_entry(ui, "network_sftp_enabled", "SFTP",
                         device->sftp_service_running);
  add_bool_setting_entry(ui, "network_samba_enabled", "Samba",
                         device->samba_service_running);
  add_bool_setting_entry(ui, "network_adb_enabled", "ADB",
                         device->adb_service_running);
}

static void add_network_information_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  if (strcmp(device->wifi_state, "COMPLETED") == 0) {
    copy_string(value, sizeof(value),
                device->wifi_ip[0] ? "Connected" : "No IP Address");
  } else if (strcmp(device->wifi_state, "DISCONNECTED") == 0) {
    copy_string(value, sizeof(value), "Disconnected");
  } else if (strcmp(device->wifi_state, "NO_USB_WIFI_DONGLE") == 0) {
    copy_string(value, sizeof(value), "Dongle Missing");
  } else if (strcmp(device->wifi_state, "NO_WIFI_INTERFACE") == 0) {
    copy_string(value, sizeof(value), "No Wi-Fi Interface");
  } else if (device->wifi_state[0]) {
    copy_string(value, sizeof(value), device->wifi_state);
  } else {
    copy_string(value, sizeof(value), "No Runtime Status");
  }
  add_setting_entry(ui, "network_connection", "Connection",
                    value);
  add_setting_entry(ui, "network_ip_address", "IP Address",
                    device->wifi_ip[0] ? device->wifi_ip : "-");
  if (device->wifi_rssi[0]) {
    snprintf(value, sizeof(value), "%s dBm", device->wifi_rssi);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_signal", "Signal", value);
  if (device->wifi_linkspeed[0]) {
    snprintf(value, sizeof(value), "%s Mbps", device->wifi_linkspeed);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_link_speed", "Link Speed", value);
  if (device->wifi_frequency[0]) {
    snprintf(value, sizeof(value), "%s MHz", device->wifi_frequency);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_frequency", "Frequency", value);
  add_setting_entry(ui, "network_ssh", "SSH", device->ssh_status);
  add_setting_entry(ui, "network_ftp_status", "FTP", device->ftp_status);
  add_setting_entry(ui, "network_sftp_status", "SFTP", device->sftp_status);
  add_setting_entry(ui, "network_samba_status", "Samba", device->samba_status);
  add_setting_entry(ui, "network_adb_status", "ADB", device->adb_status);
}

static int performance_top_entry_is_real(const struct top_entry *entry) {
  return entry && !entry->virtual_entry && valid_system_id(entry->id);
}

static int performance_find_top_entry_index(const struct ui_state *ui,
                                            const char *system_id) {
  size_t i;

  if (!ui || !system_id || !system_id[0]) {
    return -1;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (performance_top_entry_is_real(&ui->top_entries[i]) &&
        strcmp(ui->top_entries[i].id, system_id) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int performance_first_top_entry_index(const struct ui_state *ui) {
  size_t i;

  if (!ui) {
    return -1;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (performance_top_entry_is_real(&ui->top_entries[i])) {
      return (int)i;
    }
  }
  return -1;
}

static void performance_select_top_entry(struct ui_state *ui, size_t index) {
  const struct top_entry *entry;

  if (!ui || index >= ui->top_count) {
    return;
  }
  entry = &ui->top_entries[index];
  copy_string(ui->performance_system_id, sizeof(ui->performance_system_id), entry->id);
  copy_string(ui->performance_system_name, sizeof(ui->performance_system_name),
              entry->display_name[0] ? entry->display_name : entry->id);
}

static int performance_ensure_system(struct ui_state *ui) {
  int index;
  int old_show_all;

  if (!ui) {
    return 0;
  }
  if (ui->top_count == 0) {
    load_top_entries(ui);
  }
  index = performance_find_top_entry_index(ui, ui->performance_system_id);
  if (index < 0) {
    index = performance_find_top_entry_index(ui, ui->current_system_id);
  }
  if (index < 0) {
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0) {
    old_show_all = ui->show_all;
    ui->show_all = 1;
    load_top_entries(ui);
    ui->show_all = old_show_all;
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0) {
    ui->performance_system_id[0] = '\0';
    ui->performance_system_name[0] = '\0';
    return 0;
  }
  performance_select_top_entry(ui, (size_t)index);
  return 1;
}

static int performance_cycle_system(struct ui_state *ui, int direction) {
  int index;
  size_t step;

  if (!ui || direction == 0 || !performance_ensure_system(ui)) {
    return 0;
  }
  index = performance_find_top_entry_index(ui, ui->performance_system_id);
  if (index < 0) {
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0 || ui->top_count == 0) {
    return 0;
  }
  for (step = 1; step <= ui->top_count; step++) {
    int next = direction > 0
                   ? (int)(((size_t)index + step) % ui->top_count)
                   : (int)(((size_t)index + ui->top_count - (step % ui->top_count)) %
                           ui->top_count);
    if (performance_top_entry_is_real(&ui->top_entries[next])) {
      performance_select_top_entry(ui, (size_t)next);
      return 1;
    }
  }
  return 0;
}

static void copy_trimmed_range(char *out, size_t out_size, const char *start,
                               size_t len) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!start) {
    return;
  }
  while (len > 0 && isspace((unsigned char)*start)) {
    start++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)start[len - 1])) {
    len--;
  }
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static void copy_parenthesized_source(char *out, size_t out_size, const char *source_start) {
  const char *end;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!source_start) {
    return;
  }
  source_start += 2;
  end = strchr(source_start, ')');
  if (!end) {
    return;
  }
  copy_trimmed_range(out, out_size, source_start, (size_t)(end - source_start));
}

static void performance_format_cpu_label(char *out, size_t out_size,
                                         const char *policy, long freq_khz) {
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

static int parse_cpu_label_value(const char *label, char *policy,
                                 size_t policy_size, long *freq_khz) {
  if (policy && policy_size > 0) {
    policy[0] = '\0';
  }
  if (freq_khz) {
    *freq_khz = 0;
  }
  if (!label || !policy || policy_size == 0 || !freq_khz) {
    return 0;
  }
  if (strcmp(label, "interactive") == 0 || strcmp(label, "Interactive") == 0) {
    copy_string(policy, policy_size, "interactive");
    return 1;
  } else if (strcmp(label, "performance") == 0 || strcmp(label, "Performance") == 0) {
    copy_string(policy, policy_size, "performance");
    return 1;
  } else if (strcmp(label, "ondemand") == 0 || strcmp(label, "Ondemand") == 0) {
    copy_string(policy, policy_size, "ondemand");
    return 1;
  } else if (strcmp(label, "schedutil") == 0 || strcmp(label, "Schedutil") == 0) {
    copy_string(policy, policy_size, "schedutil");
    return 1;
  } else if (strcmp(label, "conservative") == 0 ||
             strcmp(label, "Conservative") == 0) {
    copy_string(policy, policy_size, "conservative");
    return 1;
  }
  return 0;
}

static int performance_cpu_preset_index(const char *policy, long freq_khz) {
  size_t i;
  (void)freq_khz;

  if (!policy || !policy[0]) {
    return 0;
  }
  for (i = 0; i < PERFORMANCE_CPU_PRESET_COUNT; i++) {
    const struct performance_cpu_preset *preset = &PERFORMANCE_CPU_PRESETS[i];
    if (strcmp(policy, preset->policy) != 0) {
      continue;
    }
    return (int)i;
  }
  return 0;
}

static void performance_parse_current_cpu(struct ui_state *ui, const char *line) {
  const char *value;
  const char *source;
  char label[96];

  if (!ui || !line || strncmp(line, "current_cpu: ", 13) != 0) {
    return;
  }
  value = line + 13;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));

  ui->performance_cpu_policy[0] = '\0';
  ui->performance_cpu_freq_khz = 0;
  parse_cpu_label_value(label, ui->performance_cpu_policy,
                        sizeof(ui->performance_cpu_policy),
                        &ui->performance_cpu_freq_khz);
  performance_format_cpu_label(ui->performance_cpu_label,
                               sizeof(ui->performance_cpu_label),
                               ui->performance_cpu_policy,
                               ui->performance_cpu_freq_khz);
}

static int load_performance_core_state(struct ui_state *ui) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  pid_t pipe_pid;
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  ui->performance_cpu_policy[0] = '\0';
  ui->performance_cpu_freq_khz = 0;
  copy_string(ui->performance_cpu_label, sizeof(ui->performance_cpu_label),
              "launcher default");

  if (!performance_ensure_system(ui)) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    return 0;
  }
  if (!file_exists(text_ui)) {
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->performance_system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    return 0;
  }

  pipe = open_runtime_shell_pipe(cmd, &pipe_pid);
  if (!pipe) {
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    performance_parse_current_cpu(ui, line);
  }
  rc = close_runtime_shell_pipe(pipe, pipe_pid);
  return rc == 0;
}

static void add_performance_settings_entries(struct ui_state *ui) {
  load_performance_core_state(ui);
  add_setting_entry(ui, "performance_system", "System",
                    ui->performance_system_name[0] ? ui->performance_system_name : "-");
  add_setting_entry(ui, "performance_cpu_policy", "CPU Governor",
                    ui->performance_cpu_label);
  add_setting_entry(ui, "performance_clear_cpu_override", "Reset to Default", "");
}

static int load_settings_entries(struct ui_state *ui) {
  struct frontend_settings settings;

  ui->setting_count = 0;
  ui->settings_cursor = 0;
  if (!load_settings(ui->settings_path, &settings)) {
    return 0;
  }
  ui->frontend_settings = settings;
  load_theme_state(ui, settings.graphic_theme_id);
  apply_theme_setting_overrides(&ui->theme, &settings);
  switch (ui->settings_category) {
  case SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR:
    load_device_settings(ui);
    add_system_display_color_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST:
    load_device_settings(ui);
    add_system_brightness_test_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_TIME:
    load_device_settings(ui);
    add_system_time_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL:
    load_device_settings(ui);
    add_system_time_manual_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_INFORMATION:
    load_device_settings(ui);
    add_system_information_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_FACTORY_RESET:
    add_system_factory_reset_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM:
    load_device_settings(ui);
    add_system_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK:
    load_device_settings(ui);
    add_network_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK_SERVICE:
    load_device_settings(ui);
    add_network_service_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK_INFORMATION:
    load_device_settings(ui);
    load_device_runtime_status(ui);
    add_network_information_entries(ui);
    break;
  case SETTINGS_CATEGORY_PERFORMANCE:
    load_device_settings(ui);
    add_performance_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_UI_THEME:
    add_ui_theme_settings_entries(ui, &settings);
    break;
  case SETTINGS_CATEGORY_UI:
  default:
    add_ui_settings_entries(ui, &settings);
    break;
  }
  return 1;
}

static void select_setting_entry_by_id(struct ui_state *ui, const char *id) {
  size_t i;

  if (!ui || !id) {
    return;
  }
  for (i = 0; i < ui->setting_count; i++) {
    if (strcmp(ui->setting_entries[i].id, id) == 0) {
      ui->settings_cursor = i;
      return;
    }
  }
}

static int load_top_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;
  struct frontend_settings settings;

  ui->top_count = 0;
  if ((ui->refresh || !file_exists(ui->top_cache_path)) &&
      !run_scanner(ui->plumos_root, ui->sdcard_root, NULL, 0)) {
    copy_string(ui->status, sizeof(ui->status), "full scan failed or scanner is missing");
  }
  if (!load_settings(ui->settings_path, &settings)) {
    copy_string(ui->status, sizeof(ui->status), "settings read failed");
  }
  ui->frontend_settings = settings;

  json = read_file(ui->top_cache_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &start, &end)) {
    free(json);
    return 0;
  }

  cursor = start;
  while (ui->top_count < UI_MAX_TOP) {
    const char *obj_start;
    const char *obj_end;
    struct top_entry entry;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
    json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj_start, obj_end, "default_launch_profile", entry.default_launch_profile,
                    sizeof(entry.default_launch_profile));
    entry.rom_count = json_get_long(obj_start, obj_end, "rom_count", 0);
    entry.pinned = json_get_bool(obj_start, obj_end, "pinned", 0);
    if (!entry.display_name[0]) {
      copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
    }
    if (ui->show_all || settings.show_empty_systems || entry.rom_count > 0 || entry.pinned) {
      ui->top_entries[ui->top_count++] = entry;
    }
  }
  free(json);

  if (settings.show_favorites_on_top && ui->top_count < UI_MAX_TOP) {
    struct top_entry fav;
    memset(&fav, 0, sizeof(fav));
    copy_string(fav.id, sizeof(fav.id), "favorites");
    copy_string(fav.display_name, sizeof(fav.display_name), "Favorites");
    copy_string(fav.default_launch_profile, sizeof(fav.default_launch_profile),
                "internal:favorites");
    fav.rom_count = count_json_array_objects(ui->favorites_path, "favorites");
    fav.pinned = 1;
    fav.virtual_entry = 1;
    ui->top_entries[ui->top_count++] = fav;
  }
  if (settings.show_recent_on_top && ui->top_count < UI_MAX_TOP) {
    struct top_entry recent;
    memset(&recent, 0, sizeof(recent));
    copy_string(recent.id, sizeof(recent.id), "recent");
    copy_string(recent.display_name, sizeof(recent.display_name), "Recent");
    copy_string(recent.default_launch_profile, sizeof(recent.default_launch_profile),
                "internal:recent");
    recent.rom_count = count_json_array_objects(ui->recent_path, "recents");
    recent.pinned = 1;
    recent.virtual_entry = 1;
    ui->top_entries[ui->top_count++] = recent;
  }
  if (strcmp(settings.sort_systems, "name") == 0 && ui->top_count > 1) {
    qsort(ui->top_entries, ui->top_count, sizeof(ui->top_entries[0]), cmp_top_entry_name);
  }
  if (ui->top_cursor >= ui->top_count) {
    ui->top_cursor = ui->top_count ? ui->top_count - 1 : 0;
  }
  return 1;
}

static int load_start_menu_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *menus_start;
  const char *menus_end;
  const char *menu_cursor;

  ui->menu_count = 0;
  ui->menu_cursor = 0;
  copy_string(ui->menu_id, sizeof(ui->menu_id), "start");
  copy_tr(ui, "menu.title.start", "START", ui->menu_title, sizeof(ui->menu_title));
  json = read_file(ui->menus_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "menus", &menus_start, &menus_end)) {
    free(json);
    return 0;
  }

  menu_cursor = menus_start;
  while (ui->menu_count < UI_MAX_MENU) {
    const char *menu_start;
    const char *menu_end;
    const char *entries_start;
    const char *entries_end;
    const char *entry_cursor;
    char menu_id[64] = "";

    if (!json_next_object(&menu_cursor, menus_end, &menu_start, &menu_end)) {
      break;
    }
    json_get_string(menu_start, menu_end, "id", menu_id, sizeof(menu_id));
    if (strcmp(menu_id, "start") != 0) {
      continue;
    }
    if (!json_find_array(menu_start, menu_end, "entries", &entries_start, &entries_end)) {
      break;
    }
    entry_cursor = entries_start;
    while (ui->menu_count < UI_MAX_MENU) {
      const char *obj_start;
      const char *obj_end;
      struct menu_entry entry;

      if (!json_next_object(&entry_cursor, entries_end, &obj_start, &obj_end)) {
        break;
      }
      memset(&entry, 0, sizeof(entry));
      json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
      json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                      sizeof(entry.display_name));
      json_get_string(obj_start, obj_end, "kind", entry.kind, sizeof(entry.kind));
      json_get_string(obj_start, obj_end, "action", entry.action, sizeof(entry.action));
      entry.confirm = json_get_bool(obj_start, obj_end, "confirm", 0);
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      if (entry.id[0]) {
        char key[128];
        snprintf(key, sizeof(key), "menu.entry.%s.name", entry.id);
        copy_tr(ui, key, entry.display_name, entry.display_name,
                sizeof(entry.display_name));
      }
      ui->menu_entries[ui->menu_count++] = entry;
    }
    break;
  }
  free(json);
  return 1;
}

static int load_apps_menu_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *apps_start;
  const char *apps_end;
  const char *app_cursor;

  ui->menu_count = 0;
  ui->menu_cursor = 0;
  copy_string(ui->menu_id, sizeof(ui->menu_id), "apps");
  copy_tr(ui, "menu.title.apps", "Apps", ui->menu_title, sizeof(ui->menu_title));
  json = read_file(ui->apps_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "apps", &apps_start, &apps_end)) {
    free(json);
    return 0;
  }

  app_cursor = apps_start;
  while (ui->menu_count < UI_MAX_MENU) {
    const char *obj_start;
    const char *obj_end;
    struct menu_entry entry;
    char menu_id[64] = "";
    int visible;

    if (!json_next_object(&app_cursor, apps_end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    visible = json_get_bool(obj_start, obj_end, "visible", 1);
    json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
    json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj_start, obj_end, "kind", entry.kind, sizeof(entry.kind));
    json_get_string(obj_start, obj_end, "launch_profile", entry.action,
                    sizeof(entry.action));
    json_get_string(obj_start, obj_end, "menu", menu_id, sizeof(menu_id));
    entry.confirm = json_get_bool(obj_start, obj_end, "confirm", 0);
    entry.background = json_get_bool(obj_start, obj_end, "background", 0);
    entry.show_results = json_get_bool(obj_start, obj_end, "show_results", 0);
    if (!visible || !entry.id[0] || strcmp(menu_id, "apps") != 0) {
      continue;
    }
    if (!entry.display_name[0]) {
      copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
    }
    if (entry.id[0]) {
      char key[128];
      snprintf(key, sizeof(key), "app.%s.name", entry.id);
      copy_tr(ui, key, entry.display_name, entry.display_name,
              sizeof(entry.display_name));
    }
    ui->menu_entries[ui->menu_count++] = entry;
  }
  free(json);
  return 1;
}

static void select_menu_entry_by_id(struct ui_state *ui, const char *id) {
  size_t i;

  if (!ui || !id) {
    return;
  }
  for (i = 0; i < ui->menu_count; i++) {
    if (strcmp(ui->menu_entries[i].id, id) == 0) {
      ui->menu_cursor = i;
      return;
    }
  }
}

static int system_scraper_enabled_from_json(const char *json, size_t json_size,
                                            const char *system_id) {
  const char *systems_start;
  const char *systems_end;
  const char *cursor;

  if (!json || !system_id || !system_id[0]) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    return 0;
  }
  cursor = systems_start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    const char *scraper_start;
    const char *scraper_end;
    char id[64] = "";

    if (!json_next_object(&cursor, systems_end, &obj_start, &obj_end)) {
      break;
    }
    json_get_string(obj_start, obj_end, "id", id, sizeof(id));
    if (strcmp(id, system_id) != 0) {
      continue;
    }
    if (!json_find_object(obj_start, obj_end, "scraper", &scraper_start, &scraper_end)) {
      return 0;
    }
    return json_get_bool(scraper_start, scraper_end, "enabled", 0);
  }
  return 0;
}

static void clamp_scraping_choice_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->scraping_choice_count == 0) {
    ui->scraping_choice_cursor = 0;
  } else if (ui->scraping_choice_cursor > ui->scraping_choice_count) {
    ui->scraping_choice_cursor = ui->scraping_choice_count;
  }
}

static void clamp_scraping_menu_cursor(struct ui_state *ui) {
  if (!ui || ui->scraping_menu_cursor >= UI_SCRAPING_FIELD_COUNT) {
    if (ui) {
      ui->scraping_menu_cursor = 0;
    }
  }
}

static void clamp_scraping_kind_cursor(struct ui_state *ui) {
  if (!ui ||
      ui->scraping_kind_cursor >=
          sizeof(SCRAPING_KIND_CHOICES) / sizeof(SCRAPING_KIND_CHOICES[0])) {
    if (ui) {
      ui->scraping_kind_cursor = 0;
    }
  }
}

static const struct scraping_kind_choice *scraping_selected_kind(struct ui_state *ui) {
  clamp_scraping_kind_cursor(ui);
  return &SCRAPING_KIND_CHOICES[ui ? ui->scraping_kind_cursor : 0];
}

static const char *scraping_kind_display_name(const struct ui_state *ui,
                                              const struct scraping_kind_choice *kind) {
  if (!kind) {
    return "";
  }
  if (strcmp(kind->scraper_kind, "Named_Boxarts") == 0) {
    return tr(ui, "scraping.kind.box_art", kind->display_name);
  }
  if (strcmp(kind->scraper_kind, "Named_Titles") == 0) {
    return tr(ui, "scraping.kind.title_screen", kind->display_name);
  }
  return kind->display_name;
}

static const char *scraping_existing_label(const struct ui_state *ui) {
  return ui && ui->scraping_replace_existing
             ? tr(ui, "scraping.existing.replace", "Replace")
             : tr(ui, "scraping.existing.skip", "Skip");
}

static void cycle_scraping_kind(struct ui_state *ui, int delta) {
  size_t count = sizeof(SCRAPING_KIND_CHOICES) / sizeof(SCRAPING_KIND_CHOICES[0]);

  if (!ui || count == 0) {
    return;
  }
  clamp_scraping_kind_cursor(ui);
  if (delta < 0) {
    ui->scraping_kind_cursor =
        ui->scraping_kind_cursor == 0 ? count - 1 : ui->scraping_kind_cursor - 1;
  } else {
    ui->scraping_kind_cursor =
        ui->scraping_kind_cursor + 1 >= count ? 0 : ui->scraping_kind_cursor + 1;
  }
}

static int load_scraping_choices(struct ui_state *ui) {
  char *json;
  size_t json_size;
  char selected_id[64] = "";
  size_t i;

  if (!ui) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    copy_string(selected_id, sizeof(selected_id),
                ui->scraping_choices[ui->scraping_choice_cursor - 1].id);
  }
  ui->scraping_choice_count = 0;
  json = read_file(ui->systems_path, &json_size);
  if (!json) {
    ui->scraping_choice_cursor = 0;
    return 0;
  }
  for (i = 0; i < ui->top_count && ui->scraping_choice_count < UI_MAX_SCRAPING_CHOICES; i++) {
    const struct top_entry *top = &ui->top_entries[i];
    struct scraping_choice *choice;

    if (top->virtual_entry || top->rom_count <= 0 || !valid_system_id(top->id) ||
        !system_scraper_enabled_from_json(json, json_size, top->id)) {
      continue;
    }
    choice = &ui->scraping_choices[ui->scraping_choice_count++];
    memset(choice, 0, sizeof(*choice));
    copy_string(choice->id, sizeof(choice->id), top->id);
    copy_string(choice->display_name, sizeof(choice->display_name),
                top->display_name[0] ? top->display_name : top->id);
    choice->rom_count = top->rom_count;
  }
  free(json);
  ui->scraping_choice_cursor = 0;
  if (selected_id[0]) {
    for (i = 0; i < ui->scraping_choice_count; i++) {
      if (strcmp(ui->scraping_choices[i].id, selected_id) == 0) {
        ui->scraping_choice_cursor = i + 1;
        break;
      }
    }
  }
  clamp_scraping_choice_cursor(ui);
  return 1;
}

static long scraping_selected_rom_count(const struct ui_state *ui) {
  long total = 0;
  size_t i;

  if (!ui || ui->scraping_choice_count == 0) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return ui->scraping_choices[ui->scraping_choice_cursor - 1].rom_count;
  }
  for (i = 0; i < ui->scraping_choice_count; i++) {
    total += ui->scraping_choices[i].rom_count;
  }
  return total;
}

static const char *scraping_selected_label(const struct ui_state *ui) {
  if (!ui || ui->scraping_choice_count == 0) {
    return tr(ui, "scraping.system.none", "NONE");
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return ui->scraping_choices[ui->scraping_choice_cursor - 1].display_name;
  }
  return tr(ui, "scraping.system.all", "ALL");
}

static void add_thumbnail_result_line(struct ui_state *ui, const char *line) {
  if (!ui || !line || !line[0]) {
    return;
  }
  if (ui->thumbnail_result_count >= UI_THUMBNAIL_RESULT_MAX_LINES) {
    memmove(ui->thumbnail_result_lines, ui->thumbnail_result_lines + 1,
            sizeof(ui->thumbnail_result_lines[0]) *
                (UI_THUMBNAIL_RESULT_MAX_LINES - 1));
    ui->thumbnail_result_count = UI_THUMBNAIL_RESULT_MAX_LINES - 1;
  }
  copy_truncated_string(ui->thumbnail_result_lines[ui->thumbnail_result_count],
                        sizeof(ui->thumbnail_result_lines[ui->thumbnail_result_count]), line);
  ui->thumbnail_result_count++;
}

static const char *thumbnail_action_label(const struct ui_state *ui, const char *id) {
  if (!id) {
    return tr(ui, "thumbnail_task.title", "Thumbnail Task");
  }
  if (strcmp(id, "thumbnail-plan") == 0) {
    return tr(ui, "app.thumbnail-plan.name", "Thumbnail Plan");
  }
  if (strcmp(id, "thumbnail-fetch") == 0) {
    return tr(ui, "app.thumbnail-fetch.name", "Fetch Thumbnails");
  }
  if (strcmp(id, "thumbnail-scraping") == 0) {
    return tr(ui, "app.scraping.name", "Scraping");
  }
  return id;
}

static void clamp_thumbnail_result_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->thumbnail_result_count == 0) {
    ui->thumbnail_result_cursor = 0;
  } else if (ui->thumbnail_result_cursor >= ui->thumbnail_result_count) {
    ui->thumbnail_result_cursor = ui->thumbnail_result_count - 1;
  }
}

static void reset_thumbnail_running_progress(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->thumbnail_running_phase[0] = '\0';
  ui->thumbnail_running_system[0] = '\0';
  ui->thumbnail_progress_current = 0;
  ui->thumbnail_progress_total = 0;
  ui->thumbnail_progress_downloaded = 0;
  ui->thumbnail_progress_no_match = 0;
  ui->thumbnail_progress_failed = 0;
}

static int update_thumbnail_running_progress_from_log_line(struct ui_state *ui,
                                                           const char *line) {
  char buf[UI_RENDER_LINE_MAX];
  char *fields[10];
  char *save = NULL;
  char *token;
  size_t count = 0;
  long current = 0;
  long total = 0;
  long downloaded = 0;
  long no_match = 0;
  long failed = 0;

  if (!ui || !line || strncmp(line, "progress\t", 9) != 0) {
    return 0;
  }
  copy_truncated_string(buf, sizeof(buf), line);
  for (token = strtok_r(buf, "\t", &save);
       token && count < sizeof(fields) / sizeof(fields[0]);
       token = strtok_r(NULL, "\t", &save)) {
    fields[count++] = token;
  }
  if (count < 5 ||
      !parse_nonnegative_long(fields[3], &current) ||
      !parse_nonnegative_long(fields[4], &total)) {
    return 0;
  }
  if (count >= 6) {
    parse_nonnegative_long(fields[5], &downloaded);
  }
  if (count >= 7) {
    parse_nonnegative_long(fields[6], &no_match);
  }
  if (count >= 8) {
    parse_nonnegative_long(fields[7], &failed);
  }

  copy_string(ui->thumbnail_running_phase, sizeof(ui->thumbnail_running_phase),
              fields[1]);
  copy_string(ui->thumbnail_running_system, sizeof(ui->thumbnail_running_system),
              fields[2]);
  ui->thumbnail_progress_current = current;
  ui->thumbnail_progress_total = total;
  ui->thumbnail_progress_downloaded = downloaded;
  ui->thumbnail_progress_no_match = no_match;
  ui->thumbnail_progress_failed = failed;
  return 1;
}

static void add_thumbnail_result_from_log_line(struct ui_state *ui, const char *line) {
  char buf[UI_RENDER_LINE_MAX];
  char *fields[20];
  char *save = NULL;
  char *token;
  size_t count = 0;
  char out[UI_RENDER_LINE_MAX];

  if (!ui || !line || !line[0] || strncmp(line, "status\t", 7) == 0 ||
      strncmp(line, "progress\t", 9) == 0) {
    return;
  }
  copy_truncated_string(buf, sizeof(buf), line);
  for (token = strtok_r(buf, "\t", &save);
       token && count < sizeof(fields) / sizeof(fields[0]);
       token = strtok_r(NULL, "\t", &save)) {
    fields[count++] = token;
  }
  if (count == 0) {
    return;
  }
  if (strcmp(fields[0], "scraping_options") == 0 && count >= 4) {
    snprintf(out, sizeof(out), "%s", "options");
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[2]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[3]);
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "plan") == 0 && count >= 8) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "reason %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "aliases seen %s", fields[4]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "ROMs %s", fields[5]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "existing %s", fields[6]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "missing %s", fields[7]);
    add_thumbnail_result_line(ui, out);
    if (count >= 10) {
      snprintf(out, sizeof(out), "CRC workers %s", fields[8]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "DL workers %s", fields[9]);
      add_thumbnail_result_line(ui, out);
    }
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "fetch") == 0 && count >= 15) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "reason %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "aliases seen %s", fields[4]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "ROMs %s", fields[5]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "existing %s", fields[6]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "missing %s", fields[7]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC checked %s", fields[8]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC matched %s", fields[9]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "downloaded %s", fields[10]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "no match %s", fields[11]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC miss %s", fields[12]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "image miss %s", fields[13]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "download failed %s", fields[14]);
    add_thumbnail_result_line(ui, out);
    if (count >= 18) {
      snprintf(out, sizeof(out), "invalid PNG %s", fields[15]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "skipped zip %s", fields[16]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "skipped tool %s", fields[17]);
      add_thumbnail_result_line(ui, out);
    }
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "disabled") == 0 && count >= 4) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "disabled %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "app_start") == 0 && count >= 2) {
    snprintf(out, sizeof(out), "%s", thumbnail_action_label(ui, fields[1]));
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "started");
  } else if (strcmp(fields[0], "app_finish") == 0 && count >= 3) {
    snprintf(out, sizeof(out), "%s", thumbnail_action_label(ui, fields[1]));
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "finished %s", fields[2]);
    add_thumbnail_result_line(ui, out);
  } else if (strcmp(fields[0], "app_already_running") == 0 && count >= 2) {
    snprintf(out, sizeof(out), "%s is already running",
             thumbnail_action_label(ui, fields[1]));
    add_thumbnail_result_line(ui, out);
  } else if (strcmp(ui->thumbnail_result_return_app_id, "scraping") != 0) {
    char *p;

    copy_truncated_string(out, sizeof(out), line);
    for (p = out; *p; p++) {
      if (*p == '\t') {
        *p = ' ';
      }
    }
    add_thumbnail_result_line(ui, out);
  }
}

static int load_thumbnail_results(struct ui_state *ui) {
  char log_path[PATH_MAX];
  FILE *f;
  char line[UI_RENDER_LINE_MAX];

  if (!ui) {
    return 0;
  }
  ui->thumbnail_result_count = 0;
  if (!join_path(log_path, sizeof(log_path), ui->plumos_root,
                 "logs/frontend-apps-latest.log")) {
    add_thumbnail_result_line(ui, "result log path is too long");
    clamp_thumbnail_result_cursor(ui);
    return 0;
  }
  f = fopen(log_path, "rb");
  if (!f) {
    add_thumbnail_result_line(ui, "No thumbnail result yet");
    clamp_thumbnail_result_cursor(ui);
    return 1;
  }
  while (fgets(line, sizeof(line), f)) {
    trim_line_end(line);
    add_thumbnail_result_from_log_line(ui, line);
  }
  fclose(f);
  if (ui->thumbnail_result_count == 0) {
    add_thumbnail_result_line(ui, "No thumbnail result yet");
  }
  clamp_thumbnail_result_cursor(ui);
  return 1;
}

static int load_favorite_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;

  ui_clear_rom_entries(ui);
  json = read_file(ui->favorites_path, &json_size);
  if (!json) {
    return file_exists(ui->favorites_path) ? 0 : 1;
  }
  if (!json_find_array(json, json + json_size, "favorites", &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    const char *media_start;
    const char *media_end;
    struct rom_entry entry;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    entry.is_favorite = 1;
    {
      size_t pos = 0;
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.system_id);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.relative_path);
    }
    if (!ui_append_rom_entry(ui, &entry)) {
      free(json);
      return 0;
    }
  }
  free(json);
  return 1;
}

static int favorite_entry_index(const struct rom_entry *entries, size_t count,
                                const char *system_id,
                                const char *relative_path) {
  size_t i;

  if (!entries || !system_id || !relative_path) {
    return -1;
  }
  for (i = 0; i < count; i++) {
    if (strcmp(entries[i].system_id, system_id) == 0 &&
        strcmp(entries[i].relative_path, relative_path) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static void update_top_favorites_count(struct ui_state *ui, size_t count) {
  size_t i;

  if (!ui) {
    return;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (strcmp(ui->top_entries[i].id, "favorites") == 0) {
      ui->top_entries[i].rom_count = (long)count;
      return;
    }
  }
}

static int current_rom_is_favorite(const struct ui_state *ui,
                                   const struct rom_entry *entry) {
  if (!ui || !entry || entry->is_navigation_directory ||
      !entry->relative_path[0]) {
    return 0;
  }
  if (ui->screen == SCREEN_FAVORITES) {
    return 1;
  }
  return entry->is_favorite;
}

static void mark_favorite_flags(struct ui_state *ui) {
  struct ui_state *favorites;
  size_t i;

  if (!ui || ui->screen == SCREEN_FAVORITES) {
    return;
  }
  for (i = 0; i < ui->rom_count; i++) {
    ui->rom_entries[i].is_favorite = 0;
  }
  favorites = calloc(1, sizeof(*favorites));
  if (!favorites) {
    return;
  }
  copy_string(favorites->favorites_path, sizeof(favorites->favorites_path),
              ui->favorites_path);
  if (load_favorite_entries(favorites)) {
    for (i = 0; i < ui->rom_count; i++) {
      const char *system_id =
          ui->rom_entries[i].system_id[0] ? ui->rom_entries[i].system_id
                                          : ui->current_system_id;
      if (ui->rom_entries[i].is_navigation_directory ||
          !valid_system_id(system_id) ||
          !valid_relative_rom_path(ui->rom_entries[i].relative_path)) {
        continue;
      }
      ui->rom_entries[i].is_favorite =
          favorite_entry_index(favorites->rom_entries, favorites->rom_count,
                               system_id,
                               ui->rom_entries[i].relative_path) >= 0;
    }
  }
  ui_free_rom_entries(favorites);
  free(favorites);
}

static int save_favorite_entries(const struct ui_state *ui,
                                 const struct rom_entry *entries,
                                 size_t count) {
  char tmp_path[PATH_MAX];
  FILE *f;
  int fd;
  size_t i;

  if (!ui || (!entries && count > 0) ||
      !ensure_parent_dir_for_file(ui->favorites_path)) {
    return 0;
  }
  if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", ui->favorites_path) >=
      (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }
  fprintf(f, "{\n  \"version\": 1,\n  \"favorites\": [\n");
  for (i = 0; i < count; i++) {
    fprintf(f, "    { \"system_id\": ");
    fprint_json_string(f, entries[i].system_id);
    fprintf(f, ", \"relative_path\": ");
    fprint_json_string(f, entries[i].relative_path);
    fprintf(f, ", \"title\": ");
    fprint_json_string(f, entries[i].title);
    fprintf(f, ", \"path\": ");
    fprint_json_string(f, entries[i].path);
    fprintf(f, ", \"media\": { \"thumbnail\": ");
    fprint_json_string(f, entries[i].thumbnail);
    fprintf(f, " } }%s\n", i + 1 < count ? "," : "");
  }
  fprintf(f, "  ]\n}\n");

  fd = fileno(f);
  if (fflush(f) != 0 || (fd >= 0 && fsync(fd) != 0) || fclose(f) != 0) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, ui->favorites_path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  sync();
  return 1;
}

static int toggle_current_favorite(struct ui_state *ui) {
  struct ui_state *temp;
  struct rom_entry *entry;
  const char *system_id;
  int idx;

  if (!ui) {
    return 0;
  }
  if (ui->rom_count == 0 || ui->rom_cursor >= ui->rom_count) {
    set_status(ui, "no ROM selected");
    return 0;
  }
  entry = &ui->rom_entries[ui->rom_cursor];
  if (entry->is_navigation_directory) {
    set_status(ui, "directory cannot be favorited");
    return 0;
  }
  system_id = entry->system_id[0] ? entry->system_id : ui->current_system_id;
  if (!valid_system_id(system_id) ||
      !valid_relative_rom_path(entry->relative_path)) {
    set_status(ui, "favorite target is invalid");
    return 0;
  }

  temp = calloc(1, sizeof(*temp));
  if (!temp) {
    set_status(ui, "cannot load Favorites");
    return 0;
  }
  copy_string(temp->favorites_path, sizeof(temp->favorites_path), ui->favorites_path);
  if (!load_favorite_entries(temp)) {
    ui_free_rom_entries(temp);
    free(temp);
    set_status(ui, "cannot load Favorites");
    return 0;
  }

  idx = favorite_entry_index(temp->rom_entries, temp->rom_count, system_id,
                             entry->relative_path);
  if (idx >= 0) {
    if ((size_t)idx + 1 < temp->rom_count) {
      memmove(&temp->rom_entries[idx], &temp->rom_entries[idx + 1],
              (temp->rom_count - (size_t)idx - 1) * sizeof(temp->rom_entries[0]));
    }
    temp->rom_count--;
    if (!save_favorite_entries(ui, temp->rom_entries, temp->rom_count)) {
      ui_free_rom_entries(temp);
      free(temp);
      set_status(ui, "cannot update Favorites");
      return 0;
    }
    update_top_favorites_count(ui, temp->rom_count);
    if (ui->screen == SCREEN_FAVORITES ||
        (ui->screen == SCREEN_GALLERY &&
         ui->gallery_back_screen == SCREEN_FAVORITES)) {
      size_t old_cursor = ui->rom_cursor;
      load_favorite_entries(ui);
      if (ui->rom_count > 0 && old_cursor >= ui->rom_count) {
        ui->rom_cursor = ui->rom_count - 1;
      } else if (ui->rom_count > 0) {
        ui->rom_cursor = old_cursor;
      }
    } else {
      entry->is_favorite = 0;
    }
    ui_free_rom_entries(temp);
    free(temp);
    set_status(ui, "removed from Favorites");
    return 1;
  }

  {
    struct rom_entry favorite;
    memset(&favorite, 0, sizeof(favorite));
    copy_string(favorite.system_id, sizeof(favorite.system_id), system_id);
    copy_string(favorite.relative_path, sizeof(favorite.relative_path),
                entry->relative_path);
    copy_string(favorite.title, sizeof(favorite.title),
                entry->title[0] ? entry->title : entry->relative_path);
    copy_string(favorite.path, sizeof(favorite.path), entry->path);
    copy_string(favorite.thumbnail, sizeof(favorite.thumbnail), entry->thumbnail);
    if (!ui_append_rom_entry(temp, &favorite)) {
      ui_free_rom_entries(temp);
      free(temp);
      set_status(ui, "cannot update Favorites");
      return 0;
    }
  }
  if (!save_favorite_entries(ui, temp->rom_entries, temp->rom_count)) {
    ui_free_rom_entries(temp);
    free(temp);
    set_status(ui, "cannot update Favorites");
    return 0;
  }
  update_top_favorites_count(ui, temp->rom_count);
  entry->is_favorite = 1;
  ui_free_rom_entries(temp);
  free(temp);
  set_status(ui, "added to Favorites");
  return 1;
}

static int load_recent_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;

  ui_clear_rom_entries(ui);
  json = read_file(ui->recent_path, &json_size);
  if (!json) {
    return file_exists(ui->recent_path) ? 0 : 1;
  }
  if (!json_find_array(json, json + json_size, "recents", &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    const char *media_start;
    const char *media_end;
    struct rom_entry entry;
    char last_played_at[64] = "";

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
    json_get_string(obj_start, obj_end, "launch_profile", entry.launch_profile,
                    sizeof(entry.launch_profile));
    json_get_string(obj_start, obj_end, "last_played_at", last_played_at,
                    sizeof(last_played_at));
    entry.resume_available = json_get_bool(obj_start, obj_end, "resume_available", 0);
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    {
      size_t pos = strlen(entry.detail);
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.system_id);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / resume=");
      append_string(entry.detail, sizeof(entry.detail), &pos,
                    entry.resume_available ? "yes" : "no");
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos,
                    entry.launch_profile[0] ? entry.launch_profile : "-");
    }
    if (last_played_at[0]) {
      size_t pos = strlen(entry.detail);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos, last_played_at);
    }
    if (!ui_append_rom_entry(ui, &entry)) {
      free(json);
      return 0;
    }
  }
  free(json);
  mark_favorite_flags(ui);
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

static int rom_browser_child_for_path(const char *relative_path, const char *current_dir,
                                      char *dir_relative_path, size_t dir_relative_path_size,
                                      char *child_name, size_t child_name_size,
                                      int *is_directory) {
  const char *tail;
  const char *slash;
  size_t base_len = 0;
  size_t segment_len;
  int written;

  if (!relative_path || !relative_path[0] || !dir_relative_path ||
      dir_relative_path_size == 0 || !child_name || child_name_size == 0 ||
      !is_directory) {
    return 0;
  }
  dir_relative_path[0] = '\0';
  child_name[0] = '\0';
  *is_directory = 0;

  if (current_dir && current_dir[0]) {
    size_t prefix_len = strlen(current_dir);
    if (strncmp(relative_path, current_dir, prefix_len) != 0 ||
        relative_path[prefix_len] != '/') {
      return 0;
    }
    base_len = prefix_len;
    tail = relative_path + prefix_len + 1;
  } else {
    slash = strchr(relative_path, '/');
    if (slash && slash[1]) {
      base_len = (size_t)(slash - relative_path);
      tail = slash + 1;
    } else {
      tail = relative_path;
    }
  }
  if (!tail[0]) {
    return 0;
  }

  slash = strchr(tail, '/');
  if (!slash) {
    return 1;
  }
  segment_len = (size_t)(slash - tail);
  if (segment_len == 0 || segment_len >= child_name_size) {
    return 0;
  }
  if ((segment_len == 1 && tail[0] == '.') ||
      (segment_len == 2 && tail[0] == '.' && tail[1] == '.')) {
    return 0;
  }
  memcpy(child_name, tail, segment_len);
  child_name[segment_len] = '\0';
  if (base_len > 0) {
    written = snprintf(dir_relative_path, dir_relative_path_size, "%.*s/%s",
                       (int)base_len, relative_path, child_name);
  } else {
    written = snprintf(dir_relative_path, dir_relative_path_size, "%s",
                       child_name);
  }
  if (written <= 0 || (size_t)written >= dir_relative_path_size) {
    dir_relative_path[0] = '\0';
    child_name[0] = '\0';
    return 0;
  }
  *is_directory = 1;
  return 1;
}

static int rom_navigation_directory_exists(const struct ui_state *ui,
                                           const char *relative_path) {
  size_t i;

  if (!ui || !relative_path || !relative_path[0]) {
    return 0;
  }
  for (i = 0; i < ui->rom_count; i++) {
    if (ui->rom_entries[i].is_navigation_directory &&
        strcmp(ui->rom_entries[i].relative_path, relative_path) == 0) {
      return 1;
    }
  }
  return 0;
}

static int build_navigation_directory_path(char *out, size_t out_size,
                                           const struct rom_entry *source,
                                           const char *dir_relative_path) {
  size_t rel_len;
  size_t path_len;
  size_t dir_len;
  size_t prefix_len;

  if (!out || out_size == 0 || !source || !source->path[0] ||
      !source->relative_path[0] || !dir_relative_path ||
      !dir_relative_path[0]) {
    return 0;
  }
  out[0] = '\0';
  rel_len = strlen(source->relative_path);
  path_len = strlen(source->path);
  dir_len = strlen(dir_relative_path);
  if (path_len >= rel_len &&
      strcmp(source->path + path_len - rel_len, source->relative_path) == 0) {
    prefix_len = path_len - rel_len;
    if (prefix_len + dir_len + 1 > out_size) {
      return 0;
    }
    memcpy(out, source->path, prefix_len);
    memcpy(out + prefix_len, dir_relative_path, dir_len);
    out[prefix_len + dir_len] = '\0';
    return 1;
  }
  return dirname_path(out, out_size, source->path);
}

static int add_navigation_directory_entry(struct ui_state *ui,
                                          const struct rom_entry *source,
                                          const char *relative_path,
                                          const char *display_name) {
  struct rom_entry entry;
  int written;

  if (!ui || !source || !relative_path || !relative_path[0] ||
      !display_name || !display_name[0] ||
      rom_navigation_directory_exists(ui, relative_path)) {
    return 1;
  }
  memset(&entry, 0, sizeof(entry));
  copy_string(entry.system_id, sizeof(entry.system_id), source->system_id);
  written = snprintf(entry.title, sizeof(entry.title), "[DIR] %s", display_name);
  if (written <= 0 || (size_t)written >= sizeof(entry.title)) {
    copy_string(entry.title, sizeof(entry.title), "[DIR]");
  }
  copy_string(entry.relative_path, sizeof(entry.relative_path), relative_path);
  build_navigation_directory_path(entry.path, sizeof(entry.path), source, relative_path);
  copy_string(entry.detail, sizeof(entry.detail), "Directory");
  copy_string(entry.extension, sizeof(entry.extension), "dir");
  entry.is_navigation_directory = 1;
  return ui_append_rom_entry(ui, &entry);
}

static int load_rom_entries(struct ui_state *ui, const char *system_id) {
  char path[PATH_MAX];
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;
  struct frontend_settings settings;
  int cache_exists;
  int scan_on_enter;
  int with_thumbnails;

  ui_clear_rom_entries(ui);
  ui->rom_scan_background_started = 0;
  if (strcmp(system_id, "favorites") == 0) {
    return load_favorite_entries(ui);
  }
  if (!build_system_cache_path(path, sizeof(path), ui->plumos_root, system_id)) {
    return 0;
  }
  if (!load_settings(ui->settings_path, &settings)) {
    copy_string(ui->status, sizeof(ui->status), "settings read failed; using scan defaults");
  }
  ui->frontend_settings = settings;
  with_thumbnails =
      strcmp(settings.rom_mode[0] ? settings.rom_mode : settings.ui_mode, "graphic") == 0;
  cache_exists = file_exists(path);
  scan_on_enter = rom_scan_policy_is_on_enter(settings.rom_scan_policy);
  if (!cache_exists) {
    if (!run_scanner(ui->plumos_root, ui->sdcard_root, system_id, with_thumbnails)) {
      return 0;
    }
    cache_exists = file_exists(path);
    if (!cache_exists) {
      return 0;
    }
  } else if ((scan_on_enter || ui->refresh) && !ui->rom_scan_refresh_suppressed) {
    if (with_thumbnails) {
      if (!run_scanner(ui->plumos_root, ui->sdcard_root, system_id, 1)) {
        copy_string(ui->status, sizeof(ui->status),
                    "thumbnail scan failed; using cached ROM list");
      }
    } else {
      trigger_rom_scan_refresh(ui, system_id, 0);
    }
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "roms", &start, &end)) {
    free(json);
    return 0;
  }

  cursor = start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    const char *media_start;
    const char *media_end;
    struct rom_entry entry;
    char dir_relative_path[UI_PATH_MAX];
    char child_name[256];
    int child_is_directory = 0;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    copy_string(entry.system_id, sizeof(entry.system_id), system_id);
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    json_get_string(obj_start, obj_end, "extension", entry.extension,
                    sizeof(entry.extension));
    if (!rom_browser_child_for_path(entry.relative_path, ui->rom_directory,
                                    dir_relative_path, sizeof(dir_relative_path),
                                    child_name, sizeof(child_name),
                                    &child_is_directory)) {
      continue;
    }
    if (child_is_directory) {
      if (!add_navigation_directory_entry(ui, &entry, dir_relative_path, child_name)) {
        free(json);
        return 0;
      }
      continue;
    }
    if (!ui_append_rom_entry(ui, &entry)) {
      free(json);
      return 0;
    }
  }
  free(json);
  if (ui->rom_count > 1) {
    if (strcmp(settings.sort_roms, "path") == 0) {
      qsort(ui->rom_entries, ui->rom_count, sizeof(ui->rom_entries[0]), cmp_rom_entry_path);
    } else {
      qsort(ui->rom_entries, ui->rom_count, sizeof(ui->rom_entries[0]), cmp_rom_entry_name);
    }
  }
  mark_favorite_flags(ui);
  return 1;
}

static int ui_has_rom_cursor_context(const struct ui_state *ui) {
  return ui && ui->current_system_id[0];
}

static int ui_find_rom_cursor_memory(const struct ui_state *ui,
                                     const char *system_id,
                                     const char *directory) {
  size_t i;

  if (!ui || !system_id || !system_id[0] || !directory) {
    return -1;
  }
  for (i = 0; i < ui->rom_cursor_memory_count; i++) {
    if (strcmp(ui->rom_cursor_memory[i].system_id, system_id) == 0 &&
        strcmp(ui->rom_cursor_memory[i].directory, directory) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static void remember_current_rom_cursor(struct ui_state *ui) {
  const struct rom_entry *entry;
  int idx;

  if (!ui_has_rom_cursor_context(ui) || ui->rom_count == 0 ||
      ui->rom_cursor >= ui->rom_count) {
    return;
  }
  entry = &ui->rom_entries[ui->rom_cursor];
  if (!entry->relative_path[0]) {
    return;
  }
  idx = ui_find_rom_cursor_memory(ui, ui->current_system_id, ui->rom_directory);
  if (idx < 0) {
    if (ui->rom_cursor_memory_count >= UI_ROM_CURSOR_MEMORY_MAX) {
      memmove(&ui->rom_cursor_memory[0], &ui->rom_cursor_memory[1],
              (UI_ROM_CURSOR_MEMORY_MAX - 1) * sizeof(ui->rom_cursor_memory[0]));
      idx = UI_ROM_CURSOR_MEMORY_MAX - 1;
    } else {
      idx = (int)ui->rom_cursor_memory_count++;
    }
  }
  copy_string(ui->rom_cursor_memory[idx].system_id,
              sizeof(ui->rom_cursor_memory[idx].system_id),
              ui->current_system_id);
  copy_string(ui->rom_cursor_memory[idx].directory,
              sizeof(ui->rom_cursor_memory[idx].directory), ui->rom_directory);
  copy_string(ui->rom_cursor_memory[idx].relative_path,
              sizeof(ui->rom_cursor_memory[idx].relative_path),
              entry->relative_path);
}

static int restore_current_rom_cursor(struct ui_state *ui) {
  int idx;
  size_t i;
  const char *relative_path;

  if (!ui_has_rom_cursor_context(ui) || ui->rom_count == 0) {
    return 0;
  }
  idx = ui_find_rom_cursor_memory(ui, ui->current_system_id, ui->rom_directory);
  if (idx < 0) {
    ui->rom_cursor = 0;
    return 0;
  }
  relative_path = ui->rom_cursor_memory[idx].relative_path;
  if (!relative_path[0]) {
    ui->rom_cursor = 0;
    return 0;
  }
  for (i = 0; i < ui->rom_count; i++) {
    if (strcmp(ui->rom_entries[i].relative_path, relative_path) == 0) {
      ui->rom_cursor = i;
      return 1;
    }
  }
  ui->rom_cursor = 0;
  return 0;
}

static void ui_append_render_line(struct ui_state *ui, const char *line, size_t len) {
  if (ui->render_line_count >= UI_RENDER_MAX_LINES) {
    return;
  }
  if (len >= UI_RENDER_LINE_MAX) {
    len = UI_RENDER_LINE_MAX - 1;
  }
  memcpy(ui->render_lines[ui->render_line_count], line, len);
  ui->render_lines[ui->render_line_count][len] = '\0';
  ui->render_line_count++;
}

static void core_append_line(struct ui_state *ui, const char *line) {
  size_t len;
  if (ui->core_line_count >= UI_RENDER_MAX_LINES || !line) {
    return;
  }
  len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    len--;
  }
  if (len >= UI_RENDER_LINE_MAX) {
    len = UI_RENDER_LINE_MAX - 1;
  }
  memcpy(ui->core_lines[ui->core_line_count], line, len);
  ui->core_lines[ui->core_line_count][len] = '\0';
  ui->core_line_count++;
}

static const char *core_profile_display_name(const char *profile) {
  static char display[128];
  const char *label = NULL;
  const char *value = NULL;

  if (!profile || !profile[0]) {
    return "auto";
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
    snprintf(display, sizeof(display), "%s: %s", label, value);
    display[sizeof(display) - 1] = '\0';
    return display;
  }
  return profile;
}

static int core_menu_row_is_selectable(size_t row) {
  return row < CORE_MENU_ROW_COUNT && row != CORE_MENU_ROW_SEPARATOR;
}

static void core_menu_clamp_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->core_menu_cursor >= CORE_MENU_ROW_COUNT) {
    ui->core_menu_cursor = CORE_MENU_ROW_PROFILE;
  }
  if (!core_menu_row_is_selectable(ui->core_menu_cursor)) {
    ui->core_menu_cursor = CORE_MENU_ROW_CPU_FREQ;
  }
}

static void move_core_menu_cursor(struct ui_state *ui, int direction) {
  size_t row;

  if (!ui || direction == 0) {
    return;
  }
  core_menu_clamp_cursor(ui);
  row = ui->core_menu_cursor;
  while (1) {
    if (direction > 0) {
      row = (row + 1) % CORE_MENU_ROW_COUNT;
    } else if (row == 0) {
      row = CORE_MENU_ROW_COUNT - 1;
    } else {
      row--;
    }
    if (core_menu_row_is_selectable(row)) {
      ui->core_menu_cursor = row;
      return;
    }
  }
}

static void reset_core_profile_choices(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  memset(ui->core_profiles, 0, sizeof(ui->core_profiles));
  ui->core_profile_count = 0;
  ui->core_profile_cursor = 0;
  ui->core_current_profile[0] = '\0';
  ui->core_current_source[0] = '\0';
  ui->core_cpu_policy[0] = '\0';
  ui->core_cpu_freq_khz = 0;
  copy_string(ui->core_cpu_label, sizeof(ui->core_cpu_label), "launcher default");
  copy_string(ui->core_cpu_source, sizeof(ui->core_cpu_source), "unavailable");
  ui->core_line_count = 0;
}

static int add_core_profile_choice(struct ui_state *ui, const char *profile) {
  size_t i;

  if (!ui || !valid_launch_profile_id(profile)) {
    return 0;
  }
  for (i = 0; i < ui->core_profile_count; i++) {
    if (strcmp(ui->core_profiles[i].id, profile) == 0) {
      return 1;
    }
  }
  if (ui->core_profile_count >= UI_MAX_CORE_PROFILES) {
    return 0;
  }
  if (!copy_string(ui->core_profiles[ui->core_profile_count].id,
                   sizeof(ui->core_profiles[ui->core_profile_count].id), profile)) {
    return 0;
  }
  ui->core_profile_count++;
  return 1;
}

static void parse_core_current_profile_line(struct ui_state *ui, const char *line) {
  const char *prefix = "current_profile:";
  const char *value;
  const char *source_start;
  const char *source_end;

  if (!ui || !line || strncmp(line, prefix, strlen(prefix)) != 0) {
    return;
  }
  value = line + strlen(prefix);
  while (*value == ' ') {
    value++;
  }
  source_start = strstr(value, " (");
  if (source_start) {
    copy_trimmed_range(ui->core_current_profile, sizeof(ui->core_current_profile),
                       value, (size_t)(source_start - value));
    source_start += 2;
    source_end = strchr(source_start, ')');
    if (source_end) {
      copy_trimmed_range(ui->core_current_source, sizeof(ui->core_current_source),
                         source_start, (size_t)(source_end - source_start));
    }
  } else {
    copy_truncated_string(ui->core_current_profile, sizeof(ui->core_current_profile), value);
  }
}

static void parse_core_current_cpu_line(struct ui_state *ui, const char *line) {
  const char *value;
  const char *source;
  char label[96];

  if (!ui || !line || strncmp(line, "current_cpu: ", 13) != 0) {
    return;
  }
  value = line + 13;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));
  if (source) {
    copy_parenthesized_source(ui->core_cpu_source, sizeof(ui->core_cpu_source),
                              source);
  }

  ui->core_cpu_policy[0] = '\0';
  ui->core_cpu_freq_khz = 0;
  parse_cpu_label_value(label, ui->core_cpu_policy,
                        sizeof(ui->core_cpu_policy), &ui->core_cpu_freq_khz);
  performance_format_cpu_label(ui->core_cpu_label, sizeof(ui->core_cpu_label),
                               ui->core_cpu_policy, ui->core_cpu_freq_khz);
}

static int parse_core_profile_choice_line(struct ui_state *ui, const char *line) {
  const char *p = line;
  char profile[128];
  size_t len = 0;

  if (!ui || !line) {
    return 0;
  }
  while (*p == ' ') {
    p++;
  }
  if (!isdigit((unsigned char)*p)) {
    return 0;
  }
  while (isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != '.') {
    return 0;
  }
  p++;
  while (*p == ' ') {
    p++;
  }
  while (*p && *p != ' ' && len + 1 < sizeof(profile)) {
    profile[len++] = *p++;
  }
  profile[len] = '\0';
  if (!valid_launch_profile_id(profile)) {
    return 0;
  }
  return add_core_profile_choice(ui, profile);
}

static void select_current_core_profile(struct ui_state *ui) {
  size_t i;

  if (!ui || !ui->core_current_profile[0]) {
    return;
  }
  for (i = 0; i < ui->core_profile_count; i++) {
    if (strcmp(ui->core_profiles[i].id, ui->core_current_profile) == 0) {
      ui->core_profile_cursor = i;
      return;
    }
  }
}

static void ui_vprintf(struct ui_state *ui, const char *fmt, va_list ap) {
  char buf[512];
  const char *start;
  const char *p;

  if (!ui->renderer_mali && !ui->renderer_fbdev && !ui->renderer_pixel2_compat_gfx) {
    vprintf(fmt, ap);
    return;
  }

  vsnprintf(buf, sizeof(buf), fmt, ap);
  start = buf;
  for (p = buf; ; p++) {
    if (*p == '\n' || *p == '\0') {
      if (p > start || *p == '\n') {
        ui_append_render_line(ui, start, (size_t)(p - start));
      }
      if (*p == '\0') {
        break;
      }
      start = p + 1;
    }
  }
}

static void ui_printf(struct ui_state *ui, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  ui_vprintf(ui, fmt, ap);
  va_end(ap);
}

static void clear_screen(struct ui_state *ui) {
  if (ui->renderer_mali || ui->renderer_fbdev || ui->renderer_pixel2_compat_gfx) {
    ui->render_line_count = 0;
    return;
  }
  if (!ui->no_clear) {
    printf("\033[2J\033[H");
  }
}

static int ui_renderer_graphic_capable(const struct ui_state *ui) {
  return ui && (ui->renderer_mali || ui->renderer_fbdev || ui->renderer_pixel2_compat_gfx);
}

static int ui_renderer_fbdev_only(const struct ui_state *ui) {
  return ui && ui->renderer_fbdev && !ui->renderer_mali && !ui->renderer_pixel2_compat_gfx;
}

static int ui_renderer_pixel2_compat2_tty_capable(const struct ui_state *ui) {
  return ui && (ui->renderer_mali || ui->renderer_fbdev ||
                ui->renderer_pixel2_compat_gfx);
}

#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
static int ui_fbdev_uses_settings_family_layout(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  switch (ui->screen) {
  case SCREEN_START_MENU:
  case SCREEN_SETTINGS:
  case SCREEN_HELP:
  case SCREEN_CORE_SELECT:
  case SCREEN_NETWORK_RESCUE:
  case SCREEN_WIFI_CONNECT:
  case SCREEN_THUMBNAIL_RESULTS:
  case SCREEN_THUMBNAIL_RUNNING:
  case SCREEN_SCRAPING:
  case SCREEN_TOP_REFRESH_RUNNING:
  case SCREEN_POWER_ACTION_RUNNING:
    return 1;
  default:
    return 0;
  }
}
#endif

#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
static int ui_fbdev_reserves_footer_space(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  switch (ui->screen) {
  case SCREEN_SETTINGS:
  case SCREEN_CORE_SELECT:
  case SCREEN_WIFI_CONNECT:
  case SCREEN_THUMBNAIL_RUNNING:
  case SCREEN_SCRAPING:
  case SCREEN_TOP_REFRESH_RUNNING:
  case SCREEN_POWER_ACTION_RUNNING:
    return 1;
  default:
    return 0;
  }
}
#endif

static size_t ui_fbdev_text_window_size(const struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
  int h = 480;
  int settings_family;
  int footer_reserved;
  int entry_scale;
  int line_height;
  int first_entry_y;
  int bottom_y;
  size_t rows;

  if (!ui || !ui->renderer_fbdev) {
    return 0;
  }
  if (ui->renderer_active && ui->fbdev_renderer.var.yres > 0) {
    h = (int)ui->fbdev_renderer.var.yres;
  }

  settings_family = ui_fbdev_uses_settings_family_layout(ui);
  footer_reserved = ui_fbdev_reserves_footer_space(ui);
  entry_scale = 2;
  line_height = entry_scale * 12;
  first_entry_y = settings_family ? 82 : 104;
  bottom_y = h - (footer_reserved ? 76 : 34);
  if (line_height <= 0 || bottom_y < first_entry_y) {
    return 1;
  }

  rows = (size_t)((bottom_y - first_entry_y) / line_height) + 1;
  if (rows > 18) {
    rows = 18;
  }
  return rows ? rows : 1;
#else
  (void)ui;
  return 0;
#endif
}

static int ui_uses_graphic_mode(const struct ui_state *ui) {
  return ui_renderer_graphic_capable(ui) &&
         strcmp(ui->frontend_settings.ui_mode, "graphic") == 0;
}

#define UI_GRAPHIC_TOP_GRID_COLUMNS 3
#define UI_GRAPHIC_TOP_GRID_ROWS 2
#define UI_GRAPHIC_TOP_GRID_PAGE_SIZE \
  (UI_GRAPHIC_TOP_GRID_COLUMNS * UI_GRAPHIC_TOP_GRID_ROWS)
#define UI_GRAPHIC_TOP_STRIP_COLUMNS 2
#define UI_GRAPHIC_TOP_STRIP_ROWS 1
#define UI_GRAPHIC_TOP_STRIP_PAGE_SIZE \
  (UI_GRAPHIC_TOP_STRIP_COLUMNS * UI_GRAPHIC_TOP_STRIP_ROWS)
#define UI_GRAPHIC_ROM_PAGE_SIZE 10
#define UI_GRAPHIC_TOP_TRANSITION_DEFAULT_MS 260
#define UI_GRAPHIC_SCROLL_REFRESH_MS 16

static int ui_graphic_top_uses_strip(const struct ui_state *ui) {
  return ui_uses_graphic_mode(ui) &&
         strcmp(ui->theme.graphic_top_layout, "tile_strip") == 0;
}

static size_t ui_graphic_top_page_size(const struct ui_state *ui) {
  return ui_graphic_top_uses_strip(ui) ? UI_GRAPHIC_TOP_STRIP_PAGE_SIZE
                                       : UI_GRAPHIC_TOP_GRID_PAGE_SIZE;
}

static size_t ui_graphic_top_columns(const struct ui_state *ui) {
  return ui_graphic_top_uses_strip(ui) ? UI_GRAPHIC_TOP_STRIP_COLUMNS
                                       : UI_GRAPHIC_TOP_GRID_COLUMNS;
}

static int ui_is_rom_list_screen(const struct ui_state *ui) {
  return ui && (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
                ui->screen == SCREEN_RECENT);
}

static size_t ui_graphic_rom_page_size(const struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  if (ui && ui->renderer_pixel2_compat_gfx && ui->renderer_active) {
    return plumos_pixel2_compat_gfx_renderer_graphic_rom_page_size(&ui->pixel2_compat_gfx_renderer);
  }
#endif
  (void)ui;
  return UI_GRAPHIC_ROM_PAGE_SIZE;
}

static size_t ui_pixel2_compat_gfx_text_top_rom_window_size(const struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  int w = 640;
  int h = 480;
  int entry_scale;
  int line_height;
  int first_entry_y = 104;
  int bottom_y;
  size_t rows;

  if (!ui || !ui->renderer_pixel2_compat_gfx ||
      (ui->screen != SCREEN_TOP && !ui_is_rom_list_screen(ui))) {
    return 0;
  }
  if (ui->renderer_active && ui->pixel2_compat_gfx_renderer.var.xres > 0 &&
      ui->pixel2_compat_gfx_renderer.var.yres > 0) {
    w = (int)ui->pixel2_compat_gfx_renderer.var.xres;
    h = (int)ui->pixel2_compat_gfx_renderer.var.yres;
  }

  entry_scale = (w >= 640 && h >= 400) ? 2 : 1;
  line_height = entry_scale * 12;
  bottom_y = h - 34;
  if (line_height <= 0 || bottom_y < first_entry_y) {
    return 1;
  }

  rows = (size_t)((bottom_y - first_entry_y) / line_height) + 1;
  if (rows > PLUMOS_PIXEL2_COMPAT_GFX_TTY_ENTRY_CAPACITY) {
    rows = PLUMOS_PIXEL2_COMPAT_GFX_TTY_ENTRY_CAPACITY;
  }
  return rows ? rows : 1;
#else
  (void)ui;
  return 0;
#endif
}

static size_t ui_list_window_size(const struct ui_state *ui) {
  size_t fbdev_window;
  size_t pixel2_compat_text_window;

  if (ui_uses_graphic_mode(ui)) {
    if (ui->screen == SCREEN_TOP) {
      return ui_graphic_top_page_size(ui);
    }
    if (ui_is_rom_list_screen(ui)) {
      return ui_graphic_rom_page_size(ui);
    }
  }
  fbdev_window = ui_fbdev_text_window_size(ui);
  if (fbdev_window > 0) {
    return fbdev_window;
  }
  pixel2_compat_text_window = ui_pixel2_compat_gfx_text_top_rom_window_size(ui);
  if (pixel2_compat_text_window > 0) {
    return pixel2_compat_text_window;
  }
  if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
    if (strcmp(ui->mali_tty_entry_scale, "2") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "2.0") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "20") == 0) {
      return 8;
    }
    if (strcmp(ui->mali_tty_entry_scale, "1.5") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "15") == 0) {
      return 10;
    }
    return 15;
  }
  if (ui && (ui->renderer_fbdev || ui->renderer_pixel2_compat_gfx)) {
    return 22;
  }
  return 10;
}

static size_t ui_scrolled_window_start(size_t cursor, size_t count,
                                       size_t window) {
  size_t start;
  size_t max_start;

  if (window == 0) {
    window = 1;
  }
  if (count == 0 || count <= window) {
    return 0;
  }
  if (cursor >= count) {
    cursor = count - 1;
  }
  if (cursor + 1 > window) {
    start = cursor + 1 - window;
  } else {
    start = 0;
  }
  max_start = count - window;
  if (start > max_start) {
    start = max_start;
  }
  return start;
}

static size_t ui_graphic_top_page_for_cursor(const struct ui_state *ui,
                                             size_t cursor) {
  size_t page_size = ui_graphic_top_page_size(ui);
  if (page_size == 0) {
    page_size = 1;
  }
  return cursor / page_size;
}

static int ui_graphic_top_slide_enabled(const struct ui_state *ui) {
  return ui && ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP &&
         strcmp(ui->theme.graphic_transition, "slide") == 0;
}

static long ui_graphic_top_transition_duration_ms(const struct ui_state *ui) {
  long duration;

  if (!ui_graphic_top_slide_enabled(ui)) {
    return 0;
  }
  duration = ui->theme.graphic_transition_ms > 0
                 ? ui->theme.graphic_transition_ms
                 : UI_GRAPHIC_TOP_TRANSITION_DEFAULT_MS;
  if (duration < 80) {
    duration = 80;
  } else if (duration > 1000) {
    duration = 1000;
  }
  return duration;
}

static void ui_start_graphic_top_transition(struct ui_state *ui,
                                            size_t from_cursor,
                                            size_t to_cursor) {
  size_t from_page;
  size_t to_page;
  long duration;

  if (!ui || from_cursor == to_cursor || !ui_graphic_top_slide_enabled(ui)) {
    return;
  }
  from_page = ui_graphic_top_page_for_cursor(ui, from_cursor);
  to_page = ui_graphic_top_page_for_cursor(ui, to_cursor);
  if (from_page == to_page) {
    return;
  }
  duration = ui_graphic_top_transition_duration_ms(ui);
  if (duration <= 0) {
    return;
  }
  ui->top_transition_from_cursor = from_cursor;
  ui->top_transition_from_page = from_page;
  ui->top_transition_to_page = to_page;
  ui->top_transition_start_ms = current_time_ms();
  ui->top_transition_duration_ms = duration;
  ui->top_transition_direction = to_cursor > from_cursor ? 1 : -1;
  ui->top_transition_active = 1;
}

static double ui_graphic_top_transition_progress(struct ui_state *ui) {
  long long elapsed;
  double progress;

  if (!ui || !ui->top_transition_active ||
      ui->top_transition_duration_ms <= 0) {
    return 1.0;
  }
  elapsed = current_time_ms() - ui->top_transition_start_ms;
  if (elapsed >= ui->top_transition_duration_ms) {
    ui->top_transition_active = 0;
    return 1.0;
  }
  if (elapsed <= 0) {
    return 0.0;
  }
  progress = (double)elapsed / (double)ui->top_transition_duration_ms;
  return progress;
}

static void ui_move_graphic_top_cursor(struct ui_state *ui, enum ui_action action) {
  size_t cursor;
  size_t next;

  if (!ui || ui->top_count == 0) {
    return;
  }
  cursor = ui->top_cursor;
  next = cursor;
  if (ui_graphic_top_uses_strip(ui)) {
    if (action == ACTION_LEFT && cursor > 0) {
      next = cursor - 1;
    } else if (action == ACTION_RIGHT && cursor + 1 < ui->top_count) {
      next = cursor + 1;
    }
  } else if (action == ACTION_LEFT) {
    if (cursor > 0) {
      next = cursor - 1;
    }
  } else if (action == ACTION_RIGHT) {
    if (cursor + 1 < ui->top_count) {
      next = cursor + 1;
    }
  } else if (action == ACTION_UP) {
    size_t columns = ui_graphic_top_columns(ui);
    if (cursor >= columns) {
      next = cursor - columns;
    }
  } else if (action == ACTION_DOWN) {
    size_t columns = ui_graphic_top_columns(ui);
    next = cursor + columns;
    if (next >= ui->top_count) {
      size_t next_row_start =
          ((cursor / columns) + 1) * columns;
      if (next_row_start < ui->top_count) {
        next = ui->top_count - 1;
      } else {
        next = cursor;
      }
    }
  }
  if (next != cursor) {
    ui_start_graphic_top_transition(ui, cursor, next);
    ui->top_cursor = next;
  }
}

static void reset_marquee(struct ui_state *ui);

static void ui_start_gallery_transition(struct ui_state *ui, size_t from_cursor,
                                        size_t to_cursor, int direction) {
  if (!ui || from_cursor == to_cursor || ui->rom_count == 0) {
    return;
  }
  ui->gallery_transition_from_cursor = from_cursor;
  ui->gallery_transition_to_cursor = to_cursor;
  ui->gallery_transition_start_ms = current_time_ms();
  ui->gallery_transition_duration_ms = UI_GALLERY_TRANSITION_MS;
  if (direction == 0) {
    direction = to_cursor > from_cursor ? 1 : -1;
  }
  ui->gallery_transition_direction = direction < 0 ? -1 : 1;
  ui->gallery_transition_active = 1;
}

static int ui_start_pending_gallery_transition(struct ui_state *ui) {
  size_t from_cursor;
  size_t to_cursor;

  if (!ui || !ui->gallery_pending_active || ui->rom_count == 0) {
    return 0;
  }
  from_cursor = ui->rom_cursor;
  to_cursor = ui->gallery_pending_cursor;
  ui->gallery_pending_active = 0;
  if (to_cursor >= ui->rom_count) {
    to_cursor = ui->rom_count - 1;
  }
  if (to_cursor == from_cursor) {
    return 0;
  }
  ui_start_gallery_transition(ui, from_cursor, to_cursor,
                              ui->gallery_pending_direction);
  ui->rom_cursor = to_cursor;
  remember_current_rom_cursor(ui);
  reset_marquee(ui);
  return 1;
}

static double ui_gallery_transition_progress(struct ui_state *ui) {
  long long elapsed;
  double progress;

  if (!ui || !ui->gallery_transition_active ||
      ui->gallery_transition_duration_ms <= 0) {
    return 1.0;
  }
  elapsed = current_time_ms() - ui->gallery_transition_start_ms;
  if (elapsed >= ui->gallery_transition_duration_ms) {
    ui->gallery_transition_active = 0;
    return 1.0;
  }
  if (elapsed <= 0) {
    return 0.0;
  }
  progress = (double)elapsed / (double)ui->gallery_transition_duration_ms;
  return progress;
}

static void ui_emit_graphic_theme_color(struct ui_state *ui, const char *key,
                                        const char *value) {
  if (!ui || !key || !key[0] || !value || !value[0]) {
    return;
  }
  ui_printf(ui, "graphic_theme_color\t%s\t%s\n", key, value);
}

static void ui_emit_graphic_theme_motion(struct ui_state *ui, const char *key,
                                         const char *value) {
  if (!ui || !key || !key[0] || !value || !value[0]) {
    return;
  }
  ui_printf(ui, "graphic_theme_motion\t%s\t%s\n", key, value);
}

static void ui_emit_graphic_theme_asset(struct ui_state *ui, const char *key,
                                        const char *asset) {
  char resolved[PATH_MAX];

  if (!ui || !key || !key[0] || !asset || !asset[0]) {
    return;
  }
  if (!resolve_theme_asset_path(&ui->theme, asset, resolved, sizeof(resolved))) {
    return;
  }
  ui_printf(ui, "graphic_theme_asset\t%s\t%s\n", key, resolved);
}

static void ui_emit_graphic_theme_gallery_background(struct ui_state *ui) {
  static const char *candidates[] = {
      "images/rom_gallery_background.png",
      "images/rom-gallery-background.png",
      "images/gallery_background.png",
      "images/gallery-background.png",
      "images/games_background.png",
  };
  char resolved[PATH_MAX];
  size_t i;

  if (!ui) {
    return;
  }
  if (ui->theme.gallery_background[0]) {
    ui_emit_graphic_theme_asset(ui, "gallery_background",
                                ui->theme.gallery_background);
    return;
  }
  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (resolve_theme_asset_path(&ui->theme, candidates[i], resolved,
                                 sizeof(resolved))) {
      ui_printf(ui, "graphic_theme_asset\tgallery_background\t%s\n",
                resolved);
      return;
    }
  }
}

static void ui_emit_graphic_theme(struct ui_state *ui) {
  if (!ui || !ui->theme.loaded || ui->theme.fallback) {
    return;
  }
  ui_printf(ui, "graphic_theme_id=%s\n", ui->theme.id[0] ? ui->theme.id : "default");
  ui_emit_graphic_theme_asset(ui, "background", ui->theme.background);
  ui_emit_graphic_theme_gallery_background(ui);
  ui_emit_graphic_theme_asset(ui, "placeholder", ui->theme.placeholder_thumbnail);
  ui_emit_graphic_theme_color(ui, "background", ui->theme.color_background);
  ui_emit_graphic_theme_color(ui, "foreground", ui->theme.color_foreground);
  ui_emit_graphic_theme_color(ui, "muted", ui->theme.color_muted);
  ui_emit_graphic_theme_color(ui, "accent", ui->theme.color_accent);
  ui_emit_graphic_theme_color(ui, "panel", ui->theme.color_panel);
  ui_emit_graphic_theme_color(ui, "panel_inner", ui->theme.color_panel_inner);
  ui_emit_graphic_theme_color(ui, "media_panel", ui->theme.color_media_panel);
  ui_emit_graphic_theme_color(ui, "selection_background",
                              ui->theme.color_selection_background);
  ui_emit_graphic_theme_color(ui, "selection_foreground",
                              ui->theme.color_selection_foreground);
  ui_emit_graphic_theme_color(ui, "danger", ui->theme.color_danger);
  ui_emit_graphic_theme_motion(ui, "top_layout", ui->theme.graphic_top_layout);
  ui_emit_graphic_theme_motion(ui, "transition", ui->theme.graphic_transition);
  ui_emit_graphic_theme_motion(ui, "transition_axis",
                               ui->theme.graphic_transition_axis);
  ui_emit_graphic_theme_motion(ui, "transition_easing",
                               ui->theme.graphic_transition_easing);
}

static void ui_emit_graphic_top_entry(struct ui_state *ui,
                                      const struct top_entry *entry,
                                      const char *prefix,
                                      int selected) {
  char logo_path[PATH_MAX] = "";

  if (!ui || !entry || !prefix) {
    return;
  }
  resolve_theme_system_logo_path(ui, entry->id, logo_path, sizeof(logo_path));
  ui_printf(ui, "%s\t%d\t%s\t\t%s\n", prefix, selected ? 1 : 0,
            entry->display_name, logo_path);
}

static void ui_emit_graphic_rom_entry(struct ui_state *ui,
                                      const struct rom_entry *entry,
                                      const char *prefix,
                                      int selected) {
  const char *detail;
  const char *display_title;
  char marked_title[UI_RENDER_LINE_MAX];

  if (!ui || !entry || !prefix) {
    return;
  }
  detail = entry->detail[0] ? entry->detail : entry->relative_path;
  display_title = entry->title;
  if (current_rom_is_favorite(ui, entry)) {
    snprintf(marked_title, sizeof(marked_title), "* %s", entry->title);
    display_title = marked_title;
  }
  ui_printf(ui, "%s\t%d\t%s\t%s\t%s\n", prefix, selected ? 1 : 0,
            display_title, detail, entry->thumbnail);
}

static void render_top_graphic(struct ui_state *ui, size_t start, size_t end) {
  size_t i;
  double transition_progress = 1.0;
  int transition_active = 0;

  ui_printf(ui, "plumOS controller UI - TOP\n");
  ui_printf(ui, "graphic_mode=top\n");
  ui_emit_graphic_theme(ui);
  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->top_count,
            ui->top_count ? ui->top_cursor + 1 : 0);

  if (ui->top_transition_active &&
      ui->top_transition_to_page ==
          ui_graphic_top_page_for_cursor(ui, ui->top_cursor) &&
      ui_graphic_top_slide_enabled(ui)) {
    transition_progress = ui_graphic_top_transition_progress(ui);
    transition_active = ui->top_transition_active && transition_progress < 1.0;
  }
  if (transition_active) {
    size_t page_size = ui_graphic_top_page_size(ui);
    size_t prev_start = ui->top_transition_from_page * page_size;
    size_t prev_end = prev_start + page_size;

    if (prev_end > ui->top_count) {
      prev_end = ui->top_count;
    }
    ui_printf(ui, "graphic_transition=slide\n");
    ui_printf(ui, "graphic_transition_direction=%d\n",
              ui->top_transition_direction < 0 ? -1 : 1);
    ui_printf(ui, "graphic_transition_progress=%.3f\n", transition_progress);
    for (i = prev_start; i < prev_end; i++) {
      ui_emit_graphic_top_entry(ui, &ui->top_entries[i], "graphic_prev_entry",
                                i == ui->top_transition_from_cursor);
    }
  }

  for (i = start; i < end; i++) {
    const struct top_entry *entry = &ui->top_entries[i];
    ui_emit_graphic_top_entry(ui, entry, "graphic_entry",
                              i == ui->top_cursor);
  }
  if (ui->top_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Systems\t\t\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void ui_cursor_page_down(size_t *cursor, size_t count, size_t page_size) {
  if (!cursor || count == 0) {
    return;
  }
  if (page_size == 0) {
    page_size = 1;
  }
  if (*cursor + page_size >= count) {
    *cursor = count - 1;
  } else {
    *cursor += page_size;
  }
}

static void ui_cursor_page_up(size_t *cursor, size_t page_size) {
  if (!cursor) {
    return;
  }
  if (page_size == 0) {
    page_size = 1;
  }
  if (*cursor <= page_size) {
    *cursor = 0;
  } else {
    *cursor -= page_size;
  }
}

static void render_top(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;

  if (window == 0) {
    window = 1;
  }
  start = (ui->top_cursor / window) * window;
  end = start + window;
  if (end > ui->top_count) {
    end = ui->top_count;
  }

  if (ui_uses_graphic_mode(ui)) {
    render_top_graphic(ui, start, end);
    return;
  }

  ui_printf(ui, "plumOS controller UI - TOP\n");
  ui_printf(ui, "A: open  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->top_count, ui->top_count ? ui->top_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct top_entry *entry = &ui->top_entries[i];
    ui_printf(ui, "%c %3zu  %s\n", i == ui->top_cursor ? '>' : ' ', i + 1,
              entry->display_name);
  }
  if (ui->top_count == 0) {
    ui_printf(ui, "(system entry is empty; run plumos-library-scan first)\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_roms_graphic(struct ui_state *ui, const char *title,
                                size_t start, size_t end) {
  size_t i;
  const char *mode = "roms";
  const char *base_title;
  const char *count_label = "ROMS";
  char counted_title[160];

  if (ui->screen == SCREEN_FAVORITES) {
    mode = "favorites";
    count_label = "ENTRIES";
  } else if (ui->screen == SCREEN_RECENT) {
    mode = "recent";
    count_label = "ENTRIES";
  }
  base_title = ui->current_system_name[0] ? ui->current_system_name : title;
  snprintf(counted_title, sizeof(counted_title), "%s  %s %zu", base_title,
           count_label, ui->rom_count);

  ui_printf(ui, "plumOS controller UI - %s\n", title);
  ui_printf(ui, "graphic_mode=%s\n", mode);
  ui_printf(ui, "graphic_system=%s\n", counted_title);
  ui_emit_graphic_theme(ui);
  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    ui_emit_graphic_rom_entry(ui, entry, "graphic_entry",
                              i == ui->rom_cursor);
  }
  if (ui->rom_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Entries\t-\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void ui_emit_gallery_window(struct ui_state *ui, const char *prefix,
                                   size_t cursor) {
  size_t start;
  size_t end;
  size_t i;

  if (!ui || !prefix || ui->rom_count == 0) {
    return;
  }
  if (cursor >= ui->rom_count) {
    cursor = ui->rom_count - 1;
  }
  start = cursor > 2 ? cursor - 2 : 0;
  end = cursor + 2;
  if (end >= ui->rom_count) {
    end = ui->rom_count - 1;
  }
  for (i = start; i <= end; i++) {
    ui_emit_graphic_rom_entry(ui, &ui->rom_entries[i], prefix,
                              i == cursor);
  }
}

static void render_gallery(struct ui_state *ui) {
  double transition_progress = 1.0;
  int transition_active = 0;
  const char *base_title;
  char counted_title[160];

  base_title = ui->current_system_name[0] ? ui->current_system_name : "ROMS";
  snprintf(counted_title, sizeof(counted_title), "%s  ROMS %zu", base_title,
           ui->rom_count);

  ui_printf(ui, "plumOS controller UI - Gallery\n");
  ui_printf(ui, "graphic_mode=gallery\n");
  ui_printf(ui, "graphic_system=%s\n", counted_title);
  ui_emit_graphic_theme(ui);

  if (ui->gallery_transition_active &&
      ui->gallery_transition_to_cursor == ui->rom_cursor) {
    transition_progress = ui_gallery_transition_progress(ui);
    if (!ui->gallery_transition_active && ui->gallery_pending_active &&
        ui_start_pending_gallery_transition(ui)) {
      transition_progress = ui_gallery_transition_progress(ui);
    }
  } else if (!ui->gallery_transition_active &&
             ui->gallery_pending_active &&
             ui_start_pending_gallery_transition(ui)) {
    transition_progress = ui_gallery_transition_progress(ui);
  }
  transition_active =
      ui->gallery_transition_active &&
      ui->gallery_transition_to_cursor == ui->rom_cursor &&
      transition_progress < 1.0;

  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);

  if (transition_active) {
    ui_printf(ui, "graphic_transition=slide\n");
    ui_printf(ui, "graphic_transition_direction=%d\n",
              ui->gallery_transition_direction < 0 ? -1 : 1);
    ui_printf(ui, "graphic_transition_progress=%.3f\n", transition_progress);
    ui_emit_gallery_window(ui, "graphic_prev_entry",
                           ui->gallery_transition_from_cursor);
  }
  ui_emit_gallery_window(ui, "graphic_entry", ui->rom_cursor);
  if (ui->rom_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Entries\t-\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void render_roms(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;
  const char *title = "ROMS";
  const char *count_label = "ROMS";
  const char *display_title;
  char counted_title[160];
  const char *subtitle =
      "A: launch  Y: favorite  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit";

  if (window == 0) {
    window = 1;
  }
  start = (ui->rom_cursor / window) * window;
  end = start + window;
  if (end > ui->rom_count) {
    end = ui->rom_count;
  }

  if (ui->screen == SCREEN_FAVORITES) {
    title = "FAVORITES";
    count_label = "ENTRIES";
    subtitle =
        "A: launch  Y: remove  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit";
  } else if (ui->screen == SCREEN_RECENT) {
    title = "RECENT";
    count_label = "ENTRIES";
    subtitle =
        "A: resume  Y: favorite  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit";
  } else if (ui->rom_directory[0]) {
    subtitle =
        "A: open/launch  Y: favorite  B: parent  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit";
  } else {
    subtitle =
        "A: open/launch  Y: favorite  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  POWER: power menu  Q: quit";
  }

  if (ui_uses_graphic_mode(ui)) {
    render_roms_graphic(ui, title, start, end);
    return;
  }

  display_title = ui->screen == SCREEN_ROMS && ui->current_system_name[0]
                      ? ui->current_system_name
                      : title;
  snprintf(counted_title, sizeof(counted_title), "%s  %s %zu", display_title,
           count_label, ui->rom_count);
  ui_printf(ui, "plumOS controller UI - %s\n", counted_title);
  if (ui->screen == SCREEN_ROMS) {
    char prompt_path[PATH_MAX];
    ui_printf(ui, "system=%s ROMs=%zu (%s) dir=%s\n", ui->current_system_id,
              ui->rom_count, ui->current_system_name,
              ui->rom_directory[0] ? ui->rom_directory : "/");
    if (ui_renderer_pixel2_compat2_tty_capable(ui) && ui->rom_count > 0 &&
        rom_entry_alias_root_path(&ui->rom_entries[ui->rom_cursor],
                                  prompt_path, sizeof(prompt_path))) {
      ui_printf(ui, "prompt_path=%s\n", prompt_path);
    }
  }
  ui_printf(ui, "%s\n", subtitle);
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    const char *detail = entry->detail[0] ? entry->detail : entry->relative_path;
    char favorite_marker = current_rom_is_favorite(ui, entry) ? '*' : ' ';
    if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
      ui_printf(ui, "%c%c %3zu  %s\n",
                i == ui->rom_cursor ? '>' : ' ', favorite_marker, i + 1,
                entry->title);
    } else {
      ui_printf(ui, "%c%c %3zu  %-30s %s\n",
                i == ui->rom_cursor ? '>' : ' ', favorite_marker, i + 1,
                entry->title, detail);
    }
  }
  if (ui->rom_count == 0) {
    ui_printf(ui, "(entry is empty)\n");
  }
  if (ui->screen == SCREEN_FAVORITES) {
    ui_printf(ui, "\nsource: %s\n", ui->favorites_path);
  } else if (ui->screen == SCREEN_RECENT) {
    ui_printf(ui, "\nsource: %s\n", ui->recent_path);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_start_menu(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start;
  size_t end;

  trigger_sdcard_cleanup_from_start_menu(ui);
  if (window == 0) {
    window = 1;
  }
  start = ui_scrolled_window_start(ui->menu_cursor, ui->menu_count, window);
  end = start + window;
  if (end > ui->menu_count) {
    end = ui->menu_count;
  }

  ui_printf(ui, "plumOS controller UI - %s\n",
            ui->menu_title[0] ? ui->menu_title : "START");
  ui_printf(ui, "menu_screen=1\n");
  ui_printf(ui, "A: open/run  B: back  UP/DOWN: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->menu_count,
            ui->menu_count ? ui->menu_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct menu_entry *entry = &ui->menu_entries[i];
    if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
      ui_printf(ui, "%c %2zu  %s\n",
                i == ui->menu_cursor ? '>' : ' ', i + 1, entry->display_name);
    } else {
      ui_printf(ui, "%c %3zu  %-24s %-10s %-24s\n",
                i == ui->menu_cursor ? '>' : ' ', i + 1, entry->display_name,
                entry->kind[0] ? entry->kind : "-", entry->action);
    }
  }
  if (ui->menu_count == 0) {
    ui_printf(ui, "(menu entry is empty)\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static int setting_value_is_true(const char *value) {
  return value && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                   strcmp(value, "on") == 0 || strcmp(value, "enabled") == 0 ||
                   strcmp(value, "Enable") == 0 || strcmp(value, "Enabled") == 0);
}

static int settings_blink_arrow_active(const struct ui_state *ui, size_t row,
                                       int direction) {
  long long now;

  if (!ui || ui->settings_blink_direction != direction ||
      ui->settings_blink_cursor != row) {
    return 0;
  }
  now = current_time_ms();
  if (now >= ui->settings_blink_until_ms) {
    return 0;
  }
  return 1;
}

static const char *ui_utf8_next(const char *s, unsigned int *codepoint) {
  const unsigned char *p = (const unsigned char *)s;
  unsigned int cp;

  if (!p || !*p) {
    if (codepoint) {
      *codepoint = 0;
    }
    return s;
  }
  if (p[0] < 0x80) {
    if (codepoint) {
      *codepoint = p[0];
    }
    return s + 1;
  }
  if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x1f) << 6) | (unsigned int)(p[1] & 0x3f);
    if (cp >= 0x80) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 2;
    }
  } else if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x0f) << 12) |
         ((unsigned int)(p[1] & 0x3f) << 6) |
         (unsigned int)(p[2] & 0x3f);
    if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 3;
    }
  } else if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x07) << 18) |
         ((unsigned int)(p[1] & 0x3f) << 12) |
         ((unsigned int)(p[2] & 0x3f) << 6) |
         (unsigned int)(p[3] & 0x3f);
    if (cp >= 0x10000 && cp <= 0x10ffff) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 4;
    }
  }
  if (codepoint) {
    *codepoint = '?';
  }
  return s + 1;
}

static int ui_unicode_is_combining(unsigned int codepoint) {
  return (codepoint >= 0x0300 && codepoint <= 0x036f) ||
         (codepoint >= 0x1ab0 && codepoint <= 0x1aff) ||
         (codepoint >= 0x1dc0 && codepoint <= 0x1dff) ||
         (codepoint >= 0x20d0 && codepoint <= 0x20ff) ||
         (codepoint >= 0xfe20 && codepoint <= 0xfe2f);
}

static int ui_unicode_is_wide(unsigned int codepoint) {
  return (codepoint >= 0x1100 && codepoint <= 0x115f) ||
         (codepoint >= 0x2329 && codepoint <= 0x232a) ||
         (codepoint >= 0x2e80 && codepoint <= 0xa4cf) ||
         (codepoint >= 0xac00 && codepoint <= 0xd7a3) ||
         (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
         (codepoint >= 0xfe10 && codepoint <= 0xfe19) ||
         (codepoint >= 0xfe30 && codepoint <= 0xfe6f) ||
         (codepoint >= 0xff00 && codepoint <= 0xff60) ||
         (codepoint >= 0xffe0 && codepoint <= 0xffe6) ||
         (codepoint >= 0x1f200 && codepoint <= 0x1f251) ||
         (codepoint >= 0x20000 && codepoint <= 0x3fffd);
}

static size_t ui_utf8_cell_width(unsigned int codepoint) {
  if (ui_unicode_is_combining(codepoint)) {
    return 0;
  }
  return ui_unicode_is_wide(codepoint) ? 2 : 1;
}

static size_t ui_utf8_cell_count(const char *text) {
  const char *p = text;
  size_t cells = 0;

  if (!text) {
    return 0;
  }
  while (*p) {
    unsigned int cp;
    const char *next = ui_utf8_next(p, &cp);
    cells += ui_utf8_cell_width(cp);
    p = next;
  }
  return cells;
}

static void format_setting_row_mali(const struct ui_state *ui, const struct setting_entry *entry,
                                    size_t row, char *out, size_t out_size) {
  enum setting_control_type control = setting_control_type_for_id(entry ? entry->id : NULL);
  int flash_direction = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!entry) {
    return;
  }
  if (control == SETTING_CONTROL_CHECKBOX) {
    snprintf(out, out_size, "[%c] %s",
             setting_value_is_true(entry->value) ? 'x' : ' ', entry->display_name);
    return;
  }
  if (control == SETTING_CONTROL_CHOICE || control == SETTING_CONTROL_NUMBER) {
    if (settings_blink_arrow_active(ui, row, -1)) {
      flash_direction = -1;
    } else if (settings_blink_arrow_active(ui, row, 1)) {
      flash_direction = 1;
    }
    if (flash_direction != 0) {
      snprintf(out, out_size, "%s < %s >%s%c}", entry->display_name, entry->value,
               PLUMOS_MALI_SETTING_FLASH_MARKER, flash_direction < 0 ? 'L' : 'R');
    } else {
      snprintf(out, out_size, "%s < %s >", entry->display_name, entry->value);
    }
    return;
  }
  if (control == SETTING_CONTROL_ACTION) {
    snprintf(out, out_size, "%s", entry->display_name);
    return;
  }
  if (entry->value[0] &&
      ui_utf8_cell_count(entry->display_name) + 2 +
          ui_utf8_cell_count(entry->value) <= 33) {
    snprintf(out, out_size, "%s: %s", entry->display_name, entry->value);
  } else {
    snprintf(out, out_size, "%s", entry->display_name);
  }
}

static void setting_help_lines(const struct ui_state *ui,
                               const struct setting_entry *entry,
                               char *line1, size_t line1_size,
                               char *line2, size_t line2_size) {
  const char *id = entry ? entry->id : "";
  enum setting_control_type control = setting_control_type_for_id(id);
  char key[128];

  if (line1 && line1_size > 0) {
    line1[0] = '\0';
  }
  if (line2 && line2_size > 0) {
    line2[0] = '\0';
  }
  if (!entry) {
    return;
  }

  if (strcmp(id, "ui_mode") == 0) {
    copy_string(line1, line1_size, "Switch all screens: Text or Graphic.");
    copy_string(line2, line2_size, "Text is console-style; Graphic is artwork-focused.");
  } else if (strcmp(id, "show_empty_systems") == 0) {
    copy_string(line1, line1_size, "Show systems with no ROMs.");
    copy_string(line2, line2_size, "Useful while preparing folders or testing aliases.");
  } else if (strcmp(id, "show_favorites_on_top") == 0) {
    copy_string(line1, line1_size, "Show Favorites on the TOP list.");
    copy_string(line2, line2_size, "Favorites behave like a virtual system.");
  } else if (strcmp(id, "show_recent_on_top") == 0) {
    copy_string(line1, line1_size, "Show Recent on the TOP list.");
    copy_string(line2, line2_size, "Recent behaves like a virtual system.");
  } else if (strcmp(id, "boot_resume_mode") == 0) {
    copy_string(line1, line1_size, "Open the last ROM at startup.");
    copy_string(line2, line2_size, "No save states are created or loaded.");
  } else if (strcmp(id, "sort_systems") == 0) {
    copy_string(line1, line1_size, "Choose TOP system ordering.");
    copy_string(line2, line2_size, "Sort Order follows config; Name sorts A-Z.");
  } else if (strcmp(id, "sort_roms") == 0) {
    copy_string(line1, line1_size, "Choose ROM list ordering.");
    copy_string(line2, line2_size, "Affects each system's ROM list.");
  } else if (strcmp(id, "rom_scan_policy") == 0) {
    copy_string(line1, line1_size, "Scan ROM folders when entering a system.");
    copy_string(line2, line2_size, "Off keeps cached lists until refresh.");
  } else if (strcmp(id, "refresh_top") == 0) {
    copy_string(line1, line1_size, "Re-scan systems and reload the TOP list.");
    copy_string(line2, line2_size, "Use after adding ROM folders or changing files.");
  } else if (strcmp(id, "ui_theme_settings") == 0) {
    copy_string(line1, line1_size, "Open Graphic theme settings.");
    copy_string(line2, line2_size, "Theme selection affects Graphic mode only.");
  } else if (strcmp(id, "graphic_theme_id") == 0) {
    copy_string(line1, line1_size, "Graphic mode theme package.");
    copy_string(line2, line2_size, "Text mode ignores theme colors, layout, and assets.");
  } else if (strcmp(id, "theme_top_layout") == 0) {
    copy_string(line1, line1_size, "Graphic TOP tile layout.");
    copy_string(line2, line2_size, "Grid shows 6 tiles; Strip shows 2 wide tiles.");
  } else if (strcmp(id, "theme_transition") == 0) {
    copy_string(line1, line1_size, "Graphic TOP page transition.");
    copy_string(line2, line2_size, "Presentation only; input behavior does not change.");
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    copy_string(line1, line1_size, "Graphic TOP transition duration.");
    copy_string(line2, line2_size, "LEFT/RIGHT changes 80..1000 ms.");
  } else if (strcmp(id, "theme_transition_axis") == 0) {
    copy_string(line1, line1_size, "Graphic TOP slide direction.");
    copy_string(line2, line2_size, "Vertical or Horizontal animation axis.");
  } else if (strcmp(id, "theme_transition_easing") == 0) {
    copy_string(line1, line1_size, "Graphic TOP motion curve.");
    copy_string(line2, line2_size, "Smooth is eased; Linear is constant speed.");
  } else if (strcmp(id, "theme_name") == 0 ||
             strcmp(id, "theme_status") == 0 ||
             strcmp(id, "theme_layout") == 0 ||
             strcmp(id, "theme_font") == 0) {
    copy_string(line1, line1_size, "Read-only Graphic theme information.");
    copy_string(line2, line2_size, "Theme package metadata; values above can override motion.");
  } else if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    copy_string(line1, line1_size, "Slow scan warning threshold.");
    copy_string(line2, line2_size, "Higher values make warnings less sensitive.");
  } else if (strcmp(id, "rom_scan_test_file_count") == 0) {
    copy_string(line1, line1_size, "Synthetic scan test count.");
    copy_string(line2, line2_size, "Used by scan performance checks.");
  } else if (strcmp(id, "network_connect_wifi") == 0) {
    copy_string(line1, line1_size, "Scan SSIDs and connect with password.");
    copy_string(line2, line2_size, "Saves Wi-Fi config only after IP is acquired.");
  } else if (strcmp(id, "network_rescue") == 0) {
    copy_string(line1, line1_size, "Network Recovery is disabled.");
    copy_string(line2, line2_size, "Use Connect Wi-Fi or NW Service instead.");
  } else if (strcmp(id, "network_services") == 0) {
    copy_string(line1, line1_size, "Open network services.");
    copy_string(line2, line2_size, "SSH, FTP, SFTP, Samba, and ADB.");
  } else if (strcmp(id, "network_information") == 0) {
    copy_string(line1, line1_size, "Open read-only network information.");
    copy_string(line2, line2_size, "Connection, IP, signal, link speed, SSH, and ADB.");
  } else if (strncmp(id, "network_", 8) == 0) {
    if (strcmp(id, "network_wifi_enabled") == 0) {
      copy_string(line1, line1_size, "Turn the Wi-Fi runtime on or off.");
      copy_string(line2, line2_size, "Use Connect Wi-Fi to start a new connection.");
    } else if (strcmp(id, "network_ssh_enabled") == 0) {
      copy_string(line1, line1_size, "SSH remote shell service.");
      copy_string(line2, line2_size, "Port 22; SFTP depends on this service.");
    } else if (strcmp(id, "network_ftp_enabled") == 0) {
      copy_string(line1, line1_size, "FTP file transfer service.");
      copy_string(line2, line2_size, "Home is /mnt/plumos-user; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_sftp_enabled") == 0) {
      copy_string(line1, line1_size, "SFTP file transfer over SSH.");
      copy_string(line2, line2_size, "SFTP uses SSH port 22; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_samba_enabled") == 0) {
      copy_string(line1, line1_size, "Windows/macOS network drive service.");
      copy_string(line2, line2_size, "Share is SDCARD; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_adb_enabled") == 0) {
      copy_string(line1, line1_size, "ADB over USB cable.");
      copy_string(line2, line2_size,
                  "Changes apply after reboot on Pixel2.");
    } else if (strcmp(id, "network_ftp_status") == 0) {
      copy_string(line1, line1_size, "Current FTP service status.");
      copy_string(line2, line2_size, "FTP home is /mnt/plumos-user.");
    } else if (strcmp(id, "network_sftp_status") == 0) {
      copy_string(line1, line1_size, "Current SFTP service status.");
      copy_string(line2, line2_size, "SFTP uses the SSH service on port 22.");
    } else if (strcmp(id, "network_samba_status") == 0) {
      copy_string(line1, line1_size, "Current Samba service status.");
      copy_string(line2, line2_size, "SDCARD shares the SD card root.");
    } else if (strcmp(id, "network_adb_status") == 0) {
      copy_string(line1, line1_size, "Current USB ADB service status.");
      copy_string(line2, line2_size, "Use a trusted host and normal adb client tools.");
    } else if (strcmp(id, "network_config_source") == 0) {
      copy_string(line1, line1_size, "Read-only Wi-Fi config inventory.");
      copy_string(line2, line2_size, "Credential editing waits for backup and rollback.");
    } else if (strcmp(id, "network_connection") == 0 ||
               strcmp(id, "network_ip_address") == 0 ||
               strcmp(id, "network_signal") == 0 ||
               strcmp(id, "network_link_speed") == 0 ||
               strcmp(id, "network_frequency") == 0 ||
               strcmp(id, "network_status_source") == 0) {
      copy_string(line1, line1_size, "Current Wi-Fi connection information.");
      copy_string(line2, line2_size, "Use Connect Wi-Fi if this looks wrong.");
    } else if (strcmp(id, "network_ssh") == 0) {
      copy_string(line1, line1_size, "Remote access path used for development.");
      copy_string(line2, line2_size, "SFTP uses the same SSH service.");
    } else if (strcmp(id, "network_credentials") == 0) {
      copy_string(line1, line1_size, "Wi-Fi credentials are intentionally hidden.");
      copy_string(line2, line2_size, "Do not expose SSID or PSK in UI/logs/git.");
    } else if (strcmp(id, "network_runtime_control") == 0) {
      copy_string(line1, line1_size, "Runtime control helper used by this screen.");
      copy_string(line2, line2_size, "It does not edit saved Wi-Fi credentials.");
    } else {
      copy_string(line1, line1_size, "Read-only network setting.");
      copy_string(line2, line2_size, "Write support needs safe Wi-Fi editor flow.");
    }
  } else if (strncmp(id, "performance_", 12) == 0) {
    if (strcmp(id, "performance_system") == 0) {
      copy_string(line1, line1_size, "Choose the system CPU setting to edit.");
      copy_string(line2, line2_size, "Values are saved as plumOS core overrides.");
    } else if (strcmp(id, "performance_cpu_policy") == 0) {
      copy_string(line1, line1_size, "Select the CPU governor for this system.");
      copy_string(line2, line2_size, "Ondemand is the default; Performance is available.");
    } else if (strcmp(id, "performance_clear_cpu_override") == 0) {
      copy_string(line1, line1_size, "Reset this system to the plumOS default.");
      copy_string(line2, line2_size, "Default comes from the system profile.");
    } else {
      copy_string(line1, line1_size, "Read-only performance source information.");
      copy_string(line2, line2_size, "Shows whether defaults or overrides are active.");
    }
  } else if (strcmp(id, "system_information") == 0) {
    copy_string(line1, line1_size, "Open read-only device information.");
    copy_string(line2, line2_size, "Shows model, kernel, storage, memory, and firmware.");
  } else if (strncmp(id, "system_", 7) == 0) {
    if (strcmp(id, "system_volume") == 0) {
      copy_string(line1, line1_size, "System-wide volume setting.");
      copy_string(line2, line2_size, "Applies to the validated device mixer backend.");
    } else if (strcmp(id, "system_brightness") == 0) {
      copy_string(line1, line1_size, "Screen brightness setting.");
      copy_string(line2, line2_size,
                  runtime_device_is_pixel2()
                      ? "Uses the Pixel2 PWM backlight."
                      : "Uses the active system backlight backend.");
    } else if (strcmp(id, "system_lid_suspend") == 0) {
      copy_string(line1, line1_size, "Pixel2 has no lid switch.");
      copy_string(line2, line2_size,
                  "The hall sensor wakes and restores display, audio, input, Wi-Fi, and FE.");
    } else if (strcmp(id, "system_lumination") == 0) {
      copy_string(line1, line1_size, "Display lumination setting.");
      copy_string(line2, line2_size, "Uses the active Pixel2 display backend when available.");
    } else if (strcmp(id, "system_display_color") == 0) {
      copy_string(line1, line1_size, "Open display color tuning.");
      copy_string(line2, line2_size, "Contrast, color temperature, and saturation are changed inside.");
    } else if (strcmp(id, "system_time_settings") == 0) {
      copy_string(line1, line1_size, "Open OS time and timezone settings.");
      copy_string(line2, line2_size, "Timezone is saved for plumOS processes.");
    } else if (strcmp(id, "system_storage_check") == 0) {
      copy_string(line1, line1_size, "Run a bounded read-only FAT32 check.");
      copy_string(line2, line2_size,
                  "Dirty media is logged; repair is never attempted while mounted.");
    } else if (strcmp(id, "system_current_time") == 0) {
      copy_string(line1, line1_size, "Current OS time in the selected timezone.");
      copy_string(line2, line2_size, "Automatic Time synchronizes it after Wi-Fi connects.");
    } else if (strcmp(id, "system_automatic_time") == 0) {
      copy_string(line1, line1_size, "Synchronize time when Wi-Fi connects.");
      copy_string(line2, line2_size, "A toggles it; enabling also updates RTC now.");
    } else if (strcmp(id, "system_sync_now") == 0) {
      copy_string(line1, line1_size, "Synchronize system time and RTC now.");
      copy_string(line2, line2_size, "Works once even when Automatic Time is OFF.");
    } else if (strcmp(id, "system_rtc_status") == 0) {
      copy_string(line1, line1_size, "Difference between RTC and system time.");
      copy_string(line2, line2_size,
                  "Synced means the clocks differ by at most five seconds.");
    } else if (strcmp(id, "system_timezone") == 0) {
      copy_string(line1, line1_size, "OS runtime timezone.");
      copy_string(line2, line2_size, "LEFT/RIGHT saves and applies it immediately.");
    } else if (strcmp(id, "system_manual_time") == 0) {
      copy_string(line1, line1_size, "Open manual time editor.");
      copy_string(line2, line2_size, "Values are interpreted in the selected timezone.");
    } else if (strcmp(id, "system_manual_time_apply") == 0) {
      copy_string(line1, line1_size, "Apply the manual OS time.");
      copy_string(line2, line2_size,
                  "A disables Automatic Time and saves system time to RTC.");
    } else if (strncmp(id, "system_manual_time_", 19) == 0) {
      copy_string(line1, line1_size, "Manual time field.");
      copy_string(line2, line2_size, "LEFT/RIGHT changes the value; A applies at bottom.");
    } else if (strcmp(id, "system_contrast") == 0) {
      copy_string(line1, line1_size, "Display contrast setting.");
      copy_string(line2, line2_size, "Applies to the display backend and saves to plumOS.");
    } else if (strcmp(id, "system_hue") == 0) {
      copy_string(line1, line1_size, "Display color temperature setting.");
      copy_string(line2, line2_size, "Uses the active Pixel2 display backend when available.");
    } else if (strcmp(id, "system_saturation") == 0) {
      copy_string(line1, line1_size, "Display saturation setting.");
      copy_string(line2, line2_size, "Applies to the display backend and saves to plumOS.");
    } else if (strcmp(id, "system_language") == 0) {
      copy_string(line1, line1_size, "Frontend language setting.");
      copy_string(line2, line2_size, "Saves language to plumOS config.");
    } else if (strcmp(id, "system_update") == 0) {
      copy_string(line1, line1_size, "plumOS Pixel2 update support is under development.");
      copy_string(line2, line2_size, "The current development image is replaced from a PC.");
    } else if (strcmp(id, "system_model") == 0 ||
               strcmp(id, "system_plumos_version") == 0 ||
               strcmp(id, "system_vendor_runtime") == 0 ||
               strcmp(id, "system_kernel") == 0 ||
               strcmp(id, "system_gpu_runtime") == 0 ||
               strcmp(id, "system_display_backend") == 0 ||
               strcmp(id, "system_audio_backend") == 0 ||
               strcmp(id, "system_sdcard") == 0 ||
               strcmp(id, "system_storage_health") == 0 ||
               strcmp(id, "system_memory") == 0 ||
               strcmp(id, "system_firmware") == 0) {
      copy_string(line1, line1_size, "Read-only device and runtime information.");
      if (strcmp(id, "system_plumos_version") == 0) {
        copy_string(line2, line2_size, "Read from the FAT32 plumOS VERSION file.");
      } else if (strcmp(id, "system_vendor_runtime") == 0) {
        copy_string(line2, line2_size, "Compatible vendor-firmware vendor runtime.");
      } else if (strcmp(id, "system_display_backend") == 0 ||
                 strcmp(id, "system_audio_backend") == 0) {
        copy_string(line2, line2_size,
                    "Live runtime backend selected for Pixel2.");
      } else if (strcmp(id, "system_firmware") == 0) {
        copy_string(line2, line2_size,
                    "Read from vendor firmware or plumOS or rootfs release metadata.");
      } else {
        copy_string(line2, line2_size, "Used to confirm the active device environment.");
      }
    } else {
      copy_string(line1, line1_size, "Read-only system setting.");
      copy_string(line2, line2_size, "Write support needs backup and rollback.");
    }
  } else if (control == SETTING_CONTROL_CHECKBOX && !setting_is_writable(id)) {
    copy_string(line1, line1_size, "Read-only checkbox from theme/system.");
    copy_string(line2, line2_size, "Needs a validated write backend.");
  } else {
    copy_string(line1, line1_size, "Read-only information for this screen.");
    copy_string(line2, line2_size, "Write support will be added after backend checks.");
  }

  if (id[0]) {
    if (line1 && line1_size > 0) {
      snprintf(key, sizeof(key), "settings.item.%s.help1", id);
      copy_string(line1, line1_size, tr(ui, key, line1));
    }
    if (line2 && line2_size > 0) {
      snprintf(key, sizeof(key), "settings.item.%s.help2", id);
      copy_string(line2, line2_size, tr(ui, key, line2));
    }
  }
}

static size_t brightness_test_nearest_index(long brightness) {
  size_t i;
  size_t best = 0;
  long best_delta = brightness > BRIGHTNESS_TEST_VALUES[0]
                        ? brightness - BRIGHTNESS_TEST_VALUES[0]
                        : BRIGHTNESS_TEST_VALUES[0] - brightness;

  for (i = 1; i < BRIGHTNESS_TEST_COUNT; i++) {
    long value = BRIGHTNESS_TEST_VALUES[i];
    long delta = brightness > value ? brightness - value : value - brightness;
    if (delta < best_delta) {
      best = i;
      best_delta = delta;
    }
  }
  return best;
}

static void render_brightness_test_settings(struct ui_state *ui) {
  size_t i;
  long current = brightness_raw_value(ui->device.brightness);
  long selected = current;

  if (ui->setting_count > 0 && ui->settings_cursor < ui->setting_count) {
    selected = BRIGHTNESS_TEST_VALUES[ui->settings_cursor];
  }

  ui_printf(ui, "plumOS controller UI - %s\n",
            settings_category_title(ui, ui->settings_category));
  ui_printf(ui, "settings_screen=1\n");
  ui_printf(ui, "brightness_test=1\n");
  ui_printf(ui, "A: apply  B: back  UP/DOWN/LEFT/RIGHT: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->setting_count,
            ui->setting_count ? ui->settings_cursor + 1 : 0);
  ui_printf(ui, "brightness_current=%ld\n", current);
  ui_printf(ui, "\n");

  if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
    for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
      long value = BRIGHTNESS_TEST_VALUES[i];
      ui_printf(ui, "brightness_tile=%ld selected=%d current=%d\n",
                value, i == ui->settings_cursor ? 1 : 0,
                value == current ? 1 : 0);
    }
    ui_printf(ui, "footer1=Current brightness: %ld\n", current);
    ui_printf(ui, "footer2=Selected preset: %ld\n", selected);
  } else {
    for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
      long value = BRIGHTNESS_TEST_VALUES[i];
      if (i % BRIGHTNESS_TEST_COLUMNS == 0) {
        ui_printf(ui, "  ");
      }
      ui_printf(ui, "[%c%3ld%s]", i == ui->settings_cursor ? '>' : ' ',
                value, value == current ? "*" : " ");
      if (i % BRIGHTNESS_TEST_COLUMNS == BRIGHTNESS_TEST_COLUMNS - 1 ||
          i + 1 == BRIGHTNESS_TEST_COUNT) {
        ui_printf(ui, "\n");
      } else {
        ui_printf(ui, "  ");
      }
    }
    ui_printf(ui, "\ncurrent=%ld selected=%ld\n", current, selected);
  }

  ui_printf(ui, "\nsource: %s\n", ui->system_config_path);
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_settings(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;
  char help1[256];
  char help2[256];

  if (window == 0) {
    window = 1;
  }
  if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST) {
    render_brightness_test_settings(ui);
    return;
  }
  start = ui_scrolled_window_start(ui->settings_cursor, ui->setting_count, window);
  end = start + window;
  if (end > ui->setting_count) {
    end = ui->setting_count;
  }

  ui_printf(ui, "plumOS controller UI - %s\n",
            settings_category_title(ui, ui->settings_category));
  ui_printf(ui, "settings_screen=1\n");
  ui_printf(ui, "A: toggle/run  B: back  LEFT/RIGHT: change  UP/DOWN: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->setting_count,
            ui->setting_count ? ui->settings_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct setting_entry *entry = &ui->setting_entries[i];
    if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
      char row[256];
      format_setting_row_mali(ui, entry, i, row, sizeof(row));
      ui_printf(ui, "%c %3zu  %s\n",
                i == ui->settings_cursor ? '>' : ' ', i + 1, row);
    } else {
      enum setting_control_type control = setting_control_type_for_id(entry->id);
      if (control == SETTING_CONTROL_ACTION) {
        ui_printf(ui, "%c %3zu  %s\n",
                  i == ui->settings_cursor ? '>' : ' ', i + 1,
                  entry->display_name);
      } else {
        ui_printf(ui, "%c %3zu  %-24s %s\n",
                  i == ui->settings_cursor ? '>' : ' ', i + 1,
                  entry->display_name, entry->value);
      }
    }
  }
  if (ui_renderer_pixel2_compat2_tty_capable(ui) && ui->setting_count > 0) {
    const struct setting_entry *entry = &ui->setting_entries[ui->settings_cursor];
    setting_help_lines(ui, entry, help1, sizeof(help1), help2, sizeof(help2));
    ui_printf(ui, "footer1=%s\n", help1);
    ui_printf(ui, "footer2=%s\n", help2);
  }
  ui_printf(ui, "\nsource: %s\n", ui->settings_path);
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_power_menu(struct ui_state *ui) {
  size_t i;

  ui_printf(ui, "plumOS controller UI - POWER\n");
  ui_printf(ui, "A: run  B: cancel  UP/DOWN: move  Q: quit\n");
  if (ui->power_target_relative_path[0]) {
    ui_printf(ui, "target=%s / %s\n", ui->power_target_system_id,
              ui->power_target_relative_path);
    ui_printf(ui, "profile=%s\n",
              ui->power_target_launch_profile[0] ? ui->power_target_launch_profile : "-");
  } else {
    ui_printf(ui, "target=(no active ROM target)\n");
  }
  ui_printf(ui, "entries=%zu cursor=%zu\n", POWER_ENTRY_COUNT,
            POWER_ENTRY_COUNT ? ui->power_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = 0; i < POWER_ENTRY_COUNT; i++) {
    const struct power_entry *entry = &POWER_ENTRIES[i];
    ui_printf(ui, "%c %3zu  %-12s %s\n",
              i == ui->power_cursor ? '>' : ' ', i + 1, entry->display_name, entry->detail);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_help(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - HELP\n");
  ui_printf(ui, "B: back  Q: quit\n");
  ui_printf(ui, "entries=9 cursor=0\n");
  ui_printf(ui, "\n");
  ui_printf(ui, "1. Up / Down: Move cursor\n");
  ui_printf(ui, "2. A: Open or run\n");
  ui_printf(ui, "3. B: Back or cancel\n");
  ui_printf(ui, "4. START: Open START menu\n");
  ui_printf(ui, "5. SELECT: Core menu\n");
  ui_printf(ui, "6. Power: Power menu\n");
  ui_printf(ui, "7. Settings HELP: This screen\n");
  ui_printf(ui, "8. Network: Run Wi-Fi and SSH rescue\n");
  ui_printf(ui, "9. Q: Quit from SSH text input only\n");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_core_select(struct ui_state *ui) {
  size_t i;
  const char *profile = NULL;
  const char *default_label;
  const char *footer1 = "Launch core/profile used for this target.";
  const char *footer2 = "TOP saves system; ROM saves ROM override.";

  ui_printf(ui, "plumOS controller UI - Core Settings\n");
  ui_printf(ui, "settings_screen=1\n");
  ui_printf(ui, "core_settings_screen=1\n");
  if (!ui_renderer_pixel2_compat2_tty_capable(ui)) {
    ui_printf(ui, "target=%s",
              ui->core_target_system_id[0] ? ui->core_target_system_id : "-");
    if (ui->core_target_relative_path[0]) {
      ui_printf(ui, " / %s", ui->core_target_relative_path);
    }
    ui_printf(ui, "\n");
    if (ui->core_current_source[0]) {
      ui_printf(ui, "source=%s\n", ui->core_current_source);
    }
  }
  core_menu_clamp_cursor(ui);
  ui_printf(ui, "entries=%d cursor=%zu\n\n", CORE_MENU_ROW_COUNT,
            ui->core_menu_cursor + 1);
  if (ui->core_profile_count > 0) {
    if (ui->core_profile_cursor >= ui->core_profile_count) {
      ui->core_profile_cursor = ui->core_profile_count - 1;
    }
    profile = ui->core_profiles[ui->core_profile_cursor].id;
  } else if (ui->core_current_profile[0]) {
    profile = ui->core_current_profile;
  }
  default_label = ui->core_target_relative_path[0] ? "Inherit TOP" : "plumOS default";

  ui_printf(ui, "%c   1  Cores < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_PROFILE ? '>' : ' ',
            core_profile_display_name(profile));
  ui_printf(ui, "%c   2  Default < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_DEFAULT ? '>' : ' ',
            default_label);
  ui_printf(ui, "    3  ------------------------------\n");
  ui_printf(ui, "%c   4  CPU governor < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ ? '>' : ' ',
            ui->core_cpu_label);
  for (i = 0; i < ui->core_line_count; i++) {
    ui_printf(ui, "%s\n", ui->core_lines[i]);
  }
  if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ) {
    footer1 = "CPU governor for this target.";
    footer2 = ui->core_cpu_source[0] ? ui->core_cpu_source : "source unavailable";
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_DEFAULT) {
    footer1 = "A removes this target core override.";
    footer2 = ui->core_target_relative_path[0] ? "ROM will inherit TOP core."
                                               : "TOP will use plumOS default.";
  }
  if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
    ui_printf(ui, "footer1=%s\n", footer1);
    ui_printf(ui, "footer2=%s\n", footer2);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_scraping(struct ui_state *ui) {
  const char *label;
  const struct scraping_kind_choice *kind;
  const char *kind_label;
  size_t target_count;
  long rom_count;

  clamp_scraping_menu_cursor(ui);
  clamp_scraping_choice_cursor(ui);
  kind = scraping_selected_kind(ui);
  kind_label = scraping_kind_display_name(ui, kind);
  label = scraping_selected_label(ui);
  rom_count = scraping_selected_rom_count(ui);
  target_count = ui->scraping_choice_cursor == 0 ? ui->scraping_choice_count : (size_t)1;

  ui_printf(ui, "plumOS controller UI - %s\n",
            tr(ui, "scraping.title", "Scraping"));
  ui_printf(ui, "scraping_screen=1\n");
  ui_printf(ui, "%s\n",
            tr(ui, "scraping.instructions",
               "UP/DOWN: item  LEFT/RIGHT: change  A: start  SELECT: results  B: Apps  Q: quit"));
  ui_printf(ui, "entries=7 cursor=%zu\n", ui->scraping_menu_cursor + 1);
  ui_printf(ui, "\n");
  ui_printf(ui, "%c   1  %s < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE ? '>' : ' ',
            tr(ui, "scraping.field.image", "Image"), kind_label);
  ui_printf(ui, "%c   2  %s < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING ? '>' : ' ',
            tr(ui, "scraping.field.existing", "Existing"), scraping_existing_label(ui));
  ui_printf(ui, "%c   3  %s < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_SYSTEM ? '>' : ' ',
            tr(ui, "scraping.field.system", "System"), label);
  ui_printf(ui, "    4  %s: %zu\n",
            tr(ui, "scraping.field.targets", "Targets"), target_count);
  ui_printf(ui, "    5  %s: %ld\n", tr(ui, "scraping.field.roms", "ROMs"), rom_count);
  ui_printf(ui, "    6  %s\n",
            tr(ui, "scraping.hint.start", "A starts plan and fetch"));
  ui_printf(ui, "    7  %s\n",
            tr(ui, "scraping.hint.results", "SELECT opens latest results"));
  ui_printf(ui, "footer1=%s\n",
            tr(ui, "scraping.footer1",
               "Box Art and Title Screen share the same PNG path."));
  ui_printf(ui, "footer2=%s\n",
            tr(ui, "scraping.footer2",
               "Replace overwrites current images after successful download."));
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_thumbnail_results(struct ui_state *ui) {
  const char *result_owner = ui->thumbnail_running_title[0]
                                 ? ui->thumbnail_running_title
                                 : tr(ui, "app.scraping.name", "Scraping");
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;

  if (window == 0) {
    window = UI_THUMBNAIL_RESULT_WINDOW;
  }
  if (strcmp(ui->thumbnail_result_return_app_id, "scraping") == 0) {
    ui_printf(ui, "plumOS controller UI - %s\n",
              tr(ui, "scraping_results.title", "Scraping Results"));
  } else {
    ui_printf(ui, "plumOS controller UI - %s Results\n", result_owner);
  }
  ui_printf(ui, "thumbnail_results_screen=1\n");
  ui_printf(ui, "%s\n",
            tr(ui, "scraping_results.instructions",
               "UP/DOWN: scroll  LEFT/RIGHT: page  A/SELECT: refresh  B: Apps  Q: quit"));
  clamp_thumbnail_result_cursor(ui);
  if (ui->thumbnail_result_count > 0) {
    start = ui_scrolled_window_start(ui->thumbnail_result_cursor,
                                     ui->thumbnail_result_count, window);
  }
  end = start + window;
  if (end > ui->thumbnail_result_count) {
    end = ui->thumbnail_result_count;
  }
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->thumbnail_result_count,
            ui->thumbnail_result_count ? ui->thumbnail_result_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    ui_printf(ui, "%c   %2zu  %s\n",
              i == ui->thumbnail_result_cursor ? '>' : ' ',
              i + 1, ui->thumbnail_result_lines[i]);
  }
  if (ui->thumbnail_result_count == 0) {
    ui_printf(ui, "%s\n",
              tr(ui, "scraping_results.empty", "(no thumbnail result yet)"));
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_thumbnail_running(struct ui_state *ui) {
  const char *title = ui->thumbnail_running_title[0]
                          ? ui->thumbnail_running_title
                          : "Thumbnail Task";
  int scraping = strcmp(ui->thumbnail_result_return_app_id, "scraping") == 0;
  int pyxel_setup = strcmp(ui->thumbnail_result_return_app_id, "pyxel_setup") == 0;
  const char *display_title = scraping
                                  ? tr(ui, "scraping.title", "Scraping")
                                  : (pyxel_setup
                                         ? title
                                         : tr(ui, "thumbnail_task.title", title));
  const char *phase = ui->thumbnail_running_phase[0]
                          ? ui->thumbnail_running_phase
                          : "starting";
  const char *phase_label = phase;
  long percent = -1;

  if (strcmp(phase, "plan") == 0) {
    phase_label = tr(ui, "scraping_running.phase.plan", "Plan");
  } else if (strcmp(phase, "fetch") == 0) {
    phase_label = tr(ui, "scraping_running.phase.fetch", "Fetch");
  } else if (strcmp(phase, "done") == 0) {
    phase_label = tr(ui, "scraping_running.phase.done", "Done");
  } else if (strcmp(phase, "starting") == 0) {
    phase_label = tr(ui, "scraping_running.phase.starting", "starting");
  }
  if (ui->thumbnail_progress_total > 0) {
    percent = (ui->thumbnail_progress_current * 100) / ui->thumbnail_progress_total;
  }

  if (scraping) {
    ui_printf(ui, "plumOS controller UI - %s\n",
              tr(ui, "scraping_running.title", "Scraping Running"));
  } else if (pyxel_setup) {
    ui_printf(ui, "plumOS controller UI - %s Running\n", title);
  } else {
    ui_printf(ui, "plumOS controller UI - %s\n",
              tr(ui, "thumbnail_running.title", "Thumbnail Running"));
  }
  ui_printf(ui, "thumbnail_running=1\n");
  ui_printf(ui, "thumbnail_running_title=%s\n", title);
  ui_printf(ui, "thumbnail_running_phase=%s\n", phase_label);
  ui_printf(ui, "thumbnail_running_system=%s\n",
            ui->thumbnail_running_system[0] ? ui->thumbnail_running_system : "-");
  ui_printf(ui, "thumbnail_running_progress=%ld/%ld\n",
            ui->thumbnail_progress_current, ui->thumbnail_progress_total);
  ui_printf(ui, "thumbnail_running_stats=%ld/%ld/%ld\n",
            ui->thumbnail_progress_downloaded,
            ui->thumbnail_progress_no_match,
            ui->thumbnail_progress_failed);
  ui_printf(ui, "entries=%d cursor=1\n", pyxel_setup ? 4 : 6);
  ui_printf(ui, "\n");
  ui_printf(ui, ">   1  %s: %s\n",
            tr(ui, "scraping_running.field.running", "RUNNING"), display_title);
  if (pyxel_setup) {
    ui_printf(ui, "    2  %s\n",
              tr(ui, "pyxel_setup.running.installing",
                 "Installing modules from requirements.txt"));
    ui_printf(ui, "    3  %s\n",
              tr(ui, "scraping_running.hint.results_auto",
                 "Results will open automatically"));
    ui_printf(ui, "    4  %s\n",
              tr(ui, "scraping_running.hint.no_poweroff", "Do not power off"));
    ui_printf(ui, "footer1=%s is running.\n", title);
    ui_printf(ui, "footer2=%s\n",
              tr(ui, "scraping_running.footer2",
                 "The latest result will open when it finishes."));
    if (ui->status[0]) {
      ui_printf(ui, "\nstatus: %s\n", ui->status);
    }
    return;
  }
  if (percent >= 0) {
    ui_printf(ui, "    2  %s: %ld / %ld (%ld%%)\n",
              tr(ui, "scraping_running.field.progress", "Progress"),
              ui->thumbnail_progress_current, ui->thumbnail_progress_total, percent);
  } else {
    ui_printf(ui, "    2  %s: %s\n",
              tr(ui, "scraping_running.field.progress", "Progress"),
              tr(ui, "scraping_running.progress.preparing", "preparing"));
  }
  ui_printf(ui, "    3  %s: %s %s\n",
            tr(ui, "scraping_running.field.phase", "Phase"), phase_label,
            ui->thumbnail_running_system[0] ? ui->thumbnail_running_system : "-");
  ui_printf(ui, "    4  %s %ld  %s %ld  %s %ld\n",
            tr(ui, "scraping_running.field.saved", "Saved"),
            ui->thumbnail_progress_downloaded,
            tr(ui, "scraping_running.field.no_match", "NoMatch"),
            ui->thumbnail_progress_no_match,
            tr(ui, "scraping_running.field.failed", "Failed"),
            ui->thumbnail_progress_failed);
  ui_printf(ui, "    5  %s\n",
            tr(ui, "scraping_running.hint.results_auto",
               "Results will open automatically"));
  ui_printf(ui, "    6  %s\n",
            tr(ui, "scraping_running.hint.no_poweroff", "Do not power off"));
  ui_printf(ui, "footer1=%s\n",
            scraping ? tr(ui, "scraping_running.footer1", "Scraping is running.")
                     : tr(ui, "thumbnail_running.footer1", "Thumbnail task is running."));
  ui_printf(ui, "footer2=%s\n",
            tr(ui, "scraping_running.footer2",
               "The latest result will open when it finishes."));
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_network_rescue(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - Network Recovery Disabled\n");
  ui_printf(ui, "B: back  Q: quit\n");
  ui_printf(ui, "target=(disabled compatibility screen)\n");
  ui_printf(ui, "\n");
  ui_printf(ui, "1. Network Recovery is not exposed from FE.\n");
  ui_printf(ui, "2. Use Network Settings > Connect Wi-Fi.\n");
  ui_printf(ui, "3. Use Network Settings > NW Service.\n");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_wifi_connect(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end = 0;
  char footer1[160];
  char footer2[160];

  if (window == 0) {
    window = 1;
  }
  ui_printf(ui, "plumOS controller UI - Network Settings - Connect Wi-Fi\n");
  if (ui->wifi_stage == WIFI_CONNECT_SELECT) {
    start = ui_scrolled_window_start(ui->wifi_cursor, ui->wifi_count, window);
    end = start + window;
    if (end > ui->wifi_count) {
      end = ui->wifi_count;
    }
    ui_printf(ui, "A: select  B: back  SELECT: rescan  UP/DOWN: move  Q: quit\n");
    ui_printf(ui, "entries=%zu cursor=%zu\n", ui->wifi_count,
              ui->wifi_count ? ui->wifi_cursor + 1 : 0);
    ui_printf(ui, "\n");
    for (i = start; i < end; i++) {
      const struct wifi_network_entry *entry = &ui->wifi_networks[i];
      ui_printf(ui, "%c %3zu  %s  [%s]  %s dBm\n",
                i == ui->wifi_cursor ? '>' : ' ', i + 1, entry->ssid,
                entry->security, entry->signal);
    }
    if (ui->wifi_count == 0) {
      ui_printf(ui, "(no SSIDs found)\n");
    }
    ui_printf(ui, "footer1=SSID scan results\n");
    ui_printf(ui, "footer2=A selects a network; SELECT rescans.\n");
  } else if (ui->wifi_stage == WIFI_CONNECT_PASSWORD) {
    const char *ssid = ui->wifi_count > 0 && ui->wifi_cursor < ui->wifi_count
                           ? ui->wifi_networks[ui->wifi_cursor].ssid
                           : "-";
    ui_printf(ui, "A: type/run  B: SSID list  UP/DOWN/LEFT/RIGHT: move  Q: quit\n");
    ui_printf(ui, "target=%s\n", ssid);
    ui_printf(ui, "entries=%d cursor=%zu\n", UI_WIFI_KEYBOARD_ROWS,
              ui->wifi_key_row + 1);
    if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
      ui_printf(ui, "wifi_keyboard_cursor=%zu,%zu\n", ui->wifi_key_row,
                ui->wifi_key_col);
      ui_printf(ui, "wifi_password=%s\n",
                ui->wifi_password[0] ? ui->wifi_password : "-");
    }
    ui_printf(ui, "\n");
    for (i = 0; i < UI_WIFI_KEYBOARD_ROWS; i++) {
      char row[128];
      wifi_format_keyboard_row(ui, i, row, sizeof(row));
      ui_printf(ui, "%c %3zu  %s\n",
                !ui_renderer_pixel2_compat2_tty_capable(ui) && i == ui->wifi_key_row ? '>' : ' ',
                i + 1, row);
    }
    snprintf(footer1, sizeof(footer1), "Password: %s (%zu chars)",
             ui->wifi_password[0] ? ui->wifi_password : "-",
             strlen(ui->wifi_password));
    snprintf(footer2, sizeof(footer2), "SHIFT toggles case. CONNECT runs.");
    ui_printf(ui, "footer1=%s\n", footer1);
    ui_printf(ui, "footer2=%s\n", footer2);
  } else {
    ui_printf(ui, "A: done  B: Network Settings  Q: quit\n");
    ui_printf(ui, "entries=4 cursor=1\n");
    ui_printf(ui, "\n");
    ui_printf(ui, ">   1  %s\n",
              ui->wifi_result_title[0] ? ui->wifi_result_title : "Connection result");
    ui_printf(ui, "    2  IP Address: %s\n",
              ui->wifi_result_ip[0] ? ui->wifi_result_ip : "-");
    ui_printf(ui, "    3  Gateway: %s\n",
              ui->wifi_result_gateway[0] ? ui->wifi_result_gateway : "-");
    ui_printf(ui, "    4  Gateway Ping: %s\n",
              ui->wifi_result_gateway_ping[0] ? ui->wifi_result_gateway_ping : "-");
    ui_printf(ui, "footer1=%s\n",
              ui->wifi_result_success ? "Connection complete." : "Connection failed.");
    ui_printf(ui, "footer2=%s\n",
              ui->wifi_result_stage[0] ? ui->wifi_result_stage : "B returns.");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_top_refresh_running(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - Refresh TOP\n");
  ui_printf(ui, "top_refresh_running=1\n");
  ui_printf(ui, "entries=5 cursor=1\n");
  ui_printf(ui, "\n");
  ui_printf(ui, ">   1  REFRESH TOP\n");
  ui_printf(ui, "    2  Scanning systems\n");
  ui_printf(ui, "    3  Reloading TOP list\n");
  ui_printf(ui, "    4  Please wait\n");
  ui_printf(ui, "    5  Do not power off\n");
  ui_printf(ui, "footer1=%s\n", "Updating the TOP system list.");
  ui_printf(ui, "footer2=%s\n", "This screen stays visible for at least 1 second.");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_power_action_running(struct ui_state *ui) {
  int rebooting = strcmp(ui->power_action, "reboot") == 0;
  const char *title = rebooting
                          ? tr(ui, "power_action.reboot.title", "Restarting")
                          : tr(ui, "power_action.shutdown.title", "Shutting Down");

  ui_printf(ui, "plumOS controller UI - %s\n", title);
  ui_printf(ui, "power_action_running=1\n");
  ui_printf(ui, "power_action=%s\n", rebooting ? "reboot" : "shutdown");
  ui_printf(ui, "power_action_title=%s\n", title);
  ui_printf(ui, "power_action_wait=%s\n",
            tr(ui, "power_action.running", "PLEASE WAIT"));
  ui_printf(ui, "power_action_saving=%s\n",
            tr(ui, "power_action.saving", "Saving data safely"));
  ui_printf(ui, "power_action_no_remove=%s\n",
            tr(ui, "power_action.no_remove", "Do not remove the SD card"));
  ui_printf(ui, "entries=4 cursor=0\n");
  ui_printf(ui, "\n");
  ui_printf(ui, "    1  %s\n",
            tr(ui, "power_action.running", "PLEASE WAIT"));
  ui_printf(ui, "    2  %s\n",
            tr(ui, "power_action.saving", "Saving data safely"));
  ui_printf(ui, "    3  %s\n",
            tr(ui, "power_action.controls_disabled", "Controls are disabled"));
  ui_printf(ui, "    4  %s\n",
            tr(ui, "power_action.no_remove", "Do not remove the SD card"));
  ui_printf(ui, "footer1=%s\n", title);
  ui_printf(ui, "footer2=%s\n",
            tr(ui, "power_action.wait", "Wait until the device finishes."));
}

static void render_ui(struct ui_state *ui) {
  clear_screen(ui);
  if (ui->screen != SCREEN_TOP) {
    ui->top_transition_active = 0;
  }
  if (ui->screen != SCREEN_GALLERY) {
    ui->gallery_transition_active = 0;
    ui->gallery_pending_active = 0;
    ui->gallery_pending_direction = 0;
  }
  if (ui->rescue_network) {
    render_network_rescue(ui);
  } else if (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
      ui->screen == SCREEN_RECENT) {
    render_roms(ui);
  } else if (ui->screen == SCREEN_GALLERY) {
    render_gallery(ui);
  } else if (ui->screen == SCREEN_START_MENU) {
    render_start_menu(ui);
  } else if (ui->screen == SCREEN_SETTINGS) {
    render_settings(ui);
  } else if (ui->screen == SCREEN_POWER_MENU) {
    render_power_menu(ui);
  } else if (ui->screen == SCREEN_HELP) {
    render_help(ui);
  } else if (ui->screen == SCREEN_CORE_SELECT) {
    render_core_select(ui);
  } else if (ui->screen == SCREEN_SCRAPING) {
    render_scraping(ui);
  } else if (ui->screen == SCREEN_THUMBNAIL_RESULTS) {
    render_thumbnail_results(ui);
  } else if (ui->screen == SCREEN_THUMBNAIL_RUNNING) {
    render_thumbnail_running(ui);
  } else if (ui->screen == SCREEN_NETWORK_RESCUE) {
    render_network_rescue(ui);
  } else if (ui->screen == SCREEN_WIFI_CONNECT) {
    render_wifi_connect(ui);
  } else if (ui->screen == SCREEN_TOP_REFRESH_RUNNING) {
    render_top_refresh_running(ui);
  } else if (ui->screen == SCREEN_POWER_ACTION_RUNNING) {
    render_power_action_running(ui);
  } else {
    render_top(ui);
  }
  if (ui->renderer_mali) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
    if (!ui->renderer_active) {
      ui->render_failed = 1;
      return;
    }
    if (!plumos_mali_render_lines(&ui->mali_renderer, ui->render_lines,
                                  ui->render_line_count)) {
      copy_string(ui->status, sizeof(ui->status), "Mali renderer swap failed");
      ui->render_failed = 1;
    }
#endif
  } else if (ui->renderer_pixel2_compat_gfx) {
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
    if (!ui->renderer_active) {
      ui->render_failed = 1;
      return;
    }
    if (!plumos_pixel2_compat_gfx_render_lines(&ui->pixel2_compat_gfx_renderer, ui->render_lines,
                                     ui->render_line_count)) {
      copy_string(ui->status, sizeof(ui->status), "Pixel2 GFX renderer draw failed");
      ui->render_failed = 1;
    }
#else
    ui->render_failed = 1;
#endif
  } else if (ui->renderer_fbdev) {
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
    if (!ui->renderer_active) {
      ui->render_failed = 1;
      return;
    }
    if (!plumos_fbdev_render_lines(&ui->fbdev_renderer, ui->render_lines,
                                   ui->render_line_count)) {
      copy_string(ui->status, sizeof(ui->status), "fbdev renderer draw failed");
      ui->render_failed = 1;
    }
#else
    ui->render_failed = 1;
#endif
  } else {
    fflush(stdout);
  }
  if (!ui->render_failed) {
    record_frame_stats();
  }
}

static void mark_frontend_ready_if_needed(struct ui_state *ui) {
  char content[128];

  if (!ui || ui->fe_ready_flag_written ||
      (!ui->renderer_mali && !ui->renderer_fbdev && !ui->renderer_pixel2_compat_gfx) ||
      !ui->renderer_active || ui->render_failed || ui->rescue_network ||
      ui->power_overlay) {
    return;
  }
  snprintf(content, sizeof(content), "pid=%ld\nscreen=%d\n",
           (long)getpid(), (int)ui->screen);
  if (write_text_file_create(UI_FE_READY_FLAG_PATH, content)) {
    ui->fe_ready_flag_written = 1;
  }
}

static void set_status(struct ui_state *ui, const char *text) {
  copy_string(ui->status, sizeof(ui->status), text);
}

static int capture_frontend_screenshot(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
  static unsigned int screenshot_sequence;
  char media_root[PATH_MAX];
  char directory[PATH_MAX];
  char final_path[PATH_MAX];
  char temporary_path[PATH_MAX];
  char timestamp[32];
  char render_error[256];
  int directory_fd;
  size_t root_length;
  time_t now;
  struct tm local_now;

  if (!ui || !ui->renderer_fbdev || !ui->renderer_active) {
    set_status(ui, "Screenshot unavailable");
    return 0;
  }
  copy_string(media_root, sizeof(media_root), ui->plumos_root);
  root_length = strlen(media_root);
  if (root_length > 7 &&
      strcmp(media_root + root_length - 7, "/plumos") == 0) {
    media_root[root_length - 7] = '\0';
  }
  if (!media_root[0]) {
    copy_string(media_root, sizeof(media_root), "/mnt/SDCARD");
  }
  if (!join_path(directory, sizeof(directory), media_root, "Screenshots") ||
      !ensure_dir_recursive(directory)) {
    set_status(ui, "Screenshot directory failed");
    return 0;
  }
  now = time(NULL);
  if (!localtime_r(&now, &local_now) ||
      strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &local_now) ==
          0) {
    copy_string(timestamp, sizeof(timestamp), "unknown-time");
  }
  if (snprintf(final_path, sizeof(final_path),
               "%s/plumos-fe-%s-%ld-%u.png", directory, timestamp,
               (long)getpid(), ++screenshot_sequence) >=
          (int)sizeof(final_path) ||
      snprintf(temporary_path, sizeof(temporary_path), "%s.next-%ld",
               final_path, (long)getpid()) >= (int)sizeof(temporary_path)) {
    set_status(ui, "Screenshot path too long");
    return 0;
  }
  render_error[0] = '\0';
  if (!plumos_fbdev_write_screenshot_png(
          &ui->fbdev_renderer, temporary_path, render_error,
          sizeof(render_error)) ||
      rename(temporary_path, final_path) != 0) {
    unlink(temporary_path);
    snprintf(ui->status, sizeof(ui->status), "Screenshot failed: %.180s",
             render_error[0] ? render_error : strerror(errno));
    fprintf(stderr, "screenshot=result-failed path=%.180s reason=%.180s\n",
            final_path, render_error[0] ? render_error : strerror(errno));
    return 0;
  }
  directory_fd = open(directory, O_RDONLY | O_DIRECTORY);
  if (directory_fd >= 0) {
    (void)fsync(directory_fd);
    close(directory_fd);
  }
  snprintf(ui->status, sizeof(ui->status), "Screenshot saved: %.180s",
           strrchr(final_path, '/') ? strrchr(final_path, '/') + 1
                                    : final_path);
  fprintf(stderr, "screenshot=result-saved path=%s\n", final_path);
  return 1;
#else
  (void)ui;
  return 0;
#endif
}

static void reset_marquee(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui && ui->renderer_mali && ui->renderer_active) {
    plumos_mali_renderer_reset_marquee(&ui->mali_renderer);
  }
#endif
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
  if (ui && ui->renderer_fbdev && ui->renderer_active) {
    plumos_fbdev_renderer_reset_marquee(&ui->fbdev_renderer);
  }
#endif
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  if (ui && ui->renderer_pixel2_compat_gfx && ui->renderer_active) {
    plumos_pixel2_compat_gfx_renderer_reset_marquee(&ui->pixel2_compat_gfx_renderer);
  }
#endif
#if !defined(PLUMOS_ENABLE_MALI_RENDERER) && !defined(PLUMOS_ENABLE_FBDEV_RENDERER) && \
    !defined(PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER)
  (void)ui;
#endif
}

static void settle_input_after_child(struct ui_state *ui);

static int init_ui_renderer(struct ui_state *ui) {
  if (!ui->renderer_mali && !ui->renderer_fbdev && !ui->renderer_pixel2_compat_gfx) {
    return 1;
  }
  if (ui->renderer_pixel2_compat_gfx) {
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
    char render_error[256];
    if (ui->renderer_active) {
      return 1;
    }
    render_error[0] = '\0';
    if (!plumos_pixel2_compat_gfx_renderer_init(&ui->pixel2_compat_gfx_renderer,
                                      ui->fb_path[0] ? ui->fb_path : "/dev/fb0",
                                      render_error, sizeof(render_error))) {
      snprintf(ui->status, sizeof(ui->status), "Pixel2 GFX renderer init failed: %.200s",
               render_error[0] ? render_error : "-");
      return 0;
    }
    plumos_pixel2_compat_gfx_renderer_set_rotation(&ui->pixel2_compat_gfx_renderer,
                                         ui->pixel2_compat_gfx_rotation);
    plumos_pixel2_compat_gfx_renderer_reset_marquee(&ui->pixel2_compat_gfx_renderer);
    copy_string(ui->status, sizeof(ui->status), "Pixel2 GFX renderer ready");
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_FREETYPE
    if (ui->mali_font_path[0]) {
      render_error[0] = '\0';
      if (plumos_pixel2_compat_gfx_renderer_load_font(&ui->pixel2_compat_gfx_renderer,
                                            ui->mali_font_path,
                                            render_error,
                                            sizeof(render_error))) {
        snprintf(ui->status, sizeof(ui->status),
                 "Pixel2 GFX renderer ready font=%.160s", ui->mali_font_path);
        if (ui->mali_fallback_font_path[0]) {
          render_error[0] = '\0';
          if (!plumos_pixel2_compat_gfx_renderer_load_fallback_font(
                  &ui->pixel2_compat_gfx_renderer, ui->mali_fallback_font_path,
                  render_error, sizeof(render_error))) {
            snprintf(ui->status, sizeof(ui->status),
                     "Pixel2 GFX fallback font failed: %.160s",
                     render_error[0] ? render_error
                                     : ui->mali_fallback_font_path);
          }
        }
      } else {
        snprintf(ui->status, sizeof(ui->status),
                 "Pixel2 GFX font failed: %.180s",
                 render_error[0] ? render_error : ui->mali_font_path);
      }
    }
#endif
    ui->renderer_active = 1;
    return 1;
#else
    set_status(ui, "Pixel2 GFX renderer unavailable in this build");
    return 0;
#endif
  }
  if (ui->renderer_fbdev) {
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
    char render_error[256];
    if (ui->renderer_active) {
      return 1;
    }
    render_error[0] = '\0';
    if (!plumos_fbdev_renderer_init(&ui->fbdev_renderer,
                                    ui->fb_path[0] ? ui->fb_path : "/dev/fb0",
                                    render_error, sizeof(render_error))) {
      snprintf(ui->status, sizeof(ui->status), "fbdev renderer init failed: %.200s",
               render_error[0] ? render_error : "-");
      return 0;
    }
    plumos_fbdev_renderer_set_rotation(&ui->fbdev_renderer, ui->fbdev_rotation);
    plumos_fbdev_renderer_reset_marquee(&ui->fbdev_renderer);
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
    if (ui->mali_font_path[0]) {
      render_error[0] = '\0';
      if (plumos_fbdev_renderer_load_font(&ui->fbdev_renderer,
                                          ui->mali_font_path, render_error,
                                          sizeof(render_error))) {
        if (ui->mali_fallback_font_path[0]) {
          render_error[0] = '\0';
          if (!plumos_fbdev_renderer_load_fallback_font(
                  &ui->fbdev_renderer, ui->mali_fallback_font_path,
                  render_error, sizeof(render_error))) {
            snprintf(ui->status, sizeof(ui->status),
                     "fbdev fallback font failed: %.160s",
                     render_error[0] ? render_error
                                     : ui->mali_fallback_font_path);
          }
        }
      } else {
        snprintf(ui->status, sizeof(ui->status),
                 "fbdev font failed: %.180s",
                 render_error[0] ? render_error : ui->mali_font_path);
      }
    }
#endif
    ui->renderer_active = 1;
    return 1;
#else
    set_status(ui, "fbdev renderer unavailable in this build");
    return 0;
#endif
  }
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui->renderer_active) {
    return 1;
  }
  {
    char render_error[256];
    render_error[0] = '\0';
    if (!plumos_mali_renderer_init(&ui->mali_renderer,
                                   ui->fb_path[0] ? ui->fb_path : "/dev/fb0",
                                   ui->egl_path[0] ? ui->egl_path : "/usr/lib/libEGL.so",
                                   ui->gles_path[0] ? ui->gles_path : "/usr/lib/libGLESv2.so",
                                   ui->mali_rotation, render_error, sizeof(render_error))) {
      snprintf(ui->status, sizeof(ui->status), "Mali renderer init failed: %.200s",
               render_error[0] ? render_error : "-");
      return 0;
    }
    plumos_mali_renderer_set_tty_entry_scale(&ui->mali_renderer,
                                             ui->mali_tty_entry_scale);
    plumos_mali_renderer_reset_marquee(&ui->mali_renderer);
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
    if (ui->mali_font_path[0]) {
      render_error[0] = '\0';
      if (plumos_mali_renderer_load_font(&ui->mali_renderer, ui->mali_font_path,
                                         render_error, sizeof(render_error))) {
        snprintf(ui->status, sizeof(ui->status), "Mali renderer ready font=%.160s",
                 ui->mali_font_path);
        if (ui->mali_fallback_font_path[0]) {
          render_error[0] = '\0';
          if (!plumos_mali_renderer_load_fallback_font(
                  &ui->mali_renderer, ui->mali_fallback_font_path,
                  render_error, sizeof(render_error))) {
            snprintf(ui->status, sizeof(ui->status), "Mali fallback font failed: %.160s",
                     render_error[0] ? render_error : ui->mali_fallback_font_path);
          }
        }
      } else {
        snprintf(ui->status, sizeof(ui->status), "Mali font failed: %.180s",
                 render_error[0] ? render_error : ui->mali_font_path);
      }
    }
#endif
  }
  ui->renderer_active = 1;
  return 1;
#else
  set_status(ui, "Mali renderer unavailable in this build");
  return 0;
#endif
}

static void shutdown_ui_renderer(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui->renderer_mali && ui->renderer_active) {
    plumos_mali_renderer_shutdown(&ui->mali_renderer);
    ui->renderer_active = 0;
  }
#endif
#ifdef PLUMOS_ENABLE_FBDEV_RENDERER
  if (ui->renderer_fbdev && ui->renderer_active) {
    plumos_fbdev_renderer_shutdown(&ui->fbdev_renderer);
    ui->renderer_active = 0;
  }
#endif
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  if (ui->renderer_pixel2_compat_gfx && ui->renderer_active) {
    plumos_pixel2_compat_gfx_renderer_shutdown(&ui->pixel2_compat_gfx_renderer);
    ui->renderer_active = 0;
  }
#endif
#if !defined(PLUMOS_ENABLE_MALI_RENDERER) && !defined(PLUMOS_ENABLE_FBDEV_RENDERER) && \
    !defined(PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER)
  (void)ui;
#endif
}

static void shutdown_ui_renderer_for_launch(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER
  if (ui && ui->renderer_pixel2_compat_gfx && ui->renderer_active) {
    ui->pixel2_compat_gfx_renderer.original_yoffset =
        ui->pixel2_compat_gfx_renderer.front_yoffset;
  }
#endif
  shutdown_ui_renderer(ui);
}

static void open_start_menu(struct ui_state *ui) {
  ui->back_screen = ui->screen;
  ui->screen = SCREEN_START_MENU;
  trigger_sdcard_cleanup_from_start_menu(ui);
  if (!load_start_menu_entries(ui)) {
    set_status(ui, tr(ui, "menu.status.start_load_failed",
                      "cannot load START menu"));
  } else {
    set_status(ui, tr(ui, "menu.status.start_ready", "START menu ready"));
  }
}

static void open_apps_menu(struct ui_state *ui) {
  ui->screen = SCREEN_START_MENU;
  trigger_sdcard_cleanup_from_start_menu(ui);
  if (!load_apps_menu_entries(ui)) {
    set_status(ui, tr(ui, "menu.status.apps_load_failed",
                      "cannot load Apps menu"));
  } else {
    set_status(ui, tr(ui, "menu.status.apps_ready", "Apps ready"));
  }
}

static void capture_power_target(struct ui_state *ui) {
  ui->power_target_system_id[0] = '\0';
  ui->power_target_relative_path[0] = '\0';
  ui->power_target_launch_profile[0] = '\0';

  if ((ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
      ui->screen == SCREEN_RECENT) &&
      ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    if (entry->is_navigation_directory) {
      return;
    }
    copy_string(ui->power_target_system_id, sizeof(ui->power_target_system_id),
                entry->system_id[0] ? entry->system_id : ui->current_system_id);
    copy_string(ui->power_target_relative_path, sizeof(ui->power_target_relative_path),
                entry->relative_path);
    copy_string(ui->power_target_launch_profile, sizeof(ui->power_target_launch_profile),
                entry->launch_profile);
  }
}

static void close_power_menu(struct ui_state *ui, const char *status) {
  if (ui->power_overlay) {
    ui->exit_requested = 1;
  }
  ui->screen = ui->power_back_screen;
  set_status(ui, status);
}

static void open_power_menu(struct ui_state *ui) {
  ui->power_back_screen = ui->screen;
  ui->power_cursor = POWER_ENTRY_COUNT > 0 ? POWER_ENTRY_COUNT - 1 : 0;
  capture_power_target(ui);
  ui->screen = SCREEN_POWER_MENU;
  set_status(ui, "power menu ready");
}

static void open_power_menu_for_action(struct ui_state *ui, const char *action) {
  size_t i;

  open_power_menu(ui);
  for (i = 0; i < POWER_ENTRY_COUNT; i++) {
    if (strcmp(POWER_ENTRIES[i].id, action) == 0) {
      ui->power_cursor = i;
      break;
    }
  }
  set_status(ui, "A: confirm  B: cancel");
}

static void open_favorites_screen(struct ui_state *ui) {
  ui->screen = SCREEN_FAVORITES;
  ui->rom_directory[0] = '\0';
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), "favorites");
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), "Favorites");
  if (!load_favorite_entries(ui)) {
    set_status(ui, "cannot load Favorites");
  } else {
    restore_current_rom_cursor(ui);
    set_status(ui, "Favorites ready");
  }
  reset_marquee(ui);
}

static void open_recent_screen(struct ui_state *ui) {
  ui->screen = SCREEN_RECENT;
  ui->rom_directory[0] = '\0';
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), "recent");
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), "Recent");
  if (!load_recent_entries(ui)) {
    set_status(ui, "cannot load Recent");
  } else {
    restore_current_rom_cursor(ui);
    set_status(ui, "Recent ready");
  }
  reset_marquee(ui);
}

static void open_settings_screen(struct ui_state *ui, enum settings_category category) {
  const char *title;

  ui->settings_category = category;
  ui->screen = SCREEN_SETTINGS;
  ui->factory_reset_pending_target[0] = '\0';
  ui->factory_reset_pending_until_ms = 0;
  title = settings_category_title(ui, ui->settings_category);
  if (!load_settings_entries(ui)) {
    snprintf(ui->status, sizeof(ui->status), "cannot load %s", title);
  } else {
    snprintf(ui->status, sizeof(ui->status), "%s ready", title);
  }
}

static void open_system_brightness_test_screen(struct ui_state *ui) {
  open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST);
  if (ui->setting_count > 0) {
    ui->settings_cursor = brightness_test_nearest_index(
        brightness_raw_value(ui->device.brightness));
    if (ui->settings_cursor >= ui->setting_count) {
      ui->settings_cursor = ui->setting_count - 1;
    }
  }
}

static void open_help_screen(struct ui_state *ui) {
  ui->screen = SCREEN_HELP;
  set_status(ui, "help ready");
}

static void open_thumbnail_results_screen(struct ui_state *ui) {
  copy_tr(ui, "app.scraping.name", "Scraping", ui->thumbnail_running_title,
          sizeof(ui->thumbnail_running_title));
  copy_string(ui->thumbnail_result_return_app_id,
              sizeof(ui->thumbnail_result_return_app_id), "scraping");
  ui->screen = SCREEN_THUMBNAIL_RESULTS;
  ui->thumbnail_result_cursor = 0;
  if (!load_thumbnail_results(ui)) {
    set_status(ui, tr(ui, "scraping.status.results_load_failed",
                      "cannot load thumbnail results"));
  } else {
    set_status(ui, tr(ui, "scraping.status.results_ready",
                      "scraping results ready"));
  }
}

static void open_scraping_screen(struct ui_state *ui) {
  copy_tr(ui, "app.scraping.name", "Scraping", ui->thumbnail_running_title,
          sizeof(ui->thumbnail_running_title));
  copy_string(ui->thumbnail_result_return_app_id,
              sizeof(ui->thumbnail_result_return_app_id), "scraping");
  ui->screen = SCREEN_SCRAPING;
  set_status(ui, tr(ui, "scraping.status.refreshing_roms", "refreshing ROM list"));
  render_ui(ui);
  if (!run_scanner(ui->plumos_root, ui->sdcard_root, NULL, 0)) {
    set_status(ui, tr(ui, "scraping.status.scan_failed",
                      "ROM scan failed; using cached counts"));
  } else if (!load_top_entries(ui)) {
    set_status(ui, tr(ui, "scraping.status.scan_reload_failed",
                      "ROM scan done; cannot reload TOP"));
  }
  if (!load_scraping_choices(ui)) {
    set_status(ui, tr(ui, "scraping.status.systems_load_failed",
                      "cannot load scraping systems"));
  } else if (ui->scraping_choice_count == 0) {
    set_status(ui, tr(ui, "scraping.status.no_targets",
                      "no scraping target with ROMs"));
  } else {
    set_status(ui, tr(ui, "scraping.status.ready", "Scraping ready"));
  }
}

static int load_core_select_lines(struct ui_state *ui, const char *system_id,
                                  const char *relative_path) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  pid_t pipe_pid;
  size_t pos = 0;
  int rc;
  int in_launch_profiles = 0;

  reset_core_profile_choices(ui);
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    core_append_line(ui, "error: plumos-text-ui path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    core_append_line(ui, "error: plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, relative_path && relative_path[0]
                                             ? " core rom "
                                             : " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id)) {
    core_append_line(ui, "error: core command too long");
    return 0;
  }
  if (relative_path && relative_path[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, relative_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " --no-scan")) {
      core_append_line(ui, "error: core command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    core_append_line(ui, "error: core command too long");
    return 0;
  }

  pipe = open_runtime_shell_pipe(cmd, &pipe_pid);
  if (!pipe) {
    core_append_line(ui, "error: cannot run plumos-text-ui core");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    if (strcmp(line, "Launch profiles") == 0) {
      in_launch_profiles = 1;
      continue;
    }
    if (strcmp(line, "CPU governor presets") == 0) {
      in_launch_profiles = 0;
      continue;
    }
    parse_core_current_profile_line(ui, line);
    parse_core_current_cpu_line(ui, line);
    if (in_launch_profiles && parse_core_profile_choice_line(ui, line)) {
      continue;
    }
    if (strncmp(line, "error:", 6) == 0) {
      core_append_line(ui, line);
    }
  }
  rc = close_runtime_shell_pipe(pipe, pipe_pid);
  select_current_core_profile(ui);
  if (rc == 0 && ui->core_profile_count == 0 && ui->core_line_count == 0) {
    core_append_line(ui, "no launch profiles for this system");
  }
  return rc == 0;
}

static int run_core_text_ui_extra(struct ui_state *ui, const char *extra_args) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    set_status(ui, "core command path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     ui->core_target_relative_path[0] ? " core rom "
                                                      : " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->core_target_system_id)) {
    set_status(ui, "core command too long");
    return 0;
  }
  if (ui->core_target_relative_path[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->core_target_relative_path)) {
      set_status(ui, "core command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, extra_args ? extra_args : "") ||
      (ui->core_target_relative_path[0] &&
       !append_string(cmd, sizeof(cmd), &pos, " --no-scan")) ||
      !append_string(cmd, sizeof(cmd), &pos, " >/dev/null 2>&1")) {
    set_status(ui, "core command too long");
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  if (system_command_succeeded(rc)) {
    return 1;
  }
  set_status(ui, rc == -1 ? "core command failed to start" : "core command returned non-zero");
  return 0;
}

static int run_core_set_profile(struct ui_state *ui, const char *profile) {
  char extra[192];
  size_t pos = 0;

  if (!ui || !valid_launch_profile_id(profile)) {
    return 0;
  }
  extra[0] = '\0';
  if (!append_string(extra, sizeof(extra), &pos, " --set ") ||
      !append_shell_quoted(extra, sizeof(extra), &pos, profile)) {
    set_status(ui, "core command too long");
    return 0;
  }
  return run_core_text_ui_extra(ui, extra);
}

static int run_core_clear_profile_override(struct ui_state *ui) {
  const char *target_label;

  if (!ui) {
    return 0;
  }
  target_label = ui->core_target_relative_path[0] ? "inherit TOP" : "plumOS default";
  if (!run_core_text_ui_extra(ui, " --clear-profile")) {
    return 0;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "default restored; reload failed");
    return 0;
  }
  snprintf(ui->status, sizeof(ui->status), "Core default: %s", target_label);
  return 1;
}

static void cycle_core_profile(struct ui_state *ui, int direction) {
  const char *profile;
  char label[128];

  if (!ui || direction == 0) {
    return;
  }
  if (ui->core_profile_count == 0) {
    set_status(ui, "no core choices");
    return;
  }
  if (ui->core_profile_count == 1) {
    set_status(ui, "only one core choice");
    return;
  }
  if (direction > 0) {
    ui->core_profile_cursor = (ui->core_profile_cursor + 1) % ui->core_profile_count;
  } else if (ui->core_profile_cursor == 0) {
    ui->core_profile_cursor = ui->core_profile_count - 1;
  } else {
    ui->core_profile_cursor--;
  }
  profile = ui->core_profiles[ui->core_profile_cursor].id;
  copy_truncated_string(label, sizeof(label), core_profile_display_name(profile));
  if (!run_core_set_profile(ui, profile)) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "core saved; reload failed");
    return;
  }
  snprintf(ui->status, sizeof(ui->status), "Cores saved: %.80s", label);
}

static void cycle_core_cpu_policy(struct ui_state *ui, int direction) {
  const struct performance_cpu_preset *preset;
  char extra[128];
  int index;

  if (!ui || direction == 0) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "cannot refresh CPU state");
    return;
  }
  index = performance_cpu_preset_index(ui->core_cpu_policy,
                                       ui->core_cpu_freq_khz);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)PERFORMANCE_CPU_PRESET_COUNT - 1;
  } else if ((size_t)index >= PERFORMANCE_CPU_PRESET_COUNT) {
    index = 0;
  }
  preset = &PERFORMANCE_CPU_PRESETS[index];
  snprintf(extra, sizeof(extra), " --cpu %s", preset->policy);
  if (!run_core_text_ui_extra(ui, extra)) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "CPU governor saved; reload failed");
    return;
  }
  snprintf(ui->status, sizeof(ui->status), "CPU governor saved: %.80s",
           preset->label);
}

static void cycle_core_menu_current_row(struct ui_state *ui, int direction) {
  if (!ui || direction == 0) {
    return;
  }
  core_menu_clamp_cursor(ui);
  if (ui->core_menu_cursor == CORE_MENU_ROW_PROFILE) {
    cycle_core_profile(ui, direction);
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_DEFAULT) {
    set_status(ui, "press A to restore default");
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ) {
    cycle_core_cpu_policy(ui, direction);
  }
}

static void open_core_select_screen(struct ui_state *ui, const char *system_id,
                                    const char *relative_path) {
  ui->core_back_screen = ui->screen;
  ui->screen = SCREEN_CORE_SELECT;
  ui->core_menu_cursor = CORE_MENU_ROW_PROFILE;
  copy_string(ui->core_target_system_id, sizeof(ui->core_target_system_id), system_id);
  copy_string(ui->core_target_relative_path, sizeof(ui->core_target_relative_path),
              relative_path ? relative_path : "");
  if (!load_core_select_lines(ui, system_id, relative_path)) {
    set_status(ui, "cannot load core selection");
  } else {
    set_status(ui, "core selection ready");
  }
}

static void open_rom_screen(struct ui_state *ui, const struct top_entry *entry) {
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), entry->id);
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), entry->display_name);
  if (strcmp(entry->id, "favorites") == 0) {
    open_favorites_screen(ui);
    return;
  }
  if (strcmp(entry->id, "recent") == 0) {
    open_recent_screen(ui);
    return;
  }
  ui->screen = SCREEN_ROMS;
  ui->rom_directory[0] = '\0';
  set_status(ui, "loading ROM list");
  if (!load_rom_entries(ui, entry->id)) {
    set_status(ui, "cannot load ROM list");
  } else if (ui->rom_scan_background_started) {
    restore_current_rom_cursor(ui);
    set_status(ui, "ROM list ready; refreshing scan");
  } else {
    restore_current_rom_cursor(ui);
    set_status(ui, "ROM list ready");
  }
  if (ui_uses_graphic_mode(ui) && ui->rom_entry_screen == SCREEN_GALLERY &&
      ui->rom_count > 0) {
    ui->gallery_back_screen = SCREEN_TOP;
    ui->screen = SCREEN_GALLERY;
    ui->gallery_transition_active = 0;
    ui->gallery_pending_active = 0;
    ui->gallery_pending_direction = 0;
    set_status(ui, "Gallery ready");
  }
  reset_marquee(ui);
}

static void open_system_rom_screen_by_id(struct ui_state *ui, const char *system_id,
                                         const char *fallback_name) {
  struct top_entry entry;
  size_t i;

  if (!ui || !valid_system_id(system_id)) {
    set_status(ui, "invalid system");
    return;
  }

  for (i = 0; i < ui->top_count; i++) {
    if (strcmp(ui->top_entries[i].id, system_id) == 0) {
      open_rom_screen(ui, &ui->top_entries[i]);
      return;
    }
  }

  memset(&entry, 0, sizeof(entry));
  copy_string(entry.id, sizeof(entry.id), system_id);
  copy_string(entry.display_name, sizeof(entry.display_name),
              fallback_name && fallback_name[0] ? fallback_name : system_id);
  open_rom_screen(ui, &entry);
}

static void open_gallery_screen(struct ui_state *ui) {
  size_t saved_cursor;
  char selected_relative_path[UI_PATH_MAX] = "";
  int saved_suppressed;
  int refreshed_thumbnails = 0;
  int reload_ok = 1;
  size_t i;

  if (!ui || ui->rom_count == 0) {
    set_status(ui, "no ROM entries for Gallery");
    return;
  }

  saved_cursor = ui->rom_cursor;
  if (ui->rom_count > 0 && ui->rom_cursor < ui->rom_count) {
    copy_string(selected_relative_path, sizeof(selected_relative_path),
                ui->rom_entries[ui->rom_cursor].relative_path);
  }
  if (ui->screen == SCREEN_ROMS && ui->current_system_id[0] &&
      valid_system_id(ui->current_system_id)) {
    set_status(ui, "loading Gallery artwork");
    if (run_scanner(ui->plumos_root, ui->sdcard_root, ui->current_system_id, 1)) {
      refreshed_thumbnails = 1;
      saved_suppressed = ui->rom_scan_refresh_suppressed;
      ui->rom_scan_refresh_suppressed = 1;
      reload_ok = load_rom_entries(ui, ui->current_system_id);
      ui->rom_scan_refresh_suppressed = saved_suppressed;
      if (reload_ok && selected_relative_path[0]) {
        for (i = 0; i < ui->rom_count; i++) {
          if (strcmp(ui->rom_entries[i].relative_path, selected_relative_path) == 0) {
            ui->rom_cursor = i;
            break;
          }
        }
      } else if (reload_ok && ui->rom_count > 0 && saved_cursor < ui->rom_count) {
        ui->rom_cursor = saved_cursor;
      }
    }
  }

  ui->gallery_back_screen = ui->screen;
  ui->screen = SCREEN_GALLERY;
  ui->gallery_transition_active = 0;
  ui->gallery_pending_active = 0;
  ui->gallery_pending_direction = 0;
  if (!reload_ok) {
    set_status(ui, "Gallery ready; artwork reload failed");
  } else if (refreshed_thumbnails) {
    set_status(ui, "Gallery ready; artwork refreshed");
  } else {
    set_status(ui, "Gallery ready");
  }
  reset_marquee(ui);
}

static int reload_rom_directory_no_scan(struct ui_state *ui, const char *status) {
  int saved_suppressed;
  int ok;

  if (!ui || !ui->current_system_id[0]) {
    return 0;
  }
  saved_suppressed = ui->rom_scan_refresh_suppressed;
  ui->rom_scan_refresh_suppressed = 1;
  ok = load_rom_entries(ui, ui->current_system_id);
  ui->rom_scan_refresh_suppressed = saved_suppressed;
  if (!ok) {
    set_status(ui, "cannot load ROM directory");
    return 0;
  }
  restore_current_rom_cursor(ui);
  set_status(ui, status ? status : "ROM directory ready");
  reset_marquee(ui);
  return 1;
}

static int open_rom_directory_entry(struct ui_state *ui,
                                    const struct rom_entry *entry) {
  char old_directory[UI_PATH_MAX];

  if (!ui || !entry || !entry->is_navigation_directory ||
      !entry->relative_path[0]) {
    return 0;
  }
  remember_current_rom_cursor(ui);
  copy_string(old_directory, sizeof(old_directory), ui->rom_directory);
  copy_string(ui->rom_directory, sizeof(ui->rom_directory), entry->relative_path);
  if (reload_rom_directory_no_scan(ui, "ROM directory ready")) {
    return 1;
  }
  copy_string(ui->rom_directory, sizeof(ui->rom_directory), old_directory);
  return 0;
}

static int open_parent_rom_directory(struct ui_state *ui) {
  char old_directory[UI_PATH_MAX];
  char *slash;
  char *first_slash;

  if (!ui || !ui->rom_directory[0]) {
    return 0;
  }
  remember_current_rom_cursor(ui);
  copy_string(old_directory, sizeof(old_directory), ui->rom_directory);
  slash = strrchr(ui->rom_directory, '/');
  first_slash = strchr(ui->rom_directory, '/');
  if (!slash || slash == first_slash) {
    ui->rom_directory[0] = '\0';
  } else {
    *slash = '\0';
  }
  if (reload_rom_directory_no_scan(ui, ui->rom_directory[0]
                                           ? "parent directory ready"
                                           : "ROM list ready")) {
    return 1;
  }
  copy_string(ui->rom_directory, sizeof(ui->rom_directory), old_directory);
  return 0;
}

static int update_settings_entries_after_save(struct ui_state *ui);

static int run_network_wifi_control(struct ui_state *ui, int enable) {
  char script[PATH_MAX];
  char line[256];
  char ip[64];
  char stage[64];
  FILE *pipe;
  pid_t child_pid;
  int rc;
  int connected = 0;
  int ready = 0;

  if (!ui) {
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }

  if (enable) {
    if (!save_system_config_bool(ui, "wifi_enabled", 1)) {
      set_status(ui, "Wi-Fi setting write failed");
      return 0;
    }
    ui->device.wifi_enabled = 1;
    update_settings_entries_after_save(ui);
    set_status(ui, "Starting Wi-Fi");
    render_ui(ui);

    ip[0] = '\0';
    stage[0] = '\0';
    pipe = open_plumos_script_pipe(ui, script, "--wifi", "on", &child_pid);
    if (!pipe) {
      set_status(ui, "Wi-Fi start failed");
      return 1;
    }
    while (fgets(line, sizeof(line), pipe)) {
      trim_line_end(line);
      if (strcmp(line, "result=connected") == 0) {
        connected = 1;
      } else if (strcmp(line, "result=ready") == 0) {
        ready = 1;
      } else if (strncmp(line, "ip=", 3) == 0) {
        copy_truncated_string(ip, sizeof(ip), line + 3);
      } else if (strncmp(line, "stage=", 6) == 0) {
        copy_truncated_string(stage, sizeof(stage), line + 6);
      }
    }
    rc = close_plumos_script_pipe(pipe, child_pid);
    settle_input_after_child(ui);
    load_wifi_runtime_status(ui);
    if (connected && rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
      ui->device.wifi_enabled = 1;
      ui->device.wifi_runtime_enabled = 1;
      update_settings_entries_after_save(ui);
      snprintf(ui->status, sizeof(ui->status), "Wi-Fi connected IP=%s",
               ip[0] ? ip : "-");
      return 1;
    }
    if (ready && rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
      ui->device.wifi_enabled = 1;
      ui->device.wifi_runtime_enabled = 1;
      update_settings_entries_after_save(ui);
      set_status(ui, "Wi-Fi on; use Connect Wi-Fi");
      return 1;
    }
    ui->device.wifi_enabled = 1;
    update_settings_entries_after_save(ui);
    if (stage[0]) {
      snprintf(ui->status, sizeof(ui->status), "Wi-Fi on; failed at %s", stage);
    } else {
      set_status(ui, "Wi-Fi on; no IP yet");
    }
    return 1;
  }

  rc = run_network_control_quiet(ui, script, "--wifi", "off");
  if (rc == -1) {
    set_status(ui, "network control system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    if (!save_system_config_bool(ui, "wifi_enabled", 0)) {
      set_status(ui, "Wi-Fi off; setting write failed");
      return 0;
    }
    ui->device.wifi_enabled = 0;
    ui->device.wifi_runtime_enabled = 0;
    update_settings_entries_after_save(ui);
    set_status(ui, "Wi-Fi off; saved");
    return 1;
  }
  set_status(ui, "network control returned non-zero");
  return 0;
}

static int run_network_service_control(struct ui_state *ui, const char *service,
                                       int enable) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char status_message[128];
  size_t pos = 0;
  int rc;

  if (!ui || !service || !service[0]) {
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-services")) {
    set_status(ui, "network services path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network services script missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, enable ? " start " : " stop ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, service) ||
      !append_string(cmd, sizeof(cmd), &pos, " >/dev/null 2>&1")) {
    set_status(ui, "network service command too long");
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  update_settings_entries_after_save(ui);
  if (rc == -1) {
    set_status(ui, "network service system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    if (strcmp(service, "adb") == 0) {
      set_status(ui, "ADB setting saved; reboot required");
      return 1;
    }
    snprintf(status_message, sizeof(status_message), "%s %s",
             service, enable ? "enabled" : "disabled");
    set_status(ui, status_message);
    return 1;
  }
  snprintf(status_message, sizeof(status_message), "%s %s failed",
           service, enable ? "start" : "stop");
  set_status(ui, status_message);
  return 0;
}

static void stop_ntp_for_manual_time(struct ui_state *ui) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;

  if (!ui ||
      !join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-stock-services") ||
      !file_exists(script)) {
    return;
  }
  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " ntp-stop >/dev/null 2>&1")) {
    return;
  }
  (void)run_runtime_shell_command(cmd);
}

static int run_time_sync_helper(struct ui_state *ui, const char *action) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui || !action || !action[0] ||
      !join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-time-sync") ||
      !file_exists(script)) {
    return 0;
  }
  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_SYSTEM_SETTINGS_JSON=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->system_config_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, action) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " >/run/plumos/time-sync/frontend-action.log 2>&1")) {
    return 0;
  }
  rc = run_runtime_shell_command(cmd);
  return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

static int sync_time_now(struct ui_state *ui) {
  int ok = run_time_sync_helper(ui, "force-sync");

  update_settings_entries_after_save(ui);
  set_status(ui, ok ? "system time and RTC synchronized"
                    : "time synchronization failed; see time-sync log");
  return ok;
}

static int apply_manual_system_time(struct ui_state *ui) {
  struct tm local_tm;
  struct tm check_tm;
  time_t epoch;
  struct timeval tv;
  char value[64];

  if (!ui) {
    return 0;
  }
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  clamp_manual_time_fields(ui);
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);

  memset(&local_tm, 0, sizeof(local_tm));
  local_tm.tm_year = (int)ui->manual_time_year - 1900;
  local_tm.tm_mon = (int)ui->manual_time_month - 1;
  local_tm.tm_mday = (int)ui->manual_time_day;
  local_tm.tm_hour = (int)ui->manual_time_hour;
  local_tm.tm_min = (int)ui->manual_time_minute;
  local_tm.tm_sec = 0;
  local_tm.tm_isdst = -1;
  epoch = mktime(&local_tm);
  if (epoch == (time_t)-1 || !localtime_r(&epoch, &check_tm) ||
      check_tm.tm_year != local_tm.tm_year ||
      check_tm.tm_mon != local_tm.tm_mon ||
      check_tm.tm_mday != local_tm.tm_mday ||
      check_tm.tm_hour != local_tm.tm_hour ||
      check_tm.tm_min != local_tm.tm_min) {
    set_status(ui, "manual time is invalid for this timezone");
    return 0;
  }

  stop_ntp_for_manual_time(ui);
  if (!save_system_config_bool(ui, "automatic_time", 0)) {
    set_status(ui, "automatic time setting write failed");
    return 0;
  }
  ui->device.automatic_time_enabled = 0;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  if (settimeofday(&tv, NULL) != 0) {
    snprintf(ui->status, sizeof(ui->status), "manual time failed: %s",
             strerror(errno));
    return 0;
  }
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
  if (!run_time_sync_helper(ui, "store-rtc")) {
    set_status(ui, "system time set; RTC update failed");
    return 0;
  }
  ui->manual_time_initialized = 0;
  update_settings_entries_after_save(ui);
  format_current_time_local(value, sizeof(value));
  snprintf(ui->status, sizeof(ui->status),
           "manual time set: %s; automatic time OFF; RTC saved", value);
  return 1;
}

static void wifi_clear_result(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->wifi_result_title[0] = '\0';
  ui->wifi_result_ip[0] = '\0';
  ui->wifi_result_gateway[0] = '\0';
  ui->wifi_result_gateway_ping[0] = '\0';
  ui->wifi_result_stage[0] = '\0';
  ui->wifi_result_success = 0;
}

static void open_wifi_connect_screen(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->wifi_back_screen = ui->screen;
  ui->screen = SCREEN_WIFI_CONNECT;
  ui->wifi_stage = WIFI_CONNECT_SELECT;
  ui->wifi_key_row = 0;
  ui->wifi_key_col = 0;
  ui->wifi_key_shift = 0;
  ui->wifi_password[0] = '\0';
  wifi_clear_result(ui);
  set_status(ui, "Scanning Wi-Fi SSIDs");
  render_ui(ui);
  wifi_scan_networks(ui);
}

static void wifi_back_to_network_settings(struct ui_state *ui, const char *status) {
  open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
  select_setting_entry_by_id(ui, "network_connect_wifi");
  set_status(ui, status ? status : "back to Network Settings");
}

static int wifi_selected_network_is_open(const struct ui_state *ui) {
  const struct wifi_network_entry *entry;

  if (!ui || ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
    return 0;
  }
  entry = &ui->wifi_networks[ui->wifi_cursor];
  return strcmp(entry->security, "open") == 0;
}

static int wifi_write_connect_tempfile(struct ui_state *ui, char *path, size_t path_size) {
  char template_path[] = "/tmp/plumos-wifi-connect.XXXXXX";
  int fd;
  const char *ssid;

  if (!ui || !path || path_size == 0 || ui->wifi_count == 0 ||
      ui->wifi_cursor >= ui->wifi_count) {
    return 0;
  }
  ssid = ui->wifi_networks[ui->wifi_cursor].ssid;
  if (string_contains_line_break(ssid) || string_contains_line_break(ui->wifi_password)) {
    set_status(ui, "SSID/password contains unsupported newline");
    return 0;
  }
  fd = mkstemp(template_path);
  if (fd < 0) {
    set_status(ui, "cannot create Wi-Fi temp file");
    return 0;
  }
  if (!write_all_string(fd, ssid) ||
      !write_all_string(fd, "\n") ||
      !write_all_string(fd, ui->wifi_password) ||
      !write_all_string(fd, "\n")) {
    close(fd);
    unlink(template_path);
    set_status(ui, "cannot write Wi-Fi temp file");
    return 0;
  }
  if (close(fd) != 0) {
    unlink(template_path);
    set_status(ui, "cannot close Wi-Fi temp file");
    return 0;
  }
  if (!copy_string(path, path_size, template_path)) {
    unlink(template_path);
    set_status(ui, "Wi-Fi temp path too long");
    return 0;
  }
  return 1;
}

static void wifi_parse_connect_output_line(struct ui_state *ui, const char *line) {
  const char *value;

  if (!ui || !line) {
    return;
  }
  if (strncmp(line, "result=", 7) == 0) {
    value = line + 7;
    if (strcmp(value, "connected") == 0) {
      ui->wifi_result_success = 1;
      copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connected");
    } else {
      ui->wifi_result_success = 0;
      copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connection Failed");
    }
  } else if (strncmp(line, "ip=", 3) == 0) {
    copy_truncated_string(ui->wifi_result_ip, sizeof(ui->wifi_result_ip), line + 3);
  } else if (strncmp(line, "gateway=", 8) == 0) {
    copy_truncated_string(ui->wifi_result_gateway, sizeof(ui->wifi_result_gateway),
                          line + 8);
  } else if (strncmp(line, "gateway_ping=", 13) == 0) {
    copy_truncated_string(ui->wifi_result_gateway_ping,
                          sizeof(ui->wifi_result_gateway_ping), line + 13);
  } else if (strncmp(line, "stage=", 6) == 0) {
    copy_prefixed_truncated_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
                                   "Failed at ", line + 6);
  }
}

static int run_wifi_connect_selected(struct ui_state *ui) {
  char script[PATH_MAX];
  char temp_path[PATH_MAX];
  char line[512];
  FILE *pipe;
  pid_t child_pid;
  int rc;
  size_t password_len;

  if (!ui || ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
    set_status(ui, "No SSID selected");
    return 0;
  }
  password_len = strlen(ui->wifi_password);
  if (!wifi_selected_network_is_open(ui) &&
      (password_len < 8 || password_len > UI_WIFI_PASSWORD_MAX)) {
    set_status(ui, "Password must be 8..64 chars");
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }
  if (!wifi_write_connect_tempfile(ui, temp_path, sizeof(temp_path))) {
    return 0;
  }

  wifi_clear_result(ui);
  copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connecting...");
  copy_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage), "Waiting for IP address.");
  ui->wifi_stage = WIFI_CONNECT_RESULT;
  set_status(ui, "Connecting Wi-Fi");
  render_ui(ui);

  pipe = open_plumos_script_pipe(ui, script, "--connect-file", temp_path,
                                 &child_pid);
  if (!pipe) {
    unlink(temp_path);
    set_status(ui, "Wi-Fi connect failed to start");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    wifi_parse_connect_output_line(ui, line);
  }
  rc = close_plumos_script_pipe(pipe, child_pid);
  unlink(temp_path);
  settle_input_after_child(ui);

  if (ui->wifi_result_success) {
    if (save_system_config_bool(ui, "wifi_enabled", 1)) {
      ui->device.wifi_enabled = 1;
      ui->device.wifi_runtime_enabled = 1;
    }
    if (!ui->wifi_result_stage[0]) {
      snprintf(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
               "Gateway ping: %s",
               ui->wifi_result_gateway_ping[0] ? ui->wifi_result_gateway_ping : "skipped");
    }
    snprintf(ui->status, sizeof(ui->status), "Wi-Fi connected IP=%s",
             ui->wifi_result_ip[0] ? ui->wifi_result_ip : "-");
    return 1;
  }
  if (!ui->wifi_result_title[0]) {
    copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connection Failed");
  }
  if (!ui->wifi_result_stage[0]) {
    copy_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
                "No connection result returned.");
  }
  if (rc == -1 || !(WIFEXITED(rc) && WEXITSTATUS(rc) == 0)) {
    set_status(ui, "Wi-Fi connection failed");
  } else {
    set_status(ui, "Wi-Fi connection did not complete");
  }
  return 0;
}

static int run_power_action(struct ui_state *ui, const char *action, int poweroff) {
  char script[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *dry_run;
  const char *sleep_backend;
  const char *sleep_wakeup_sec;
  const char *power_backend;
  size_t pos = 0;
  int rc;
  int terminal_action;
  int dry_run_enabled;
  int sleep_display_power_attempted = 0;
  enum ui_screen previous_screen;

  if (!action || (strcmp(action, "shutdown") != 0 &&
                  strcmp(action, "reboot") != 0 &&
                  strcmp(action, "sleep") != 0)) {
    set_status(ui, "power action is invalid");
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-safe-shutdown") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-power-action.log")) {
    set_status(ui, "power action path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "power action script missing");
    return 0;
  }

  dry_run = getenv("PLUMOS_CONTROLLER_POWER_DRY_RUN");
  if (!dry_run || !dry_run[0]) {
    dry_run = getenv("PLUMOS_CONTROLLER_SAFE_DRY_RUN");
  }
  sleep_backend = getenv("PLUMOS_CONTROLLER_POWER_SLEEP_BACKEND");
  if (!sleep_backend || !sleep_backend[0]) {
    sleep_backend = getenv("PLUMOS_CONTROLLER_SAFE_SLEEP_BACKEND");
  }
  sleep_wakeup_sec = getenv("PLUMOS_CONTROLLER_POWER_SLEEP_WAKEUP_SEC");
  if (!sleep_wakeup_sec) {
    sleep_wakeup_sec = getenv("PLUMOS_CONTROLLER_SAFE_SLEEP_WAKEUP_SEC");
  }
  power_backend = getenv("PLUMOS_CONTROLLER_POWER_BACKEND");
  if (!power_backend || !power_backend[0]) {
    power_backend = getenv("PLUMOS_CONTROLLER_SAFE_POWER_BACKEND");
  }
  if (!sleep_backend || !sleep_backend[0]) {
    const char *device_id = getenv("PLUMOS_DEVICE_ID");
    sleep_backend = (device_id && strcmp(device_id, "pixel2_compat") == 0) ? "bootfast" : "mem";
  }
  if (!sleep_wakeup_sec) {
    sleep_wakeup_sec = "";
  }
  if (!power_backend || !power_backend[0]) {
    power_backend = "auto";
  }
  terminal_action = strcmp(action, "reboot") == 0 ||
                    (strcmp(action, "shutdown") == 0 && poweroff);
  dry_run_enabled = dry_run && dry_run[0] && strcmp(dry_run, "0") != 0 &&
                    strcmp(dry_run, "false") != 0 && strcmp(dry_run, "off") != 0;

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " --") ||
      !append_string(cmd, sizeof(cmd), &pos, action)) {
    set_status(ui, "power action command too long");
    return 0;
  }
  if (strcmp(action, "sleep") == 0) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --sleep-backend ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, sleep_backend) ||
        !append_string(cmd, sizeof(cmd), &pos, " --no-poweroff --no-hold-resume")) {
      set_status(ui, "power action command too long");
      return 0;
    }
    if (sleep_wakeup_sec[0] &&
        (!append_string(cmd, sizeof(cmd), &pos, " --wakeup-sec ") ||
         !append_shell_quoted(cmd, sizeof(cmd), &pos, sleep_wakeup_sec))) {
      set_status(ui, "power action command too long");
      return 0;
    }
  } else if (strcmp(action, "reboot") == 0) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --no-hold-resume")) {
      set_status(ui, "power action command too long");
      return 0;
    }
  } else if (poweroff) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --poweroff --power-backend ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, power_backend) ||
        !append_string(cmd, sizeof(cmd), &pos, " --no-hold-resume")) {
      set_status(ui, "power action command too long");
      return 0;
    }
  } else if (!append_string(cmd, sizeof(cmd), &pos, " --no-poweroff --no-hold-resume")) {
    set_status(ui, "power action command too long");
    return 0;
  }
  if (dry_run_enabled && !append_string(cmd, sizeof(cmd), &pos, " --dry-run")) {
    set_status(ui, "power action command too long");
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "power action command too long");
    return 0;
  }

  previous_screen = ui->screen;
  if (terminal_action) {
    copy_string(ui->power_action, sizeof(ui->power_action), action);
    ui->screen = SCREEN_POWER_ACTION_RUNNING;
    ui->repeat_action = ACTION_NONE;
    ui->repeat_key_code = 0;
    ui->repeat_next_ms = 0;
    set_status(ui, "");
  } else {
    snprintf(ui->status, sizeof(ui->status), "power %s running", action);
  }
  render_ui(ui);
  if (strcmp(action, "sleep") == 0 && ui_renderer_fbdev_only(ui) &&
      !plumos_fbdev_present_black(&ui->fbdev_renderer)) {
    set_status(ui, "sleep display blank failed");
    return 0;
  }
  if (strcmp(action, "sleep") == 0 && ui_renderer_fbdev_only(ui)) {
    sleep_display_power_attempted = 1;
    if (!plumos_fbdev_set_display_power(&ui->fbdev_renderer, 0)) {
      fprintf(stderr, "frontend: sleep display power-off failed\n");
    }
  }
  rc = run_runtime_shell_command(cmd);
  if (sleep_display_power_attempted &&
      !plumos_fbdev_set_display_power(&ui->fbdev_renderer, 1)) {
    fprintf(stderr, "frontend: sleep display power-on failed\n");
  }
  if (rc == -1) {
    if (terminal_action) {
      ui->screen = previous_screen;
      ui->power_action[0] = '\0';
    }
    set_status(ui, "power action system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    if (strcmp(action, "sleep") == 0) {
      snprintf(ui->status, sizeof(ui->status), "sleep complete backend=%s", sleep_backend);
    } else if (strcmp(action, "reboot") == 0) {
      copy_string(ui->status, sizeof(ui->status), "reboot requested");
    } else {
      snprintf(ui->status, sizeof(ui->status), "shutdown complete%s",
               poweroff ? " poweroff" : " (no poweroff)");
    }
    if (terminal_action && dry_run_enabled) {
      ui->screen = previous_screen;
      ui->power_action[0] = '\0';
    }
    return 1;
  }
  if (terminal_action) {
    ui->screen = previous_screen;
    ui->power_action[0] = '\0';
  }
  set_status(ui, "power action returned non-zero; see frontend-power-action.log");
  return 0;
}

static int request_latest_system_update(struct ui_state *ui) {
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!file_exists("/usr/sbin/plumos-system-update")) {
    set_status(ui, "System Update helper missing");
    return 0;
  }
  if (!join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir,
                 "frontend-system-update.log")) {
    set_status(ui, "System Update log path too long");
    return 0;
  }
  set_status(ui, "Verifying latest update package...");
  render_ui(ui);
  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " PLUMOS_USERDATA_ROOT=/mnt/plumos-user"
                     " PLUMOS_BOOT_ROOT=/flash"
                     " /usr/sbin/plumos-system-update request-latest >") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "System Update command too long");
    return 0;
  }
  rc = run_runtime_shell_command(cmd);
  if (!system_command_succeeded(rc)) {
    set_status(ui, "No compatible update; see frontend-system-update.log");
    return 0;
  }
  set_status(ui, "Update ready; restarting safely");
  render_ui(ui);
  return run_power_action(ui, "reboot", 0);
}

static int run_storage_health_check(struct ui_state *ui) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui ||
      !join_path(script, sizeof(script), ui->plumos_root,
                 "bin/plumos-storage-health")) {
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "Storage health helper missing");
    return 0;
  }
  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " check >/dev/null 2>&1")) {
    set_status(ui, "Storage check command too long");
    return 0;
  }
  set_status(ui, "Checking FAT32 read-only; please wait");
  render_ui(ui);
  rc = run_runtime_shell_command(cmd);
  if (rc == -1 || !(WIFEXITED(rc) && WEXITSTATUS(rc) == 0)) {
    set_status(ui, "Storage check failed; see storage-health.log");
    return 0;
  }
  load_settings_entries(ui);
  select_setting_entry_by_id(ui, "system_storage_check");
  snprintf(ui->status, sizeof(ui->status), "Storage: %s",
           ui->device.storage_health);
  return 1;
}

static int write_power_overlay_selection(struct ui_state *ui, const char *action) {
  const char *path = getenv("PLUMOS_POWER_MENU_SELECTION");
  FILE *fp;

  if (!path || !path[0]) {
    set_status(ui, "power overlay selection path missing");
    return 0;
  }
  fp = fopen(path, "w");
  if (!fp) {
    snprintf(ui->status, sizeof(ui->status),
             "power overlay selection write failed: %s", strerror(errno));
    return 0;
  }
  fprintf(fp, "action=%s\n", action && action[0] ? action : "cancel");
  if (fclose(fp) != 0) {
    snprintf(ui->status, sizeof(ui->status),
             "power overlay selection close failed: %s", strerror(errno));
    return 0;
  }
  return 1;
}

static int launch_rom_entry(struct ui_state *ui, const struct rom_entry *entry) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *system_id;
  size_t pos = 0;
  int rc;

  if (!entry || !entry->relative_path[0]) {
    set_status(ui, "launch target is empty");
    return 0;
  }
  if (entry->is_navigation_directory) {
    return open_rom_directory_entry(ui, entry);
  }
  remember_current_rom_cursor(ui);
  system_id = entry->system_id[0] ? entry->system_id : ui->current_system_id;
  if (!valid_system_id(system_id)) {
    set_status(ui, "launch system id is invalid");
    return 0;
  }
  if (filesystem_is_read_only(ui->plumos_root)) {
    set_status(ui, "PLUMOS is read-only; reboot to repair it");
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-launch.log")) {
    set_status(ui, "launch path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " launch ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->relative_path)) {
    set_status(ui, "launch command too long");
    return 0;
  }
  if (ui->screen == SCREEN_RECENT && entry->launch_profile[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --profile ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->launch_profile)) {
      set_status(ui, "launch command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " --execute --no-scan >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "launch command too long");
    return 0;
  }

  snprintf(ui->status, sizeof(ui->status), "launching %.32s / %.120s", system_id,
           entry->relative_path);
  render_ui(ui);
  shutdown_ui_renderer_for_launch(ui);
  clear_pixel2_compat_launch_framebuffer(ui, system_id);
  rc = run_foreground_shell_command(cmd);
  if ((ui->renderer_mali || ui->renderer_fbdev || ui->renderer_pixel2_compat_gfx) &&
      !init_ui_renderer(ui)) {
    ui->renderer_mali = 0;
    ui->renderer_fbdev = 0;
    ui->renderer_pixel2_compat_gfx = 0;
  }
  settle_input_after_child(ui);
  load_device_settings(ui);
  if (ui->screen == SCREEN_RECENT) {
    load_recent_entries(ui);
  }
  if (rc == -1) {
    set_status(ui, "launch system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    set_status(ui, "launch finished; FE resumed");
    return 1;
  }
  set_status(ui, "launch returned non-zero; see frontend-launch.log");
  return 0;
}

static int string_is_unsigned_int(const char *value) {
  if (!value || !value[0]) {
    return 0;
  }
  while (*value) {
    if (!isdigit((unsigned char)*value)) {
      return 0;
    }
    value++;
  }
  return 1;
}

static const char *scraping_limit_env(const char *name) {
  const char *value = getenv(name);
  return string_is_unsigned_int(value) ? value : NULL;
}

static int append_scraping_target_list(char *cmd, size_t cmd_size, size_t *pos,
                                       const struct ui_state *ui) {
  size_t i;

  if (!cmd || !pos || !ui || ui->scraping_choice_count == 0) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return append_string(cmd, cmd_size, pos, " ") &&
           append_shell_quoted(cmd, cmd_size, pos,
                               ui->scraping_choices[ui->scraping_choice_cursor - 1].id);
  }
  for (i = 0; i < ui->scraping_choice_count; i++) {
    if (!append_string(cmd, cmd_size, pos, " ") ||
        !append_shell_quoted(cmd, cmd_size, pos, ui->scraping_choices[i].id)) {
      return 0;
    }
  }
  return 1;
}

static int append_scraping_runner_loop(char *cmd, size_t cmd_size, size_t *pos,
                                       const struct ui_state *ui,
                                       const char *scraper,
                                       const char *path_value,
                                       const char *image_root,
                                       int fetch_mode,
                                       const char *scraper_kind,
                                       int replace_existing,
                                       const char *limit,
                                       const char *fetch_timeout,
                                       const char *fetch_retry) {
  if (!append_string(cmd, cmd_size, pos, "; for sys in") ||
      !append_scraping_target_list(cmd, cmd_size, pos, ui)) {
    return 0;
  }
  if (fetch_mode) {
    if (!append_string(cmd, cmd_size, pos, "; do PLUMOS_THUMBNAIL_PROGRESS=1")) {
      return 0;
    }
    if (fetch_timeout &&
        (!append_string(cmd, cmd_size, pos, " PLUMOS_THUMBNAIL_FETCH_TIMEOUT=") ||
         !append_shell_quoted(cmd, cmd_size, pos, fetch_timeout))) {
      return 0;
    }
    if (fetch_retry &&
        (!append_string(cmd, cmd_size, pos, " PLUMOS_THUMBNAIL_FETCH_RETRY=") ||
         !append_shell_quoted(cmd, cmd_size, pos, fetch_retry))) {
      return 0;
    }
    if (!append_string(cmd, cmd_size, pos, " PLUMOS_SDCARD_ROOT=")) {
      return 0;
    }
  } else if (!append_string(cmd, cmd_size, pos,
                            "; do progress_i=$((progress_i + 1)); printf 'progress\\tplan\\t%s\\t%s\\t%s\\t0\\t0\\t0\\n' \"$sys\" \"$progress_i\" \"$progress_total\"; PLUMOS_SDCARD_ROOT=")) {
    return 0;
  }
  if (!append_shell_quoted(cmd, cmd_size, pos, ui->sdcard_root) ||
      !append_string(cmd, cmd_size, pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, cmd_size, pos, ui->plumos_root) ||
      !append_string(cmd, cmd_size, pos, " PLUMOS_IMAGE_ROOT=") ||
      !append_shell_quoted(cmd, cmd_size, pos, image_root) ||
      !append_string(cmd, cmd_size, pos, " PATH=") ||
      !append_shell_quoted(cmd, cmd_size, pos, path_value) ||
      !append_string(cmd, cmd_size, pos, " ") ||
      !append_runtime_script_invocation(cmd, cmd_size, pos, scraper)) {
    return 0;
  }
  if (fetch_mode && !append_string(cmd, cmd_size, pos, " --fetch")) {
    return 0;
  }
  if (scraper_kind && scraper_kind[0] &&
      (!append_string(cmd, cmd_size, pos, " --kind ") ||
       !append_shell_quoted(cmd, cmd_size, pos, scraper_kind))) {
    return 0;
  }
  if (replace_existing &&
      !append_string(cmd, cmd_size, pos, " --replace-existing")) {
    return 0;
  }
  if (!append_string(cmd, cmd_size, pos, " --system \"$sys\"")) {
    return 0;
  }
  if (limit && (!append_string(cmd, cmd_size, pos, " --limit ") ||
                !append_shell_quoted(cmd, cmd_size, pos, limit))) {
    return 0;
  }
  return append_string(cmd, cmd_size, pos,
                       "; step_rc=$?; [ \"$step_rc\" -eq 0 ] || app_rc=\"$step_rc\"; done");
}

static int run_command_with_live_log(struct ui_state *ui, const char *cmd,
                                     const char *latest_path, const char *log_path) {
  FILE *pipe;
  pid_t pipe_pid;
  FILE *latest;
  FILE *log;
  char line[UI_RENDER_LINE_MAX];
  char trimmed[UI_RENDER_LINE_MAX];
  int rc;

  if (!ui || !cmd || !latest_path || !log_path) {
    return -1;
  }
  latest = fopen(latest_path, "wb");
  if (!latest) {
    set_status(ui, "cannot write latest result log");
    return -1;
  }
  log = fopen(log_path, "ab");
  if (!log) {
    fclose(latest);
    set_status(ui, "cannot write app result log");
    return -1;
  }
  pipe = open_runtime_shell_pipe(cmd, &pipe_pid);
  if (!pipe) {
    fclose(log);
    fclose(latest);
    set_status(ui, tr(ui, "scraping.status.command_start_failed",
                      "cannot start scraping command"));
    return -1;
  }

  while (fgets(line, sizeof(line), pipe)) {
    fputs(line, latest);
    fflush(latest);
    fputs(line, log);
    fflush(log);
    copy_truncated_string(trimmed, sizeof(trimmed), line);
    trim_line_end(trimmed);
    if (update_thumbnail_running_progress_from_log_line(ui, trimmed)) {
      render_ui(ui);
    }
  }
  rc = close_runtime_shell_pipe(pipe, pipe_pid);
  fclose(log);
  fclose(latest);
  return rc;
}

static int run_scraping_action(struct ui_state *ui) {
  char scraper[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char latest_path[PATH_MAX];
  char image_root[PATH_MAX];
  char path_value[PATH_MAX * 2];
  char cmd[UI_COMMAND_MAX];
  const char *plan_limit;
  const char *fetch_limit;
  const char *fetch_timeout;
  const char *fetch_retry;
  const struct scraping_kind_choice *kind;
  struct cpu_policy_snapshot cpu_snapshot;
  size_t pos = 0;
  size_t path_pos = 0;
  size_t target_count;
  int cpu_policy_applied = 0;
  int rc;

  if (!ui || ui->scraping_choice_count == 0) {
    set_status(ui, tr(ui, "scraping.status.no_target", "no scraping target"));
    return 0;
  }
  if (!join_path(scraper, sizeof(scraper), ui->plumos_root,
                 "bin/plumos-thumbnail-scraper") ||
      !join_path(image_root, sizeof(image_root), ui->sdcard_root, "Images") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-apps.log") ||
      !join_path(latest_path, sizeof(latest_path), log_dir,
                 "frontend-apps-latest.log")) {
    set_status(ui, tr(ui, "scraping.status.path_too_long",
                      "scraping path too long"));
    return 0;
  }
  if (!file_exists(scraper)) {
    set_status(ui, tr(ui, "scraping.status.scraper_missing",
                      "scraper is missing"));
    return 0;
  }
  path_value[0] = '\0';
  if (!append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos, "/bin:") ||
      !append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos, "/gnu/bin")) {
    set_status(ui, tr(ui, "scraping.status.env_path_too_long",
                      "scraping PATH too long"));
    return 0;
  }
  plan_limit = scraping_limit_env("PLUMOS_SCRAPING_PLAN_LIMIT");
  fetch_limit = scraping_limit_env("PLUMOS_SCRAPING_FETCH_LIMIT");
  fetch_timeout = scraping_limit_env("PLUMOS_SCRAPING_FETCH_TIMEOUT");
  fetch_retry = scraping_limit_env("PLUMOS_SCRAPING_FETCH_RETRY");
  if (!fetch_timeout) {
    fetch_timeout = "12";
  }
  if (!fetch_retry) {
    fetch_retry = "0";
  }
  kind = scraping_selected_kind(ui);
  target_count = ui->scraping_choice_cursor == 0 ? ui->scraping_choice_count : (size_t)1;

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; { printf 'app_start\\t%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, "thumbnail-scraping") ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; printf 'scraping_options\\timage=%s\\tkind=%s\\texisting=%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scraping_kind_display_name(ui, kind)) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, kind->scraper_kind) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scraping_existing_label(ui)) ||
      !append_string(cmd, sizeof(cmd), &pos, "; app_rc=0; progress_i=0; progress_total=") ||
      !append_size_t(cmd, sizeof(cmd), &pos, target_count) ||
      !append_scraping_runner_loop(cmd, sizeof(cmd), &pos, ui, scraper,
                                   path_value, image_root, 0, kind->scraper_kind,
                                   ui->scraping_replace_existing,
                                   plan_limit, NULL, NULL) ||
      !append_scraping_runner_loop(cmd, sizeof(cmd), &pos, ui, scraper,
                                   path_value, image_root, 1, kind->scraper_kind,
                                   ui->scraping_replace_existing,
                                   fetch_limit, fetch_timeout, fetch_retry) ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, "thumbnail-scraping") ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " \"$app_rc\"; [ \"$app_rc\" -eq 0 ]; } 2>&1")) {
    set_status(ui, tr(ui, "scraping.status.command_too_long",
                      "scraping command too long"));
    return 0;
  }

  copy_string(ui->thumbnail_running_title, sizeof(ui->thumbnail_running_title),
              "Scraping");
  reset_thumbnail_running_progress(ui);
  ui->screen = SCREEN_THUMBNAIL_RUNNING;
  snprintf(ui->status, sizeof(ui->status), "%s %s",
           tr(ui, "scraping.status.running", "running Scraping"),
           scraping_kind_display_name(ui, kind));
  render_ui(ui);
  mkdir(log_dir, 0755);
  cpu_policy_applied = apply_scraping_cpu_policy(&cpu_snapshot);
  rc = run_command_with_live_log(ui, cmd, latest_path, log_path);
  if (cpu_policy_applied) {
    restore_cpu_policy_snapshot(&cpu_snapshot);
  }
  settle_input_after_child(ui);
  ui->screen = SCREEN_THUMBNAIL_RESULTS;
  ui->thumbnail_result_cursor = 0;
  load_thumbnail_results(ui);
  if (system_command_succeeded(rc)) {
    set_status(ui, tr(ui, "scraping.status.finished", "Scraping finished"));
    return 1;
  }
  if (rc == -1) {
    set_status(ui, tr(ui, "scraping.status.system_call_failed",
                      "scraping system call failed"));
    return 0;
  }
  set_status(ui, tr(ui, "scraping.status.nonzero",
                    "Scraping returned non-zero"));
  return 0;
}

static int run_menu_shell_action(struct ui_state *ui, const struct menu_entry *entry) {
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char latest_path[PATH_MAX];
  char lock_dir[PATH_MAX];
  char path_value[PATH_MAX * 2];
  char cmd[UI_COMMAND_MAX];
  const char *shell_cmd;
  size_t pos = 0;
  size_t path_pos = 0;
  int rc;

  if (!ui || !entry || strncmp(entry->action, "shell:", 6) != 0) {
    set_status(ui, "app action is not a shell command");
    return 0;
  }
  shell_cmd = entry->action + 6;
  if (!shell_cmd[0]) {
    set_status(ui, "app shell command is empty");
    return 0;
  }
  if (!join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-apps.log")) {
    set_status(ui, "app log path too long");
    return 0;
  }
  if (entry->show_results &&
      !join_path(latest_path, sizeof(latest_path), log_dir, "frontend-apps-latest.log")) {
    set_status(ui, "app latest log path too long");
    return 0;
  }
  path_value[0] = '\0';
  if (!append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos, "/bin:") ||
      !append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos,
                     "/gnu/bin:/usr/sbin:/usr/bin:/sbin:/bin")) {
    set_status(ui, "app PATH too long");
    return 0;
  }
  lock_dir[0] = '\0';
  if (entry->background) {
    if (!valid_system_id(entry->id)) {
      set_status(ui, "background app id is invalid");
      return 0;
    }
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/plumos-app-%s.lock", entry->id);
  }

  cmd[0] = '\0';
  if (entry->show_results) {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; { printf 'app_start\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_runtime_shell_eval(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       "; app_rc=$?; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       " \"$app_rc\"; [ \"$app_rc\" -eq 0 ]; } >") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, latest_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>&1; rc=$?; cat ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, latest_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, "; exit \"$rc\"")) {
      set_status(ui, "app command too long");
      return 0;
    }
  } else if (entry->background) {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; trap '' HUP; if mkdir ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, lock_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>/dev/null; then ( trap 'rm -rf ") ||
        !append_string(cmd, sizeof(cmd), &pos, lock_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "' EXIT INT TERM; printf 'app_start\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_runtime_shell_eval(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       "; rc=$?; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, " \"$rc\"; exit \"$rc\" ) >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       " 2>&1 </dev/null & else printf 'app_already_running\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, "; exit 3; fi")) {
      set_status(ui, "app command too long");
      return 0;
    }
  } else {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_runtime_shell_eval(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
      set_status(ui, "app command too long");
      return 0;
    }
  }

  if (entry->show_results) {
    copy_string(ui->thumbnail_running_title, sizeof(ui->thumbnail_running_title),
                entry->display_name);
    copy_string(ui->thumbnail_result_return_app_id,
                sizeof(ui->thumbnail_result_return_app_id), entry->id);
    ui->screen = SCREEN_THUMBNAIL_RUNNING;
    snprintf(ui->status, sizeof(ui->status), "running %.80s", entry->display_name);
  } else {
    snprintf(ui->status, sizeof(ui->status), "%s %.80s",
             entry->background ? "starting" : "running", entry->display_name);
  }
  render_ui(ui);
  shutdown_ui_renderer(ui);
  rc = run_foreground_shell_command(cmd);
  if ((ui->renderer_mali || ui->renderer_fbdev || ui->renderer_pixel2_compat_gfx) &&
      !init_ui_renderer(ui)) {
    ui->renderer_mali = 0;
    ui->renderer_fbdev = 0;
    ui->renderer_pixel2_compat_gfx = 0;
  }
  settle_input_after_child(ui);
  if (entry->show_results) {
    ui->screen = SCREEN_THUMBNAIL_RESULTS;
    ui->thumbnail_result_cursor = 0;
    load_thumbnail_results(ui);
    if (system_command_succeeded(rc)) {
      snprintf(ui->status, sizeof(ui->status), "%.80s finished", entry->display_name);
      return 1;
    }
    if (rc == -1) {
      set_status(ui, "app system call failed");
      return 0;
    }
    set_status(ui, "app returned non-zero; see latest result");
    return 0;
  }
  if (system_command_succeeded(rc)) {
    snprintf(ui->status, sizeof(ui->status), "%.80s %s", entry->display_name,
             entry->background ? "started; see frontend-apps.log" : "finished");
    return 1;
  }
  if (rc == -1) {
    set_status(ui, "app system call failed");
    return 0;
  }
  if (entry->background && WIFEXITED(rc) && WEXITSTATUS(rc) == 3) {
    snprintf(ui->status, sizeof(ui->status), "%.80s already running", entry->display_name);
    return 0;
  }
  set_status(ui, "app returned non-zero; see frontend-apps.log");
  return 0;
}

static int bool_from_setting_value(const char *value) {
  return setting_value_is_true(value);
}

static int brightness_test_tiles_enabled(void) {
  const char *value = getenv("PLUMOS_CONTROLLER_BRIGHTNESS_TEST");

  return value && setting_value_is_true(value);
}

static void settings_start_arrow_blink(struct ui_state *ui, int direction) {
  if (!ui || direction == 0) {
    return;
  }
  ui->settings_blink_cursor = ui->settings_cursor;
  ui->settings_blink_direction = direction < 0 ? -1 : 1;
  ui->settings_blink_until_ms = current_time_ms() + 240;
}

static long clamp_long(long value, long min_value, long max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int update_settings_entries_after_save(struct ui_state *ui) {
  size_t cursor;

  if (!ui) {
    return 0;
  }
  cursor = ui->settings_cursor;
  if (!load_settings_entries(ui)) {
    set_status(ui, "settings saved; reload failed");
    return 0;
  }
  if (ui->setting_count == 0) {
    ui->settings_cursor = 0;
  } else if (cursor >= ui->setting_count) {
    ui->settings_cursor = ui->setting_count - 1;
  } else {
    ui->settings_cursor = cursor;
  }
  return 1;
}

static int refresh_top_entries_preserve_cursor(struct ui_state *ui) {
  char selected_id[64] = "";
  size_t i;

  if (!ui) {
    return 0;
  }
  if (ui->top_count > 0 && ui->top_cursor < ui->top_count) {
    copy_string(selected_id, sizeof(selected_id), ui->top_entries[ui->top_cursor].id);
  }
  if (!load_top_entries(ui)) {
    return 0;
  }
  if (!selected_id[0]) {
    return 1;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (strcmp(ui->top_entries[i].id, selected_id) == 0) {
      ui->top_cursor = i;
      return 1;
    }
  }
  return 1;
}

static void refresh_top_entries_manual(struct ui_state *ui) {
  enum ui_screen back_screen;
  enum settings_category back_category;
  size_t back_cursor;
  long long visible_until;
  int reload_ok;
  int scan_ok;

  if (!ui) {
    return;
  }
  back_screen = ui->screen;
  back_category = ui->settings_category;
  back_cursor = ui->settings_cursor;
  visible_until = current_time_ms() + UI_TOP_REFRESH_MIN_VISIBLE_MS;
  ui->screen = SCREEN_TOP_REFRESH_RUNNING;
  set_status(ui, "refreshing TOP");
  render_ui(ui);
  scan_ok = run_scanner(ui->plumos_root, ui->sdcard_root, NULL, 0);
  reload_ok = refresh_top_entries_preserve_cursor(ui);
  wait_until_ms(visible_until);
  ui->screen = back_screen;
  ui->settings_category = back_category;
  if (ui->setting_count > 0) {
    ui->settings_cursor = back_cursor < ui->setting_count ? back_cursor
                                                          : ui->setting_count - 1;
  } else {
    ui->settings_cursor = 0;
  }
  if (!reload_ok) {
    set_status(ui, scan_ok ? "TOP scan done; reload failed"
                           : "TOP refresh failed; using cached counts");
  } else {
    set_status(ui, scan_ok ? "TOP refreshed" : "TOP refresh failed; using cached counts");
  }
}

static int refresh_current_rom_entries_preserve_cursor(struct ui_state *ui) {
  char selected_relative_path[UI_PATH_MAX] = "";
  char system_id[64] = "";
  size_t i;

  if (!ui || !ui->current_system_id[0] ||
      strcmp(ui->current_system_id, "favorites") == 0 ||
      strcmp(ui->current_system_id, "recent") == 0) {
    return 0;
  }
  copy_string(system_id, sizeof(system_id), ui->current_system_id);
  if (ui->rom_count > 0 && ui->rom_cursor < ui->rom_count) {
    copy_string(selected_relative_path, sizeof(selected_relative_path),
                ui->rom_entries[ui->rom_cursor].relative_path);
  }
  if (!load_rom_entries(ui, system_id)) {
    return 0;
  }
  if (!selected_relative_path[0]) {
    reset_marquee(ui);
    return 1;
  }
  for (i = 0; i < ui->rom_count; i++) {
    if (strcmp(ui->rom_entries[i].relative_path, selected_relative_path) == 0) {
      ui->rom_cursor = i;
      break;
    }
  }
  reset_marquee(ui);
  return 1;
}

static int poll_rom_scan_refresh(struct ui_state *ui) {
  int status = 0;
  pid_t rc;
  char finished_system_id[64];
  int refresh_ok;

  if (!ui || ui->rom_scan_refresh_pid <= 0) {
    return 0;
  }
  rc = waitpid(ui->rom_scan_refresh_pid, &status, WNOHANG);
  if (rc == 0 || (rc < 0 && errno == EINTR)) {
    return 0;
  }
  copy_string(finished_system_id, sizeof(finished_system_id),
              ui->rom_scan_refresh_system_id);
  ui->rom_scan_refresh_pid = 0;
  if (rc < 0) {
    set_status(ui, "background ROM scan status lost");
    return 1;
  }
  if (!system_command_succeeded(status)) {
    if (ui->screen == SCREEN_ROMS &&
        strcmp(ui->current_system_id, finished_system_id) == 0) {
      set_status(ui, "background ROM scan failed");
      return 1;
    }
    return 0;
  }
  if (ui->screen != SCREEN_ROMS ||
      strcmp(ui->current_system_id, finished_system_id) != 0) {
    return 0;
  }

  ui->rom_scan_refresh_suppressed = 1;
  refresh_ok = refresh_current_rom_entries_preserve_cursor(ui);
  ui->rom_scan_refresh_suppressed = 0;
  set_status(ui, refresh_ok ? "ROM list refreshed" : "ROM scan done; reload failed");
  return 1;
}

static void refresh_runtime_after_setting_save(struct ui_state *ui, const char *id) {
  if (!ui || !id) {
    return;
  }
  if (strcmp(id, "show_empty_systems") == 0 ||
      strcmp(id, "show_favorites_on_top") == 0 ||
      strcmp(id, "show_recent_on_top") == 0 ||
      strcmp(id, "sort_systems") == 0) {
    refresh_top_entries_preserve_cursor(ui);
  }
  if (strcmp(id, "sort_roms") == 0) {
    refresh_current_rom_entries_preserve_cursor(ui);
  }
}

static int run_performance_text_ui_core_system(struct ui_state *ui, const char *extra_args) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui || !performance_ensure_system(ui)) {
    set_status(ui, "no performance system selected");
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-performance.log")) {
    set_status(ui, "performance command path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->performance_system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, extra_args ? extra_args : "") ||
      !append_string(cmd, sizeof(cmd), &pos, " >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "performance command too long");
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  if (rc == -1) {
    set_status(ui, "performance command failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 1;
  }
  set_status(ui, "performance command returned non-zero");
  return 0;
}

static int save_performance_system_choice(struct ui_state *ui, int direction) {
  char selected[128];

  if (!ui || direction == 0) {
    return 0;
  }
  if (!performance_cycle_system(ui, direction)) {
    set_status(ui, "no performance system available");
    return 0;
  }
  copy_string(selected, sizeof(selected),
              ui->performance_system_name[0] ? ui->performance_system_name
                                             : ui->performance_system_id);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "selected system=%s", selected);
  return 1;
}

static int save_performance_cpu_policy_choice(struct ui_state *ui, int direction) {
  const struct performance_cpu_preset *preset;
  char extra[128];
  int index;

  if (!ui || direction == 0) {
    return 0;
  }
  load_performance_core_state(ui);
  index = performance_cpu_preset_index(ui->performance_cpu_policy,
                                       ui->performance_cpu_freq_khz);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)PERFORMANCE_CPU_PRESET_COUNT - 1;
  } else if ((size_t)index >= PERFORMANCE_CPU_PRESET_COUNT) {
    index = 0;
  }
  preset = &PERFORMANCE_CPU_PRESETS[index];
  snprintf(extra, sizeof(extra), " --cpu %s", preset->policy);
  if (!run_performance_text_ui_core_system(ui, extra)) {
    return 0;
  }
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved CPU governor=%s", preset->label);
  return 1;
}

static int save_performance_setting_choice(struct ui_state *ui, const char *id,
                                           int direction) {
  if (!id) {
    return 0;
  }
  if (strcmp(id, "performance_system") == 0) {
    return save_performance_system_choice(ui, direction);
  }
  if (strcmp(id, "performance_cpu_policy") == 0) {
    return save_performance_cpu_policy_choice(ui, direction);
  }
  return 0;
}

static int clear_performance_cpu_override(struct ui_state *ui) {
  if (!run_performance_text_ui_core_system(ui, " --clear-cpu")) {
    return 0;
  }
  update_settings_entries_after_save(ui);
  set_status(ui, "reset CPU defaults");
  return 1;
}

static int save_setting_bool(struct ui_state *ui, const char *id, int value) {
  struct frontend_settings settings;

  if (strcmp(id, "system_lid_suspend") == 0) {
    if (!save_system_config_bool(ui, "lid_suspend_enabled", value ? 1 : 0)) {
      set_status(ui, "lid suspend setting write failed");
      return 0;
    }
    ui->device.lid_suspend_enabled = value ? 1 : 0;
    update_settings_entries_after_save(ui);
    set_status(ui, value ? "lid suspend enabled" : "lid suspend disabled");
    return 1;
  }

  if (strcmp(id, "system_automatic_time") == 0) {
    int sync_ok = 1;
    if (!save_system_config_bool(ui, "automatic_time", value ? 1 : 0)) {
      set_status(ui, "automatic time setting write failed");
      return 0;
    }
    ui->device.automatic_time_enabled = value ? 1 : 0;
    if (value) {
      sync_ok = run_time_sync_helper(ui, "force-sync");
    }
    update_settings_entries_after_save(ui);
    if (value && !sync_ok) {
      set_status(ui, "automatic time ON; initial sync failed");
      return 1;
    }
    set_status(ui, value ? "automatic time ON; system and RTC synchronized"
                         : "automatic time OFF; manual clock preserved");
    return 1;
  }

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "show_empty_systems") == 0) {
    settings.show_empty_systems = value ? 1 : 0;
  } else if (strcmp(id, "show_favorites_on_top") == 0) {
    settings.show_favorites_on_top = value ? 1 : 0;
  } else if (strcmp(id, "show_recent_on_top") == 0) {
    settings.show_recent_on_top = value ? 1 : 0;
  } else if (strcmp(id, "rom_cursor_wrap") == 0) {
    settings.rom_cursor_wrap = value ? 1 : 0;
  } else if (strcmp(id, "rom_scan_policy") == 0) {
    copy_string(settings.rom_scan_policy, sizeof(settings.rom_scan_policy),
                value ? "on_enter" : "manual");
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, value ? "true" : "false");
  return 1;
}

static int choice_index_from_value(const struct setting_choice *choices, size_t count,
                                   const char *value) {
  size_t i;

  for (i = 0; i < count; i++) {
    if (strcmp(value, choices[i].raw) == 0 || strcmp(value, choices[i].display) == 0) {
      return (int)i;
    }
  }
  return 0;
}

static int graphic_theme_choice_index(const struct graphic_theme_choice *choices,
                                      size_t count, const char *raw_value,
                                      const char *display_value) {
  size_t i;

  for (i = 0; i < count; i++) {
    if ((raw_value && strcmp(choices[i].raw, raw_value) == 0) ||
        (display_value && strcmp(choices[i].display, display_value) == 0) ||
        (display_value && strcmp(choices[i].raw, display_value) == 0)) {
      return (int)i;
    }
  }
  return 0;
}

static int save_graphic_theme_choice(struct ui_state *ui,
                                     const char *display_value, int direction) {
  struct frontend_settings settings;
  struct graphic_theme_choice choices[UI_GRAPHIC_THEME_CHOICE_MAX];
  size_t count;
  int index;

  if (direction == 0) {
    set_status(ui, "setting needs LEFT/RIGHT");
    return 0;
  }
  count = load_graphic_theme_choices(ui, choices,
                                     sizeof(choices) / sizeof(choices[0]));
  if (count == 0) {
    set_status(ui, "no Graphic themes found");
    return 0;
  }
  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  index = graphic_theme_choice_index(choices, count, settings.graphic_theme_id,
                                     display_value);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)count - 1;
  } else if ((size_t)index >= count) {
    index = 0;
  }
  copy_string(settings.graphic_theme_id, sizeof(settings.graphic_theme_id),
              choices[index].raw);
  copy_string(settings.theme_id, sizeof(settings.theme_id), choices[index].raw);
  settings.graphic_top_layout[0] = '\0';
  settings.graphic_transition[0] = '\0';
  settings.graphic_transition_axis[0] = '\0';
  settings.graphic_transition_easing[0] = '\0';
  settings.graphic_transition_ms = 0;
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  ui->frontend_settings = settings;
  load_theme_state(ui, settings.graphic_theme_id);
  apply_theme_setting_overrides(&ui->theme, &settings);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved Graphic Theme=%s",
           choices[index].display);
  return 1;
}

static int save_setting_choice(struct ui_state *ui, const char *id,
                               const char *display_value, int direction) {
  struct frontend_settings settings;
  const struct setting_choice *choices;
  size_t count = 0;
  int index;
  const char *raw;

  if (strcmp(id, "graphic_theme_id") == 0) {
    return save_graphic_theme_choice(ui, display_value, direction);
  }

  choices = setting_choices(id, &count);
  if (!choices || count == 0 || direction == 0) {
    set_status(ui, "setting is not a choice");
    return 0;
  }
  index = choice_index_from_value(choices, count, display_value);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)count - 1;
  } else if ((size_t)index >= count) {
    index = 0;
  }
  raw = choices[index].raw;

  if (strcmp(id, "system_language") == 0) {
    if (!save_system_config_string(ui, "language", raw)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    copy_string(ui->device.language, sizeof(ui->device.language), raw);
    load_translations(ui);
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, choices[index].display);
    return 1;
  }

  if (strcmp(id, "system_timezone") == 0) {
    char runtime_status[128];
    if (!save_system_config_string(ui, "timezone", raw)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    copy_string(ui->device.timezone, sizeof(ui->device.timezone), raw);
    apply_system_timezone_runtime(ui, raw, runtime_status, sizeof(runtime_status));
    ui->manual_time_initialized = 0;
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    snprintf(ui->status, sizeof(ui->status), "saved Timezone=%s; %s",
             choices[index].display, runtime_status);
    return 1;
  }

  if (strcmp(id, "system_audio_output") == 0) {
    if (!save_system_config_string(ui, "audio_output", raw)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    copy_string(ui->device.audio_output, sizeof(ui->device.audio_output), raw);
    if (!run_volume_control_command("apply", ui->device.volume, 1)) {
      set_status(ui, "audio output saved; runtime apply failed");
      return 1;
    }
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    snprintf(ui->status, sizeof(ui->status), "saved Audio Output=%s",
             choices[index].display);
    return 1;
  }

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "ui_mode") == 0) {
    copy_string(settings.ui_mode, sizeof(settings.ui_mode), raw);
    copy_string(settings.top_mode, sizeof(settings.top_mode), raw);
    copy_string(settings.rom_mode, sizeof(settings.rom_mode), raw);
  } else if (strcmp(id, "boot_resume_mode") == 0) {
    copy_string(settings.boot_resume_mode, sizeof(settings.boot_resume_mode), raw);
  } else if (strcmp(id, "theme_top_layout") == 0) {
    copy_string(settings.graphic_top_layout, sizeof(settings.graphic_top_layout), raw);
  } else if (strcmp(id, "theme_transition") == 0) {
    copy_string(settings.graphic_transition, sizeof(settings.graphic_transition), raw);
  } else if (strcmp(id, "theme_transition_axis") == 0) {
    copy_string(settings.graphic_transition_axis,
                sizeof(settings.graphic_transition_axis), raw);
  } else if (strcmp(id, "theme_transition_easing") == 0) {
    copy_string(settings.graphic_transition_easing,
                sizeof(settings.graphic_transition_easing), raw);
  } else if (strcmp(id, "sort_systems") == 0) {
    copy_string(settings.sort_systems, sizeof(settings.sort_systems), raw);
  } else if (strcmp(id, "sort_roms") == 0) {
    copy_string(settings.sort_roms, sizeof(settings.sort_roms), raw);
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, choices[index].display);
  return 1;
}

static int save_manual_time_number(struct ui_state *ui, const char *id,
                                   int direction) {
  long *target = NULL;

  if (!ui || !id || direction == 0) {
    return 0;
  }
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  if (strcmp(id, "system_manual_time_year") == 0) {
    target = &ui->manual_time_year;
  } else if (strcmp(id, "system_manual_time_month") == 0) {
    target = &ui->manual_time_month;
  } else if (strcmp(id, "system_manual_time_day") == 0) {
    target = &ui->manual_time_day;
  } else if (strcmp(id, "system_manual_time_hour") == 0) {
    target = &ui->manual_time_hour;
  } else if (strcmp(id, "system_manual_time_minute") == 0) {
    target = &ui->manual_time_minute;
  }
  if (!target) {
    return 0;
  }
  *target += direction > 0 ? 1 : -1;
  clamp_manual_time_fields(ui);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  set_status(ui, "manual time draft updated");
  return 1;
}

static int save_setting_number(struct ui_state *ui, const char *id,
                               const char *display_value, int direction) {
  struct frontend_settings settings;
  long value;
  long step = 1;
  long min_value = 0;
  long max_value = 9999;
  const char *system_key = NULL;
  char runtime_status[128];

  if (direction == 0) {
    set_status(ui, "setting needs LEFT/RIGHT");
    return 0;
  }
  if (strncmp(id, "system_manual_time_", 19) == 0 &&
      strcmp(id, "system_manual_time_apply") != 0) {
    return save_manual_time_number(ui, id, direction);
  }
  if (system_number_setting_needs_runtime_backend(id) &&
      !system_number_setting_runtime_available(id)) {
    set_status(ui, "runtime backend unavailable");
    return 0;
  }
  value = strtol(display_value ? display_value : "0", NULL, 10);
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    step = 100;
    min_value = 100;
    max_value = 5000;
  } else if (strcmp(id, "rom_scan_test_file_count") == 0) {
    step = 100;
    min_value = 100;
    max_value = 5000;
  } else if (strcmp(id, "system_volume") == 0) {
    system_key = "volume";
    min_value = 0;
    max_value = PLUMOS_VOLUME_MAX;
  } else if (strcmp(id, "system_brightness") == 0) {
    system_key = "brightness";
    min_value = 1;
    max_value = runtime_device_uses_legacy_sunxi() ? 6 : 20;
  } else if (strcmp(id, "system_lumination") == 0) {
    system_key = "lumination";
    min_value = 0;
    max_value = 10;
  } else if (strcmp(id, "system_contrast") == 0) {
    system_key = "contrast";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "system_hue") == 0) {
    system_key = "hue";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "system_saturation") == 0) {
    system_key = "saturation";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    step = 20;
    min_value = 80;
    max_value = 1000;
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  value = clamp_long(value + (direction > 0 ? step : -step), min_value, max_value);
  if (system_key) {
    if (!save_system_config_number(ui, system_key, value)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    set_device_setting_number(&ui->device, id, value);
    apply_device_runtime_settings(&ui->device, id, runtime_status,
                                  sizeof(runtime_status));
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    if (runtime_status[0]) {
      snprintf(ui->status, sizeof(ui->status), "saved %s=%ld; %s", id, value,
               runtime_status);
    } else {
      snprintf(ui->status, sizeof(ui->status), "saved %s=%ld; runtime applied",
               id, value);
    }
    return 1;
  }

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    settings.rom_scan_slow_threshold_ms = value;
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    settings.graphic_transition_ms = value;
  } else {
    settings.rom_scan_test_file_count = value;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%ld", id, value);
  return 1;
}

static int change_system_volume(struct ui_state *ui, int direction) {
  long value;
  char runtime_status[128];

  if (!ui || direction == 0) {
    return 0;
  }
  value = clamp_long(ui->device.volume + (direction > 0 ? 1 : -1), 0,
                     PLUMOS_VOLUME_MAX);
  if (value == ui->device.volume) {
    if (!apply_device_runtime_settings(&ui->device, "system_volume",
                                       runtime_status,
                                       sizeof(runtime_status))) {
      snprintf(ui->status, sizeof(ui->status), "Volume %ld/%d; apply failed",
               value, PLUMOS_VOLUME_MAX);
    } else {
      snprintf(ui->status, sizeof(ui->status), "Volume %ld/%d", value,
               PLUMOS_VOLUME_MAX);
    }
    return 1;
  }
  if (!save_system_config_number(ui, "volume", value)) {
    set_status(ui, "volume write failed");
    return 0;
  }
  ui->device.volume = value;
  if (!apply_device_runtime_settings(&ui->device, "system_volume", runtime_status,
                                     sizeof(runtime_status))) {
    snprintf(ui->status, sizeof(ui->status), "Volume %ld/%d; apply failed",
             value, PLUMOS_VOLUME_MAX);
  } else {
    snprintf(ui->status, sizeof(ui->status), "Volume %ld/%d", value,
             PLUMOS_VOLUME_MAX);
  }
  update_settings_entries_after_save(ui);
  return 1;
}

static int save_brightness_test_value(struct ui_state *ui, long value) {
  char raw[64];
  long pixel2_compat_duty;

  if (!ui) {
    return 0;
  }
  value = clamp_long(value, 1, 255);
  if (runtime_pixel2_compat_lcd_backend_available()) {
    pixel2_compat_duty = value;
    snprintf(raw, sizeof(raw), "%ld\n", pixel2_compat_duty);
    if (!write_text_file(Pixel2_PWM_DUTY_PATH, raw)) {
      set_status(ui, "brightness test PWM write failed");
      return 0;
    }
    (void)write_text_file(Pixel2_PWM_ENABLE_PATH, "1\n");
    snprintf(ui->status, sizeof(ui->status),
             "test Pixel2 pwm duty=%ld; not saved", pixel2_compat_duty);
    return 1;
  }
  if (!runtime_lcd_backend_available()) {
    set_status(ui, "brightness test backend unavailable");
    return 0;
  }
  snprintf(raw, sizeof(raw), "%ld\n", value);
  if (!write_text_file(Pixel2_LCD_BACKLIGHT_PATH, raw)) {
    set_status(ui, "brightness test write failed");
    return 0;
  }
  snprintf(ui->status, sizeof(ui->status), "test lcdbl raw=%ld; not saved", value);
  return 1;
}

static int handle_setting_control(struct ui_state *ui, enum ui_action action) {
  const struct setting_entry *entry;
  enum setting_control_type control;
  char id[64];
  char value[256];
  int direction = 0;

  if (!ui || ui->setting_count == 0 || ui->settings_cursor >= ui->setting_count) {
    return 0;
  }
  entry = &ui->setting_entries[ui->settings_cursor];
  copy_string(id, sizeof(id), entry->id);
  copy_string(value, sizeof(value), entry->value);
  control = setting_control_type_for_id(id);
  if (action == ACTION_LEFT) {
    direction = -1;
  } else if (action == ACTION_RIGHT) {
    direction = 1;
  }

  if (control == SETTING_CONTROL_CHECKBOX) {
    int current = bool_from_setting_value(value);
    int next;
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    if (action == ACTION_A) {
      next = !current;
    } else {
      return 0;
    }
    if (strcmp(id, "network_wifi_enabled") == 0) {
      return run_network_wifi_control(ui, next);
    }
    if (strcmp(id, "network_ssh_enabled") == 0) {
      return run_network_service_control(ui, "ssh", next);
    }
    if (strcmp(id, "network_ftp_enabled") == 0) {
      return run_network_service_control(ui, "ftp", next);
    }
    if (strcmp(id, "network_sftp_enabled") == 0) {
      return run_network_service_control(ui, "sftp", next);
    }
    if (strcmp(id, "network_samba_enabled") == 0) {
      return run_network_service_control(ui, "samba", next);
    }
    if (strcmp(id, "network_adb_enabled") == 0) {
      return run_network_service_control(ui, "adb", next);
    }
    save_setting_bool(ui, id, next);
    return 1;
  }
  if (control == SETTING_CONTROL_CHOICE && direction != 0) {
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    if (strncmp(id, "performance_", 12) == 0) {
      save_performance_setting_choice(ui, id, direction);
      return 1;
    }
    save_setting_choice(ui, id, value, direction);
    return 1;
  }
  if (control == SETTING_CONTROL_NUMBER && direction != 0) {
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    save_setting_number(ui, id, value, direction);
    return 1;
  }
  if ((action == ACTION_LEFT || action == ACTION_RIGHT) && control != SETTING_CONTROL_ACTION) {
    set_status(ui, "setting is read-only");
    return 1;
  }
  return 0;
}

static int is_network_setting_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_rescue") == 0;
}

static int is_ui_theme_settings_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "ui_theme_settings") == 0;
}

static int is_refresh_top_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "refresh_top") == 0;
}

static int is_network_connect_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_connect_wifi") == 0;
}

static int is_network_services_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_services") == 0;
}

static int is_network_information_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_information") == 0;
}

static int is_system_display_color_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_display_color") == 0;
}

static int is_system_brightness_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_brightness") == 0;
}

static int is_system_time_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_time_settings") == 0;
}

static int is_system_storage_check_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_storage_check") == 0;
}

static int is_system_manual_time_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_manual_time") == 0;
}

static int is_system_information_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_information") == 0;
}

static int is_system_update_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_update") == 0;
}

static int is_system_factory_reset_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_factory_reset") == 0;
}

static const char *factory_reset_target_for_entry(const struct setting_entry *entry) {
  if (!entry) {
    return NULL;
  }
  if (strcmp(entry->id, "system_factory_reset_all") == 0) {
    return "all";
  }
  if (strcmp(entry->id, "system_factory_reset_ra") == 0) {
    return "ra";
  }
  if (strcmp(entry->id, "system_factory_reset_pico") == 0) {
    return "pico";
  }
  if (strcmp(entry->id, "system_factory_reset_sa") == 0) {
    return "sa";
  }
  return NULL;
}

static void clear_factory_reset_pending(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->factory_reset_pending_target[0] = '\0';
  ui->factory_reset_pending_until_ms = 0;
}

static int run_factory_reset_action(struct ui_state *ui, const char *target,
                                    const char *label) {
  char script[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui || !target || !target[0]) {
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root,
                 "bin/plumos-factory-reset")) {
    set_status(ui, "Factory Reset path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "Factory Reset helper missing");
    return 0;
  }
  if (!join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir,
                 "plumos-factory-reset.log")) {
    set_status(ui, "Factory Reset log path too long");
    return 0;
  }

  snprintf(ui->status, sizeof(ui->status), "resetting %.96s", label);
  render_ui(ui);

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " && cd / && export PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, "; exec ") ||
      !append_runtime_script_invocation(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, target) ||
      !append_string(cmd, sizeof(cmd), &pos, " >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "Factory Reset command too long");
    return 0;
  }

  rc = run_runtime_shell_command(cmd);
  if (system_command_succeeded(rc)) {
    snprintf(ui->status, sizeof(ui->status), "Factory Reset restored %.80s",
             label);
    return 1;
  }
  if (rc == -1) {
    set_status(ui, "Factory Reset system call failed");
    return 0;
  }
  set_status(ui, "Factory Reset failed; see log");
  return 0;
}

static int confirm_or_run_factory_reset(struct ui_state *ui,
                                        const struct setting_entry *entry) {
  const char *target;
  long long now;

  if (!ui || !entry) {
    return 0;
  }
  target = factory_reset_target_for_entry(entry);
  if (!target) {
    return 0;
  }
  now = current_time_ms();
  if (strcmp(ui->factory_reset_pending_target, target) == 0 &&
      ui->factory_reset_pending_until_ms >= now) {
    clear_factory_reset_pending(ui);
    return run_factory_reset_action(ui, target, entry->display_name);
  }
  copy_string(ui->factory_reset_pending_target,
              sizeof(ui->factory_reset_pending_target), target);
  ui->factory_reset_pending_until_ms = now + 5000;
  snprintf(ui->status, sizeof(ui->status), "Press A again to reset %.96s",
           entry->display_name);
  return 1;
}

static int is_help_setting_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "help") == 0;
}

static int handle_brightness_test_action(struct ui_state *ui, enum ui_action action) {
  size_t cursor;

  if (!ui || ui->settings_category != SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST) {
    return 0;
  }
  if (action == ACTION_B) {
    open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
    select_setting_entry_by_id(ui, "system_brightness");
    set_status(ui, "back to System Settings");
    return 1;
  }
  if (ui->setting_count == 0) {
    return 1;
  }
  cursor = ui->settings_cursor;
  if (cursor >= ui->setting_count) {
    cursor = ui->setting_count - 1;
  }
  if (action == ACTION_LEFT) {
    if (cursor > 0) {
      cursor--;
    }
  } else if (action == ACTION_RIGHT) {
    if (cursor + 1 < ui->setting_count) {
      cursor++;
    }
  } else if (action == ACTION_UP) {
    if (cursor >= BRIGHTNESS_TEST_COLUMNS) {
      cursor -= BRIGHTNESS_TEST_COLUMNS;
    }
  } else if (action == ACTION_DOWN) {
    if (cursor + BRIGHTNESS_TEST_COLUMNS < ui->setting_count) {
      cursor += BRIGHTNESS_TEST_COLUMNS;
    }
  } else if (action == ACTION_A) {
    if (cursor < BRIGHTNESS_TEST_COUNT) {
      save_brightness_test_value(ui, BRIGHTNESS_TEST_VALUES[cursor]);
    }
    return 1;
  } else {
    return 0;
  }
  ui->settings_cursor = cursor;
  return 1;
}

static int handle_wifi_password_key(struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  wifi_clamp_key_cursor(ui);
  if (ui->wifi_key_row == UI_WIFI_COMMAND_ROW) {
    switch (ui->wifi_key_col) {
    case 0:
      ui->wifi_key_shift = !ui->wifi_key_shift;
      set_status(ui, ui->wifi_key_shift ? "Keyboard uppercase" : "Keyboard lowercase");
      return 1;
    case 1:
      wifi_append_password_char(ui, ' ');
      return 1;
    case 2:
      wifi_delete_password_char(ui);
      return 1;
    case 3:
      ui->wifi_password[0] = '\0';
      set_status(ui, "Password cleared");
      return 1;
    case 4:
      run_wifi_connect_selected(ui);
      return 1;
    default:
      return 1;
    }
  }
  if (ui->wifi_key_row < UI_WIFI_KEYBOARD_ROWS &&
      ui->wifi_key_col < strlen(wifi_keyboard_row_chars(ui, ui->wifi_key_row))) {
    wifi_append_password_char(
        ui, wifi_keyboard_row_chars(ui, ui->wifi_key_row)[ui->wifi_key_col]);
    return 1;
  }
  return 1;
}

static int handle_wifi_connect_action(struct ui_state *ui, enum ui_action action) {
  if (!ui || ui->screen != SCREEN_WIFI_CONNECT) {
    return 0;
  }
  if (ui->wifi_stage == WIFI_CONNECT_RESULT) {
    if (action == ACTION_A || action == ACTION_B) {
      wifi_back_to_network_settings(ui, ui->wifi_result_success
                                            ? "Wi-Fi connection complete"
                                            : "back to Network Settings");
      return 1;
    }
    return 1;
  }
  if (ui->wifi_stage == WIFI_CONNECT_SELECT) {
    if (action == ACTION_UP) {
      if (ui->wifi_cursor > 0) {
        ui->wifi_cursor--;
      }
      return 1;
    }
    if (action == ACTION_DOWN) {
      if (ui->wifi_cursor + 1 < ui->wifi_count) {
        ui->wifi_cursor++;
      }
      return 1;
    }
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->wifi_cursor, ui->wifi_count, ui_list_window_size(ui));
      return 1;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->wifi_cursor, ui_list_window_size(ui));
      return 1;
    }
    if (action == ACTION_SELECT) {
      set_status(ui, "Rescanning Wi-Fi SSIDs");
      render_ui(ui);
      wifi_scan_networks(ui);
      return 1;
    }
    if (action == ACTION_B) {
      wifi_back_to_network_settings(ui, "back to Network Settings");
      return 1;
    }
    if (action == ACTION_A) {
      if (ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
        set_status(ui, "No SSID selected");
        return 1;
      }
      ui->wifi_password[0] = '\0';
      if (wifi_selected_network_is_open(ui)) {
        run_wifi_connect_selected(ui);
        return 1;
      }
      ui->wifi_stage = WIFI_CONNECT_PASSWORD;
      ui->wifi_key_row = 0;
      ui->wifi_key_col = 0;
      ui->wifi_key_shift = 0;
      set_status(ui, "Enter Wi-Fi password");
      return 1;
    }
    return 1;
  }
  if (ui->wifi_stage == WIFI_CONNECT_PASSWORD) {
    if (action == ACTION_B) {
      ui->wifi_stage = WIFI_CONNECT_SELECT;
      set_status(ui, "back to SSID list");
      return 1;
    }
    if (action == ACTION_UP) {
      if (ui->wifi_key_row > 0) {
        ui->wifi_key_row--;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_DOWN) {
      if (ui->wifi_key_row + 1 < UI_WIFI_KEYBOARD_ROWS) {
        ui->wifi_key_row++;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_LEFT) {
      if (ui->wifi_key_col > 0) {
        ui->wifi_key_col--;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_RIGHT) {
      size_t row_len = wifi_keyboard_row_len(ui, ui->wifi_key_row);
      if (ui->wifi_key_col + 1 < row_len) {
        ui->wifi_key_col++;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_A) {
      handle_wifi_password_key(ui);
      return 1;
    }
    return 1;
  }
  return 1;
}

static size_t rom_cursor_after_delta(const struct ui_state *ui,
                                     size_t base_cursor, long delta) {
  size_t next_cursor;

  if (!ui || ui->rom_count == 0) {
    return 0;
  }
  if (base_cursor >= ui->rom_count) {
    base_cursor = ui->rom_count - 1;
  }
  if (ui->frontend_settings.rom_cursor_wrap && ui->rom_count > 1) {
    long long count = (long long)ui->rom_count;
    long long next = (long long)base_cursor + (long long)delta;
    next %= count;
    if (next < 0) {
      next += count;
    }
    return (size_t)next;
  }
  next_cursor = base_cursor;
  if (delta < 0) {
    size_t step = (size_t)(-delta);
    next_cursor = base_cursor > step ? base_cursor - step : 0;
  } else {
    size_t step = (size_t)delta;
    next_cursor = base_cursor + step;
    if (next_cursor >= ui->rom_count) {
      next_cursor = ui->rom_count - 1;
    }
  }
  return next_cursor;
}

static void move_gallery_cursor(struct ui_state *ui, long delta) {
  size_t old_cursor;
  size_t base_cursor;
  size_t next_cursor;

  if (!ui || ui->rom_count == 0 || delta == 0) {
    return;
  }
  if (delta < -1 || delta > 1) {
    next_cursor = rom_cursor_after_delta(ui, ui->rom_cursor, delta);
    if (next_cursor != ui->rom_cursor) {
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      ui->gallery_pending_direction = 0;
      ui->rom_cursor = next_cursor;
      remember_current_rom_cursor(ui);
      reset_marquee(ui);
    }
    return;
  }
  if (ui->gallery_transition_active) {
    base_cursor = ui->gallery_transition_to_cursor;
    next_cursor = rom_cursor_after_delta(ui, base_cursor, delta);
    if (next_cursor != base_cursor) {
      ui->gallery_pending_cursor = next_cursor;
      ui->gallery_pending_direction = delta < 0 ? -1 : 1;
      ui->gallery_pending_active = 1;
    }
    return;
  }
  old_cursor = ui->rom_cursor;
  next_cursor = rom_cursor_after_delta(ui, old_cursor, delta);
  if (next_cursor != old_cursor) {
    ui_start_gallery_transition(ui, old_cursor, next_cursor,
                                delta < 0 ? -1 : 1);
    ui->rom_cursor = next_cursor;
    remember_current_rom_cursor(ui);
    reset_marquee(ui);
  }
}

static void handle_action(struct ui_state *ui, enum ui_action action) {
  if (action == ACTION_NONE) {
    return;
  }
  if (ui->screen == SCREEN_POWER_ACTION_RUNNING) {
    return;
  }
  if (action == ACTION_VOLUME_DOWN || action == ACTION_VOLUME_UP) {
    change_system_volume(ui, action == ACTION_VOLUME_UP ? 1 : -1);
    return;
  }
  if (action == ACTION_FUNCTION) {
    (void)capture_frontend_screenshot(ui);
    return;
  }
  if (action == ACTION_QUIT) {
    set_status(ui, "quit");
    ui->exit_requested = 1;
    return;
  }
  if (ui->rescue_network) {
    if (action == ACTION_A) {
      set_status(ui, "Network Recovery is disabled");
    } else if (action == ACTION_B) {
      set_status(ui, "Network Recovery is disabled");
    }
    return;
  }
  if (action == ACTION_POWER) {
    if (ui->screen != SCREEN_POWER_MENU) {
      open_power_menu(ui);
    }
    return;
  }

  if (ui->screen == SCREEN_POWER_MENU) {
    if (action == ACTION_UP) {
      if (ui->power_cursor > 0) {
        ui->power_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->power_cursor + 1 < POWER_ENTRY_COUNT) {
        ui->power_cursor++;
      }
      return;
    }
    if (action == ACTION_B) {
      if (ui->power_overlay) {
        write_power_overlay_selection(ui, "cancel");
      }
      close_power_menu(ui, "power menu cancelled");
      return;
    }
    if (action == ACTION_A && ui->power_cursor < POWER_ENTRY_COUNT) {
      const struct power_entry *entry = &POWER_ENTRIES[ui->power_cursor];
      if (strcmp(entry->id, "cancel") == 0) {
        if (ui->power_overlay) {
          write_power_overlay_selection(ui, "cancel");
        }
        close_power_menu(ui, "power menu cancelled");
        return;
      }
      if (ui->power_overlay) {
        if (write_power_overlay_selection(ui, "handled")) {
          (void)run_power_action(ui, entry->id,
                                 strcmp(entry->id, "shutdown") == 0);
          ui->exit_requested = 1;
        }
        return;
      }
      if (run_power_action(ui, entry->id, strcmp(entry->id, "shutdown") == 0) &&
          strcmp(entry->id, "sleep") == 0) {
        ui->screen = ui->power_back_screen;
      }
      if (ui->power_overlay) {
        ui->exit_requested = 1;
      }
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_CORE_SELECT) {
    if (action == ACTION_B) {
      ui->screen = ui->core_back_screen;
      set_status(ui, "");
      return;
    }
    if (action == ACTION_UP) {
      move_core_menu_cursor(ui, -1);
      return;
    }
    if (action == ACTION_DOWN) {
      move_core_menu_cursor(ui, 1);
      return;
    }
    if (action == ACTION_LEFT) {
      cycle_core_menu_current_row(ui, -1);
      return;
    }
    if (action == ACTION_RIGHT) {
      cycle_core_menu_current_row(ui, 1);
      return;
    }
    if (action == ACTION_A) {
      core_menu_clamp_cursor(ui);
      if (ui->core_menu_cursor == CORE_MENU_ROW_DEFAULT) {
        run_core_clear_profile_override(ui);
      } else {
        set_status(ui, "use LEFT/RIGHT to change values");
      }
      return;
    }
    if (action == ACTION_SELECT) {
      if (!load_core_select_lines(ui, ui->core_target_system_id,
                                  ui->core_target_relative_path[0]
                                      ? ui->core_target_relative_path
                                      : NULL)) {
        set_status(ui, "cannot refresh core selection");
      } else {
        set_status(ui, "core selection refreshed");
      }
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_SCRAPING) {
    clamp_scraping_menu_cursor(ui);
    if (action == ACTION_UP) {
      ui->scraping_menu_cursor =
          ui->scraping_menu_cursor == 0 ? UI_SCRAPING_FIELD_COUNT - 1
                                        : ui->scraping_menu_cursor - 1;
      set_status(ui, tr(ui, "scraping.status.selected_item",
                        "selected scraping item"));
      return;
    }
    if (action == ACTION_DOWN) {
      ui->scraping_menu_cursor =
          ui->scraping_menu_cursor + 1 >= UI_SCRAPING_FIELD_COUNT
              ? 0
              : ui->scraping_menu_cursor + 1;
      set_status(ui, tr(ui, "scraping.status.selected_item",
                        "selected scraping item"));
      return;
    }
    if (action == ACTION_LEFT) {
      if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE) {
        cycle_scraping_kind(ui, -1);
        set_status(ui, tr(ui, "scraping.status.selected_image",
                          "selected image type"));
      } else if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING) {
        ui->scraping_replace_existing = !ui->scraping_replace_existing;
        set_status(ui, tr(ui, "scraping.status.selected_existing",
                          "selected existing image mode"));
      } else {
        if (ui->scraping_choice_cursor == 0) {
          ui->scraping_choice_cursor = ui->scraping_choice_count;
        } else {
          ui->scraping_choice_cursor--;
        }
        set_status(ui, tr(ui, "scraping.status.selected_system",
                          "selected scraping system"));
      }
      return;
    }
    if (action == ACTION_RIGHT) {
      if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE) {
        cycle_scraping_kind(ui, 1);
        set_status(ui, tr(ui, "scraping.status.selected_image",
                          "selected image type"));
      } else if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING) {
        ui->scraping_replace_existing = !ui->scraping_replace_existing;
        set_status(ui, tr(ui, "scraping.status.selected_existing",
                          "selected existing image mode"));
      } else {
        if (ui->scraping_choice_cursor >= ui->scraping_choice_count) {
          ui->scraping_choice_cursor = 0;
        } else {
          ui->scraping_choice_cursor++;
        }
        set_status(ui, tr(ui, "scraping.status.selected_system",
                          "selected scraping system"));
      }
      return;
    }
    if (action == ACTION_A) {
      run_scraping_action(ui);
      return;
    }
    if (action == ACTION_SELECT) {
      open_thumbnail_results_screen(ui);
      return;
    }
    if (action == ACTION_B) {
      open_apps_menu(ui);
      select_menu_entry_by_id(ui, "scraping");
      set_status(ui, tr(ui, "scraping.status.back_to_apps", "back to Apps"));
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_THUMBNAIL_RESULTS) {
    if (action == ACTION_UP) {
      if (ui->thumbnail_result_cursor > 0) {
        ui->thumbnail_result_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->thumbnail_result_cursor + 1 < ui->thumbnail_result_count) {
        ui->thumbnail_result_cursor++;
      }
      return;
    }
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->thumbnail_result_cursor, ui->thumbnail_result_count,
                          ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->thumbnail_result_cursor, ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_B) {
      open_apps_menu(ui);
      select_menu_entry_by_id(ui, ui->thumbnail_result_return_app_id[0]
                                      ? ui->thumbnail_result_return_app_id
                                      : "scraping");
      return;
    }
    if (action == ACTION_A || action == ACTION_SELECT) {
      if (!load_thumbnail_results(ui)) {
        set_status(ui, tr(ui, "scraping.status.results_refresh_failed",
                          "cannot refresh scraping results"));
      } else {
        set_status(ui, tr(ui, "scraping.status.results_refreshed",
                          "scraping results refreshed"));
      }
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_NETWORK_RESCUE) {
    if (action == ACTION_A) {
      set_status(ui, "Network Recovery is disabled");
      return;
    }
    if (action == ACTION_B) {
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_WIFI_CONNECT) {
    handle_wifi_connect_action(ui, action);
    return;
  }

  if (ui->screen == SCREEN_GALLERY) {
    if (action == ACTION_LEFT) {
      move_gallery_cursor(ui, -1);
      return;
    }
    if (action == ACTION_RIGHT) {
      move_gallery_cursor(ui, 1);
      return;
    }
    if (action == ACTION_UP) {
      move_gallery_cursor(ui, -5);
      return;
    }
    if (action == ACTION_DOWN) {
      move_gallery_cursor(ui, 5);
      return;
    }
    if (action == ACTION_B) {
      if (ui->rom_directory[0] && open_parent_rom_directory(ui)) {
        return;
      }
      remember_current_rom_cursor(ui);
      ui->rom_entry_screen = SCREEN_GALLERY;
      ui->screen = SCREEN_TOP;
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      ui->gallery_pending_direction = 0;
      set_status(ui, "back to TOP");
      reset_marquee(ui);
      return;
    }
    if (action == ACTION_X) {
      ui->rom_entry_screen = SCREEN_ROMS;
      ui->screen = (ui->gallery_back_screen == SCREEN_FAVORITES ||
                    ui->gallery_back_screen == SCREEN_RECENT)
                       ? ui->gallery_back_screen
                       : SCREEN_ROMS;
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      ui->gallery_pending_direction = 0;
      set_status(ui, "back to ROM list");
      reset_marquee(ui);
      return;
    }
    if (action == ACTION_Y && ui->rom_count > 0) {
      toggle_current_favorite(ui);
      reset_marquee(ui);
      return;
    }
    if (action == ACTION_A && ui->rom_count > 0) {
      const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
      launch_rom_entry(ui, entry);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    if (action == ACTION_SELECT && ui->rom_count > 0) {
      const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
      if (entry->is_navigation_directory) {
        set_status(ui, "directory has no core menu");
        return;
      }
      open_core_select_screen(ui, entry->system_id[0] ? entry->system_id
                                                      : ui->current_system_id,
                              entry->relative_path);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_TOP) {
    if (ui_uses_graphic_mode(ui) &&
        (action == ACTION_UP || action == ACTION_DOWN ||
         action == ACTION_LEFT || action == ACTION_RIGHT)) {
      ui_move_graphic_top_cursor(ui, action);
      return;
    }
    if (action == ACTION_UP) {
      if (ui->top_cursor > 0) {
        ui->top_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->top_cursor + 1 < ui->top_count) {
        ui->top_cursor++;
      }
      return;
    }
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->top_cursor, ui->top_count, ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->top_cursor, ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_A && ui->top_count > 0) {
      const struct top_entry *entry = &ui->top_entries[ui->top_cursor];
      open_rom_screen(ui, entry);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    if (action == ACTION_SELECT && ui->top_count > 0) {
      const struct top_entry *entry = &ui->top_entries[ui->top_cursor];
      if (entry->virtual_entry || !valid_system_id(entry->id)) {
        set_status(ui, tr(ui, "core.status.virtual_top_unavailable",
                          "Core Settings are unavailable for Favorites and Recent."));
        return;
      }
      open_core_select_screen(ui, entry->id, NULL);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_START_MENU) {
    if (action == ACTION_UP) {
      if (ui->menu_cursor > 0) {
        ui->menu_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->menu_cursor + 1 < ui->menu_count) {
        ui->menu_cursor++;
      }
      return;
    }
    if (action == ACTION_B) {
      if (strcmp(ui->menu_id, "apps") == 0) {
        if (!load_start_menu_entries(ui)) {
          set_status(ui, tr(ui, "menu.status.start_load_failed",
                            "cannot load START menu"));
        } else {
          select_menu_entry_by_id(ui, "apps");
          set_status(ui, tr(ui, "menu.status.back_to_start", "back to START"));
        }
        return;
      }
      ui->screen = ui->back_screen;
      set_status(ui, "");
      return;
    }
    if (action == ACTION_A) {
      const struct menu_entry *entry;
      if (ui->menu_count == 0) {
        return;
      }
      entry = &ui->menu_entries[ui->menu_cursor];
      if (strcmp(entry->action, "internal:settings") == 0 ||
          strcmp(entry->action, "internal:ui-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI);
      } else if (strcmp(entry->action, "internal:system-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
      } else if (strcmp(entry->action, "internal:system-information") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_INFORMATION);
      } else if (strcmp(entry->action, "internal:network-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
      } else if (strcmp(entry->action, "internal:network-information") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_INFORMATION);
      } else if (strcmp(entry->action, "internal:performance-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_PERFORMANCE);
      } else if (strcmp(entry->action, "internal:favorites") == 0) {
        open_favorites_screen(ui);
      } else if (strcmp(entry->action, "internal:recent") == 0) {
        open_recent_screen(ui);
      } else if (strcmp(entry->action, "internal:music") == 0) {
        open_system_rom_screen_by_id(ui, "music", entry->display_name);
      } else if (strcmp(entry->action, "internal:network-recovery") == 0) {
        set_status(ui, "Network Recovery is disabled");
      } else if (strcmp(entry->action, "internal:network") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
      } else if (strcmp(entry->action, "internal:help") == 0) {
        open_help_screen(ui);
      } else if (strcmp(entry->action, "internal:scraping") == 0) {
        open_scraping_screen(ui);
      } else if (strcmp(entry->action, "internal:thumbnail-results") == 0) {
        open_thumbnail_results_screen(ui);
      } else if (strcmp(entry->action, "system:sleep") == 0) {
        run_power_action(ui, "sleep", 0);
      } else if (strcmp(entry->action, "system:reboot") == 0) {
        if (entry->confirm) {
          open_power_menu_for_action(ui, "reboot");
        } else {
          run_power_action(ui, "reboot", 0);
        }
      } else if (strcmp(entry->action, "system:shutdown") == 0) {
        if (entry->confirm) {
          open_power_menu_for_action(ui, "shutdown");
        } else {
          run_power_action(ui, "shutdown", 1);
        }
      } else if (strcmp(entry->action, "menu:apps") == 0) {
        open_apps_menu(ui);
      } else if (strncmp(entry->action, "shell:", 6) == 0) {
        run_menu_shell_action(ui, entry);
      } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "menu preview: %s action=%s", entry->display_name,
                 entry->action);
        set_status(ui, msg);
      }
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_SETTINGS) {
    if (handle_brightness_test_action(ui, action)) {
      return;
    }
    if (action == ACTION_UP) {
      if (ui->settings_cursor > 0) {
        ui->settings_cursor--;
        clear_factory_reset_pending(ui);
        ui->settings_blink_direction = 0;
        ui->settings_blink_until_ms = 0;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->settings_cursor + 1 < ui->setting_count) {
        ui->settings_cursor++;
        clear_factory_reset_pending(ui);
        ui->settings_blink_direction = 0;
        ui->settings_blink_until_ms = 0;
      }
      return;
    }
    if (action == ACTION_LEFT || action == ACTION_RIGHT) {
      clear_factory_reset_pending(ui);
      handle_setting_control(ui, action);
      return;
    }
    if (action == ACTION_B) {
      clear_factory_reset_pending(ui);
      if (ui->settings_category == SETTINGS_CATEGORY_UI_THEME) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI);
        select_setting_entry_by_id(ui, "ui_theme_settings");
        set_status(ui, "back to UI Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_display_color");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_TIME) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_time_settings");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME);
        select_setting_entry_by_id(ui, "system_manual_time");
        set_status(ui, "back to Time Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_INFORMATION) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_information");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_FACTORY_RESET) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_factory_reset");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_NETWORK_INFORMATION) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
        select_setting_entry_by_id(ui, "network_information");
        set_status(ui, "back to Network Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_NETWORK_SERVICE) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
        select_setting_entry_by_id(ui, "network_services");
        set_status(ui, "back to Network Settings");
        return;
      }
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_A && ui->setting_count > 0) {
      const struct setting_entry *entry = &ui->setting_entries[ui->settings_cursor];
      char msg[256];
      if (handle_setting_control(ui, action)) {
        return;
      }
      if (is_ui_theme_settings_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI_THEME);
        return;
      }
      if (is_refresh_top_entry(entry)) {
        refresh_top_entries_manual(ui);
        return;
      }
      if (is_network_connect_entry(entry)) {
        open_wifi_connect_screen(ui);
        return;
      }
      if (is_network_services_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
        return;
      }
      if (is_network_setting_entry(entry)) {
        set_status(ui, "Network Recovery is disabled");
        return;
      }
      if (is_network_information_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_INFORMATION);
        return;
      }
      if (is_system_display_color_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR);
        return;
      }
      if (is_system_time_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME);
        return;
      }
      if (is_system_storage_check_entry(entry)) {
        run_storage_health_check(ui);
        return;
      }
      if (strcmp(entry->id, "system_sync_now") == 0) {
        sync_time_now(ui);
        return;
      }
      if (is_system_manual_time_entry(entry)) {
        init_manual_time_from_current(ui);
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL);
        return;
      }
      if (strcmp(entry->id, "system_manual_time_apply") == 0) {
        apply_manual_system_time(ui);
        return;
      }
      if (is_system_brightness_entry(entry)) {
        if (!system_number_setting_runtime_available("system_brightness")) {
          set_status(ui, "Brightness backend unavailable");
        } else if (brightness_test_tiles_enabled()) {
          open_system_brightness_test_screen(ui);
        } else {
          set_status(ui, "Brightness changes with LEFT/RIGHT");
        }
        return;
      }
      if (is_system_information_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_INFORMATION);
        return;
      }
      if (is_system_update_entry(entry)) {
        request_latest_system_update(ui);
        return;
      }
      if (is_system_factory_reset_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_FACTORY_RESET);
        return;
      }
      if (confirm_or_run_factory_reset(ui, entry)) {
        return;
      }
      if (strcmp(entry->id, "performance_clear_cpu_override") == 0) {
        clear_performance_cpu_override(ui);
        return;
      }
      if (is_help_setting_entry(entry)) {
        open_help_screen(ui);
        return;
      }
      snprintf(msg, sizeof(msg), "read-only: %s", entry->display_name);
      set_status(ui, msg);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_HELP) {
    if (action == ACTION_B) {
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (action == ACTION_X &&
      (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
       ui->screen == SCREEN_RECENT)) {
    open_gallery_screen(ui);
    return;
  }

  if (action == ACTION_UP) {
    size_t old_cursor = ui->rom_cursor;
    ui->rom_cursor = rom_cursor_after_delta(ui, ui->rom_cursor, -1);
    if (ui->rom_cursor != old_cursor) {
      remember_current_rom_cursor(ui);
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_DOWN) {
    size_t old_cursor = ui->rom_cursor;
    ui->rom_cursor = rom_cursor_after_delta(ui, ui->rom_cursor, 1);
    if (ui->rom_cursor != old_cursor) {
      remember_current_rom_cursor(ui);
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_RIGHT) {
    size_t old_cursor = ui->rom_cursor;
    ui_cursor_page_down(&ui->rom_cursor, ui->rom_count, ui_list_window_size(ui));
    if (ui->rom_cursor != old_cursor) {
      remember_current_rom_cursor(ui);
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_LEFT) {
    size_t old_cursor = ui->rom_cursor;
    ui_cursor_page_up(&ui->rom_cursor, ui_list_window_size(ui));
    if (ui->rom_cursor != old_cursor) {
      remember_current_rom_cursor(ui);
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_B) {
    if (ui->screen == SCREEN_ROMS && ui->rom_directory[0] &&
        open_parent_rom_directory(ui)) {
      return;
    }
    remember_current_rom_cursor(ui);
    ui->rom_entry_screen = SCREEN_ROMS;
    ui->screen = SCREEN_TOP;
    set_status(ui, "back to TOP");
    return;
  }
  if (action == ACTION_Y && ui->rom_count > 0) {
    toggle_current_favorite(ui);
    reset_marquee(ui);
    return;
  }
  if (action == ACTION_A && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    launch_rom_entry(ui, entry);
    return;
  }
  if (action == ACTION_START) {
    open_start_menu(ui);
    return;
  }
  if (action == ACTION_SELECT && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    if (entry->is_navigation_directory) {
      set_status(ui, "directory has no core menu");
      return;
    }
    open_core_select_screen(ui, entry->system_id[0] ? entry->system_id : ui->current_system_id,
                            entry->relative_path);
    return;
  }
}

static enum ui_action action_from_key_code(unsigned int code) {
  const char *ab_layout = getenv("PLUMOS_INPUT_AB_LAYOUT");
  int east_is_confirm = ab_layout && strcmp(ab_layout, "east-confirm") == 0;

  switch (code) {
  case KEY_UP:
  case BTN_DPAD_UP:
    return ACTION_UP;
  case KEY_DOWN:
  case BTN_DPAD_DOWN:
    return ACTION_DOWN;
  case KEY_LEFT:
  case BTN_DPAD_LEFT:
    return ACTION_LEFT;
  case KEY_RIGHT:
  case BTN_DPAD_RIGHT:
    return ACTION_RIGHT;
  case KEY_SPACE:
  case KEY_Z:
  case 7:
    return ACTION_A;
  case BTN_SOUTH:
    return east_is_confirm ? ACTION_B : ACTION_A;
  case KEY_LEFTCTRL:
  case 9:
    return ACTION_B;
  case BTN_EAST:
    return east_is_confirm ? ACTION_A : ACTION_B;
  case KEY_LEFTSHIFT:
  case BTN_NORTH:
  case KEY_X:
    return ACTION_X;
  case BTN_WEST:
  case KEY_LEFTALT:
  case KEY_Y:
    return ACTION_Y;
  case KEY_ENTER:
  case KEY_MENU:
  case BTN_START:
  case KEY_HOME:
  case 10:
    return ACTION_START;
  case BTN_MODE:
  case BTN_TRIGGER_HAPPY1:
    return ACTION_FUNCTION;
  case KEY_RIGHTCTRL:
  case KEY_SELECT:
  case BTN_SELECT:
    return ACTION_SELECT;
  case KEY_POWER:
    return ACTION_POWER;
  case KEY_ESC:
    return ACTION_NONE;
  case KEY_VOLUMEDOWN:
  case KEY_VOLUMEUP:
    /* The system service owns physical volume keys so they remain available
     * after the frontend exits. */
    return ACTION_NONE;
  case KEY_Q:
    return ACTION_QUIT;
  default:
    return ACTION_NONE;
  }
}

static const char *ui_action_name(enum ui_action action) {
  switch (action) {
  case ACTION_UP:
    return "UP";
  case ACTION_DOWN:
    return "DOWN";
  case ACTION_LEFT:
    return "LEFT";
  case ACTION_RIGHT:
    return "RIGHT";
  case ACTION_A:
    return "A";
  case ACTION_B:
    return "B";
  case ACTION_START:
    return "START";
  case ACTION_SELECT:
    return "SELECT";
  case ACTION_X:
    return "X";
  case ACTION_Y:
    return "Y";
  case ACTION_FUNCTION:
    return "FUNCTION";
  case ACTION_POWER:
    return "POWER";
  case ACTION_VOLUME_DOWN:
    return "VOLUME_DOWN";
  case ACTION_VOLUME_UP:
    return "VOLUME_UP";
  case ACTION_QUIT:
    return "QUIT";
  case ACTION_NONE:
  default:
    return "NONE";
  }
}

static enum ui_action action_from_abs_event(unsigned int code, int value,
                                            int *released) {
  const char *analog_navigation;

  if (released) {
    *released = 0;
  }
  analog_navigation = getenv("PLUMOS_FE_ANALOG_NAVIGATION");
  switch (code) {
  case ABS_X:
    if (analog_navigation && strcmp(analog_navigation, "0") == 0) {
      if (released) {
        *released = 1;
      }
      return ACTION_NONE;
    }
    if (value <= -UI_ABS_AXIS_DEADZONE) {
      return ACTION_LEFT;
    }
    if (value >= UI_ABS_AXIS_DEADZONE) {
      return ACTION_RIGHT;
    }
    if (released) {
      *released = 1;
    }
    return ACTION_NONE;
  case ABS_Y:
    if (analog_navigation && strcmp(analog_navigation, "0") == 0) {
      if (released) {
        *released = 1;
      }
      return ACTION_NONE;
    }
    if (value <= -UI_ABS_AXIS_DEADZONE) {
      return ACTION_UP;
    }
    if (value >= UI_ABS_AXIS_DEADZONE) {
      return ACTION_DOWN;
    }
    if (released) {
      *released = 1;
    }
    return ACTION_NONE;
  case ABS_HAT0X:
    if (value < 0) {
      return ACTION_LEFT;
    }
    if (value > 0) {
      return ACTION_RIGHT;
    }
    if (released) {
      *released = 1;
    }
    return ACTION_NONE;
  case ABS_HAT0Y:
    if (value < 0) {
      return ACTION_UP;
    }
    if (value > 0) {
      return ACTION_DOWN;
    }
    if (released) {
      *released = 1;
    }
    return ACTION_NONE;
  default:
    return ACTION_NONE;
  }
}

static enum ui_action action_from_stdin_char(int ch) {
  switch (ch) {
  case 'w':
  case 'W':
    return ACTION_UP;
  case 's':
  case 'S':
    return ACTION_DOWN;
  case 'a':
  case 'A':
    return ACTION_LEFT;
  case 'd':
  case 'D':
    return ACTION_RIGHT;
  case 'e':
  case 'E':
  case '\n':
  case ' ':
    return ACTION_A;
  case 'b':
  case 'B':
    return ACTION_B;
  case 'm':
  case 'M':
    return ACTION_START;
  case 'c':
  case 'C':
    return ACTION_SELECT;
  case 'p':
  case 'P':
    return ACTION_POWER;
  case 'x':
  case 'X':
    return ACTION_X;
  case 'y':
  case 'Y':
    return ACTION_Y;
  case '-':
  case '[':
    return ACTION_VOLUME_DOWN;
  case '+':
  case '=':
  case ']':
    return ACTION_VOLUME_UP;
  case 'q':
  case 'Q':
    return ACTION_QUIT;
  default:
    return ACTION_NONE;
  }
}

static enum ui_action action_from_script_token(const char *token) {
  if (strcmp(token, "up") == 0) {
    return ACTION_UP;
  }
  if (strcmp(token, "down") == 0) {
    return ACTION_DOWN;
  }
  if (strcmp(token, "left") == 0) {
    return ACTION_LEFT;
  }
  if (strcmp(token, "right") == 0) {
    return ACTION_RIGHT;
  }
  if (strcmp(token, "a") == 0 || strcmp(token, "open") == 0) {
    return ACTION_A;
  }
  if (strcmp(token, "b") == 0 || strcmp(token, "back") == 0) {
    return ACTION_B;
  }
  if (strcmp(token, "start") == 0) {
    return ACTION_START;
  }
  if (strcmp(token, "select") == 0) {
    return ACTION_SELECT;
  }
  if (strcmp(token, "x") == 0 || strcmp(token, "gallery") == 0) {
    return ACTION_X;
  }
  if (strcmp(token, "y") == 0 || strcmp(token, "favorite") == 0 ||
      strcmp(token, "favourite") == 0) {
    return ACTION_Y;
  }
  if (strcmp(token, "power") == 0 || strcmp(token, "power_menu") == 0 ||
      strcmp(token, "safe") == 0) {
    return ACTION_POWER;
  }
  if (strcmp(token, "function") == 0) {
    return ACTION_FUNCTION;
  }
  if (strcmp(token, "volume_down") == 0 || strcmp(token, "volumedown") == 0 ||
      strcmp(token, "voldown") == 0) {
    return ACTION_VOLUME_DOWN;
  }
  if (strcmp(token, "volume_up") == 0 || strcmp(token, "volumeup") == 0 ||
      strcmp(token, "volup") == 0) {
    return ACTION_VOLUME_UP;
  }
  if (strcmp(token, "q") == 0 || strcmp(token, "quit") == 0) {
    return ACTION_QUIT;
  }
  return ACTION_NONE;
}

static int settings_value_action_repeats(const struct ui_state *ui,
                                         enum ui_action action) {
  const struct setting_entry *entry;
  enum setting_control_type control;

  if (!ui || ui->screen != SCREEN_SETTINGS ||
      (action != ACTION_LEFT && action != ACTION_RIGHT) ||
      ui->setting_count == 0 || ui->settings_cursor >= ui->setting_count) {
    return 0;
  }
  entry = &ui->setting_entries[ui->settings_cursor];
  control = setting_control_type_for_id(entry->id);
  if (control != SETTING_CONTROL_NUMBER && control != SETTING_CONTROL_CHOICE) {
    return 0;
  }
  return setting_is_writable(entry->id) ||
         strncmp(entry->id, "performance_", 12) == 0;
}

static int action_repeat_interval_ms(const struct ui_state *ui,
                                     enum ui_action action) {
  if (action == ACTION_UP || action == ACTION_DOWN) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (action == ACTION_VOLUME_DOWN || action == ACTION_VOLUME_UP) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_WIFI_CONNECT &&
      ui->wifi_stage == WIFI_CONNECT_PASSWORD &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_TOP && ui_graphic_top_uses_strip(ui) &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_GALLERY &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_SETTINGS &&
      ui->settings_category == SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (settings_value_action_repeats(ui, action)) {
    return UI_SETTING_VALUE_REPEAT_INTERVAL_MS;
  }
  return 0;
}

static int discover_named_input_event(char *out, size_t out_size, const char *target_name,
                                      const char *fallback_path) {
  FILE *f;
  char line[512];
  int in_target = 0;

  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    return copy_string(out, out_size, fallback_path);
  }
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n') {
      in_target = 0;
      continue;
    }
    if (target_name && strstr(line, "Name=\"") && strstr(line, target_name)) {
      in_target = 1;
      continue;
    }
    if (in_target && strstr(line, "Handlers=")) {
      char *event = strstr(line, "event");
      if (event) {
        char name[32];
        size_t n = 0;
        while (event[n] && !isspace((unsigned char)event[n]) && n + 1 < sizeof(name)) {
          name[n] = event[n];
          n++;
        }
        name[n] = '\0';
        fclose(f);
        return join_path(out, out_size, "/dev/input", name);
      }
    }
  }
  fclose(f);
  if (fallback_path && fallback_path[0]) {
    return copy_string(out, out_size, fallback_path);
  }
  return 0;
}

static int discover_input_event(char *out, size_t out_size) {
  static const char *names[] = {
    "adc_gamepad",
    "adc gamepad",
    "sunxi-keyboard",
    "soc:gpio_keys",
    "gpio-keys"
  };
  size_t i;

  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    if (discover_named_input_event(out, out_size, names[i], NULL)) {
      return 1;
    }
  }
  return copy_string(out, out_size, "/dev/input/event0");
}

static int discover_power_input_event(char *out, size_t out_size) {
  if (discover_named_input_event(out, out_size, "rk805 pwrkey", NULL) ||
      discover_named_input_event(out, out_size, "rk817 pwrkey", NULL) ||
      discover_named_input_event(out, out_size, "axp2202-pek", NULL) ||
      discover_named_input_event(out, out_size, "soc:gpio_keys", NULL) ||
      discover_named_input_event(out, out_size, "gpio-keys", NULL)) {
    return 1;
  }
  return copy_string(out, out_size, "/dev/input/event0");
}

static int same_path(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static void dump_input_events(const char *event_path, int timeout_sec) {
  int fd;
  time_t deadline;

  fd = open(event_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    fprintf(stderr, "error: cannot open input event device: %s: %s\n", event_path,
            strerror(errno));
    return;
  }
  deadline = time(NULL) + timeout_sec;
  printf("plumOS controller UI - dump events\n");
  printf("event: %s timeout=%d\n", event_path, timeout_sec);
  fflush(stdout);

  while (time(NULL) < deadline) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 250) > 0 && (pfd.revents & POLLIN)) {
      struct input_event ev;
      while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        enum ui_action action =
            ev.type == EV_KEY ? action_from_key_code(ev.code) : ACTION_NONE;
        printf("type=%u code=%u value=%d action=%s\n", ev.type, ev.code, ev.value,
               ui_action_name(action));
        fflush(stdout);
      }
    }
  }
  close(fd);
}

static void drain_input_fd(int fd) {
  struct input_event ev;
  if (fd < 0) {
    return;
  }
  while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
  }
}

static void trace_input_event(const struct input_event *ev, int power_only,
                              enum ui_action action) {
  const char *path = getenv("PLUMOS_INPUT_TRACE_PATH");
  FILE *f;

  if (!ev || !path || !path[0]) {
    return;
  }
  f = fopen(path, "a");
  if (!f) {
    return;
  }
  fprintf(f, "ms=%lld source=%s type=%u code=%u value=%d action=%s\n",
          current_time_ms(), power_only ? "power" : "controller",
          ev->type, ev->code, ev->value, ui_action_name(action));
  fclose(f);
}

static void trace_dispatched_action(const struct ui_state *ui, enum ui_action action,
                                    enum ui_screen screen_before) {
  const char *path = getenv("PLUMOS_INPUT_TRACE_PATH");
  FILE *f;

  if (!ui || !path || !path[0] || action == ACTION_NONE) {
    return;
  }
  f = fopen(path, "a");
  if (!f) {
    return;
  }
  fprintf(f,
          "ms=%lld dispatch action=%s screen_before=%d screen_after=%d "
          "top_cursor=%zu top_count=%zu rom_count=%zu status=%s\n",
          current_time_ms(), ui_action_name(action), (int)screen_before,
          (int)ui->screen, ui->top_cursor, ui->top_count, ui->rom_count,
          ui->status);
  fclose(f);
}

static int consume_power_wake_suppression(void) {
  const char *runtime_root = getenv("PLUMOS_RUNTIME_ROOT");
  char path[PATH_MAX];

  if (!runtime_root || !runtime_root[0]) {
    runtime_root = "/run/plumos";
  }
  if (snprintf(path, sizeof(path), "%s/power-wake-suppress", runtime_root) >=
      (int)sizeof(path)) {
    return 0;
  }
  return unlink(path) == 0;
}

static void read_input_actions(struct ui_state *ui, int fd, int power_only,
                               enum ui_action *action) {
  struct input_event ev;

  if (!ui || fd < 0 || !action) {
    return;
  }
  while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
    if (ev.type == EV_KEY) {
      enum ui_action event_action;
      if (power_only && ev.code != KEY_POWER) {
        trace_input_event(&ev, power_only, ACTION_NONE);
        continue;
      }
      if (power_only && ev.code == KEY_POWER && ev.value == 1 &&
          consume_power_wake_suppression()) {
        trace_input_event(&ev, power_only, ACTION_NONE);
        continue;
      }
      event_action = action_from_key_code(ev.code);
      trace_input_event(&ev, power_only, event_action);
      if (ev.value == 0) {
        if (!power_only && ui->repeat_action != ACTION_NONE &&
            ui->repeat_key_code == ev.code) {
          ui->repeat_action = ACTION_NONE;
          ui->repeat_key_code = 0;
          ui->repeat_next_ms = 0;
        }
        continue;
      }
      if (ev.value == 1 || ev.value == 2) {
        int repeat_interval_ms;
        *action = event_action;
        repeat_interval_ms = power_only ? 0 : action_repeat_interval_ms(ui, event_action);
        if (repeat_interval_ms > 0) {
          ui->repeat_action = event_action;
          ui->repeat_key_code = ev.code;
          ui->repeat_next_ms = current_time_ms() +
                               (ev.value == 1 ? UI_KEY_REPEAT_DELAY_MS
                                              : repeat_interval_ms);
        }
      }
      if (event_action == ACTION_NONE && ev.value == 1 && !power_only) {
        snprintf(ui->status, sizeof(ui->status), "unmapped key code=%u value=%d", ev.code,
                 ev.value);
      }
    } else if (!power_only && ev.type == EV_ABS) {
      trace_input_event(&ev, power_only, ACTION_NONE);
      enum ui_action event_action;
      unsigned int repeat_code;
      int released = 0;
      event_action = action_from_abs_event(ev.code, ev.value, &released);
      repeat_code = UI_ABS_REPEAT_CODE_BASE | ev.code;
      if (released) {
        if (ui->repeat_action != ACTION_NONE && ui->repeat_key_code == repeat_code) {
          ui->repeat_action = ACTION_NONE;
          ui->repeat_key_code = 0;
          ui->repeat_next_ms = 0;
        }
        continue;
      }
      if (event_action != ACTION_NONE) {
        int repeat_interval_ms;
        if (ui->repeat_action == event_action &&
            ui->repeat_key_code == repeat_code) {
          continue;
        }
        *action = event_action;
        repeat_interval_ms = action_repeat_interval_ms(ui, event_action);
        if (repeat_interval_ms > 0) {
          ui->repeat_action = event_action;
          ui->repeat_key_code = repeat_code;
          ui->repeat_next_ms = current_time_ms() + UI_KEY_REPEAT_DELAY_MS;
        }
      }
    } else {
      trace_input_event(&ev, power_only, ACTION_NONE);
    }
  }
}

static void settle_input_after_child(struct ui_state *ui) {
  long long deadline;
  long long quiet_until;
  long long now;
  int fd;

  if (!ui) {
    return;
  }
  fd = ui->input_event_fd;
  if (fd < 0) {
    ui->ignore_input_until_ms = current_time_ms() + 750;
    return;
  }

  drain_input_fd(fd);
  drain_input_fd(ui->power_event_fd);
  now = current_time_ms();
  deadline = now + 1500;
  quiet_until = now + 250;

  while ((now = current_time_ms()) < deadline && now < quiet_until) {
    struct pollfd pfd;
    int timeout_ms = (int)(quiet_until - now);
    long long remaining_ms = deadline - now;
    if (timeout_ms > remaining_ms) {
      timeout_ms = (int)remaining_ms;
    }
    if (timeout_ms < 0) {
      timeout_ms = 0;
    }
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
      drain_input_fd(fd);
      quiet_until = current_time_ms() + 250;
    }
  }

  ui->ignore_input_until_ms = current_time_ms() + 750;
}

static int run_script(struct ui_state *ui, const char *script) {
  char *copy;
  char *token;
  char *save = NULL;

  copy = strdup(script);
  if (!copy) {
    return 0;
  }
  render_ui(ui);
  for (token = strtok_r(copy, ",", &save); token; token = strtok_r(NULL, ",", &save)) {
    enum ui_action action;
    while (*token && isspace((unsigned char)*token)) {
      token++;
    }
    action = action_from_script_token(token);
    handle_action(ui, action);
    render_ui(ui);
    if (action == ACTION_QUIT) {
      break;
    }
  }
  free(copy);
  return ui->render_failed ? 0 : 1;
}

static int ui_needs_periodic_refresh(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  if (ui_renderer_fbdev_only(ui)) {
    if (ui->top_transition_active) {
      return 1;
    }
    if (ui->gallery_transition_active ||
        (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY)) {
      return 1;
    }
    if (ui_is_rom_list_screen(ui)) {
      return 1;
    }
    if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP) {
      return 1;
    }
    return ui->rescue_network || ui->rom_scan_refresh_pid > 0;
  }
  if (ui->rescue_network) {
    return 1;
  }
  if (ui_renderer_graphic_capable(ui) && ui->top_transition_active) {
    return 1;
  }
  if (ui_renderer_graphic_capable(ui) && ui->gallery_transition_active) {
    return 1;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY) {
    return 1;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 1;
  }
  if (ui_uses_graphic_mode(ui) && ui_is_rom_list_screen(ui)) {
    return 1;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP) {
    return 0;
  }
  return ui_renderer_pixel2_compat2_tty_capable(ui);
}

static int ui_periodic_refresh_interval_ms(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  if (ui_renderer_fbdev_only(ui)) {
    if (ui->top_transition_active) {
      return 16;
    }
    if (ui->gallery_transition_active ||
        (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY)) {
      return 16;
    }
    if (ui->rom_scan_refresh_pid > 0) {
      return 250;
    }
    if (ui_is_rom_list_screen(ui)) {
      return UI_GRAPHIC_SCROLL_REFRESH_MS;
    }
    if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP) {
      return UI_TOP_STATUS_REFRESH_MS;
    }
    return ui->rescue_network ? 1000 : 0;
  }
  if (ui_renderer_graphic_capable(ui) && ui->top_transition_active) {
    return 16;
  }
  if (ui_renderer_graphic_capable(ui) && ui->gallery_transition_active) {
    return 16;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY) {
    return 16;
  }
  if (ui_uses_graphic_mode(ui) && ui_is_rom_list_screen(ui)) {
    return UI_GRAPHIC_SCROLL_REFRESH_MS;
  }
  if (ui_renderer_pixel2_compat2_tty_capable(ui)) {
    return 100;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 250;
  }
  if (ui->rescue_network) {
    return 1000;
  }
  return 0;
}

static long long ui_periodic_refresh_interval_us(const struct ui_state *ui) {
  int interval_ms = ui_periodic_refresh_interval_ms(ui);
  if (interval_ms <= 0) {
    return 0;
  }
#if defined(PLUMOS_ENABLE_FBDEV_RENDERER) && defined(PLUMOS_FBDEV_ENABLE_DRM)
  if (interval_ms == UI_GRAPHIC_SCROLL_REFRESH_MS && ui &&
      ui->renderer_fbdev && ui->renderer_active &&
      ui->fbdev_renderer.drm_active) {
    /*
     * A DRM page flip already blocks until the next VBlank. Scheduling another
     * full refresh interval after it would halve the effective frame rate.
     */
    return 1;
  }
#endif
  if (interval_ms == UI_GRAPHIC_SCROLL_REFRESH_MS) {
    return 16667;
  }
  return (long long)interval_ms * 1000LL;
}

static long long ui_next_refresh_deadline_us(const struct ui_state *ui,
                                             long long previous_deadline_us) {
  long long interval_us = ui_periodic_refresh_interval_us(ui);
  long long now_us;

  if (interval_us <= 0) {
    return 0;
  }
  now_us = current_time_us();
  if (previous_deadline_us <= 0) {
    return now_us + interval_us;
  }
  do {
    previous_deadline_us += interval_us;
  } while (previous_deadline_us <= now_us);
  return previous_deadline_us;
}

static int run_event_loop(struct ui_state *ui, const char *event_path) {
  int event_fd = -1;
  int power_fd = -1;
  int stdin_fd = STDIN_FILENO;
  int stdin_active = 1;
  int old_flags = -1;
  time_t deadline = 0;
  long long next_refresh_us = 0;

  if (event_path && event_path[0]) {
    event_fd = open(event_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (event_fd < 0) {
      snprintf(ui->status, sizeof(ui->status), "input open failed: %s", strerror(errno));
    }
  }
  ui->input_event_fd = event_fd;
  if (ui->power_event_path[0] &&
      (!event_path || !event_path[0] || !same_path(event_path, ui->power_event_path))) {
    power_fd = open(ui->power_event_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (power_fd < 0 && !ui->status[0]) {
      snprintf(ui->status, sizeof(ui->status), "power input open failed: %s", strerror(errno));
    }
  }
  ui->power_event_fd = power_fd;
  old_flags = fcntl(stdin_fd, F_GETFL, 0);
  if (old_flags >= 0) {
    fcntl(stdin_fd, F_SETFL, old_flags | O_NONBLOCK);
  } else {
    stdin_active = 0;
  }
  if (ui->timeout_sec > 0) {
    deadline = time(NULL) + ui->timeout_sec;
  }

  render_ui(ui);
  mark_frontend_ready_if_needed(ui);
  if (ui_needs_periodic_refresh(ui)) {
    next_refresh_us = ui_next_refresh_deadline_us(ui, 0);
  }
  while (1) {
    struct pollfd pfds[3];
    nfds_t count = 0;
    nfds_t event_index = (nfds_t)-1;
    nfds_t power_index = (nfds_t)-1;
    nfds_t stdin_index = (nfds_t)-1;
    int rc;
    enum ui_action action = ACTION_NONE;
    long long now_ms;
    int poll_timeout = ui->repeat_action != ACTION_NONE ? 50 :
                       (ui_needs_periodic_refresh(ui) ? 60 : 250);

    if (g_terminate_requested) {
      ui->exit_requested = 1;
      break;
    }
    now_ms = current_time_ms();
    (void)run_scheduled_pixel2_compat_brightness_reapply(ui, now_ms);
    if (poll_rom_scan_refresh(ui)) {
      render_ui(ui);
      if (ui_needs_periodic_refresh(ui)) {
        next_refresh_us = ui_next_refresh_deadline_us(ui, 0);
      } else {
        next_refresh_us = 0;
      }
      continue;
    }
    if (ui_needs_periodic_refresh(ui) && next_refresh_us > 0) {
      long long refresh_due_us = next_refresh_us - current_time_us();
      if (refresh_due_us <= 0) {
        poll_timeout = 0;
      } else {
        int refresh_timeout = (int)((refresh_due_us + 999) / 1000);
        if (refresh_timeout < poll_timeout) {
          poll_timeout = refresh_timeout;
        }
      }
    }
    if (ui->pixel2_compat_brightness_reapply_due_ms > 0) {
      long long reapply_due_ms = ui->pixel2_compat_brightness_reapply_due_ms - now_ms;
      if (reapply_due_ms <= 0) {
        poll_timeout = 0;
      } else if (reapply_due_ms < poll_timeout) {
        poll_timeout = (int)reapply_due_ms;
      }
    }

    if (event_fd >= 0) {
      event_index = count;
      pfds[count].fd = event_fd;
      pfds[count].events = POLLIN;
      pfds[count].revents = 0;
      count++;
    }
    if (power_fd >= 0) {
      power_index = count;
      pfds[count].fd = power_fd;
      pfds[count].events = POLLIN;
      pfds[count].revents = 0;
      count++;
    }
    if (stdin_active) {
      stdin_index = count;
      pfds[count].fd = stdin_fd;
      pfds[count].events = POLLIN | POLLHUP | POLLERR | POLLNVAL;
      pfds[count].revents = 0;
      count++;
    }

    if (ui->timeout_sec > 0 && time(NULL) >= deadline) {
      break;
    }
    rc = poll(pfds, count, poll_timeout);
    if (g_terminate_requested) {
      ui->exit_requested = 1;
      break;
    }
    if (rc < 0 && errno != EINTR) {
      break;
    }
    if (rc <= 0) {
      if (ui->repeat_action != ACTION_NONE &&
          current_time_ms() >= ui->repeat_next_ms) {
        int repeat_interval_ms = action_repeat_interval_ms(ui, ui->repeat_action);
        if (repeat_interval_ms > 0) {
          action = ui->repeat_action;
          ui->repeat_next_ms = current_time_ms() + repeat_interval_ms;
        } else {
          ui->repeat_action = ACTION_NONE;
          ui->repeat_key_code = 0;
          ui->repeat_next_ms = 0;
        }
      }
      if (action != ACTION_NONE) {
        enum ui_screen screen_before = ui->screen;
        handle_action(ui, action);
        trace_dispatched_action(ui, action, screen_before);
        if (action == ACTION_QUIT || ui->exit_requested) {
          break;
        }
        render_ui(ui);
        if (ui_needs_periodic_refresh(ui)) {
          next_refresh_us = ui_next_refresh_deadline_us(ui, 0);
        }
        continue;
      }
      if (ui_needs_periodic_refresh(ui)) {
        long long now_us = current_time_us();
        if (next_refresh_us <= 0 || now_us >= next_refresh_us) {
          render_ui(ui);
          next_refresh_us =
              ui_next_refresh_deadline_us(ui, next_refresh_us);
        }
      }
      continue;
    }

    if (ui->ignore_input_until_ms > current_time_ms()) {
      if (event_index != (nfds_t)-1 && (pfds[event_index].revents & POLLIN)) {
        drain_input_fd(event_fd);
      }
      if (power_index != (nfds_t)-1 && (pfds[power_index].revents & POLLIN)) {
        drain_input_fd(power_fd);
      }
      if (ui_needs_periodic_refresh(ui)) {
        long long now_us = current_time_us();
        if (next_refresh_us <= 0 || now_us >= next_refresh_us) {
          render_ui(ui);
          next_refresh_us =
              ui_next_refresh_deadline_us(ui, next_refresh_us);
        }
      }
      continue;
    }

    if (event_index != (nfds_t)-1 && (pfds[event_index].revents & POLLIN)) {
      read_input_actions(ui, event_fd, 0, &action);
    }
    if (power_index != (nfds_t)-1 && (pfds[power_index].revents & POLLIN)) {
      read_input_actions(ui, power_fd, 1, &action);
    }
    if (stdin_index != (nfds_t)-1 &&
        (pfds[stdin_index].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
      if (pfds[stdin_index].revents & POLLIN) {
        char buf[32];
        ssize_t n = read(stdin_fd, buf, sizeof(buf));
        ssize_t i;
        if (n > 0) {
          for (i = 0; i < n; i++) {
            enum ui_action stdin_action =
                action_from_stdin_char((unsigned char)buf[i]);
            if (stdin_action != ACTION_NONE) {
              action = stdin_action;
            }
          }
        } else {
          stdin_active = 0;
        }
      } else {
        stdin_active = 0;
      }
    }

    if (action == ACTION_NONE && ui->repeat_action != ACTION_NONE &&
        current_time_ms() >= ui->repeat_next_ms) {
      int repeat_interval_ms = action_repeat_interval_ms(ui, ui->repeat_action);
      if (repeat_interval_ms > 0) {
        action = ui->repeat_action;
        ui->repeat_next_ms = current_time_ms() + repeat_interval_ms;
      } else {
        ui->repeat_action = ACTION_NONE;
        ui->repeat_key_code = 0;
        ui->repeat_next_ms = 0;
      }
    }

    if (action == ACTION_NONE && ui_needs_periodic_refresh(ui)) {
      long long now_us = current_time_us();
      if (next_refresh_us <= 0 || now_us >= next_refresh_us) {
        render_ui(ui);
        next_refresh_us =
            ui_next_refresh_deadline_us(ui, next_refresh_us);
      }
    }

    if (action != ACTION_NONE) {
      enum ui_screen screen_before = ui->screen;
      handle_action(ui, action);
      trace_dispatched_action(ui, action, screen_before);
      if (action == ACTION_QUIT || ui->exit_requested) {
        break;
      }
      render_ui(ui);
      if (ui_needs_periodic_refresh(ui)) {
        next_refresh_us = ui_next_refresh_deadline_us(ui, 0);
      }
    }
  }
  if (old_flags >= 0) {
    fcntl(stdin_fd, F_SETFL, old_flags);
  }
  if (event_fd >= 0) {
    close(event_fd);
  }
  if (power_fd >= 0) {
    close(power_fd);
  }
  ui->input_event_fd = -1;
  ui->power_event_fd = -1;
  return ui->render_failed ? 1 : 0;
}

static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s [--all] [--refresh] [--once] [--timeout SEC] [--event PATH]\n", argv0);
  printf("     [--renderer text|mali|fbdev|pixel2-compat-gfx] [--fb PATH]\n");
  printf("     [--fbdev-rotation none|cw|ccw|180] [--pixel2-compat-gfx-rotation none|180]\n");
  printf("     [--egl-lib PATH] [--gles-lib PATH]\n");
  printf("     [--rotation auto|none|cw|ccw] [--font PATH]\n");
  printf("     [--tty-entry-scale 1|1.5|2]\n");
  printf("     [--power-overlay] [--power-event PATH]\n");
  printf("     [--rescue-network]  # disabled compatibility screen\n");
  printf("  %s --script up,down,a,b,x,select,start,function,power,volume_up,volume_down,q [--no-clear]\n", argv0);
  printf("  %s --dump-events [--timeout SEC] [--event PATH]\n", argv0);
  printf("\n");
  printf("Keyboard fallback over SSH: w/s/a/d, e or space for A, b, x, m, c, p, +/- for volume, q.\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_INPUT_EVENT  Default: auto-detect soc:gpio_keys\n");
  printf("  PLUMOS_POWER_INPUT_EVENT  Default: auto-detect soc:gpio_keys\n");
  printf("  PLUMOS_RENDERER     text, mali, fbdev, or pixel2-compat-gfx. Default: text\n");
  printf("  PLUMOS_FB           Default for Mali renderer: /dev/fb0\n");
  printf("  PLUMOS_FBDEV_ROTATION  none, cw, ccw or 180. Default: none\n");
  printf("  PLUMOS_PIXEL2_COMPAT_GFX_ROTATION  none or 180. Default: PLUMOS_FBDEV_ROTATION\n");
  printf("  PLUMOS_EGL_LIB      Default for Mali renderer: /usr/lib/libEGL.so\n");
  printf("  PLUMOS_GLES_LIB     Default for Mali renderer: /usr/lib/libGLESv2.so\n");
  printf("  PLUMOS_MALI_ROTATION auto, none, cw, or ccw. Default: auto\n");
  printf("  PLUMOS_MALI_FONT    Optional FreeType font for non-ASCII Mali text\n");
  printf("  PLUMOS_MALI_TTY_ENTRY_SCALE  1, 1.5, or 2. Default: 1\n");
  printf("  PLUMOS_MALI_SWAP_INTERVAL  0 or 1. Default: 1\n");
  printf("  PLUMOS_CONTROLLER_RESCUE network opens a disabled compatibility screen\n");
  printf("  PLUMOS_SYSTEM_SETTINGS_JSON  Default: $PLUMOS_ROOT/config/system/settings.json\n");
  printf("  PLUMOS_WPA_STATUS   Default: /run/plumos/network-control/wpa_status.txt\n");
  printf("  PLUMOS_Pixel2_WPA_STATUS   Legacy status-path override\n");
  printf("  PLUMOS_CONTROLLER_CPU_DEFAULT  Ondemand/all-core FE default; set 0 to skip\n");
  printf("  PLUMOS_CPU_BASELINE_GOVERNOR  interactive, performance, ondemand, schedutil, or conservative\n");
}

static int run_boot_resume_if_needed(struct ui_state *ui,
                                     const struct frontend_settings *settings) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *frontend_mode;
  size_t pos = 0;
  int rc;

  if (!ui || !settings) {
    return 0;
  }
  frontend_mode = getenv("PLUMOS_FRONTEND_MODE");
  if ((frontend_mode && strcmp(frontend_mode, "manual") == 0) ||
      strcmp(settings->boot_resume_mode, "off") == 0) {
    return 0;
  }
  if (strcmp(settings->boot_resume_mode, "on") != 0) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-boot-last-rom.log")) {
    set_status(ui, "boot last ROM path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "boot last ROM skipped; plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " boot --execute >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "boot last ROM command too long");
    return 0;
  }

  rc = run_foreground_shell_command(cmd);
  if (rc == -1) {
    set_status(ui, "boot last ROM system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    set_status(ui, "boot last ROM checked; TOP ready");
    return 1;
  }
  set_status(ui, "boot last ROM failed; see frontend-boot-last-rom.log");
  return 0;
}

int main(int argc, char **argv) {
  static struct ui_state ui;
  const char *plumos_root_env;
  const char *renderer_env;
  const char *system_config_env;
  const char *wpa_status_env;
  const char *fb_path;
  const char *fbdev_rotation_env;
  const char *pixel2_compat_gfx_rotation_env;
  const char *egl_path;
  const char *gles_path;
  const char *rotation_env;
  const char *mali_font_env;
  const char *mali_tty_entry_scale_env;
  const char *script = NULL;
  char event_path[PATH_MAX];
  char power_event_path[PATH_MAX];
  struct frontend_settings initial_settings;
  int initial_settings_loaded = 0;
  int startup_resume_allowed = 0;
  int power_overlay_fast_ui = 0;
  int dump_events = 0;
  int exit_code = 0;
  int i;

  memset(&ui, 0, sizeof(ui));
  signal(SIGTERM, handle_terminate_signal);
  signal(SIGINT, handle_terminate_signal);
  ui.input_event_fd = -1;
  ui.power_event_fd = -1;
  ui.rom_entry_screen = SCREEN_ROMS;
  ui.sdcard_root = getenv("PLUMOS_SDCARD_ROOT");
  if (!ui.sdcard_root || !ui.sdcard_root[0]) {
    ui.sdcard_root = "/mnt/SDCARD";
  }
  plumos_root_env = getenv("PLUMOS_ROOT");
  if (plumos_root_env && plumos_root_env[0]) {
    copy_string(ui.plumos_root, sizeof(ui.plumos_root), plumos_root_env);
  } else {
    if (!join_path(ui.plumos_root, sizeof(ui.plumos_root), ui.sdcard_root,
                   "plumos")) {
      fprintf(stderr, "error: plumOS root path is too long\n");
      return 1;
    }
  }
  ui.timeout_sec = 0;
  renderer_env = getenv("PLUMOS_RENDERER");
  if (renderer_env && strcmp(renderer_env, "mali") == 0) {
    ui.renderer_mali = 1;
    ui.no_clear = 1;
  } else if (renderer_env && strcmp(renderer_env, "fbdev") == 0) {
    ui.renderer_fbdev = 1;
    ui.no_clear = 1;
  } else if (renderer_env &&
             (strcmp(renderer_env, "pixel2-compat-gfx") == 0 ||
              strcmp(renderer_env, "pixel2_compat_gfx") == 0 ||
              strcmp(renderer_env, "pixel2_compat") == 0)) {
    ui.renderer_pixel2_compat_gfx = 1;
    ui.no_clear = 1;
  }
  discover_input_event(event_path, sizeof(event_path));
  discover_power_input_event(power_event_path, sizeof(power_event_path));
  if (getenv("PLUMOS_INPUT_EVENT") && getenv("PLUMOS_INPUT_EVENT")[0]) {
    copy_string(event_path, sizeof(event_path), getenv("PLUMOS_INPUT_EVENT"));
  }
  if (getenv("PLUMOS_POWER_INPUT_EVENT") && getenv("PLUMOS_POWER_INPUT_EVENT")[0]) {
    copy_string(power_event_path, sizeof(power_event_path), getenv("PLUMOS_POWER_INPUT_EVENT"));
  }
  system_config_env = getenv("PLUMOS_SYSTEM_SETTINGS_JSON");
  if (system_config_env && system_config_env[0] &&
      !copy_string(ui.system_config_path, sizeof(ui.system_config_path), system_config_env)) {
    fprintf(stderr, "error: plumOS system settings path is too long\n");
    return 1;
  }
  wpa_status_env = getenv("PLUMOS_WPA_STATUS");
  if (!wpa_status_env || !wpa_status_env[0]) {
    wpa_status_env = getenv("PLUMOS_Pixel2_WPA_STATUS");
  }
  if (!copy_string(ui.wpa_status_path, sizeof(ui.wpa_status_path),
                   wpa_status_env && wpa_status_env[0] ? wpa_status_env
                                                       : "/run/plumos/network-control/wpa_status.txt")) {
    fprintf(stderr, "error: WPA status path is too long\n");
    return 1;
  }
  fb_path = getenv("PLUMOS_FB");
  fbdev_rotation_env = getenv("PLUMOS_FBDEV_ROTATION");
  pixel2_compat_gfx_rotation_env = getenv("PLUMOS_PIXEL2_COMPAT_GFX_ROTATION");
  egl_path = getenv("PLUMOS_EGL_LIB");
  gles_path = getenv("PLUMOS_GLES_LIB");
  rotation_env = getenv("PLUMOS_MALI_ROTATION");
  mali_font_env = getenv("PLUMOS_MALI_FONT");
  mali_tty_entry_scale_env = getenv("PLUMOS_MALI_TTY_ENTRY_SCALE");
  copy_string(ui.mali_rotation, sizeof(ui.mali_rotation),
              rotation_env && rotation_env[0] ? rotation_env : "auto");
  copy_string(ui.mali_tty_entry_scale, sizeof(ui.mali_tty_entry_scale),
              mali_tty_entry_scale_env && mali_tty_entry_scale_env[0]
                  ? mali_tty_entry_scale_env
                  : "1");
  copy_string(ui.fbdev_rotation, sizeof(ui.fbdev_rotation),
              fbdev_rotation_env && fbdev_rotation_env[0] ? fbdev_rotation_env : "none");
  copy_string(ui.pixel2_compat_gfx_rotation, sizeof(ui.pixel2_compat_gfx_rotation),
              pixel2_compat_gfx_rotation_env && pixel2_compat_gfx_rotation_env[0]
                  ? pixel2_compat_gfx_rotation_env
                  : ui.fbdev_rotation);
  if (getenv("PLUMOS_CONTROLLER_RESCUE") &&
      strcmp(getenv("PLUMOS_CONTROLLER_RESCUE"), "network") == 0) {
    ui.rescue_network = 1;
    copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--all") == 0) {
      ui.show_all = 1;
    } else if (strcmp(argv[i], "--refresh") == 0) {
      ui.refresh = 1;
    } else if (strcmp(argv[i], "--once") == 0) {
      ui.once = 1;
    } else if (strcmp(argv[i], "--no-clear") == 0) {
      ui.no_clear = 1;
    } else if (strcmp(argv[i], "--mali") == 0) {
      ui.renderer_mali = 1;
      ui.renderer_fbdev = 0;
      ui.renderer_pixel2_compat_gfx = 0;
      ui.no_clear = 1;
    } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
      const char *renderer = argv[++i];
      if (strcmp(renderer, "mali") == 0) {
        ui.renderer_mali = 1;
        ui.renderer_fbdev = 0;
        ui.renderer_pixel2_compat_gfx = 0;
        ui.no_clear = 1;
      } else if (strcmp(renderer, "fbdev") == 0) {
        ui.renderer_mali = 0;
        ui.renderer_fbdev = 1;
        ui.renderer_pixel2_compat_gfx = 0;
        ui.no_clear = 1;
      } else if (strcmp(renderer, "pixel2-compat-gfx") == 0 ||
                 strcmp(renderer, "pixel2_compat_gfx") == 0 ||
                 strcmp(renderer, "pixel2_compat") == 0) {
        ui.renderer_mali = 0;
        ui.renderer_fbdev = 0;
        ui.renderer_pixel2_compat_gfx = 1;
        ui.no_clear = 1;
      } else if (strcmp(renderer, "text") == 0) {
        ui.renderer_mali = 0;
        ui.renderer_fbdev = 0;
        ui.renderer_pixel2_compat_gfx = 0;
      } else {
        fprintf(stderr, "error: unknown renderer: %s\n", renderer);
        return 2;
      }
    } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
      ui.timeout_sec = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      copy_string(event_path, sizeof(event_path), argv[++i]);
    } else if (strcmp(argv[i], "--power-event") == 0 && i + 1 < argc) {
      copy_string(power_event_path, sizeof(power_event_path), argv[++i]);
    } else if (strcmp(argv[i], "--power-overlay") == 0) {
      ui.power_overlay = 1;
      if (!ui.renderer_fbdev && !ui.renderer_pixel2_compat_gfx) {
        ui.renderer_mali = 1;
      }
      ui.no_clear = 1;
    } else if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc) {
      fb_path = argv[++i];
    } else if (strcmp(argv[i], "--fbdev-rotation") == 0 && i + 1 < argc) {
      const char *rotation = argv[++i];
      if (strcmp(rotation, "none") != 0 && strcmp(rotation, "cw") != 0 &&
          strcmp(rotation, "ccw") != 0 && strcmp(rotation, "90") != 0 &&
          strcmp(rotation, "270") != 0 && strcmp(rotation, "180") != 0 &&
          strcmp(rotation, "rotate180") != 0 && strcmp(rotation, "inverted") != 0) {
        fprintf(stderr, "error: unknown fbdev rotation: %s\n", rotation);
        return 2;
      }
      copy_string(ui.fbdev_rotation, sizeof(ui.fbdev_rotation), rotation);
    } else if (strcmp(argv[i], "--pixel2-compat-gfx-rotation") == 0 && i + 1 < argc) {
      const char *rotation = argv[++i];
      if (strcmp(rotation, "none") != 0 && strcmp(rotation, "180") != 0 &&
          strcmp(rotation, "rotate180") != 0 && strcmp(rotation, "inverted") != 0) {
        fprintf(stderr, "error: unknown Pixel2 GFX rotation: %s\n", rotation);
        return 2;
      }
      copy_string(ui.pixel2_compat_gfx_rotation, sizeof(ui.pixel2_compat_gfx_rotation), rotation);
    } else if (strcmp(argv[i], "--egl-lib") == 0 && i + 1 < argc) {
      egl_path = argv[++i];
    } else if (strcmp(argv[i], "--gles-lib") == 0 && i + 1 < argc) {
      gles_path = argv[++i];
    } else if (strcmp(argv[i], "--rotation") == 0 && i + 1 < argc) {
      const char *rotation = argv[++i];
      if (strcmp(rotation, "auto") != 0 && strcmp(rotation, "none") != 0 &&
          strcmp(rotation, "cw") != 0 && strcmp(rotation, "ccw") != 0) {
        fprintf(stderr, "error: unknown rotation: %s\n", rotation);
        return 2;
      }
      copy_string(ui.mali_rotation, sizeof(ui.mali_rotation), rotation);
    } else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
      mali_font_env = argv[++i];
    } else if (strcmp(argv[i], "--tty-entry-scale") == 0 && i + 1 < argc) {
      const char *scale = argv[++i];
      if (strcmp(scale, "1") != 0 && strcmp(scale, "1.0") != 0 &&
          strcmp(scale, "default") != 0 && strcmp(scale, "1.5") != 0 &&
          strcmp(scale, "2") != 0 && strcmp(scale, "2.0") != 0) {
        fprintf(stderr, "error: unknown TTY entry scale: %s\n", scale);
        return 2;
      }
      copy_string(ui.mali_tty_entry_scale, sizeof(ui.mali_tty_entry_scale), scale);
    } else if (strcmp(argv[i], "--rescue-network") == 0) {
      ui.rescue_network = 1;
      copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
    } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      script = argv[++i];
    } else if (strcmp(argv[i], "--dump-events") == 0) {
      dump_events = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "error: unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }
  copy_string(ui.input_event_path, sizeof(ui.input_event_path), event_path);
  copy_string(ui.power_event_path, sizeof(ui.power_event_path), power_event_path);

  if (dump_events) {
    dump_input_events(event_path, ui.timeout_sec > 0 ? ui.timeout_sec : 10);
    return 0;
  }

  if (!ui.system_config_path[0] &&
      !join_path(ui.system_config_path, sizeof(ui.system_config_path), ui.plumos_root,
                 "config/system/settings.json")) {
    fprintf(stderr, "error: plumOS system settings path is too long\n");
    return 1;
  }

  if (!join_path(ui.top_cache_path, sizeof(ui.top_cache_path), ui.plumos_root,
                 "state/frontend/library-index.json") ||
      !join_path(ui.settings_path, sizeof(ui.settings_path), ui.plumos_root,
                 "config/frontend/settings.json") ||
      !join_path(ui.systems_path, sizeof(ui.systems_path), ui.plumos_root,
                 "config/frontend/systems.json") ||
      !join_path(ui.menus_path, sizeof(ui.menus_path), ui.plumos_root,
                 "config/frontend/menus.json") ||
      !join_path(ui.apps_path, sizeof(ui.apps_path), ui.plumos_root,
                 "config/frontend/apps.json") ||
      !join_path(ui.favorites_path, sizeof(ui.favorites_path), ui.plumos_root,
                 "state/frontend/favorites.json") ||
      !join_path(ui.recent_path, sizeof(ui.recent_path), ui.plumos_root,
                 "state/frontend/recent.json")) {
    fprintf(stderr, "error: frontend path is too long\n");
    return 1;
  }

  memset(&initial_settings, 0, sizeof(initial_settings));
  power_overlay_fast_ui =
      ui.power_overlay && env_flag_enabled_default("PLUMOS_POWER_MENU_FAST_UI", 1);
  if (power_overlay_fast_ui) {
    init_frontend_settings(&initial_settings);
    ui.frontend_settings = initial_settings;
    init_theme_state(&ui.theme, "default", "");
    copy_string(ui.theme.status, sizeof(ui.theme.status), "power overlay fast path");
    ui.translation_count = 0;
    ui.translation_language[0] = '\0';
    ui.translation_status[0] = '\0';
    ui.mali_font_path[0] = '\0';
    ui.mali_fallback_font_path[0] = '\0';
  } else {
    apply_frontend_cpu_default();
    if (load_device_settings(&ui) && !runtime_device_is_pixel2()) {
      apply_device_runtime_settings(&ui.device, NULL, NULL, 0);
    }
    load_translations(&ui);

    initial_settings_loaded = load_settings(ui.settings_path, &initial_settings);
    if (!initial_settings_loaded) {
      init_frontend_settings(&initial_settings);
    }
    ui.frontend_settings = initial_settings;
    load_theme_state(&ui, initial_settings_loaded ? initial_settings.graphic_theme_id : "default");
    apply_theme_setting_overrides(&ui.theme, &initial_settings);
    choose_mali_font_path(&ui, mali_font_env, ui.mali_font_path, sizeof(ui.mali_font_path));
    choose_mali_fallback_font_path(&ui, ui.mali_font_path, ui.mali_fallback_font_path,
                                   sizeof(ui.mali_fallback_font_path));
  }

  startup_resume_allowed =
      !ui.rescue_network && !ui.power_overlay && !script && !ui.once && initial_settings_loaded &&
      (!getenv("PLUMOS_FRONTEND_MODE") ||
       strcmp(getenv("PLUMOS_FRONTEND_MODE"), "manual") != 0);
  if (startup_resume_allowed) {
    run_boot_resume_if_needed(&ui, &initial_settings);
  }

  if (!ui.rescue_network && !ui.power_overlay && !load_top_entries(&ui)) {
    fprintf(stderr, "error: cannot load TOP entries: %s\n", ui.top_cache_path);
    return 1;
  }
  if (startup_resume_allowed && strcmp(initial_settings.boot_resume_mode, "recent") == 0) {
    open_recent_screen(&ui);
    set_status(&ui, "boot Recent ready");
  }
  if (ui.power_overlay) {
    open_power_menu(&ui);
  }

#if !defined(PLUMOS_ENABLE_MALI_RENDERER) && !defined(PLUMOS_ENABLE_FBDEV_RENDERER) && \
    !defined(PLUMOS_ENABLE_PIXEL2_COMPAT_GFX_RENDERER)
  (void)fb_path;
  (void)egl_path;
  (void)gles_path;
#endif
  copy_string(ui.fb_path, sizeof(ui.fb_path),
              fb_path && fb_path[0] ? fb_path : "/dev/fb0");
  copy_string(ui.egl_path, sizeof(ui.egl_path),
              egl_path && egl_path[0] ? egl_path : "/usr/lib/libEGL.so");
  copy_string(ui.gles_path, sizeof(ui.gles_path),
              gles_path && gles_path[0] ? gles_path : "/usr/lib/libGLESv2.so");

  if (ui.renderer_mali || ui.renderer_fbdev || ui.renderer_pixel2_compat_gfx) {
    if (!init_ui_renderer(&ui)) {
      fprintf(stderr, "error: %s\n", ui.status[0] ? ui.status : "renderer init failed");
      return 1;
    }
    if (ui.renderer_mali &&
        (!ui.status[0] || strncmp(ui.status, "Mali renderer ready font=", 25) == 0)) {
      copy_string(ui.status, sizeof(ui.status), "Mali renderer ready");
    }
    if (ui.rescue_network) {
      copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
    }
    if (!ui.rescue_network && !ui.power_overlay &&
        !runtime_device_is_pixel2()) {
      apply_device_runtime_settings(&ui.device, "system_brightness", NULL, 0);
      schedule_pixel2_compat_brightness_reapply(&ui);
    }
  }

  if (script) {
    exit_code = run_script(&ui, script) ? 0 : 1;
    if ((ui.renderer_mali || ui.renderer_fbdev || ui.renderer_pixel2_compat_gfx) &&
        ui.timeout_sec > 0) {
      sleep((unsigned int)ui.timeout_sec);
    }
  } else if (ui.once) {
    render_ui(&ui);
    if ((ui.renderer_mali || ui.renderer_fbdev || ui.renderer_pixel2_compat_gfx) &&
        ui.timeout_sec > 0) {
      sleep((unsigned int)ui.timeout_sec);
    }
    exit_code = ui.render_failed ? 1 : 0;
  } else {
    exit_code = run_event_loop(&ui, event_path);
  }
  if (ui.renderer_mali || ui.renderer_fbdev || ui.renderer_pixel2_compat_gfx) {
    shutdown_ui_renderer(&ui);
  }
  ui_free_rom_entries(&ui);
  return exit_code;
}
