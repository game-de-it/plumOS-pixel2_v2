#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * DraStic 2.5.0.4 creates small POSIX shared-memory files and later maps the
 * complete emulated RAM/VRAM ranges from them.  On the Pixel2 plumOS runtime
 * this can SIGBUS when the core touches a page beyond the current file end.
 * Extend only the two exact DraStic mappings; unrelated mmap calls are left
 * untouched.
 */

static int main_memory_fd = -1;
static off_t main_memory_size;
static int vram_fd = -1;
static off_t vram_size;

int ftruncate(int fd, off_t length)
{
    if (fd == main_memory_fd && length < main_memory_size) {
        length = main_memory_size;
    } else if (fd == vram_fd && length < vram_size) {
        length = vram_size;
    }
    return syscall(SYS_ftruncate, fd, length);
}

void *mmap(void *address, size_t length, int protection, int flags,
           int fd, off_t offset)
{
    struct stat status;

    if (fd >= 0 && (flags & MAP_SHARED) != 0 &&
        fstat(fd, &status) == 0) {
        off_t required_size = offset + (off_t)length;
        int is_main_memory =
            (uintptr_t)address == 0x8000U &&
            length == 0x3ff9000U &&
            status.st_size == 0x414000;
        int is_vram =
            address == NULL &&
            length == 0x800000U &&
            status.st_size == 0xa8000;

        if (is_main_memory || is_vram) {
            if (is_main_memory) {
                main_memory_fd = fd;
                main_memory_size = required_size;
            } else {
                vram_fd = fd;
                vram_size = required_size;
            }
            if (syscall(SYS_ftruncate, fd, required_size) != 0) {
                return MAP_FAILED;
            }
        }
    }

#if defined(__arm__) && defined(SYS_mmap2)
    if ((offset & 4095) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    return (void *)syscall(SYS_mmap2, address, length, protection, flags, fd,
                           (unsigned long)offset >> 12);
#else
    return (void *)syscall(SYS_mmap, address, length, protection, flags, fd,
                           offset);
#endif
}
