#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vfs/vnode.h>
#include <vfs/dentry.h>

#define MAX_SYMLINK_DEPTH 32

#define MAY_EXEC  1
#define MAY_WRITE 2
#define MAY_READ  4

#define AT_FDCWD -100

namespace vfs{
    void initialize();
    int allocate_filesystem_id();

    vnode_t *resolve_path(const char *path, int intent, int &err, bool follow_symlinks = true, bool follow_trailing = true);
    dentry_t *resolve_path_dentry(const char *path, int intent, int &err, bool follow_symlinks = true, bool follow_trailing = true);
    dentry_t *resolve_path_dentry_at(dentry_t *base_dir, const char *path, int intent, int &err, bool follow_symlinks = true, bool follow_trailing = true);
    
    vnode_t *resolve_path(const char *path);
    dentry_t *resolve_path_dentry(const char *path);

    void __release_vnode(vnode_t *node);
    void __invalidate_dentry_cache(const char *path);
    void __invalidate_vnode_cache(vnode_t *node);
    vnode_t *_get_vnode(dentry_t *dentry);
    dentry_t *_get_dentry(dentry_t *parent, const char *name);

    void mount(dentry_t *mountpoint, dentry_t *target);
    dentry_t *get_root();
    
    int mkdir(const char *path, uint16_t mode);
    int mkfile(const char *path, uint16_t mode);
    int mklink(const char *path, const char *target);
}