#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>
#include <helpers/pipes.h>


long pipe(int pipefd[2]){
    task_t *self = task_scheduler::get_current_task();
    pipe_t *pipe = create_pipe();

    fd_t *read = self->open_node(pipe->read_node);
    //read->flags = O_RDONLY;

    fd_t *write = self->open_node(pipe->write_node);
    //write->flags = O_WRONLY;

    int out[2];
    out[0] = read->num;
    out[1] = write->num;

    bool success = self->write_memory(pipefd, out, sizeof(int) * 2);
    return success ? 0 : -EFAULT;
}

REGISTER_SYSCALL(SYS_pipe, pipe);

long pipe2(int pipefd[2], int flags){
    task_t *self = task_scheduler::get_current_task();
    pipe_t *pipe = create_pipe();

    fd_t *read = self->open_node(pipe->read_node);
    //read->flags = flags | O_RDONLY;

    fd_t *write = self->open_node(pipe->write_node);
    //write->flags = flags | O_WRONLY;

    int out[2];
    out[0] = read->num;
    out[1] = write->num;

    bool success = self->write_memory(pipefd, out, sizeof(int) * 2);
    return success ? 0 : -EFAULT;
}
REGISTER_SYSCALL(SYS_pipe2, pipe2);
