/*******************************************************************/
/* This file contains definitions for the vfs (Virtual File System)*/
/*******************************************************************/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "vfs_common.h"
#include <BasicRenderer.h>

namespace vfs{
    int def_read_dir(vnode** out_list, size_t* out_count, vnode_t* this_node);
    void _print_dir(vnode_t* node);
    void _print_tree(vnode_t* node, int depth = 0, bool is_last = true, char* prefix = nullptr);
    vnode_t* get_root_node();
    vnode_t* mount_node(char* name, VNODE_TYPE type, vnode_t* parent);
    vnode_t* resolve_path(const char* path);
    char* get_full_path_name(vnode_t* node);
    bool is_root();
}