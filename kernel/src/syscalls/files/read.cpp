#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>

#define READ_WINDOW 0x4000

int sys_recvfrom(int sockfd, void *buf, size_t size, int flags, const struct sockaddr *source_addr, uint64_t addrlen);

long sys_read(int fd, char* user_buffer, size_t count){
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(fd);
    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type == VDIR) return -EISDIR;

    if (ofd->node->type == VSOC) return sys_recvfrom(fd, user_buffer, count, 0, nullptr, 0);
    /*if ((ofd->flags & O_WRONLY) == O_WRONLY)
        return -EACCES;*/

    uint64_t original_offset = ofd->offset;

    void* temp = malloc(READ_WINDOW);
    if (temp == nullptr) return -ENOMEM;


    size_t i = 0;
    while (i < count){
        if ((ofd->flags & O_NONBLOCK) == O_NONBLOCK && !ofd->node->pollout()) return -EWOULDBLOCK;

        size_t remaining_to_read = count - i;

        size_t chunk_size = (remaining_to_read > READ_WINDOW) ? READ_WINDOW : remaining_to_read;

        int64_t c = ofd->node->read(ofd->offset, chunk_size, temp); 
        
        if (c <= 0) {
            if (c < 0) { free(temp); return c; }
            break;
        }

        if (!self->write_memory(user_buffer + i, temp, c)) {
            free(temp);
            return -EFAULT;
        }
        
        ofd->offset += c;
        i += c;

        if ((size_t)c < chunk_size) break;
    }
    
    free(temp);

    return i;
}
REGISTER_SYSCALL(SYS_read, sys_read);

long sys_pread64(int fd, char* buffer, size_t count, size_t off){
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(fd);
    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type == VDIR) return -EISDIR;

    // Check bounds against file size (optional, but standard behavior varies)
    // Generally pread allows reading past EOF (returns 0 bytes)
    
    void* temp = malloc(READ_WINDOW);
    if (!temp) return -ENOMEM;

    size_t total_read = 0;
    uint64_t current_offset = off;

    while (total_read < count) {
        size_t remaining = count - total_read;
        size_t chunk = (remaining > READ_WINDOW) ? READ_WINDOW : remaining;

        int64_t bytes = ofd->node->read(current_offset, chunk, temp);
        
        if (bytes <= 0) {
            if (bytes < 0 && total_read == 0) {
                 free(temp); return bytes;
            }
            break; // EOF or error after partial read
        }

        // Copy to user buffer
        if (!self->write_memory(buffer + total_read, temp, bytes)) {
            free(temp);
            return -EFAULT;
        }

        current_offset += bytes;
        total_read += bytes;
        
        if ((size_t)bytes < chunk) break;
    }

    free(temp);
    return total_read;
}
REGISTER_SYSCALL(SYS_pread64, sys_pread64);

#define UIO_MAXIOV 1024

long sys_readv(int fd, const struct iovec *iov_src, int iovcnt){
    task_t* self = task_scheduler::get_current_task();

    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) return -EINVAL;
    if (iovcnt == 0) return 0;
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
        char* base = (char*)iov[i].iov_base;
        size_t len = iov[i].iov_len;

        int res = sys_read(fd, base, len);
        
        if (res < 0) {
            if (total_len > 0) break;
            free(iov);
            return res;
        }
        
        total_len += res;
        if ((size_t)res < len) break; // EOF hit
    }

    free(iov);
    return total_len;
}
REGISTER_SYSCALL(SYS_readv, sys_readv);

long sys_readlink(char* fn, char* buf, size_t bufsiz){
    task_t* self = task_scheduler::get_current_task();

    char pathname[512] = { };
    fix_path(fn, pathname, sizeof(pathname), self);

    if (strcmp(pathname, "/proc/self/fd/0") == 0){
        self->write_memory(buf, "/dev/tty0", 9);
        return 9;
    }

    vnode_t* node = vfs::resolve_path(pathname, false, false);
    
    if (!node) return -ENOENT;
    if (node->type != VLNK) return -EINVAL;

    char* target = node->read_link();
    node->close();
    if (!target) return -EIO;

    size_t len = strlen(target);
    size_t to_copy = (bufsiz < len) ? bufsiz : len;

    if (!self->write_memory(buf, target, to_copy)) {
        free(target);
        return -EFAULT;
    }
    
    free(target);
    
    return to_copy;
}
REGISTER_SYSCALL(SYS_readlink, sys_readlink);