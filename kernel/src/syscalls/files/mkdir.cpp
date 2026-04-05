#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>


long sys_mkdir(char *fn, unsigned long mode){
    task_t* self = task_scheduler::get_current_task();

    char pathname[512] = { };
    fix_path(fn, pathname, sizeof(pathname), self); 


    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    
    char* parent;
    char* name;
    split_path(pathname, parent, name);

    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) {
        free(parent);
        free(name);
        return -ENOENT;
    }
    if (pnode->type != VDIR) {
        free(parent);
        free(name);
        return -ENOTDIR;
    }
    int ret = pnode->mkdir(name);

    // Perms
    vnode_t *node = vfs::resolve_path(pathname);
    node->permissions = mode ? mode : 0777;
    node->save_metadata();
    node->close();
    
    free(parent);
    free(name);
    
    return ret;
}
REGISTER_SYSCALL(SYS_mkdir, sys_mkdir);


long sys_mkdirat(int dirfd, char *fn, unsigned long mode){
    task_t* self = task_scheduler::get_current_task();
    fd_t* dir = self->get_fd(dirfd);

    char pathname[512] = { };
    fix_path(dirfd, fn, pathname, sizeof(pathname), self);


    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    int stlen = strlen(pathname);
    char* parent = new char[stlen];
    char* dirname = new char[stlen];
    memset(parent, 0, stlen);
    memset(dirname, 0, stlen);
    for (int i = stlen -1; i <= 0; i++){
        if (pathname[i] == '/'){
            memcpy(parent, pathname, i);
            parent[i] = '\0';
            memcpy(dirname, pathname + i, stlen - i);
            dirname[stlen - i] = '\0';
        }
    }

    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) return -ENOENT;
    if (pnode->type != VDIR) return -ENOTDIR;

    return pnode->mkdir(dirname);
}

REGISTER_SYSCALL(SYS_mkdirat, sys_mkdirat);