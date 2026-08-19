#include <scheduling/task_scheduler/file_descriptors.h>
#include <kerrno.h>
#include <cstr.h>

void file_t::open(){
    __atomic_fetch_add(&this->refcount, 1, __ATOMIC_SEQ_CST);
    if (this->node) this->node->open();

    if (this->dentry) this->dentry->ref();
}

void file_t::close(){
    if (this->node)   this->node->close();
    if (this->dentry) this->dentry->unref();
    
    int ref = __atomic_sub_fetch(&this->refcount, 1, __ATOMIC_SEQ_CST);

    if (ref == 0){
        delete this;
    }
}

void fd_table_t::open(){
    __atomic_fetch_add(&this->refcount, 1, __ATOMIC_SEQ_CST);
}

void fd_table_t::close(){
    int ref = __atomic_sub_fetch(&this->refcount, 1, __ATOMIC_SEQ_CST);

    if (ref <= 0){
        // Close any remaining open files before destroying the table
        for (int i = 0; i < MAX_FDS; i++) {
            this->close_file(i);
        }

        if (this->cwd)  this->cwd->unref();
        delete this;
    }
}

fd_table_t *fd_table_t::clone(bool cloexec){
    fd_table_t *new_table = new fd_table_t();
    new_table->open();

    for (int i = 0; i < MAX_FDS; i++){
        if (this->entries[i] != nullptr){
            file_t *file = this->entries[i];

            if (cloexec && file->flags & O_CLOEXEC) continue; // Skip if cloexec

            new_table->open_file(file->dentry, file->node, file->flags, i);
            new_table->entries[i]->offset = file->offset;
        }
    }
    
    if (this->cwd) {
        this->cwd->ref();
        new_table->cwd = this->cwd;
    }
    return new_table;
}

int fd_table_t::allocate_fd(file_t *file){
    uint64_t rflags = spin_lock(&this->lock);

    int fd = -EMFILE;

    for (int i = 0; i < MAX_FDS; i++){
        if (this->entries[i] == nullptr){
            file->open();
            this->entries[i] = file;
            fd = i;
            break;
        }
    }

    spin_unlock(&this->lock, rflags);
    return fd;
}

int fd_table_t::open_file(int dirfd, const char *filename, uint16_t flags, int mode, int fixed_fd) {
    if (fixed_fd != -1 && (fixed_fd < 0 || fixed_fd >= MAX_FDS)) return -EBADF;

    // Determine the base directory
    dentry_t *base_dir = nullptr;
    
    if (filename[0] != '/') {
        if (dirfd == AT_FDCWD) {
            if (!this->cwd) return -ENOENT;
            base_dir = this->cwd;
        } else {
            file_t *dir_file = this->get_file(dirfd);
            if (!dir_file || !dir_file->dentry) return -EBADF;
            
            vnode_t *dir_vnode = vfs::_get_vnode(dir_file->dentry);
            if (!dir_vnode) return -EIO;
            
            bool is_dir = ((dir_vnode->attributes.mode & S_IFMT) == S_IFDIR);
            dir_vnode->close();
            if (!is_dir) return -ENOTDIR;

            base_dir = dir_file->dentry;
        }
    }

    // Parse Intent
    int intent = 0;
    int acc = flags & O_ACCMODE;
    if (acc == O_RDONLY) intent |= MAY_READ;
    if (acc == O_WRONLY) intent |= MAY_WRITE;
    if (acc == O_RDWR)   intent |= (MAY_READ | MAY_WRITE);
    if (flags & O_TRUNC) intent |= MAY_WRITE;

    bool follow_trailing = !(flags & O_NOFOLLOW);

    // Resolve Path
    int err = 0;
    dentry_t *dentry = vfs::resolve_path_dentry_at(base_dir, filename, intent, err, true, follow_trailing);

    // Handle O_CREAT
    if (!dentry) {
        if (err == -ENOENT && (flags & O_CREAT)) {
            int err = vfs::mkfile(filename, mode);

            if (err < 0) return err;

            // Re-resolve after creation
            dentry = vfs::resolve_path_dentry_at(base_dir, filename, intent, err, true, follow_trailing);
            if (!dentry) return err;
        } else {
            return err;
        }
    } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
        dentry->unref();
        return -EEXIST;
    }

    vnode_t *node = vfs::_get_vnode(dentry);
    if (!node) {
        dentry->unref();
        return -EIO;
    }

    // Handle O_DIRECTORY
    bool is_dir = ((node->attributes.mode & S_IFMT) == S_IFDIR);
    if ((flags & O_DIRECTORY) && !is_dir) {
        node->close();
        dentry->unref();
        return -ENOTDIR;
    }

    // Handle O_TRUNC
    if ((flags & O_TRUNC) && (acc == O_WRONLY || acc == O_RDWR) && !is_dir) {
        //node->truncate(0);
    }

    file_t *file = new file_t;
    file->node = node;
    file->dentry = dentry;
    file->flags = flags;
    file->offset = 0;

    file->open();

    if (fixed_fd != -1) {
        this->close_file(fixed_fd);
        uint64_t rflags = spin_lock(&this->lock);
        
        this->entries[fixed_fd] = file;
        
        spin_unlock(&this->lock, rflags);
        return fixed_fd;
    }

    int allocated_fd = this->allocate_fd(file);
    if (allocated_fd < 0) {
        file->close();
    }
    
    return allocated_fd;
}

