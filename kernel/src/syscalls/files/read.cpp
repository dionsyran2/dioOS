#include <syscalls/syscalls.h>
#include <bits/fcntl.h>
#include <kerrno.h>
#include <bits/poll.h>

#define MAX_READ_CHUNK 4096

int64_t read(unsigned int fd, char *buf, size_t count){
    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADF;

    if (file->node->poll(POLLIN, nullptr) == 0 && file->flags & O_NONBLOCK) return -EWOULDBLOCK;
    
    // Check if opened for reading (Must be RDONLY or RDWR)
    int acc = file->flags & O_ACCMODE;
    if (acc != O_RDONLY && acc != O_RDWR) return -EBADF;

    // Fast path for 0-byte writes
    if (count == 0) return 0;

    // Allocate a safe, fixed-size buffer in the kernel
    size_t chunk_size = (count > MAX_READ_CHUNK) ? MAX_READ_CHUNK : count;
    void *kbuf = malloc(chunk_size);
    if (!kbuf) return -ENOMEM;

    int err = 0;
    size_t bytes_read_total = 0;
    const char* user_ptr = buf;

    // Acquire the mutex so we dont mangle the offset
    file->lock.lock();

    while (bytes_read_total < count){
        size_t to_read = min(count - bytes_read_total, MAX_READ_CHUNK);
        // Read the next chunk
        int r = file->node->read(kbuf, to_read, file->offset);

        // Check for errors
        if (r < 0){
            err = r;
            break;
        }

        // Write to the userspace
        self->write_to_userspace(buf + bytes_read_total, kbuf, r);

        // Advance offset
        file->offset += r;
        bytes_read_total += r;

        // If we did not read the full amount, we probably reached EOF
        if (r < MAX_READ_CHUNK) break;
    }

    // Free the mutex
    file->lock.unlock();

    free(kbuf);
    return bytes_read_total ? bytes_read_total : err;
}

REGISTER_SYSCALL(SYS_read, read);