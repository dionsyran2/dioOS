#include <alloca.h>
#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>
#include <drivers/timers/common.h>

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200

#define FD_ZERO(s) do { int __i; unsigned long *__b=(s)->fds_bits; for(__i=sizeof (fd_set)/sizeof (long); __i; __i--) *__b++=0; } while(0)
#define FD_SET(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] |= (1UL<<((d)%(8*sizeof(long)))))
#define FD_CLR(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] &= ~(1UL<<((d)%(8*sizeof(long)))))
#define FD_ISSET(d, s) !!((s)->fds_bits[(d)/(8*sizeof(long))] & (1UL<<((d)%(8*sizeof(long)))))

#define FD_SETSIZE 1024
struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;



long sys_poll(pollfd *ufds, int nfds, int timeout_ms) {
    task_t* self = task_scheduler::get_current_task();

    pollfd* fds = (pollfd*)alloca(sizeof(pollfd) * nfds);
    self->read_memory(ufds, fds, sizeof(pollfd) * nfds);

    uint64_t start_time = GetTicks();
    bool any_ready = false;

    asm ("sti");
    while (true) {
        self->ScheduleFor(5, BLOCKED);

        any_ready = false;

        for (int i = 0; i < nfds; ++i) {
            fds[i].revents = 0;
            fd_t* fd = self->get_fd(fds[i].fd);

            if (!fd) continue;

            if ((fds[i].events & POLLIN) && fd->node->data_ready_to_read) {
                fds[i].revents |= POLLIN;
                any_ready = true;
            }

            if (fds[i].events & POLLOUT && fd->node->ready_to_receive_data) {
                fds[i].revents |= POLLOUT;
                any_ready = true;
            }
        }

        if (any_ready){
            self->write_memory(ufds, fds, sizeof(pollfd) * nfds);
            return nfds; // you could count the number of ready fds too
        }

        if (timeout_ms == 0){
            return 0;
        }

        if (timeout_ms > 0 && (GetTicks() - start_time) >= (uint64_t)timeout_ms){
            return 0;
        }

    }
    
    return -EAGAIN; // unreachable
}

REGISTER_SYSCALL(SYS_poll, sys_poll);

long sys_pselect(int nfds, fd_set* readfds_src, fd_set* writefds_src, fd_set* exceptfds_src, const timespec* timeout, void* sigmask){
    task_t* self = task_scheduler::get_current_task();
    
    fd_set rfd, wfd, efd, outrfd, outwfd, outefd;
    FD_ZERO(&rfd); 
    FD_ZERO(&wfd); 
    FD_ZERO(&efd);
    FD_ZERO(&outrfd); 
    FD_ZERO(&outwfd); 
    FD_ZERO(&outefd);

    if (readfds_src)   self->read_memory(readfds_src, &rfd, sizeof(fd_set));
    if (writefds_src)  self->read_memory(writefds_src, &wfd, sizeof(fd_set));
    if (exceptfds_src) self->read_memory(exceptfds_src, &efd, sizeof(fd_set));
    
    if (nfds < 0 || nfds > FD_SETSIZE) return -EINVAL;

    int timeout_ms = -1;
    if (timeout) {
        timespec ts;
        self->read_memory((void*)timeout, &ts, sizeof(timespec));
        timeout_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }

    int active_count = 0;
    for (int i = 0; i < nfds; ++i) {
        if ((readfds_src && FD_ISSET(i, &rfd)) || 
            (writefds_src && FD_ISSET(i, &wfd)) || 
            (exceptfds_src && FD_ISSET(i, &efd))) {
            active_count++;
        }
    }

    if (active_count == 0 && timeout_ms > 0) {
        self->ScheduleFor(timeout_ms, BLOCKED);
        return 0;
    }

    int ready = 0;

    uint64_t start_time = GetTicks();
    while (true){
        bool any_ready = false;

        for (int fd = 0; fd < nfds; ++fd) {
            bool is_ready = false;

            fd_t *ofd = self->get_fd(fd);
            if (!ofd) continue;


            if (readfds_src && FD_ISSET(fd, &rfd) && ofd->node->data_ready_to_read) {
                is_ready = true;
                FD_SET(fd, &outrfd);
            }

            if (writefds_src && FD_ISSET(fd, &wfd) && ofd->node->ready_to_receive_data) {
                is_ready = true;
                FD_SET(fd, &outwfd);
            }

            /*if (exceptfds_src && FD_ISSET(fd, &efd)) {
                is_ready = true;
                FD_SET(fd, &outefd);
            }*/

            if (is_ready){
                any_ready = true;
                ready++;
            }
        }

        if (any_ready) break;

        if (timeout_ms == 0){
            break;
        }

        if (timeout_ms > 0 && (GetTicks() - start_time) >= (uint64_t)timeout_ms){
            break;
        }
    }

    if (readfds_src) self->write_memory(readfds_src, &outrfd, sizeof(fd_set));
    if (writefds_src) self->write_memory(writefds_src, &outwfd, sizeof(fd_set));
    if (exceptfds_src) self->write_memory(exceptfds_src, &outefd, sizeof(fd_set));

    return ready;
}

REGISTER_SYSCALL(SYS_pselect, sys_pselect);