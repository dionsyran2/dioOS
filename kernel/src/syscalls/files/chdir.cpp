#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>


long sys_chdir(const char *fn){
    task_t* self = task_scheduler::get_current_task();

    char pathname[512] = { };
    fix_path(fn, pathname, sizeof(pathname), self); 


    vnode_t* node = vfs::resolve_path(pathname);
     
    if (node == nullptr) return -ENOENT;
    
    self->close_fd(self->cwd->num);
    self->cwd = self->open_node(node);
    return 0;
}

REGISTER_SYSCALL(SYS_chdir, sys_chdir);

long sys_fchdir(int fd){
    task_t* self = task_scheduler::get_current_task();
    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) return -ENOENT;
    self->close_fd(self->cwd->num);
    self->cwd = ofd;
    return 0;
}
REGISTER_SYSCALL(SYS_fchdir, sys_fchdir);


long sys_getcwd(char* buf, size_t size){
    if (buf == nullptr) return -EFAULT;
    if (size == 0) return -EINVAL;

    task_t* self = task_scheduler::get_current_task();

    char* path = vfs::get_full_path_name(self->cwd->node);
    if (strlen(path) >= size){
        free(path);
        return -ENAMETOOLONG;
    }
    
    int length = strlen(path) + 1;
    self->write_memory(buf, path, length);
    
    free(path);
    return length;
}
REGISTER_SYSCALL(SYS_getcwd, sys_getcwd);