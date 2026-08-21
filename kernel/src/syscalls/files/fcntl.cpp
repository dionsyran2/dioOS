#include <syscalls/syscalls.h>

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_SETLK		6
#define F_SETLKW	7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11


#define F_DUPFD_CLOEXEC	(1024 + 6)

struct f_owner_ex {
        int     type;
        int     pid;
};

long sys_dup(int oldfd);

uint64_t sys_fcntl(int fd, uint64_t op, uint64_t arg){
    task_t* self = task_scheduler::get_current_task();

    file_t* file = self->fd_table->get_file(fd);

    if (file == nullptr) return -EBADF;

    switch(op){
        case F_DUPFD_CLOEXEC:{
            int r = sys_dup(fd);
            file_t* new_fd = self->fd_table->get_file(r);
            new_fd->flags |= O_CLOEXEC;
            return r;
        }
        case F_DUPFD:
            return sys_dup(fd);

        case F_SETFD:
            if (arg & 1) {
                file->flags |= O_CLOEXEC;
            } else {
                file->flags &= ~O_CLOEXEC;
            }
            break;

        case F_GETFD:
            // Return 1 if FD_CLOEXEC is active, 0 otherwise
            return (file->flags & O_CLOEXEC) ? 1 : 0;

        case F_SETFL:
            // F_SETFL updates O_NONBLOCK, O_APPEND, etc. We must preserve O_CLOEXEC!
            file->flags = (arg & ~O_CLOEXEC) | (file->flags & O_CLOEXEC);
            break;

        case F_GETFL:
            return file->flags & ~O_CLOEXEC;

        case F_SETLKW:
        case F_SETLK: {
            return 0;
        }
        default:
            return -ENOSYS;
    }

    return 0;
}

REGISTER_SYSCALL(SYS_fcntl, sys_fcntl);