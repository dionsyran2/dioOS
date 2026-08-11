#pragma once
#include <stdint.h>
#include <stddef.h>

struct vnode_t;
struct dentry_t;
struct vnode_attributes_t;
struct poll_table_t;

struct vnode_file_operations_t {
    // @brief Reads up to 'size' bytes starting at 'offset'
    // @returns Bytes read
    int (*read)(vnode_t *node, void *buffer, size_t size, size_t offset);

    // @brief Writes 'size' bytes starting at 'offset'
    // @returns Bytes written
    int (*write)(vnode_t *node, const void *buffer, size_t size, size_t offset);

    int (*ioctl)(vnode_t *node, int op, char* argp);

    /* It will 'poll' and if its not available, it will add poll_table_t to a list.
       After that, the caller can freely block, until poll_table_t->cb is called which 
       should unblock and remove poll_table_t from any lists its actively in
    */
    int (*poll)(vnode_t *node, int events, poll_table_t *pt);

    // @brief Sets the attributes (owner id, group id, permissions)
    // @return Returns 0 on success, < 0 on failure 
    int (*set_attributes)(vnode_t *node, vnode_attributes_t *attrs);

    // @brief Tries to lookup a child by 'name'
    // @returns A 'dentry_t *' representing the child
    dentry_t *(*lookup)(vnode_t *node, const char *name);

    // @brief Returns a listing of up to 'limit' dentry_t starting at children #'offset'
    // @returns The count of the children in the 'out' variable
    // @warning The 'out' memory is allocated via new[] and should be deleted via delete[]!
    int (*get_listing)(vnode_t *node, dentry_t *&out, size_t offset, size_t limit);

    // @brief It creates a regular file named 'name' with 'mode' permissions
    // @return Returns 0 on success, < 0 on failure 
    int (*creat)(vnode_t *node, const char *name, uint16_t mode);

    // @brief It creates a directory named 'name' with 'mode' permissions
    // @return Returns 0 on success, < 0 on failure
    int (*mkdir)(vnode_t *node, const char *name, uint16_t mode);

    // @brief It will create a link
    // @return Returns 0 on success, < 0 on failure
    int (*mklink)(vnode_t *node, const char *name, const char *linkpath);

    // @brief It will try to unlink a children by name
    // @return Returns 0 on success, < 0 on failure
    int (*unlink)(vnode_t *node, const char *child);

    // @brief It will try to remove a directory by name
    // @return Returns 0 on success, < 0 on failure
    int (*rmdir)(vnode_t *node, const char *child);


    // @brief Called after the last reference to the vnode is closed, and all links pointing to it have been removed
    void (*evict_inode)(vnode_t *node);
};