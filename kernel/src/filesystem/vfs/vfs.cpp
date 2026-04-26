#include <filesystem/vfs/vfs.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <cstr.h>
#include <kerrno.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <filesystem/fat32/fat32.h>
#include <filesystem/ext2/ext2.h>
#include <filesystem/memfs/memfs.h>
#include <structures/trees/avl_tree.h>
#include <pty/pty.h>
#include <CONFIG.h>

#define MAX_SYMLINK_DEPTH 8
#define PATH_SEPARATOR "/"

namespace filesystems{
    int current_dev_id = 0;
    int acquire_dev_id(){
        return __atomic_fetch_add(&current_dev_id, 1, __ATOMIC_SEQ_CST);
    }
}

void vnode_t::_add_child(vnode_t *child){
    uint64_t rflags = spin_lock(&this->list_lock);
    
    // Append the child to the list
    if (!this->children){
        this->children = child;
        child->next = nullptr;
        child->previous = nullptr;
    } else {
        child->next = nullptr;
        child->previous = this->last_children;
        this->last_children->next = child;
    }

    this->last_children = child;
    child->parent = this;

    spin_unlock(&this->list_lock, rflags);
}

void vnode_t::_remove_child(vnode_t *child, bool lock_held){
    uint64_t rflags = 0;
    if (!lock_held) rflags = spin_lock(&this->list_lock);
    
    if (this->last_children == child) this->last_children = child->previous;

    // Remove it from the list
    if (this->children == child){
        this->children = child->next;
        if (this->children) this->children->previous = nullptr;
    } else {
        // Patch the list
        if (child->previous) child->previous->next = child->next;
        if (child->next) child->next->previous = child->previous;
    }

    // Explicitly sever the child's ties to the list to prevent ghost pointers
    child->next = nullptr;
    child->previous = nullptr;

    // Removed!
    if (!lock_held) spin_unlock(&this->list_lock, rflags);
}

int vnode_t::_increase_refcount(){
    return __atomic_add_fetch(&this->ref_count, 1, __ATOMIC_SEQ_CST);
}

int vnode_t::_decrease_refcount(){
    return __atomic_sub_fetch(&this->ref_count, 1, __ATOMIC_SEQ_CST);
}

void vnode_t::_cleanup(){
    if (this->file_operations.cleanup) this->file_operations.cleanup(this);

    delete this;
}

void vnode_t::_cache_dir(){
    if (this->directory_cached) return;
    if (!this->file_operations.read_dir) return;

    vnode_t *list = nullptr;
    int count = this->file_operations.read_dir(&list, this);

    for (vnode_t* current = list; current != nullptr;){

        // Check if its already in the child list
        vnode_t *t = nullptr;
        this->find_file(current->name, &t);

        if (t) {
            // It somehow already exists, skip it for our own good
            t->close();
            vnode_t *next = current->next;
            current->_cleanup();
            current = next;
            continue;
        }
        
        // Add it to the list. Store the next pointer and set its own to null
        vnode_t *next = current->next;
        current->next = nullptr;
        
        // Add it to the children list
        this->_add_child(current);

        current = next;
    }
    
    this->directory_cached = true;
}

void vnode_t::_uncache_dir(){
    uint64_t rflags = spin_lock(&this->list_lock);

    // Itterate through the children
    for (vnode_t *current = this->children; current != nullptr;){
        if (current->ref_count != 0) {
            // Something has gone terribly wrong but a
            // small memory leak is better than a crash

            current = current->next;
            continue;
        }

        // Save the next one (_remove_child will not break the order)
        vnode_t *next = current->next;

        // Remove it from the list
        this->_remove_child(current, true);

        // Call its cleanup method
        current->_cleanup();

        // Continue
        current = next;
    }

    spin_unlock(&this->list_lock, rflags);
}

bool vnode_t::pollin(){
    if (this->file_operations.pollin){
        return this->file_operations.pollin(this);
    }

    return this->ready_to_receive_data;
}

bool vnode_t::pollout(){
    if (this->file_operations.pollout){
        return this->file_operations.pollout(this);
    }

    return this->data_ready_to_read;
}

int vnode_t::open(){
    this->_increase_refcount();

    if (!this->directory_cached && this->type == VDIR){
        this->_cache_dir();
    }

    if (this->parent) this->parent->open();

    return 0;
}

