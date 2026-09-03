#include <drivers/filesystems/procfs/procfs.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <kerrno.h>
#include <cstr.h>

namespace task_scheduler {
    extern kstd::avl_tree_t<task_t *> task_search_tree;
}

// Cast void* to uintptr_t before bitwise ops to satisfy the C++ compiler
inline void* pack_procfs_data(uint16_t type, int16_t fd, int32_t pid) {
    uint64_t data = 0;
    data |= ((uint64_t)type & 0xFFFF) << 48; // Top 16 bits
    data |= ((uint64_t)fd   & 0xFFFF) << 32; // Next 16 bits
    data |= ((uint64_t)pid  & 0xFFFFFFFF);   // Bottom 32 bits
    return (void*)(uintptr_t)data;
}

inline uint16_t procfs_get_type(void* data) {
    return (uint16_t)(((uintptr_t)data >> 48) & 0xFFFF);
}

inline int16_t procfs_get_fd(void* data) {
    return (int16_t)(((uintptr_t)data >> 32) & 0xFFFF);
}

inline int32_t procfs_get_pid(void* data) {
    return (int32_t)((uintptr_t)data & 0xFFFFFFFF);
}

namespace procfs {
    dentry_t *root = nullptr;
    int fs_id = 0;
    int cinode = 0;

    extern vnode_file_operations_t fops;
    
    
    vnode_t *fs_fetch_vnode(dentry_t *entry){
        vnode_t *node = new vnode_t();
        node->inode = entry->inode;
        node->fs_id = entry->fs_id;
        node->fs_data = entry->fs_data;
        node->attributes.mode = S_IFDIR | 0777;
        node->operations = &fops;
        node->kflag_bitfield = VNODE_KFLAG_NO_CACHE;

        return node;
    }

    int allocate_inode(){
        return __atomic_add_fetch(&cinode, 1, __ATOMIC_SEQ_CST);
    }

    void init(){
        fs_id = vfs::allocate_filesystem_id();
        root = new dentry_t("procfs_root", allocate_inode(), fs_id);
        root->kflags = VNODE_KFLAG_NO_CACHE;
        
        root->__fs_fetch_vnode = fs_fetch_vnode;
        root->fs_data = pack_procfs_data(PROCFS_ROOT, -1, -1);
    }

    dentry_t *get_root(){
        if (!root) init();

        return root;
    }

    int get_listing_avl_cb(task_t *task, void *c){
        kstd::linked_list_t<int> *list = (kstd::linked_list_t<int> *)c;
        list->add(task->pid);

        return AVL_REC_CONT;
    }

    dentry_t *lookup(vnode_t *node, const char *name){
        return nullptr;
        if (procfs_get_type(node->fs_data) != PROCFS_ROOT) return nullptr;

        int num = atoi(name);
        
        //task_t *task = task_scheduler::task_search_tree.search(num);

        //if (!task) return nullptr;

        dentry_t *ret = new dentry_t();
        stringf(ret->name, sizeof(ret->name), "%s", name);
        ret->inode = num;
        ret->fs_id = fs_id;
        ret->kflags = VNODE_KFLAG_NO_CACHE;

        ret->__fs_fetch_vnode = fs_fetch_vnode;
        ret->fs_data = pack_procfs_data(PROCFS_PID_DIR, -1, num);

        return ret;
    }

    int get_listing(vnode_t *node, dentry_t *&out, size_t offset, size_t limit){
        if (procfs_get_type(node->fs_data) != PROCFS_ROOT) return -ENOTDIR;

        kstd::linked_list_t<int> *pid_list = new kstd::linked_list_t<int>();
        task_scheduler::task_search_tree.inorder(get_listing_avl_cb, pid_list);

        if (pid_list->size() == 0 || offset >= pid_list->size()){
            delete pid_list;
            out = nullptr;
            return 0;
        }

        int rem = pid_list->size() - offset;
        if (rem > limit) rem = limit;

        dentry_t *ret = new dentry_t[rem];

        for (int i = 0; i < rem; i++){
            int pid = pid_list->get(offset + i);

            dentry_t *e = &ret[i];
            stringf(e->name, sizeof(e->name), "%d", pid);
            e->inode = pid;
            e->fs_id = fs_id;
            e->kflags = VNODE_KFLAG_NO_CACHE;
            
            e->__fs_fetch_vnode = fs_fetch_vnode;

            e->fs_data = pack_procfs_data(PROCFS_PID_DIR, -1, pid); 
        }

        delete pid_list;
        
        out = ret;
        return rem;
    }

    vnode_file_operations_t fops = {
        .lookup = lookup,
        .get_listing = get_listing,
    };
}