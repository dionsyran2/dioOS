#include <VFS/vfs.h>
#include <memory.h>
#include <cstr.h>
#include <kerrno.h>
#include <scheduling/apic/apic.h>
#define MAX_CACHE_SIZE 128 * 1024 * 1024 // cache files under 128MB
int64_t read_vnode_cache(void* buffer, size_t cnt, size_t offset, vnode_t* node){
    if (offset >= node->size) return EOF;

    uint64_t to_read = min(node->size - offset, cnt);
    memcpy(buffer, node->cache + offset, to_read);
    return to_read;
}

int64_t write_vnode_cache(void* buffer, size_t cnt, size_t offset, vnode_t* node) {
    size_t total_size_required = offset + cnt;

    // If too large for cache, flush and write directly
    if (total_size_required > MAX_CACHE_SIZE) {
        if (node->write_cache && node->write_cache_size != 0) {
            node->write(node->write_cache, node->write_cache_size, node->write_cache_offset);
            GlobalAllocator.FreePages(node->write_cache, node->write_cache_size_in_pages);
            node->write_cache = nullptr;
            node->write_cache_offset = 0;
            node->write_cache_size = 0;
            node->write_cache_size_in_pages = 0;
        }
        return node->write(buffer, cnt, offset);
    }

    // Check if the write is outside the current cache window
    bool outside_cache_range = node->write_cache == nullptr ||
                               offset < node->write_cache_offset ||
                               (offset + cnt) > (node->write_cache_offset + node->write_cache_size);

    if (outside_cache_range) {
        // Flush current cache if any
        if (node->write_cache && node->write_cache_size != 0) {
            node->write(node->write_cache, node->write_cache_size, node->write_cache_offset);
            GlobalAllocator.FreePages(node->write_cache, node->write_cache_size_in_pages);
        }

        // Create a new cache window starting at `offset`
        node->write_cache_offset = offset;
        node->write_cache_size = total_size_required - offset;
        node->write_cache_size_in_pages = DIV_ROUND_UP(node->write_cache_size, 0x1000);
        node->write_cache = (uint8_t*)GlobalAllocator.RequestPages(node->write_cache_size_in_pages);
        memset(node->write_cache, 0, node->write_cache_size);
    }

    // Write into cache
    memcpy_simd(node->write_cache + (offset - node->write_cache_offset), buffer, cnt);
    node->edited = true;
    return cnt;
}


int vnode_t::open(){
    if (this == nullptr) return -EFAULT;
    if (type == VREG && size < MAX_CACHE_SIZE && (flags & VFS_NO_CACHE) == 0 && cache == nullptr) {
        uint8_t* cache = new uint8_t[size];
        read(cache, size, 0);
        this->cache = cache;
    }
    ref_cnt++;
    return 0;
}

int vnode_t::close(){
    if (this == nullptr) return -EFAULT;
    if (ref_cnt > 0) ref_cnt--;
    if (ref_cnt == 0 && cache != nullptr) {
        delete[] cache;
        cache = nullptr;
    }
    if (edited && write_cache){
        edited = false;
        bypass_write_cache = true;
        write(cache, write_cache_size, write_cache_offset);
        bypass_write_cache = false;
        GlobalAllocator.FreePages(write_cache, write_cache_size_in_pages);
        write_cache = nullptr;
        write_cache_offset = 0;
        write_cache_size = 0;
        write_cache_size_in_pages = 0;
        if (cache){
            delete[] cache;
            cache = nullptr;
        }
    }
    if (ref_cnt == 0 && !is_static) delete this;
    return 0;
}

int64_t vnode_t::truncate(size_t size){
    if (ops.truncate != nullptr){
        int64_t ret = ops.truncate(size, this);
        return ret < 0 ? ret : 0;
    }
    return -EFAULT;
}

int vnode_t::iocntl(int op, char* argp){
    if (ops.iocntl != nullptr){
        return ops.iocntl(op, argp, this);
    }
    serialf("IOCNTL ENOSYS %s\n", vfs::get_full_path_name(this));
    return -ENOSYS;
}

int64_t vnode_t::read(void* buffer, size_t cnt, size_t offset){
    if (ops.read != nullptr){
        if (cache) return read_vnode_cache(buffer, cnt, offset, this);
        return ops.read(buffer, cnt, offset, this);
    }
    return -ENOSYS;
}

