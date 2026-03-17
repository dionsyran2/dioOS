#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>

#define MAX_WINDOW 500 * 1024 // 500 KB

long sendfile(int out_fd, int in_fd, int *offsetptr, size_t count){
    task_t* self = task_scheduler::get_current_task();

    fd_t *input = self->get_fd(in_fd);
    if (!input) return -EBADFD;
    //if ((input->flags & 0b11) == O_WRONLY) return -EBADF;

    fd_t *output = self->get_fd(out_fd);
    if (!output) return -EBADFD;
    //if (!((input->flags & 0b11) == O_RDWR || (input->flags & 0b11) == O_WRONLY)) return -EBADF;
    
    int offset = 0;

    if (offsetptr) {
        self->read_memory(offsetptr, &offset, sizeof(int));
    } else {
        offset = input->offset;
    }
    
    int bytes_transfered = 0;

    void *buffer = malloc(MAX_WINDOW);
    bool io_error = false;

    while (1){
        int transfer_size = MAX_WINDOW;
        int ret = input->node->read(offset, transfer_size, buffer);

        bool should_break = ret != transfer_size;

        if (ret < 0){
            io_error = true;
            break;
        }

        ret = output->node->write(output->offset, ret, buffer);
        if (ret > 0) output->offset += ret;
        
        if (should_break) break;
        
        offset += transfer_size;
        bytes_transfered += transfer_size;
    }

    free(buffer);

    if (offsetptr) {
        self->write_memory(offsetptr, &offset, sizeof(int));
    } else {
        input->offset = offset;
    }

    return io_error ? -EIO : bytes_transfered;
}

REGISTER_SYSCALL(SYS_sendfile, sendfile);