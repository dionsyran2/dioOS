#include <syscalls/syscalls.h>
#include <sys/poll.h>
#include <drivers/timers/common.h>

void sys_poll_cb(poll_table_t *pt) {
    if (!pt || !pt->ctx) return;
    task_t *task = (task_t*)pt->ctx;
    
    task->unblock();
}

int sys_poll(struct pollfd *fds, int nfds, int timeout) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->fd_table) return -EFAULT;

    if (nfds == 0) {
        if (timeout > 0) {
             self->block(timeout, nullptr);
        }
        return 0; 
    }

    size_t array_size = sizeof(struct pollfd) * nfds;
    struct pollfd *kfds = (struct pollfd *)malloc(array_size);
    if (!kfds) return -ENOMEM;
    self->read_from_userspace(kfds, fds, array_size);

    poll_table_t *pt = new poll_table_t();
    pt->callback = sys_poll_cb;
    pt->ctx = self;
    pt->events = 0;

    for (int i = 0; i < nfds; i++) {
        if (kfds[i].fd >= 0) {
            pt->events |= kfds[i].events;
        }
    }

    int ready_count = 0;
    poll_table_t *current_pt = pt;

    uint64_t deadline = 0;
    if (timeout > 0) {
        deadline = time_since_boot + timeout;
    }

    while (true) {
        ready_count = 0;

        for (int i = 0; i < nfds; i++) {
            kfds[i].revents = 0;

            if (kfds[i].fd < 0) continue;

            file_t *file = self->fd_table->get_file(kfds[i].fd);
            if (!file) {
                kfds[i].revents = POLLNVAL;
                ready_count++;
                continue;
            }

            int mask = file->node->poll(kfds[i].events, current_pt);
            kfds[i].revents = mask & (kfds[i].events | POLLERR | POLLHUP);

            if (kfds[i].revents != 0) {
                ready_count++;
            }
        }

        if (ready_count > 0) break;
        if (timeout == 0) break;
        

        int time_left = timeout;
        if (timeout > 0) {
            uint64_t now = time_since_boot;
            if (now >= deadline) break;
            time_left = deadline - now;
        }
    
        current_pt = nullptr; 
        
        self->block(time_left, nullptr);
    }

    pt->poll_list.lock();
    for (int i = 0; i < pt->poll_list.size(); i++) {
        auto q = pt->poll_list.get(i);
        q->lock();
        
        for (int j = 0; j < q->size(); j++) {
            if (q->get(j) == pt) {
                q->remove(j);
                break;
            }
        }
        q->unlock();
    }
    pt->poll_list.unlock();
    delete pt;

    self->write_to_userspace(fds, kfds, array_size);
    free(kfds);

    return ready_count;
}

REGISTER_SYSCALL(SYS_poll, sys_poll);