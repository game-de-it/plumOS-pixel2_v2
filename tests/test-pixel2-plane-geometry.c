/* Build with libdrm and --wrap for the four property functions below. */
#define main hardware_keys_main
#include "../vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
#undef main
#include <assert.h>

static const char *property_names[] = {
    "SRC_H", "CRTC_Y", "SRC_X", "CRTC_W", "SRC_Y", "SRC_W",
    "CRTC_H", "CRTC_X", "rotation"};
static uint32_t ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
static uint64_t vals[] = {
    160U << 16, 53, 1U << 15, 430, 2U << 16, 144U << 16,
    534, (uint64_t)(int64_t)-7, 1};
static drmModeObjectProperties props = {9, ids, vals};
static drmModePropertyRes prop;
drmModeObjectProperties *__wrap_drmModeObjectGetProperties(
    int fd, uint32_t id, uint32_t type) {
  (void)fd; (void)id; (void)type;
  return &props;
}
drmModePropertyRes *__wrap_drmModeGetProperty(int fd, uint32_t id) {
  (void)fd;
  strcpy(prop.name, property_names[id]);
  return &prop;
}
void __wrap_drmModeFreeProperty(drmModePropertyRes *p) { (void)p; }
void __wrap_drmModeFreeObjectProperties(drmModeObjectProperties *p) { (void)p; }
int main(void) {
  struct drm_plane_snapshot snapshot = {0};
  assert(snapshot_drm_plane_geometry(0, &snapshot));
  assert((int32_t)snapshot.crtc_x == -7 && snapshot.crtc_y == 53);
  assert(snapshot.crtc_w == 430 && snapshot.crtc_h == 534);
  assert(snapshot.src_x == (1U << 15) && snapshot.src_y == (2U << 16));
  assert(snapshot.src_w == (144U << 16) && snapshot.src_h == (160U << 16));
  props.count_props = 7;
  assert(!snapshot_drm_plane_geometry(0, &snapshot));
  props.count_props = 9;
  vals[3] = 0;
  assert(!snapshot_drm_plane_geometry(0, &snapshot));
  puts("plane_geometry_test=result-ok");
  return 0;
}
