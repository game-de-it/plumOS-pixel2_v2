// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *mode;

    if (argc != 2 || strcmp(argv[1], "charge") != 0) {
        fprintf(stderr, "usage: %s charge\n", argv[0]);
        return 2;
    }
    mode = argv[1];

    sync();
    fprintf(stderr, "power=reboot-mode-request method=restart2 mode=%s\n", mode);
    if (syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART2, mode) < 0) {
        fprintf(stderr, "plumos-reboot-mode: restart2(%s): %s\n",
                mode, strerror(errno));
        return 1;
    }

    fprintf(stderr, "plumos-reboot-mode: restart2 returned unexpectedly\n");
    return 1;
}
