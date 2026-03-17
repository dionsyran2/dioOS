#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>


long sys_creat(const char* ustring, int mode){
    task_t* self = task_scheduler::get_current_task();
    char pathname[2048];
    fix_path(ustring, pathname, sizeof(pathname), self);

    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    
    char* parent;
    char* name;
    split_path(pathname, parent, name);

    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) return -ENOENT;
    if (pnode->type != VDIR) return -ENOTDIR;
    int ret = pnode->creat(name);

    free(parent);
    free(name);

    if (ret < 0) return ret;

    vnode_t* nd = vfs::resolve_path(pathname);
    if (nd == nullptr) return -ENOENT;

    nd->uid = self->euid;
    nd->gid = self->egid;
    nd->permissions = mode & 0xFFF;

    nd->save_metadata();

    fd_t* fd = self->open_node(nd);
    fd->flags = O_WRONLY;
    return fd->num;
}
REGISTER_SYSCALL(SYS_creat, sys_creat);