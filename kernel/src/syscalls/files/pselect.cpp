#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>
#include <sys/select.h>
#include <sys/poll.h>

void sys_poll_cb(poll_table_t *pt);

long sys_pselect6(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const void *sigmask_struct) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->fd_table) return -EFAULT;

    if (nfds < 0 || nfds > FD_SETSIZE) return -EINVAL;

    fd_set in_read, in_write, in_except;
    fd_set out_read, out_write, out_except;
    
    memset(&in_read, 0, sizeof(fd_set));
    memset(&in_write, 0, sizeof(fd_set));
    memset(&in_except, 0, sizeof(fd_set));
    memset(&out_read, 0, sizeof(fd_set));
    memset(&out_write, 0, sizeof(fd_set));
    memset(&out_except, 0, sizeof(fd_set));

    if (readfds) self->read_from_userspace(&in_read, readfds, sizeof(fd_set));
    if (writefds) self->read_from_userspace(&in_write, writefds, sizeof(fd_set));
    if (exceptfds) self->read_from_userspace(&in_except, exceptfds, sizeof(fd_set));

    long timeout_ms = -1; // -1 means infinite block
    if (timeout) {
        struct timespec ts;
        if (self->read_from_userspace(&ts, (void*)timeout, sizeof(timespec)) < 0) return -EFAULT;
        timeout_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }

    uint64_t old_sigmask = self->blocked_signals;
    if (sigmask_struct) {
        pselect6_sigmask mask_data;
        if (self->read_from_userspace(&mask_data, (void*)sigmask_struct, sizeof(pselect6_sigmask)) == 0) {
            if (mask_data.ss) {
                uint64_t new_mask;
                self->read_from_userspace(&new_mask, mask_data.ss, sizeof(uint64_t));
                self->blocked_signals = new_mask;
            }
        }
    }

    poll_table_t *pt = new poll_table_t();
    pt->callback = sys_poll_cb;
    pt->ctx = self;
    pt->events = 0;

    int ready_count = 0;
    poll_table_t *current_pt = pt;
    
    uint64_t deadline = 0;
    if (timeout_ms > 0) deadline = time_since_boot + timeout_ms;

    long ret = 0;

    while (true) {
        self->blocking_lock.lock();
        self->current_state = INTERRUPTABLE;
        self->blocking_lock.unlock();

        ready_count = 0;
        
        // Wipe output masks for this iteration
        memset(&out_read, 0, sizeof(fd_set));
        memset(&out_write, 0, sizeof(fd_set));
        memset(&out_except, 0, sizeof(fd_set));

        for (int i = 0; i < nfds; i++) {
            bool wants_read = FD_ISSET(i, &in_read);
            bool wants_write = FD_ISSET(i, &in_write);
            bool wants_except = FD_ISSET(i, &in_except);

            if (!wants_read && !wants_write && !wants_except) continue;

            file_t *file = self->fd_table->get_file(i);
            if (!file) {
                ret = -EBADF; // select() must FAIL if an fd is invalid!
                goto cleanup; 
            }

            int events = 0;
            if (wants_read) events |= POLLIN;
            if (wants_write) events |= POLLOUT;
            if (wants_except) events |= POLLPRI;

            int revents = file->node->poll(events, current_pt);

            if (wants_read && (revents & (POLLIN | POLLERR | POLLHUP))) {
                FD_SET(i, &out_read);
                ready_count++;
            }
            if (wants_write && (revents & (POLLOUT | POLLERR | POLLHUP))) {
                FD_SET(i, &out_write);
                ready_count++;
            }
            if (wants_except && (revents & (POLLPRI | POLLERR | POLLHUP))) {
                FD_SET(i, &out_except);
                ready_count++;
            }
        }

        // --- Loop Exit Conditions ---
        if (ready_count > 0) {
            ret = ready_count;
            break;
        }
        if (timeout_ms == 0) {
            ret = 0;
            break;
        }
        if (self->block_status == -EINTR) {
            ret = -EINTR;
            break;
        }

        int time_left = timeout_ms;
        if (timeout_ms > 0) {
            uint64_t now = time_since_boot;
            if (now >= deadline) {
                ret = 0;
                break;
            }
            time_left = deadline - now;
        }

        current_pt = nullptr;
        
        self->blocking_lock.lock();
        if (self->current_state == INTERRUPTABLE) {
            self->block(time_left, nullptr, true);
        } else {
            self->current_state = RUNNING;
            self->blocking_lock.unlock();
        }
    }

cleanup:
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

    self->blocked_signals = old_sigmask;

    if (ret >= 0) {
        if (readfds) self->write_to_userspace(readfds, &out_read, sizeof(fd_set));
        if (writefds) self->write_to_userspace(writefds, &out_write, sizeof(fd_set));
        if (exceptfds) self->write_to_userspace(exceptfds, &out_except, sizeof(fd_set));
    }

    return ret;
}

REGISTER_SYSCALL(SYS_pselect6, sys_pselect6);
