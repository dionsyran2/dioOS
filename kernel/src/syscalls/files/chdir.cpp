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
    
    if (node->type != VDIR) {
        return -ENOTDIR;
    }
    
    self->file_list->lock();

    vnode_t* old_node = self->file_list->cwd;
    
    self->file_list->cwd = node;
    
    if (old_node) old_node->close();

    self->file_list->unlock();
    return 0;
}
REGISTER_SYSCALL(SYS_chdir, sys_chdir);


long sys_fchdir(int fd){
    task_t* self = task_scheduler::get_current_task();
    
    self->file_list->lock();
    fd_t* ofd = self->get_fd(fd, false);

    if (ofd == nullptr) {
        self->file_list->unlock();
        return -EBADF;
    }

    if (ofd->node->type != VDIR) {
        self->file_list->unlock();
        return -ENOTDIR;
    }

    vnode_t* old_node = self->file_list->cwd;
    
    ofd->node->open(); 
    self->file_list->cwd = ofd->node;
    
    if (old_node) old_node->close();

    self->file_list->unlock();
    return 0;
}
REGISTER_SYSCALL(SYS_fchdir, sys_fchdir);