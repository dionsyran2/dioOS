#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>


long sys_close(int fd){
    task_t* self = task_scheduler::get_current_task();

    self->close_fd(fd);
    return 0;
}

REGISTER_SYSCALL(SYS_close, sys_close);