int vnode_t::close(){
    int count = this->_decrease_refcount();

    // We will only clean directories. If its refcount reaches 0
    // It means it and all of its children are closed, so its safe to clean it
    // We will not remove it from its parent list, as thats the parent's job, for when its
    // refcount reaches 0

    if (count == 0){
        if (this->directory_cached) {
            this->_uncache_dir();
            this->directory_cached = false;
        }

        if (this->parent) this->parent->close();

        if (this->file_operations.close) this->file_operations.close(this);
    }

    return 0;
}

int vnode_t::ioctl(int op, char* argp){
    if (this->file_operations.ioctl){
        return this->file_operations.ioctl(op, argp, this);
    }

    return -ENOSYS;
}


vnode_t::vnode_t(){
    memset(this, 0, sizeof(vnode_t));
    ready_to_receive_data = true;
    data_ready_to_read = true;
    permissions = 0766;
}

vnode_t::vnode_t(vnode_type_t type){
    memset(this, 0, sizeof(vnode_t));

    this->type = type;
    ready_to_receive_data = true;
    data_ready_to_read = true;
    permissions = 0766;
}

vnode_t::~vnode_t(){

}

char* vnode_t::read_link(){
    if (this->type != VLNK) return nullptr;

    if (file_operations.read_link != nullptr){
        return file_operations.read_link(this);
    }

    return nullptr;
}


int vnode_t::read(uint64_t offset, uint64_t length, void* buffer){
    if (this->type == VDIR) return -EISDIR;

    if (file_operations.read != nullptr){
        return vfs::cache_read(offset, length, buffer, this);//file_operations.read(offset, length, buffer, this);
    }
    return 0;
}

int vnode_t::write(uint64_t offset, uint64_t length, const void* buffer){
    if (this->type == VDIR) return -EISDIR;

    if (file_operations.write != nullptr){
        return vfs::cache_write(offset, length, buffer, this); //file_operations.write(offset, length, buffer, this);
    }
    return 0;
}

int vnode_t::truncate(uint64_t size){
    if (!this->file_operations.truncate) return -EOPNOTSUPP;

    return this->file_operations.truncate(size, this);
}

int vnode_t::mount(vnode_t *detour){
    this->open(); // Make sure it does not get cleaned prematurely

    // Call the detour's mount before opening it, to give the fs a chance to initialize itself :)
    if (detour->file_operations.mount) detour->file_operations.mount(detour);
    detour->open();

    if (this == vfs::get_root_node()){
        // We need to create corresponding folders (like /dev, /sys)
        // In the detour and mount them to avoid shadowing them

        dirent_t *entries = nullptr;
        int count = this->read_dir(&entries);

        for (int i = 0; i < count; i++){
            if (entries[i].type != VDIR) continue;

            detour->mkdir(entries[i].name);

            vnode_t *point = nullptr;
            this->find_file(entries[i].name, &point);

            vnode_t *target = nullptr;
            detour->find_file(entries[i].name, &target);

            if (point && target){
                target->mount(point);
            }
        }

        free(entries);
    }

    if (this->mount_point) return -EALREADY;


    this->mount_point = detour;
    detour->mounted_on = this;

    return 0;
}

// @warning It will return an already open vnode!
int vnode_t::find_file(const char* filename, vnode_t** ret){
    uint64_t rflags = spin_lock(&this->list_lock);

    *ret = nullptr;

    // Itterate through the children
    for (vnode_t *current = this->children; current != nullptr; current = current->next){
        if (strcmp(current->name, filename)) continue;

        current->_increase_refcount(); 

        *ret = current;
        break;
    }

    spin_unlock(&this->list_lock, rflags);

    if (*ret) {
        (*ret)->open(); 
        (*ret)->_decrease_refcount(); 
        return 0;
    }

    return -ENOENT;
}

int vnode_t::read_dir(dirent_t** ret){
    uint64_t rflags = spin_lock(&this->list_lock);

    int child_count = 0;
    for (vnode_t *current = this->children; current != nullptr; current = current->next){
        child_count++;
    }

    spin_unlock(&this->list_lock, rflags);

    rflags = spin_lock(&this->list_lock);

    dirent_t *buffer = (dirent_t *)malloc(sizeof(dirent_t) * child_count);

    int i = 0;
    for (vnode_t *current = this->children; current != nullptr; current = current->next){
        strcpy(buffer[i].name, current->name);
        buffer[i].inode = current->inode;
        buffer[i].type = current->type;

        i++;
    }

    spin_unlock(&this->list_lock, rflags);

    *ret = buffer;
    return child_count;
}