int fd_table_t::open_file(dentry_t *dentry, vnode_t *node, uint16_t flags, int fd){
    file_t *file = new file_t;
    file->node = node;
    file->dentry = dentry;
    file->flags = flags;
    file->offset = 0;

    file->open();

    if (fd != -1) {
        this->close_file(fd);
        uint64_t rflags = spin_lock(&this->lock);
        
        file->open();
        this->entries[fd] = file;
        
        spin_unlock(&this->lock, rflags);
        return fd;
    }

    int allocated_fd = this->allocate_fd(file);
    if (allocated_fd < 0) {
        file->close();
    }
    
    return allocated_fd;
}

int fd_table_t::dup(file_t *file, int fd) {
    if (fd != -1){
        this->close_file(fd);
        
        uint64_t rflags = spin_lock(&this->lock);
        
        file->open();
        this->entries[fd] = file;
        
        spin_unlock(&this->lock, rflags);
        return fd;
    }

    int allocated_fd = this->allocate_fd(file);
    return allocated_fd;
}

int fd_table_t::close_file(int fd){
    if (fd < 0 || fd >= MAX_FDS) return -EBADF;
    
    uint64_t rflags = spin_lock(&this->lock);

    int ret = 0;

    if (this->entries[fd]){
        this->entries[fd]->close();
        this->entries[fd] = nullptr;
    } else {
        ret -EBADF;
    }

    spin_unlock(&this->lock, rflags);

    return ret;
}

file_t *fd_table_t::get_file(int fd){
    if (fd < 0 || fd >= MAX_FDS) return nullptr;

    return this->entries[fd];
}

char *fd_table_t::get_file_path(dentry_t *dentry) {
    if (!dentry) return nullptr;

    if (!dentry) {
        const char *unmounted = "[unmounted/anonymous]";
        char *result = new char[strlen(unmounted) + 1];
        strcpy(result, unmounted);
        return result;
    }

    const int max_path = 4096;
    char buffer[max_path];
    
    int pos = max_path - 1;
    buffer[pos] = '\0';

    dentry_t *current = dentry;

    while (current != nullptr && current->parent != nullptr) {
        
        if (current->parent->mounted_root == current) {
            current = current->parent;
            continue;
        }

        size_t len = strlen(current->name);
        
        if (pos - (int)len - 1 < 0) {
            return nullptr; 
        }

        pos -= len;
        memcpy(&buffer[pos], current->name, len);

        pos--;
        buffer[pos] = '/';

        current = current->parent;
    }

    if (pos == max_path - 1) {
        pos--;
        buffer[pos] = '/';
    }

    size_t final_len = (max_path - 1) - pos;
    char *result = new char[final_len + 1];
    strcpy(result, &buffer[pos]);

    return result;
}