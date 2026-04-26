#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <filesystem/memfs/memfs.h>
#include <syscalls/sockets.h>



#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN  0x001
#define EPOLLOUT 0x004

// The data union
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

// The event structure Xorg passes to the kernel
struct epoll_event {
    uint32_t events;
    epoll_data_t data;
} __attribute__((packed));

// Our internal tracking struct (attach this to the vnode)
struct watched_fd_t {
    int fd;
    uint32_t events;
    epoll_data_t data;
    watched_fd_t* next;
};

struct epoll_instance_t {
    watched_fd_t* watch_list; // Simple linked list of watched fdets
};

int sys_epoll_create1(int flags) {
    task_t *self = task_scheduler::get_current_task();
    
    // Create an anonymous node for the epoll instance
    vnode_t *epoll_node = new vnode_t(VREG); // Or V_ANON if you have it
    
    epoll_instance_t *instance = new epoll_instance_t();
    instance->watch_list = nullptr;
    
    epoll_node->file_identifier = (uint64_t)instance;
    epoll_node->open();
    
    // Assign it a file descriptor in this task
    return self->open_node(epoll_node, 0)->num;
}
REGISTER_SYSCALL(291, sys_epoll_create1);

// Sometimes Xorg uses the older epoll_create
int sys_epoll_create(int size) {
    return sys_epoll_create1(0); 
}
REGISTER_SYSCALL(213, sys_epoll_create);

int sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *user_event) {
    task_t *self = task_scheduler::get_current_task();
    
    fd_t *epoll_fd = self->get_fd(epfd);
    if (!epoll_fd) return -EBADF;

    epoll_instance_t *instance = (epoll_instance_t *)epoll_fd->node->file_identifier;
    if (!instance) return -EINVAL;

    // Read the event struct from userspace
    epoll_event event;
    if (op != EPOLL_CTL_DEL) {
        self->read_memory(user_event, &event, sizeof(epoll_event));
    }

    if (op == EPOLL_CTL_ADD) {
        watched_fd_t *watch = new watched_fd_t();
        watch->fd = fd;
        watch->events = event.events;
        watch->data = event.data;
        watch->next = instance->watch_list;
        instance->watch_list = watch;
        return 0;
    } 
    else if (op == EPOLL_CTL_DEL) {
        watched_fd_t *prev = nullptr;
        for (watched_fd_t *curr = instance->watch_list; curr != nullptr; curr = curr->next) {
            if (curr->fd == fd) {
                if (prev) prev->next = curr->next;
                else instance->watch_list = curr->next;
                delete curr;
                return 0;
            }
            prev = curr;
        }
        return -ENOENT;
    }
    else if (op == EPOLL_CTL_MOD) {
        for (watched_fd_t *curr = instance->watch_list; curr != nullptr; curr = curr->next) {
            if (curr->fd == fd) {
                curr->events = event.events;
                curr->data = event.data;
                return 0;
            }
        }
        return -ENOENT;
    }

    return -EINVAL;
}
REGISTER_SYSCALL(233, sys_epoll_ctl);

int sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    task_t *self = task_scheduler::get_current_task();
    
    fd_t *epoll_fd = self->get_fd(epfd);
    if (!epoll_fd) return -EBADF;

    epoll_instance_t *instance = (epoll_instance_t *)epoll_fd->node->file_identifier;
    if (!instance) return -EINVAL;

    // Buffer to hold events before writing to userspace
    epoll_event *kernel_events = (epoll_event *)malloc(sizeof(epoll_event) * maxevents);
    if (!kernel_events) return -ENOMEM;

    while (true) {
        asm volatile("" : : : "memory");

        int events_found = 0;

        for (watched_fd_t *curr = instance->watch_list; curr != nullptr; curr = curr->next) {
            if (events_found >= maxevents) break;

            fd_t *fd = self->get_fd(curr->fd);
            if (!fd) continue; 

            bool has_data = fd->node->pollout(); 

            if (has_data && (curr->events & EPOLLIN)) {
                kernel_events[events_found].events = EPOLLIN;
                kernel_events[events_found].data = curr->data;
                events_found++;
            }
        }

        // If we found events, copy them to userspace and return!
        if (events_found > 0) {
            self->write_memory(events, kernel_events, sizeof(epoll_event) * events_found);
            free(kernel_events);
            return events_found;
        }

        // If no events and timeout is 0 (WNOHANG), return 0 immediately.
        if (timeout == 0) {
            free(kernel_events);
            return 0;
        }

        self->ScheduleFor(20, BLOCKED, WAITING_ON_EVENT); 

        // If a signal (like SIGKILL) arrived while sleeping, abort.
        if (self->signal_count == 0 && self->woke_by_signal) {
            free(kernel_events);
            return -EINTR;
        }
        
        // If timeout > 0, decrement it so we don't block forever
        if (timeout > 0) {
            timeout -= 5;
            if (timeout <= 0) {
                free(kernel_events);
                return 0;
            }
        }
    }
}
REGISTER_SYSCALL(232, sys_epoll_wait);
REGISTER_SYSCALL(281, sys_epoll_wait);