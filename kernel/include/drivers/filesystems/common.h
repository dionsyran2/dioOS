#pragma once
#include <stdint.h>
#include <stddef.h>

struct vnode_t;
struct dentry_t;

class filesystem_t {
    public:
    virtual dentry_t *mount() = 0; // Loads any filesystem data and returns the root dentry
    virtual void umount() = 0; // Frees any filesystem data

    virtual ~filesystem_t() = default;

    protected:
    filesystem_t(vnode_t *disk);
};

class filesystem_registration_t {
    public:
    bool (*has_valid_fs)(vnode_t *disk);
    filesystem_t *(*create_fs)(vnode_t *disk);
};

void register_filesystem(filesystem_registration_t *fs);

filesystem_t *find_filesystem(vnode_t *disk);