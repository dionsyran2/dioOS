#include <VFS/vfs.h>
#include <memory.h>
#include <cstr.h>

namespace vfs{
    vnode_t* start_node = nullptr; // the start of the node (the "/" directory)

    int def_read_dir(vnode** out_list, size_t* out_count, vnode_t* this_node){
        int i = 0;
        for (; i < this_node->num_of_children; i++){
            out_list[i] = this_node->children[i];
        }

        *out_count = i;
        return 0;
    }

    void* def_load(size_t* cnt, vnode_t*){
        *cnt = 0;
        return nullptr;
    }

    void _create_start_node(){
        start_node = new vnode_t;
        memset(start_node, 0, sizeof(vnode_t));

        strcpy(start_node->name, "/");
        start_node->ops.read_dir = def_read_dir;
        start_node->ops.load = def_load;
    }

    vnode_t* get_root_node(){
        if (start_node == nullptr) _create_start_node();
        return start_node;
    }

    vnode_t* resolve_path(const char* path) {
        vnode_t* current = vfs::get_root_node();
        const char* start = path + 1; // skip the initial '/'
        const char* end;

        while (*start) {
            // Find the end of the next component
            end = start;
            while (*end && *end != '/')
                end++;

            size_t len = end - start;
            if (len == 0)
                break; // reached end of path

            // Copy component into buffer
            char component[33]; // match your `name[32]` + null terminator
            if (len >= sizeof(component))
                return nullptr; // name too long

            memcpy(component, start, len);
            component[len] = '\0';

            // Search for matching child
            vnode_t* next = nullptr;
            size_t cnt = 0;

            vnode_t* children[128];
            current->ops.read_dir(children, &cnt, current);
            for (int i = 0; i < cnt; i++) {
                if (strcmp(children[i]->name, component) == 0) {
                    next = children[i];
                    break;
                }
            }

            if (!next)
                return nullptr; // path component not found

            current = next;

            // Skip '/'
            start = (*end == '/') ? end + 1 : end;
        }

        return current;
    }


    vnode_t* mount_node(char* name, VNODE_TYPE type, vnode_t* parent){
        if (start_node == nullptr) _create_start_node();
        if (parent == nullptr) parent = start_node;


        // CHECK IF PATH ALREADY EXISTS BEFORE MOUNTING!

        vnode_t* node;

        switch(type){
            case VNODE_TYPE::VREG:{
                node = new vnode_t;
                memset(node, 0, sizeof(vnode_t));
                break;
            }

            case VNODE_TYPE::VDIR:{
                node = new vnode_t;
                memset(node, 0, sizeof(vnode_t));
                break;
            }

            case VNODE_TYPE::VBLK:{
                vblk_t* blk = new vblk_t;
                memset(blk, 0, sizeof(vblk_t));
                node = (vnode_t*)blk;
                break;
            }

            case VNODE_TYPE::VCHR:{
                node = new vnode_t;
                memset(node, 0, sizeof(vnode_t));
                break;
            }
        }
        
        parent->children[parent->num_of_children] = node;
        parent->num_of_children++;

        node->parent = parent;
        node->type = type;

        node->ops.read_dir = def_read_dir;
        node->ops.load = def_load;
        strcpy(node->name, name);

        return node;
    }

    void _print_dir(vnode_t* node){
        vnode_t* out_list[128];
        size_t out_count;

        node->ops.read_dir(out_list, &out_count, node);
        kprintf("LEN: %d\n", out_count);
        for (size_t i = 0; i < out_count; i++){
            kprintf("> %s\n", out_list[i]->name);
        }
    }

    void _print_tree(vnode_t* node, int depth, bool is_last, char* prefix) {
        static char local_prefix[256];

        if (!prefix) prefix = local_prefix;

        // Print prefix + branch
        if (depth > 0) {
            kprintf("%s", prefix);
            kprintf("%s", is_last ? "`-- " : "|-- ");
        }

        kprintf("%s\n", node->name);

        // Prepare prefix for children
        char new_prefix[256];
        strcpy(new_prefix, prefix);

        if (depth > 0) strcat(new_prefix, is_last ? "    " : "|   ");

        size_t count = 0;
        vnode_t* children[128];
        node->ops.read_dir(children, &count, node);

        for (size_t i = 0; i < count; i++) {
            bool last_child = (i == count - 1);
            _print_tree(children[i], depth + 1, last_child, new_prefix);
        }
    }
}