int vnode_t::mkdir(const char* name){
    if (this->type != VDIR) return -ENOTDIR;

    vnode_t* node = nullptr;
    int ret = this->find_file(name, &node);
    if (node) {
        node->close();
        return -EEXIST;
    }

    ret = -EOPNOTSUPP;

    if (this->file_operations.mkdir){
        ret = this->file_operations.mkdir((char*)name, this);
    }

    if (this->directory_cached && ret == 0 && this->file_operations.find_file) {
        vnode_t *newdir = nullptr;
        int r = this->file_operations.find_file(name, &newdir, this);

        if (newdir){
            newdir->next = nullptr;
            newdir->previous = nullptr;

            this->_add_child(newdir);
        }
    }

    return ret;
}

int vnode_t::creat(const char* name){
    if (this->type != VDIR) return -ENOTDIR;

    vnode_t* node = nullptr;
    int ret = this->find_file(name, &node);
    if (node) {
        node->close();
        ret = -EEXIST;
    } else if (this->file_operations.creat){
        ret = this->file_operations.creat((char*)name, this);
    }

    if (this->directory_cached && ret == 0 && this->file_operations.find_file) {
        vnode_t *newfile = nullptr;
        int r = this->file_operations.find_file(name, &newfile, this);

        if (newfile){
            newfile->next = nullptr;
            newfile->previous = nullptr;

            this->_add_child(newfile);
        }
    }

    return ret;
}

void vnode_t::save_metadata(){
    if (this->file_operations.save_metadata){
        this->file_operations.save_metadata(this);
    }
}

int vnode_t::rmdir(){
    if (this->type != VDIR) return -ENOTDIR;
    
    if (this->file_operations.rmdir){
        int ret = this->file_operations.rmdir(this);
        if (ret == 0){
            if (this->parent) this->parent->_remove_child(this);
            this->parent = nullptr;
        }

        return ret;
    }
    

    return -EOPNOTSUPP;
}

int vnode_t::unlink(){
    if (this->type == VDIR) return -EISDIR;

    if (this->file_operations.unlink){
        int ret = this->file_operations.unlink(this);
        if (ret == 0){
            if (this->parent) this->parent->_remove_child(this);
            this->parent = nullptr;
        }
        return ret;
    }

    return -EOPNOTSUPP;
}

char* procfs_read_link(vnode_t* node){
    task_t *task = task_scheduler::get_current_task();

    if (!task || !task->proc_vfs_dir){
        char *alloc = (char*)malloc(2);
        alloc[0] = '/';
        alloc[1] = '\0';
        return alloc;
    }

    return vfs::get_full_path_name(task->proc_vfs_dir);
}

int null_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    return 0;
}

int null_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    return length;
}

void create_null(){
    vnode_t *null = vfs::create_path("/dev/null", VCHR);
    null->file_operations.read = null_read;
    null->file_operations.write = null_write;
    null->permissions = 0666;
    
    null->close();
}

namespace vfs{
    vnode_t* root_node = nullptr;

    vnode_t* get_root_node(){
        return root_node;
    }

    void initialize_vfs(){
        root_node = filesystems::memfs::create_memfs();
        root_node->open(); // Make sure its not freed... ever...

        strcpy(root_node->name, "/");

        root_node->mkdir("dev");
        root_node->mkdir("proc");
        root_node->mkdir("sys");
        root_node->mkdir("tmp");
        create_null();
        initialize_pty_multiplexer();
        /*
        vnode_t* kernel = vfs::resolve_path("/proc/sys/kernel");

        if (kernel){
            kernel->creat("osrelease");
            kernel->close();
        }

        vnode_t* os_release = vfs::resolve_path("/proc/sys/kernel/osrelease");

        if (os_release){
            os_release->write(0, strlen(KERNEL_VERSION_STRING), KERNEL_VERSION_STRING);
            os_release->close();
        }*/

    }

    // @brief Returns -EACC
    int vfs_check_permission(vnode_t* node, int desired_access) {
        task_t* current = task_scheduler::get_current_task();
        
        uint32_t subject_uid = current->euid;
        uint32_t subject_gid = current->egid;

        // Root check (Root can do anything)
        if (subject_uid == 0) return 0;

        // Owner check
        if (subject_uid == node->uid) {
            return (node->permissions & (desired_access << 6)) == (desired_access << 6) ? 0 : -EACCES;
        }

        // Group check
        if (subject_gid == node->gid) { // Also check supplementary groups list here!
            return (node->permissions & (desired_access << 3)) == (desired_access << 3) ? 0 : -EACCES;
        }

        // Other check
        return (node->permissions & desired_access) == desired_access ? 0 : -EACCES;
    }

