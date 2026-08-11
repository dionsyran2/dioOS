#include <syscalls/syscalls.h>
#include <bits/fcntl.h>
#include <kerrno.h>

#define MAX_WRITE_CHUNK 4096 // 4KB chunks

int64_t write(unsigned int fd, const char *buf, size_t count){
    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADF;
    
    // Check if opened for writing (Must be WRONLY or RDWR)
    int acc = file->flags & O_ACCMODE;
    if (acc != O_WRONLY && acc != O_RDWR) return -EBADF;

    // Fast path for 0-byte writes
    if (count == 0) return 0;

    // Allocate a safe, fixed-size buffer in the kernel
    size_t chunk_size = (count > MAX_WRITE_CHUNK) ? MAX_WRITE_CHUNK : count;
    void *kbuf = malloc(chunk_size);
    if (!kbuf) return -ENOMEM;

    size_t bytes_written_total = 0;
    const char* user_ptr = buf;


    file->lock.lock();
    while (bytes_written_total < count) {
        size_t to_write = count - bytes_written_total;
        if (to_write > chunk_size) to_write = chunk_size;

        // Safely pull from userspace
        if (self->read_from_userspace(kbuf, user_ptr, to_write) < 0) {
            if (bytes_written_total == 0) bytes_written_total = -EFAULT;
            break;
        }

        // Handle O_APPEND: Force offset to EOF before writing
        if (file->flags & O_APPEND) {
            file->offset = file->node->size; 
        }

        int ret = file->node->write(kbuf, to_write, file->offset);
        
        if (ret < 0) {
            if (bytes_written_total == 0) bytes_written_total = ret; // Pass up the error
            break;
        }

        file->offset += ret;
        bytes_written_total += ret;
        user_ptr += ret;

        // If the driver wrote less than requested, stop (e.g., pipe full)
        if (ret < to_write) break; 
    }
    file->lock.unlock();

    free(kbuf);

    return bytes_written_total;
}

REGISTER_SYSCALL(SYS_write, write);