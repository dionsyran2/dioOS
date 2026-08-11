#include <syscalls/syscalls.h>

int getcwd(char *buf, size_t size){
    task_t *self = task_scheduler::get_current_task();

    char *path = self->fd_table->get_file_path(self->fd_table->cwd);

    if (!path) return -ENOENT;

    size_t path_len = strlen(path);

    if (size < path_len + 1) {
        delete[] path;
        return -ERANGE;
    }

    self->write_to_userspace(buf, path, path_len + 1);

    delete[] path;

    return path_len + 1;
};

REGISTER_SYSCALL(SYS_getcwd, getcwd);