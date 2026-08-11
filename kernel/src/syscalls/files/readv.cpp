#include <syscalls/syscalls.h>
#include <sys/uio.h>

#define MAX_READ_CHUNK 4096

int64_t sys_readv(unsigned int fd, const struct iovec *iov, int iovcnt) {
    if (iovcnt < 0) return -EINVAL; // Protect against negative counts
    if (iovcnt == 0) return 0;

    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADF;

    int acc = file->flags & O_ACCMODE;
    if (acc != O_RDONLY && acc != O_RDWR) return -EBADF;

    // 1. Safely copy the iovec array from userspace to the kernel
    size_t iov_array_size = sizeof(struct iovec) * iovcnt;
    struct iovec *kiov = (struct iovec *)malloc(iov_array_size);
    if (!kiov) return -ENOMEM;

    // *NOTE*: Assuming you have a read_from_userspace function to pair with write_to_userspace. 
    // If not, use standard memcpy if you are in a flat ring-0 space without SMAP restrictions.
    self->read_from_userspace(kiov, (void*)iov, iov_array_size);

    // 2. Allocate a single bounce buffer to reuse for all vectors
    void *kbuf = malloc(MAX_READ_CHUNK);
    if (!kbuf) {
        free(kiov);
        return -ENOMEM;
    }

    int err = 0;
    size_t total_bytes_read = 0;

    // Acquire lock once for the entire scatter/gather operation
    file->lock.lock();

    for (int i = 0; i < iovcnt; i++) {
        size_t count = kiov[i].iov_len;
        char *buf = (char *)kiov[i].iov_base;
        size_t vec_bytes_read = 0;

        if (count == 0) continue;

        while (vec_bytes_read < count) {
            size_t to_read = min(count - vec_bytes_read, MAX_READ_CHUNK);
            int r = file->node->read(kbuf, to_read, file->offset);

            if (r < 0) {
                err = r;
                break;
            }

            if (r == 0) break; // EOF reached

            self->write_to_userspace(buf + vec_bytes_read, kbuf, r);

            file->offset += r;
            vec_bytes_read += r;
            total_bytes_read += r;

            if (r < to_read) break; // Short read (EOF)
        }

        // If we hit an error or EOF in this vector, abort processing further vectors
        if (err < 0 || vec_bytes_read < count) {
            break; 
        }
    }

    file->lock.unlock();

    free(kbuf);
    free(kiov);

    return total_bytes_read ? total_bytes_read : err;
}

REGISTER_SYSCALL(SYS_readv, sys_readv);