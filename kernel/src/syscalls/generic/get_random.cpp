#include <syscalls/syscalls.h>
#include <drivers/filesystems/devfs/devfs.h>

#define MAX_READ_CHUNK 4096

int64_t getrandom(char *buf, size_t count, uint32_t flags){
    vnode_t *node = devfs::resolve_path("/urandom");
    if (!node) return -EFAULT;

    task_t *self = task_scheduler::get_current_task();
    if (count == 0) return 0;

    // Allocate a safe, fixed-size buffer in the kernel
    size_t chunk_size = (count > MAX_READ_CHUNK) ? MAX_READ_CHUNK : count;
    void *kbuf = malloc(chunk_size);
    if (!kbuf) return -ENOMEM;

    int err = 0;
    size_t bytes_read_total = 0;
    const char* user_ptr = buf;

    while (bytes_read_total < count){
        size_t to_read = min(count - bytes_read_total, MAX_READ_CHUNK);
        // Read the next chunk
        int r = node->read(kbuf, to_read, 0);

        if (r < 0){
            err = r;
            break;
        }

        self->write_to_userspace(buf + bytes_read_total, kbuf, r);

        bytes_read_total += r;

        if (r < MAX_READ_CHUNK) break;
    }


    free(kbuf);
    return bytes_read_total ? bytes_read_total : err;
}

REGISTER_SYSCALL(SYS_getrandom, getrandom);