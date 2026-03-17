#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>
#include <paging/PageFrameAllocator.h>

#define WRITE_WINDOW 0x4000 // 16KB chunk size
#define UIO_MAXIOV 1024

long sys_write(int fd, const void* data, size_t len){
    task_t* self = task_scheduler::get_current_task();
    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    
    if ((ofd->flags & (O_WRONLY | O_RDWR)) == 0) {
        return -EACCES;
    }

    if (ofd->node->type == VDIR) return -EISDIR;

    if (ofd->flags & O_APPEND)
        ofd->offset = ofd->node->size;

    void* buffer = malloc(WRITE_WINDOW);
    if (!buffer) return -ENOMEM;

    size_t total_written = 0;
    size_t remaining = len;
    const char* user_ptr = (const char*)data;

    while (remaining > 0) {
        size_t chunk_size = (remaining > WRITE_WINDOW) ? WRITE_WINDOW : remaining;

        if (!self->read_memory((void*)(user_ptr + total_written), buffer, chunk_size)) {
            free(buffer);
            return -EFAULT;
        }

        int64_t written = ofd->node->write(ofd->offset, chunk_size, buffer);

        if (written < 0) {
            if (total_written == 0) {
                free(buffer);
                return (int)written;
            }
            break; 
        }

        ofd->offset += written;
        total_written += written;

        if ((size_t)written < chunk_size) break;
        
        remaining -= written;
    }

    free(buffer);
    return total_written;
}
REGISTER_SYSCALL(SYS_write, sys_write);

long sys_writev(int fd, const struct iovec *iov_src, int iovcnt){
    task_t* self = task_scheduler::get_current_task();

    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) return -EINVAL;
    if (iov_src == nullptr) return -EFAULT;

    size_t iov_size = iovcnt * sizeof(struct iovec);
    
    struct iovec* iov = (struct iovec*)malloc(iov_size);
    if (!iov) return -ENOMEM;

    if (!self->read_memory((void*)iov_src, iov, iov_size)) {
        free(iov);
        return -EFAULT;
    }

    int total_len = 0;

    for (int i = 0; i < iovcnt; i++){
        int res = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        
        if (res < 0) {
            if (total_len > 0) break;
            free(iov); 
            return res;
        }
        total_len += res;
        
        if ((size_t)res < iov[i].iov_len) break;
    }

    free(iov);
    return total_len;
}
REGISTER_SYSCALL(SYS_writev, sys_writev);
