#pragma once
#include <stdint.h>
#include <stddef.h>

#include <filesystem/vfs/vfs_common.h>

namespace vfs{
    void initialize_vfs();
    vnode_t* get_root_node();
    int mount(vnode_t* node, vnode_t* mount_target);
    int add_node(vnode_t* parent, vnode_t* node);
    int rm_node(vnode_t* parent, vnode_t* node);
    int vfs_check_permission(vnode_t* node, int desired_access);
    vnode_t* resolve_path(const char* path, bool follow_links = true, bool follow_trailing = true);
    void print_tree(vnode_t* node, int depth = 0, int max_depth = 100);
    char* get_full_path_name(vnode_t* node);
}

vnode_t* create_root_node_fs(vnode_t* blk);
