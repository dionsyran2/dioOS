#include <syscalls/syscalls.h>
#include <sys/uio.h>

int64_t writev(int fd, const struct iovec *iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) return -EINVAL;

    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);
    
    if (!file) return -EBADF;
    int acc = file->flags & O_ACCMODE;
    if (acc != O_WRONLY && acc != O_RDWR) return -EBADF;

    // Fast path
    if (iovcnt == 0) return 0;

    // Copy the entire iovec array into kernel space
    size_t iov_array_size = iovcnt * sizeof(struct iovec);
    struct iovec *k_iov = new struct iovec[iovcnt];
    
    if (self->read_from_userspace(k_iov, iov, iov_array_size) < 0) {
        delete[] k_iov;
        return -EFAULT;
    }

    // Validate total length to prevent integer overflow attacks
    size_t total_to_write = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (INT64_MAX - total_to_write < k_iov[i].iov_len) {
            delete[] k_iov;
            return -EINVAL;
        }
        total_to_write += k_iov[i].iov_len;
    }

    if (total_to_write == 0) {
        delete[] k_iov;
        return 0;
    }

    // Allocate a single 4KB bounce buffer for streaming
    void *kbuf = malloc(MAX_WRITE_CHUNK);
    if (!kbuf) {
        delete[] k_iov;
        return -ENOMEM;
    }

    size_t total_written = 0;
    int err = 0;

    file->lock.lock();
    for (int i = 0; i < iovcnt; i++) {
        const char *user_ptr = (const char *)k_iov[i].iov_base;
        size_t remaining_in_iov = k_iov[i].iov_len;

        while (remaining_in_iov > 0) {
            size_t chunk = (remaining_in_iov > MAX_WRITE_CHUNK) ? MAX_WRITE_CHUNK : remaining_in_iov;

            // Pull the actual string/data data from userspace
            if (self->read_from_userspace(kbuf, user_ptr, chunk) < 0) {
                if (total_written == 0) err = -EFAULT;
                goto done; // Break out of BOTH loops
            }

            if (file->flags & O_APPEND) {
                file->offset = file->node->size; 
            }

            int ret = file->node->write(kbuf, chunk, file->offset);
            if (ret < 0) {
                if (total_written == 0) err = ret;
                goto done;
            }

            file->offset += ret;
            total_written += ret;
            user_ptr += ret;
            remaining_in_iov -= ret;

            // If the driver accepted less than we asked (e.g., pipe is full), 
            // we must stop immediately to maintain contiguous data integrity.
            if (ret < (int)chunk) {
                goto done;
            }
        }
    }

done:
    file->lock.unlock();

    free(kbuf);
    delete[] k_iov;

    return (total_written > 0) ? total_written : err;
}

REGISTER_SYSCALL(SYS_writev, writev);