#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*system_fn)(const char *command);

int system(const char *command) {
  static system_fn real_system;
  const char *root;
  char rewritten[4096];
  int length;

  if (real_system == NULL) {
    real_system = (system_fn)dlsym(RTLD_NEXT, "system");
    if (real_system == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }

  if (command == NULL || strncmp(command, "wget ", 5) != 0)
    return real_system(command);

  root = getenv("PLUMOS_ROOT");
  if (root == NULL || root[0] == '\0')
    root = "/mnt/plumos";
  length = snprintf(rewritten, sizeof(rewritten),
                    "%s/standalone/pico8/bin/wget%s", root, command + 4);
  if (length < 0 || (size_t)length >= sizeof(rewritten)) {
    errno = E2BIG;
    return -1;
  }

  fputs("[plumOS] PICO-8 download: using managed HTTPS adapter\n", stderr);
  return real_system(rewritten);
}