int64_t vnode_t::write(const void* buffer, size_t cnt, size_t offset){
    if (flags & VFS_RO) return -EACCES;
    if (ops.write != nullptr){
        //if (type == VREG && size < MAX_CACHE_SIZE && (flags & VFS_NO_CACHE) == 0 && !bypass_write_cache) return write_vnode_cache((void*)buffer, cnt, offset, this);
        return ops.write(buffer, cnt, offset, this);
    }
    return -EROFS;
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

int vnode_t::read_dir(vnode_t** out){
    if (type != VDIR) return -ENOTDIR;
    vnode_t* start = static_children;
    vnode_t* end = nullptr;
    int cnt = 0;

    if (start != nullptr) {
        end = start;
        cnt = 1;
        while (end->next != nullptr) {
            end = end->next;
            cnt++;
        }
    }

    if (ops.read_dir != nullptr) {
        vnode_t* fs_ret = nullptr;
        int ret = ops.read_dir(&fs_ret, this);
        if (ret < 0) return ret;
        if (ret > 0){
            cnt += ret;

            start = fs_ret;
            if (static_children != nullptr){
                end = fs_ret;
                while(end->next != nullptr) end = end->next;
                end->next = static_children;
            }
        }
    }

    *out = start;
    return cnt;
}

int vnode_t::save_entry(){
    if (ops.save_entry != nullptr){
        return ops.save_entry(this);
    }else{
        return -ENOSYS;
    }
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

    int def_read_dir(vnode** out_list, vnode_t* this_node){
        return 0;
    }

    int64_t def_read(void* buffer, size_t cnt, size_t offset, vnode_t*){
        return 0;
    }

    int64_t def_write(const void* buffer, size_t cnt, size_t offset, vnode* this_node){
        return -EACCES;
    }

    void _create_start_node(){
        start_node = new vnode_t;
        memset(start_node, 0, sizeof(vnode_t));

        start_node->type = VNODE_TYPE::VDIR;
        strcpy(start_node->name, "/");
        strcpy(start_node->pathname, "/");
        start_node->ops.read_dir = def_read_dir;
        start_node->ops.read = def_read;
        start_node->is_static = true;
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
            char component[257];
            if (len >= sizeof(component))
                return nullptr; // name too long

            memcpy(component, start, len);
            component[len] = '\0';

            // Search for matching child
            vnode_t* next = nullptr;

            vnode_t* children;
            size_t cnt = current->read_dir(&children);

            if (strcmp("..", component) == 0){
                next = current->parent != nullptr ? current->parent : get_root_node();
            }else if (strcmp(".", component) == 0){
                next = current;
            }else{
                while (children != nullptr){
                    if (strcmp(children->name, component) == 0) {
                        if (next != nullptr && next->is_static == false) delete next;
                        next = children;
                        break;
                    }
                    vnode_t* prev = children;
                    children = children->next;
                    if (prev->is_static == false) delete prev;
                }
            }
            
            if (!next)
                return nullptr; // path component not found

            current = next;

            // Skip '/'
            start = (*end == '/') ? end + 1 : end;
        }
        strcpy(current->pathname, (char*)path);
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
        
        if (parent->static_children == nullptr){
            parent->static_children = node;
            parent->child_cnt = 1;
        }else{
            vnode_t* tnode = parent->static_children;
            
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
        node->is_static = true;

        node->ops.read_dir = def_read_dir;
        node->ops.read = def_read;
        node->ops.write = def_write;
        
        strcpy(node->name, name);

        return node;
    }

    void _print_dir(vnode_t* node){
        vnode_t* out_list;
        int out_count = node->read_dir(&out_list);
        if (out_count < 0) return;

        for (size_t i = 0; i < out_count; i++){
            kprintf("> %s\n", out_list->name);
            vnode_t* prev = out_list;
            out_list = out_list->next;
            if (prev->is_static == false) delete prev;
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

        vnode_t* children;
        int count = node->read_dir(&children);
        if (count < 0) return;

        for (size_t i = 0; i < count; i++) {
            bool last_child = (i == count - 1);
            if (strcmp("..", children->name) != 0 && strcmp(".", children->name) != 0 )
                _print_tree(children, depth + 1, last_child, new_prefix);
            vnode_t* prev = children;
            children = children->next;
            if (prev->is_static == false) delete prev;
        }
    }

    char* get_full_path_name(vnode_t* node){
        return node->pathname;
    }
}