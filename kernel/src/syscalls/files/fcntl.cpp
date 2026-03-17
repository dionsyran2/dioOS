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
            fd_t* fd = self->get_fd(r);
            fd->flags |= O_CLOEXEC;
        }
        case F_DUPFD:
            return sys_dup(fd);
        case F_SETFD:
            ofd->flags = arg;
            break;
        case F_GETFD:
            return ofd->flags;
        case F_SETFL:
            ofd->flags = arg;
            break;
        case F_GETFL:
            return ofd->flags;
        /*case F_SETOWN_EX:{
            f_owner_ex* st = (f_owner_ex*)arg;
            ofd->own = st->pid;
            ofd->own_type = st->type;
            break;
        }
        case F_GETOWN_EX:{
            f_owner_ex* st = (f_owner_ex*)arg;
            st->pid = ofd->own;
            st->type = ofd->own_type;
            break;
        }*/

        case F_SETLKW:
        case F_SETLK: {
            return 0;
        }

        /*case F_SETLKW: {
            while (ofd->node->lock != 0 && ofd->node->lock != self->pid) 
                task_scheduler::schedule_until(GetTicks() + 20, DISABLED);

            flock* lock = (flock*)arg;

            if (lock->l_type == F_RDLCK || lock->l_type == F_WRLCK){
                ofd->node->lock = self->pid;
            }else if (lock->l_type == F_UNLCK){
                ofd->node->lock = 0;
            }
            
            return 0;
        }*/
        default:
            return -ENOSYS;
    }

    return 0;
}

REGISTER_SYSCALL(SYS_fcntl, sys_fcntl);