    // Forward declaration for recursion
    static vnode_t* _resolve_path_recursion(vnode_t* start_node, char* path, int depth, bool follow_links, bool follow_trailing);

    // @warning The vnode returned is already open!
    vnode_t* resolve_path(const char* path, bool follow_links, bool follow_trailing) {
        if (!path) return nullptr;

        char* path_copy = strdup(path); 
        if (!path_copy) return nullptr;

        vnode_t* start_node = root_node->mount_point ? root_node->mount_point : root_node;
        start_node->open();
        // Start the recursive walk
        vnode_t* result = _resolve_path_recursion(start_node, path_copy, 0, follow_links, follow_trailing);

        free(path_copy);
        return result;
    }

    static vnode_t* _resolve_path_recursion(vnode_t* start_node, char* path, int depth, bool follow_links, bool follow_trailing) {
        if (depth > MAX_SYMLINK_DEPTH) {
            start_node->close(); // Clean up where we were standing
            return nullptr; // -ELOOP
        }

        vnode_t* current = start_node; // We already hold a ref to this from the caller
        char* token;
        char* state; // For strtok_r context

        // 5. Tokenize the path
        token = strtok_r(path, PATH_SEPARATOR, &state);

        while (token != nullptr) {
            vnode_t* next = nullptr;

            // --- Handle ".." (Parent) ---
            if (strcmp(token, "..") == 0) {
                
                if (current->mounted_on) {
                    vnode_t* mount_dir = current->mounted_on;
                    if (mount_dir->parent) next = mount_dir->parent;
                    else next = mount_dir; // We are at root of host FS
                } else if (current->parent) {
                    // Standard parent
                    next = current->parent;
                } else {
                    // We are at root, ".." stays at root
                    next = current;
                }
                
                // Acquire next, Release current
                next->open();
                current->close();
                current = next;
                
                token = strtok_r(NULL, PATH_SEPARATOR, &state);
                continue;
            }

            if (strcmp(token, ".") == 0) {
                token = strtok_r(NULL, PATH_SEPARATOR, &state);
                continue;
            }

            if (current->find_file(token, &next) != 0) {
                current->close();
                return nullptr; // -ENOENT (Not found)
            }

            if (next->mount_point) {
                vnode_t* real_root = next->mount_point;
                real_root->open();
                next->close();
                next = real_root;
            }
            
            char* next_token_peek = strtok_r(NULL, PATH_SEPARATOR, &state); // Peek next
            bool is_last = (next_token_peek == nullptr);
            
            if ((next->type == VLNK) && follow_links && (!is_last || follow_trailing)) {
                
                // Read the link target
                char* link_path = next->read_link(); 
                if (link_path){
                    next->close();
                    
                    vnode_t* link_base;
                    if (link_path[0] == '/') {
                        link_base = root_node->mount_point ? root_node->mount_point : root_node;
                    } else {
                        link_base = current; 
                    }
                    link_base->open();

                    // Recurse!
                    vnode_t* resolved_target = _resolve_path_recursion(link_base, link_path, depth + 1, true, true);
                    
                    free(link_path);

                    if (!resolved_target) {
                        current->close();
                        return nullptr; // Broken link
                    }

                    // The resolved target becomes our 'next'
                    next = resolved_target;
                } else {
                    if (next) next->close();
                    current->close();
                    return nullptr;
                }
            }

            
            current->close();
            current = next;   // Step into the child
            
            if (is_last) break; 
            
            token = next_token_peek;
        }

        return current;
    }

