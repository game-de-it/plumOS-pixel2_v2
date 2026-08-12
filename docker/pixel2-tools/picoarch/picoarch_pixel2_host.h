#ifndef PLUMOS_PICOARCH_PIXEL2_HOST_H
#define PLUMOS_PICOARCH_PIXEL2_HOST_H

#include <stdbool.h>

bool pixel2_get_perf_interface(void *data);
bool pixel2_get_vfs_interface(void *data, const char *core_path);

#endif
