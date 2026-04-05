#pragma once
#include <stdint.h>
#include <stddef.h>

#include <filesystem/vfs/vfs_common.h>

namespace vfs{
    void initialize_vfs();
    vnode_t* get_root_node();
    int vfs_check_permission(vnode_t* node, int desired_access);
    vnode_t* resolve_path(const char* path, bool follow_links = true, bool follow_trailing = true);
    void print_tree(vnode_t* node, int depth = 0, int max_depth = 100);
    char* get_full_path_name(vnode_t* node);
    vnode_t* create_path(const char* absolute_path, vnode_type_t type);
}

vnode_t* create_root_node_fs(vnode_t* blk);
