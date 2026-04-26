#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>


#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4

#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11


struct f_owner_ex {
	int	type;
	int	pid;
};

uint64_t sys_fcntl(int fd, uint64_t op, uint64_t arg){
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) return -EBADF;

    switch(op){
        case F_DUPFD_CLOEXEC:{
            int r = sys_dup(fd);
            fd_t* new_fd = self->get_fd(r);
            new_fd->flags |= O_CLOEXEC;
            return r;
        }
        case F_DUPFD:
            return sys_dup(fd);
            
        case F_SETFD:
            // F_SETFD takes 1 (FD_CLOEXEC) or 0. We must NOT overwrite O_NONBLOCK!
            if (arg & 1) {
                ofd->flags |= O_CLOEXEC;
            } else {
                ofd->flags &= ~O_CLOEXEC;
            }
            break;
            
        case F_GETFD:
            // Return 1 if FD_CLOEXEC is active, 0 otherwise
            return (ofd->flags & O_CLOEXEC) ? 1 : 0;
            
        case F_SETFL:
            // F_SETFL updates O_NONBLOCK, O_APPEND, etc. We must preserve O_CLOEXEC!
            ofd->flags = (arg & ~O_CLOEXEC) | (ofd->flags & O_CLOEXEC);
            break;
            
        case F_GETFL:
            return ofd->flags & ~O_CLOEXEC;

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