    void print_tree(vnode_t* node, int depth, int max_depth) {
        if (depth > max_depth) return;

        // 1. Indentation
        //    Print 2 spaces per depth level to show hierarchy
        for (int i = 0; i < depth; i++) {
            kprintf("  ");
        }

        // 2. Print Node Name and Type
        //    e.g., "|- home (DIR)"

        if (depth > 0) kprintf("|- ");
        kprintf("%s", node->name[0] ? node->name : "/"); // Handle root case

        if (node->type == VDIR) kprintf(" (DIR)");
        else if (node->type == VBLK) kprintf(" (BLK)");
        else if (node->type == VCHR) kprintf(" (CHR)");
        else if (node->type == VLNK) kprintf(" (LNK)");
        
        // Check if it's a mountpoint
        if (node->mount_point) node = node->mount_point; 
        if (node->mounted_on) kprintf(" [MOUNT] (Usage: %d / %d)",
            node->size / (1024 * 1024), node->partition_total_size / (1024 * 1024));
        
        kprintf("\n");

        // 3. Recursion (If it's a directory)
        bool self_reference = memcmp(".", node->name, 2) == 0;
        bool parent_reference = memcmp("..", node->name, 3) == 0;
        if (node->type == VDIR && !self_reference && !parent_reference) {
            dirent_t* children_list = nullptr;
            
            int count = node->read_dir(&children_list);

            dirent_t* iterator = children_list;
            for (int i = 0; i < count; i++) {
                // Save next pointer because we might delete iterator
                vnode_t* child_node = nullptr;
                node->find_file(iterator->name, &child_node);

                // Recurse down
                if (child_node) {
                    print_tree(child_node, depth + 1, max_depth);
                    child_node->close();
                }

                iterator++;
            }

            free(children_list);
        }
    }

    void recursive_path_builder(vnode_t* node, char* buffer, int& offset) {
        if (!node) return;

        if (node->mounted_on) {
            recursive_path_builder(node->mounted_on, buffer, offset);
            
            return; 
        }

        if (node->parent && node != vfs::root_node) {
            recursive_path_builder(node->parent, buffer, offset);
        }

        int len = strlen(node->name);
        
        if (offset > 0 && buffer[offset-1] != '/') {
            buffer[offset++] = '/';
        }
        
        strcpy(buffer + offset, node->name);
        offset += len;
    }

    char* get_full_path_name(vnode_t* node){
        char* buffer = (char*)malloc(2048);

        int offset = 0;
        recursive_path_builder(node, buffer, offset);

        return buffer;
    }


    // @warning Please ensure that the root fs of this path is a memfs if you want to create virtual files!
    vnode_t* create_path(const char* absolute_path, vnode_type_t type) {
        if (!absolute_path || absolute_path[0] != '/') return nullptr;

        char* path_copy = strdup(absolute_path);
        if (!path_copy) return nullptr;

        vnode_t* current = vfs::get_root_node();
        
        // Sanity check 1: Make sure the VFS is actually initialized!
        if (!current) { 
            free(path_copy);
            return nullptr;
        }
        current->open();

        char* state;
        char* token = strtok_r(path_copy, "/", &state);

        while (token != nullptr) {
            char* next_token = strtok_r(nullptr, "/", &state);
            bool is_last = (next_token == nullptr);

            vnode_t* next_node = nullptr;
            int r = current->find_file(token, &next_node);

            if (r == -ENOENT) {
                if (!is_last || type == VDIR) {
                    int res = current->mkdir(token);
                    if (res != 0 && res != -EEXIST) {
                        current->close();
                        free(path_copy);
                        return nullptr;
                    }
                    current->find_file(token, &next_node);
                } else {
                    int res = current->creat(token);
                    if (res != 0 && res != -EEXIST) {
                        current->close();
                        free(path_copy);
                        return nullptr;
                    }
                    current->find_file(token, &next_node);

                    if (next_node && next_node->type != type) {
                        next_node->type = type;
                    }
                }
            } else if (r == 0) {
                // The node exists! 
                if (!is_last && next_node->type != VDIR) {
                    next_node->close();
                    current->close();
                    free(path_copy);
                    return nullptr;
                }
            } else {
                current->close();
                free(path_copy);
                return nullptr;
            }

            if (!next_node) {
                current->close();
                free(path_copy);
                return nullptr;
            }

            current->close();
            current = next_node;
            token = next_token;
        }

        free(path_copy);
        
        // Return the final node! (It is already open with ref_count = 1 because of find_file)
        return current; 
    }

}

// @brief It will attempt to detect the filesystem and create an unmounted root node for it
vnode_t* create_root_node_fs(vnode_t* blk){
    if (filesystems::is_fat32(blk)){
        filesystems::fat32_t* fs = new filesystems::fat32_t(blk);
        return fs->create_root_node();
    }

    if (filesystems::is_ext2(blk)){
        filesystems::ext2_t* fs = new filesystems::ext2_t(blk);
        return fs->create_root_node();
    }
    
    return nullptr;
}