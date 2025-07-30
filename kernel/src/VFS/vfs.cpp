#include <VFS/vfs.h>
#include <memory.h>
#include <cstr.h>
#include <kerrno.h>

int vnode_t::open(){
    ref_cnt++;
    return 0;
}

int vnode_t::close(){
    ref_cnt--;
    if (ref_cnt == 0){
        if (data != nullptr) free(data);
        data_size = 0;
        data = nullptr;
    }
    return 0;
}

int vnode_t::iocntl(int op, char* argp){
    if (ops.iocntl != nullptr){
        return ops.iocntl(op, argp, this);
    }
    serialf("IOCNTL ENOSYS %s\n", vfs::get_full_path_name(this));
    return -ENOSYS;
}

int vnode_t::read(size_t* cnt, void** buf){
    if (ops.read != nullptr){
        if (type != VFIFO && type != VCHR && (flags & VFS_NO_CACHE) == 0){
            if (data == nullptr){
                size_t c = 0;
                data = (uint8_t*)ops.read(&c, this);
                data_size = c;
            }
            *buf = (void*)data;
            *cnt = data_size;
            return 0;
        }else{
            *buf = ops.read(cnt, this);
            return 0;
        }
    }
    return -ENOSYS;
}

int vnode_t::write(const char* txt, size_t length){
    if (flags & VFS_RO) return -EACCES;
    
    if (ops.write != nullptr){
        if (type != VFIFO && type != VCHR){
            if (data != nullptr) free(data);
            data = nullptr;
            serialf("Cleared!\n");
        }
        return ops.write(txt, length, this);
    }
    return -ENOSYS;
}

int vnode_t::mkfile(const char* fn, bool dir){
    if (dir){
        if (ops.mkdir != nullptr){
            return ops.mkdir(fn, this);
        }
    }else{
        if (ops.creat != nullptr){
            return ops.creat(fn, this);
        }
    }
    return -EROFS;
}


bool vblk::read(uint64_t block, uint64_t block_count, void* buffer){
    if (blk_ops.read != nullptr){
        return blk_ops.read(block, block_count, buffer, this);
    }
    return false;
}

bool vblk::write(uint64_t block, uint64_t block_count, const void* buffer){
    if (blk_ops.write != nullptr){
        return blk_ops.write(block, block_count, buffer, this);
    }
    return false;
}


namespace vfs{
    vnode_t* start_node = nullptr; // the start of the node (the "/" directory)

    int def_read_dir(vnode** out_list, size_t* out_count, vnode_t* this_node){
        int i = 0;
        vnode_t* node = this_node->children;
        while(node != nullptr){
            out_list[i] = node;
            node = node->next;
            i++;
        }

        *out_count = i;
        return 0;
    }

    void* def_read(size_t* cnt, vnode_t*){
        *cnt = 0;
        return nullptr;
    }

    int def_write(const char* txt, size_t length, vnode* this_node){
        return -EACCES;
    }

    void _create_start_node(){
        start_node = new vnode_t;
        memset(start_node, 0, sizeof(vnode_t));

        start_node->type = VNODE_TYPE::VDIR;
        strcpy(start_node->name, "/");
        start_node->ops.read_dir = def_read_dir;
        start_node->ops.read = def_read;
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

    bool root = false;
    bool is_root(){
        // should check if partition ids match and stuff... blah blah blah boring, just mount the first partition
        if (root) return false;
        root = true;
        return true;
    }

    vnode_t* mount_node(char* name, VNODE_TYPE type, vnode_t* parent, uint16_t inode, uint16_t fs_inode, uint16_t gid, uint16_t uid, uint32_t size){
        if (start_node == nullptr) _create_start_node();
        if (parent == nullptr) parent = start_node;


        // CHECK IF PATH ALREADY EXISTS BEFORE MOUNTING!

        vnode_t* node;

        switch(type){
            case VNODE_TYPE::VBLK:{
                vblk_t* blk = new vblk_t;
                memset(blk, 0, sizeof(vblk_t));
                node = (vnode_t*)blk;
                break;
            }

            default: {
                node = new vnode_t;
                memset(node, 0, sizeof(vnode_t));
                break;
            }
        }
        
        if (parent->children == nullptr){
            parent->children = node;
            parent->child_cnt = 1;
        }else{
            vnode_t* tnode = parent->children;
            
            while(true){
                if (tnode->next == nullptr){
                    tnode->next = node;
                    parent->child_cnt++;
                    break;
                }
                tnode = tnode->next;
            }
        }


        node->parent = parent;
        node->type = type;
        node->inode = inode;
        node->fs_inode = fs_inode;
        node->gid = gid;
        node->uid = uid;
        node->size = size;
        node->next = nullptr;
        node->data_read = true;
        node->data_write = true;

        node->ops.read_dir = def_read_dir;
        node->ops.read = def_read;
        node->ops.write = def_write;
        
        strcpy(node->name, name);

        return node;
    }

    void _print_dir(vnode_t* node){
        vnode_t* out_list[128];
        size_t out_count;

        node->ops.read_dir(out_list, &out_count, node);
        for (size_t i = 0; i < out_count; i++){
            kprintf("> %s\n", out_list[i]->name);
        }
    }

    void _print_tree(vnode_t* node, int depth, bool is_last, char* prefix) {
        if (node == nullptr) node = get_root_node();
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

    char* get_full_path_name(vnode_t* node){
        vnode_t* root = get_root_node();
        if (node == nullptr || node == root) return "/"; // root directory
        char* name = new char[strlen(node->name) + 2];
        name[0] = '/';
        name[1] = '\0';
        strcat(name, node->name); //'/app' -> '/to/app' -> '/path/to/app'
        
        vnode_t* pnode = node->parent;
        while(pnode != nullptr && pnode != root){
            char* old_name = name;
            name = new char[strlen(old_name) + strlen(pnode->name) + 2]; // +2, one for the '/' and one for the '\0'
            name[0] = '/';
            name[1] = '\0'; // null terminate it for strcat to work properly (just in case)
            strcat(name, pnode->name);
            strcat(name, old_name);
            pnode = pnode->parent;

            delete[] old_name;
        }
        return name;
    }
}