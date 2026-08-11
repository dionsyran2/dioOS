#include <syscalls/syscalls.h>
#include <bits/fcntl.h>
#include <kerrno.h>
#include <bits/poll.h>

#define MAX_READ_CHUNK 4096

int64_t sys_pread64(unsigned int fd, char *buf, size_t count, off_t offset) {
    if (offset < 0) return -EINVAL; // pread requires a non-negative offset

    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADF;

    if (file->node->poll(POLLIN, nullptr) == 0 && file->flags & O_NONBLOCK) return -EWOULDBLOCK;
    
    int acc = file->flags & O_ACCMODE;
    if (acc != O_RDONLY && acc != O_RDWR) return -EBADF;

    if (count == 0) return 0;

    size_t chunk_size = (count > MAX_READ_CHUNK) ? MAX_READ_CHUNK : count;
    void *kbuf = malloc(chunk_size);
    if (!kbuf) return -ENOMEM;

    int err = 0;
    size_t bytes_read_total = 0;
    off_t current_offset = offset; // Track our local offset

    while (bytes_read_total < count) {
        size_t to_read = min(count - bytes_read_total, MAX_READ_CHUNK);
        
        // Read using our local offset, NO file->lock needed!
        int r = file->node->read(kbuf, to_read, current_offset);

        if (r < 0) {
            err = r;
            break;
        }

        if (r == 0) break; // EOF reached

        self->write_to_userspace(buf + bytes_read_total, kbuf, r);

        current_offset += r; // Advance local offset
        bytes_read_total += r;

        if (r < MAX_READ_CHUNK) break; // Short read implies EOF
    }

    free(kbuf);
    return bytes_read_total ? bytes_read_total : err;
}

REGISTER_SYSCALL(SYS_pread64, sys_pread64);