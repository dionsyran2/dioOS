#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vfs/vfs.h>
#include <bits/fcntl.h>
#include <scheduling/mutex/mutex.h>

struct vnode_t;
struct dentry_t;


// Represents a file descriptor
struct file_t {
    vnode_t *node;
    dentry_t *dentry;

    uint64_t offset = 0;
    int flags = 0;
    
    int refcount = 0;
    mutex_t lock;

    void open();
    void close();
};

#define MAX_FDS 256
class fd_table_t {
    private:
    file_t *entries[MAX_FDS];
    spinlock_t lock = 0;
    int refcount = 0;

    public:
    dentry_t *cwd;

    void open();
    void close();


    int allocate_fd(file_t *file);
    int open_file(int dirfd, const char *filename, uint16_t flags, int mode, int fd = -1);
    int open_file(dentry_t *dentry, vnode_t *vnode, uint16_t flags, int fd = -1);
    int close_file(int fd);
    char *get_file_path(dentry_t *dentry);

    file_t *get_file(int fd);
};