#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>

#define SEEK_SET            0
#define SEEK_CUR            1
#define SEEK_END            2

long lseek(int fd, uint64_t offset, int whence){
    task_t *self = task_scheduler::get_current_task();
    fd_t* ofd = self->get_fd(fd);

    if (!ofd) return -EBADFD;

    switch (whence){
        case SEEK_SET:
            ofd->offset = offset;
            break;
        case SEEK_CUR:
            ofd->offset += offset;
            break;
        case SEEK_END:
            ofd->offset = ofd->node->size + offset;
            break;
    }

    return ofd->offset;
}

REGISTER_SYSCALL(SYS_lseek, lseek);