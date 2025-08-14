#include <syscalls/syscalls.h>
#include <syscalls/linux/filesystem.h>
#include <pipe.h>
#include <random.h>

void split_path(char* fullpath, char*& parent, char*& name);
int sys_creat(char *pathname, int mode);

int sys_open(char *pathname, int flags, size_t mode){
    task_t* ctask = task_scheduler::get_current_task();

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    if (strcmp(pathname, "/dev/tty") == 0 || 
        strcmp(pathname, "/dev/console") == 0 ||
        strcmp(pathname, "/dev/vc/0") == 0 ||
        strcmp(pathname, "/dev/systty") == 0 ||
        strcmp(pathname, "/dev/tty0") == 0 ||
        strcmp(pathname, "/proc/self/fd/0") == 0 ||// WHY ARE THERE SO MANY VARIATIONS
        strcmp(pathname, "/proc/self/fd/1") == 0 ||
        strcmp(pathname, "/proc/self/fd/2") == 0 
    ){ // redirect it to its own tty
        open_fd_t* fd = ctask->open_node(ctask->tty);
        fd->flags = flags;
        return fd->fd;
    }

    vnode_t* node = vfs::resolve_path(pathname);
    if (node != nullptr && (flags & O_CREAT) && (flags & O_EXCL)) return -EEXIST;
    if (node == nullptr && (flags & O_CREAT)){
        int fd = sys_creat(pathname, mode);
        if (fd < 0) return fd;
        open_fd_t* d = ctask->get_fd(fd);
        d->flags = flags;
        return fd;
    }
    
    if (node == nullptr){
        return -ENOENT;
    }
    
    //if (flags && O_TRUNC) node->truncate(0);
    /*if (flags && O_TMPFILE) {
        char str[256] = { 0 };
        strcpy(str, "unnamed-tmp-");
        strcat(str, random_string(8));
        int ret = node->mkfile(str, false);
        if (ret < 0) return ret;

        char path[strlen(str) + strlen(pathname) + 2] = { 0 };

        strcpy(path, pathname);
        strcat(path, "/");
        strcat(path, str);
        node = vfs::resolve_path(path);
        if (node == nullptr) return -ENOENT;
    }*/

    open_fd_t* fd = ctask->open_node(node);
    fd->flags = flags;

    return fd->fd;
}

int sys_openat(int dirfd, char *pathname, int flags){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* dir = ctask->get_fd(dirfd);

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);


        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/' && dir == nullptr){
                char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);


         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    if (strcmp(pathname, "/dev/tty") == 0 || 
        strcmp(pathname, "/dev/console") == 0 ||
        strcmp(pathname, "/dev/vc/0") == 0 ||
        strcmp(pathname, "/dev/systty") == 0 ||
        strcmp(pathname, "/dev/tty0") == 0 ||
        strcmp(pathname, "/proc/self/fd/0") == 0 ||// WHY ARE THERE SO MANY VARIATIONS
        strcmp(pathname, "/proc/self/fd/1") == 0 ||
        strcmp(pathname, "/proc/self/fd/2") == 0 
    ){ // redirect it to its own tty
        open_fd_t* fd = ctask->open_node(ctask->tty);
        fd->flags = flags;
        return fd->fd;
    }

   return sys_open(pathname, flags, 0);
}

int sys_creat(char *pathname, int mode){
    task_t* ctask = task_scheduler::get_current_task();
    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }

    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    
    char* parent;
    char* name;
    split_path(pathname, parent, name);
    // serialf("PATH: %s NAME: %s\n\r", parent, name);
    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) return -ENOENT;
    if (pnode->type != VNODE_TYPE::VDIR) return -ENOTDIR;
    int ret = pnode->mkfile(name, false);
    if (ret < 0) return ret;

    vnode_t* nd = vfs::resolve_path(pathname);
    if (nd == nullptr) return -ENOENT;


    open_fd_t* fd = ctask->open_node(nd);
    fd->flags = O_WRONLY;
    return fd->fd;
}

int sys_close(int fd){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    ctask->close_fd(ofd);

    return 0;
}


int sys_write(int fd, void* data, int len){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) {
        return -EBADF;
    }

    if (ofd->flags & O_RDONLY) {
        serialf("write fault, write on read-only marked file!\n\r");
        return -EBADF;
    }

    if (ofd->node->type == VNODE_TYPE::VDIR) return -EISDIR;

    if (ofd->node->type == VNODE_TYPE::VFIFO){
        if (ofd->flags & O_RDONLY) return -EBADF;
    }

    if (ofd->flags & O_APPEND)
        ofd->offset = ofd->length;
    int64_t write = ofd->node->write(data, len, ofd->offset);
    ofd->offset += len;
    return write;
}

int sys_writev(int fd, const struct iovec *iov, int iovcnt){

    int total_len = 0;
    for (int i = 0; i < iovcnt; i++){
        int res = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (res < 0) return res;
        total_len += res;
    }
    return total_len;
}

int sys_read(int fd, char* buf, size_t count){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);
    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type == VNODE_TYPE::VDIR) return -EISDIR;

    memset(buf, 0, count);
    int64_t c = ofd->node->read(buf, count, ofd->offset);
    ofd->offset += c;
    return c;
}

int sys_pread64(int fd, char* buf, size_t count, size_t off){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);
    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type == VNODE_TYPE::VDIR) return -EISDIR;

    uint64_t c = ofd->node->read(buf, count, off);
    return c;
}

int sys_readv(int fd, const struct iovec *iov, int iovcnt){
    int total_len = 0;
    for (int i = 0; i < iovcnt; i++){
        int to_read = iov[i].iov_len;
        int res = sys_read(fd, (char*)iov[i].iov_base, to_read);
        if (res == EOF) break;
        if (res < 0) return res;
        total_len += res;
    }
    return total_len;
}


int sys_lseek(int fd, int64_t offset, int whence) {
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -EBADF;

    if (ofd->node->type == VNODE_TYPE::VDIR || ofd->node->type == VNODE_TYPE::VCHR || ofd->node->type == VNODE_TYPE::VFIFO)
        return -ESPIPE; // Not seekable

    int64_t new_offset = 0;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = (int64_t)ofd->offset + offset;
            break;
        case SEEK_END:
            new_offset = (int64_t)ofd->length + offset;
            break;
        default:
            // serialf("INVALID SEEK!\n\r");
            return -EINVAL;
    }

    if (new_offset < 0) return -EINVAL;
    if ((uint64_t)new_offset > ofd->length) new_offset = ofd->length;

    ofd->offset = (size_t)new_offset;
    return new_offset; // POSIX lseek returns new offset on success
}


int sys_ioctl(unsigned int fd, int op, char* argp){    
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) {
        return -EBADF;
    }
    
    if (ofd->node->type != VNODE_TYPE::VCHR) {
        return -ENOTTY;
    }

    int ret = ofd->node->iocntl(op, argp);

    return ret;
}

int sys_dup2(int oldfd, int newfd){
    if (oldfd == newfd) return oldfd;
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(oldfd);

    if (ofd == nullptr) return -EBADF;

    open_fd_t* nfd = ctask->get_fd(newfd);
    if (nfd != nullptr) sys_close(newfd); // attempt to close the new fd, in case its open (thats what the debian spec says)

    open_fd_t* fd = ctask->open_node(ofd->node, newfd);
    fd->data = ofd->data;
    fd->flags = ofd->flags;
    fd->length = ofd->length;
    fd->offset = ofd->offset;
    fd->size_in_memory = ofd->size_in_memory;
    return fd->fd;
}

int sys_dup(int oldfd){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(oldfd);

    if (ofd == nullptr) {
        return -EBADF;
    }

    open_fd_t* fd = ctask->open_node(ofd->node);
    fd->data = ofd->data;
    fd->flags = ofd->flags;
    fd->length = ofd->length;
    fd->offset = ofd->offset;
    fd->size_in_memory = ofd->size_in_memory;
    return fd->fd;
}


uint64_t sys_fcntl(int fd, uint64_t op, uint64_t arg){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -EBADF;

    switch(op){
        case F_DUPFD:
            return sys_dup(fd);
        case F_SETFD:
            ofd->flags = arg;
            break;
        case F_GETFD:
            return ofd->flags;
        case F_SETFL:
            ofd->node->flags = arg;
            break;
        case F_GETFL:
            return ofd->node->flags;
        case F_SETOWN_EX:{
            f_owner_ex* st = (f_owner_ex*)arg;
            ofd->own = st->pid;
            ofd->own_type = st->type;
            break;
        }
        case F_GETOWN_EX:{
            f_owner_ex* st = (f_owner_ex*)arg;
            st->pid = ofd->own;
            st->type = ofd->own_type;
            break;
        }
        default:
            // serialf("SYS_FNCNTL %u %u %u\n", fd, op, arg);
            return -ENOSYS;
    }

    return 0;
}



int sys_stat(char* pathname, struct stat* statbuf){
    task_t* task = task_scheduler::get_current_task();

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(task->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(task->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    vnode_t* node = vfs::resolve_path(pathname);
    if (node == nullptr) return -ENOENT;
    statbuf->st_dev = node->fs_inode;
    statbuf->st_ino = node->inode;
    statbuf->st_mode =  S_IRUSR | S_IWUSR | S_IXUSR |  // user: rwx
                        S_IRGRP | S_IXGRP |            // group: r-x
                        S_IROTH | S_IXOTH;             // other: r-x
    switch(node->type){
        case VNODE_TYPE::VDIR:
            statbuf->st_mode |= S_IFDIR;
            break;
        case VNODE_TYPE::VCHR :
            statbuf->st_mode |= S_IFCHR;
            break;
        case VNODE_TYPE::VBLK :
            statbuf->st_mode |= S_IFBLK;
            break;
        case VNODE_TYPE::VREG :
            statbuf->st_mode |= S_IFREG;
            break;
        case VNODE_TYPE::VLNK :
            statbuf->st_mode |= S_IFLNK;
            break;
        case VNODE_TYPE::VSOC :
            statbuf->st_mode |= S_IFSOCK;
            break;
        case VNODE_TYPE::VFIFO :
            statbuf->st_mode |= S_IFIFO;
            break;
    }

    statbuf->st_nlink = 1;
    statbuf->st_uid = node->uid;
    statbuf->st_gid = node->gid;
    statbuf->st_rdev = 1;
    statbuf->st_size = node->size;

    // Size and blocks for regular files and block devices
    if (node->type == VNODE_TYPE::VBLK) {
        vblk_t* blk = (vblk_t*)node;
        statbuf->st_size = blk->block_size * blk->block_count;
        statbuf->st_blocks = blk->block_count;
        statbuf->st_blksize = blk->block_size;
    } else if (node->type == VNODE_TYPE::VREG) {
        statbuf->st_size = node->size;
        statbuf->st_blocks = (node->size + 511) / 512; // approximate block count
        statbuf->st_blksize = 4096; // yes, i know i should be getting that from the fs, stop screaming 
    }

    statbuf->st_atim.tv_nsec = 0;
    statbuf->st_atim.tv_sec = node->last_accessed;
    statbuf->st_mtim.tv_nsec = 0;
    statbuf->st_mtim.tv_sec = node->last_modified;
    statbuf->st_ctim.tv_nsec = 0;
    statbuf->st_ctim.tv_sec = node->creation_time;

    
    return 0;
}

int sys_lstat(char* pathname, struct stat* statbuf){
    return sys_stat(pathname, statbuf);
}

int sys_fstat(int fd, struct stat* statbuf){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    return sys_stat(vfs::get_full_path_name(ofd->node), statbuf);
}


int sys_newfstatat(int dirfd, char* pathname, stat* statbuf, int flags){
    task_t* ctask = task_scheduler::get_current_task(); 

    open_fd_t* dir = ctask->get_fd(dirfd);

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/' && dir == nullptr){
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    return sys_stat(pathname, statbuf);
}
/*
int sys_poll(pollfd *fds, int nfds, int timeout){
    task_t* ctask = task_scheduler::get_current_task();
    uint64_t start_time = APICticsSinceBoot;
    if (fds[0].events & POLLOUT){
        fds[0].revents |= POLLOUT;
    }else if (fds[0].events & POLLIN){
        open_fd_t* fd = ctask->get_fd(fds[0].fd);
        if (fd == nullptr) return -EINVAL;
        asm ("sti");
        while(fd->node->data_read == false){
            if (timeout > 0 && (APICticsSinceBoot - start_time) >= (uint64_t)timeout) return -ETIMEDOUT;
        }
        asm ("cli");
    }
    
    return 1;
}*/



int sys_poll(pollfd *fds, int nfds, int timeout_ms) {
    task_t* ctask = task_scheduler::get_current_task();
    uint64_t start_time = APICticsSinceBoot;
    bool any_ready = false;

    asm ("sti");
    while (true) {
        any_ready = false;

        for (int i = 0; i < nfds; ++i) {
            fds[i].revents = 0;
            open_fd_t* fd = ctask->get_fd(fds[i].fd);
            if (!fd) continue;

            if ((fds[i].events & POLLIN) && (fd->node->data_read || ((fd->data && fd->offset < fd->length) && (fd->node->type == VNODE_TYPE::VCHR || fd->node->type == VNODE_TYPE::VFIFO)))) {
                fds[i].revents |= POLLIN;
                any_ready = true;
            }

            if (fds[i].events & POLLOUT && (fd->node->data_write || ((fd->data && fd->offset < fd->length) && (fd->node->type == VNODE_TYPE::VCHR || fd->node->type == VNODE_TYPE::VFIFO)))) {
                fds[i].revents |= POLLOUT;
                any_ready = true;
            }
        }

        if (any_ready){
            asm ("cli");
            return nfds; // you could count the number of ready fds too
        }

        if (timeout_ms == 0){
            asm ("cli");
            return 0;
        }

        if (timeout_ms > 0 && (APICticsSinceBoot - start_time) >= (uint64_t)timeout_ms){
            asm ("cli");
            return 0;
        }

        asm ("pause");
    }
    
    return -EAGAIN; // unreachable
}

int pipe(int fds[2], int flags){
    task_t* ctask = task_scheduler::get_current_task();
    vnode_t* pipe = CreatePipe(ctask->name, nullptr);
    pipe->type = VNODE_TYPE::VFIFO;
    open_fd_t* input = ctask->open_node(pipe);
    open_fd_t* out = ctask->open_node(pipe);
    input->flags = O_WRONLY;
    out->flags = O_RDONLY;
    fds[1] = input->fd;
    fds[0] = out->fd;

    input->other_end = out->fd;
    out->other_end = input->fd;
    return 0;
}

int sys_access(char *pathname, int mode){
    task_t* ctask = task_scheduler::get_current_task();
    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    vnode_t* node = vfs::resolve_path(pathname);
    if (node == nullptr) return -ENOENT;
    
    return 0;
};

int sys_faccessat(int dirfd, char *pathname, int mode, int flags){
    task_t* ctask = task_scheduler::get_current_task(); 

    open_fd_t* dir = ctask->get_fd(dirfd);

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/' && dir == nullptr){
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    vnode_t* node = vfs::resolve_path(pathname);
    if (node == nullptr) return -ENOENT;

    return X_OK | W_OK | X_OK;
}

int sys_faccessat2(int dirfd, char *pathname, int mode, int flags){
    return sys_faccessat(dirfd, pathname, mode, flags);
}

int64_t sys_getdents64(int fd, void* dirp, size_t count){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type != VNODE_TYPE::VDIR) return -ENOTDIR;
    if (count < sizeof(linux_dirent64)) return -EINVAL;

    linux_dirent64* dirent = (linux_dirent64*)dirp;
    int offset = 0;
    
    vnode_t* tnode = nullptr;
    int cnt = ofd->node->read_dir(&tnode);

    if (cnt <= 0) return cnt;
    serialf("\e[33mdloff=%d cnt=%d\e[0m\n", ofd->dl_off, cnt);
    if (ofd->dl_off >= cnt){
        ofd->dl_off = 0;
        while(tnode != nullptr){
            vnode_t* prev = tnode;
            tnode = tnode->next;
            if (prev->is_static == false) delete prev;
        }
        return 0;
    }

    if (ofd->dl_off){
        for (int i = 0; i < ofd->dl_off; i++){
            tnode = tnode->next;
            if (tnode == nullptr) break;
        }
    }

    while(tnode != nullptr){
        if ((offset + sizeof(linux_dirent64)) >= count){
            break;
        }

        vnode_t* node = tnode;
        dirent->d_ino = (unsigned long)node;
        dirent->d_reclen = sizeof(linux_dirent64);
        switch (node->type){
            case VNODE_TYPE::VBAD:
                dirent->d_type = DT_UNKNOWN;
                break;
            case VNODE_TYPE::VBLK:
                dirent->d_type = DT_BLK;
                break;
            case VNODE_TYPE::VCHR:
                dirent->d_type = DT_CHR;
                break;
            case VNODE_TYPE::VDIR:
                dirent->d_type = DT_DIR;
                break;
            case VNODE_TYPE::VLNK:
                dirent->d_type = DT_LNK;
                break;
            case VNODE_TYPE::VFIFO:
                dirent->d_type = DT_FIFO;
                break;
            case VNODE_TYPE::VREG:
                dirent->d_type = DT_REG;
                break;
            case VNODE_TYPE::VSOC:
                dirent->d_type = DT_SOCK;
                break;
            default:
                dirent->d_type = DT_UNKNOWN;
                break;
        }

        strcpy(dirent->d_name, node->name);
        dirent++;
        offset += sizeof(linux_dirent64);
        ofd->dl_off++;
        vnode_t* prev = tnode;
        tnode = tnode->next;
        if (prev->is_static == false) delete prev;
    }

    return offset;
}

int sys_chdir(char *pathname){
    task_t* ctask = task_scheduler::get_current_task();

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }

    vnode_t* node = vfs::resolve_path(pathname);
    node->open();    
    if (node == nullptr) return -ENOENT;
    if (ctask->nd_cwd) ctask->nd_cwd->close();
    
    ctask->nd_cwd = node;
    return 0;
}

int sys_fchdir(int fd){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* ofd = ctask->get_fd(fd);

    if (ofd == nullptr) return -ENOENT;

    ctask->nd_cwd = ofd->node;
    return 0;
}

int64_t sys_sendfile(int out_fd, int in_fd, int64_t* offset, size_t count){
    task_t* ctask = task_scheduler::get_current_task();

    open_fd_t* in = ctask->get_fd(in_fd); 
    open_fd_t* out = ctask->get_fd(out_fd);

    if (in == nullptr || out == nullptr) return -ENOENT;

    char* data_buffer = new char[count];
    int64_t prev_offset = 0;
    if (offset != nullptr) {
        prev_offset = sys_lseek(in_fd, 0, SEEK_CUR);
        sys_lseek(in_fd, *offset, SEEK_SET);
    }
    int ret = sys_read(in_fd, data_buffer, count);
    if (ret < 0) return 0;
    int ret2 = sys_write(out_fd, data_buffer, ret);

    delete[] data_buffer;

    if (offset != nullptr){
        sys_lseek(in_fd, prev_offset, SEEK_SET);
    }
    return ret2;
}

/* NOTE FOR PERMISSIONS (If i ever implement them) */
/*
mkdir() attempts to create a directory named pathname.

The argument mode specifies the mode for the new directory (see inode(7)).
It is modified by the process's umask in the usual way: in the absence of
a default ACL, the mode of the created directory is (mode & ~umask & 0777).
Whether other mode bits are honored for the created directory depends on the
operating system. For Linux, see NOTES below.

The newly created directory will be owned by the effective user ID of the process.
If the directory containing the file has the set-group-ID bit set, or if the filesystem
is mounted with BSD group semantics (mount -o bsdgroups or, synonymously mount -o grpid),
the new directory will inherit the group ownership from its parent; otherwise it will be
owned by the effective group ID of the process.

If the parent directory has the set-group-ID bit set, then so will the newly created directory.

https://manpages.debian.org/unstable/manpages-dev/mkdir.2.en.html
*/

void split_path(char* fullpath, char*& parent, char*& name) {
    size_t len = strlen(fullpath);
    int last_slash = -1;

    // Find the last '/'
    for (int i = len - 1; i >= 0; i--) {
        if (fullpath[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash <= 0) {
        // No slash or root only: everything is in name
        parent = new char[2];
        strcpy(parent, "/");

        name = new char[len + 1];
        strcpy(name, fullpath[0] == '/' ? fullpath + 1 : fullpath);
    } else {
        parent = new char[last_slash + 1]; // +1 for null
        memcpy(parent, fullpath, last_slash);
        parent[last_slash] = '\0';

        size_t name_len = len - last_slash - 1;
        name = new char[name_len + 1];
        memcpy(name, fullpath + last_slash + 1, name_len);
        name[name_len] = '\0';
    }
}

int sys_mkdir(char *pathname, unsigned long mode){
    task_t* ctask = task_scheduler::get_current_task();
    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }

    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    
    char* parent;
    char* name;
    split_path(pathname, parent, name);
    // serialf("PATH: %s NAME: %s\n\r", parent, name);
    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) return -ENOENT;
    if (pnode->type != VNODE_TYPE::VDIR) return -ENOTDIR;
    return pnode->mkfile(name, true);
}

int sys_mkdirat(int dirfd, char *pathname, unsigned long mode){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* dir = ctask->get_fd(dirfd);

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);


        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/' && dir == nullptr){
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->nd_cwd : dir->node);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    if (vfs::resolve_path(pathname) != nullptr) return -EEXIST;
    if (pathname[strlen(pathname) - 1] == '/') pathname[strlen(pathname) - 1] = '\0'; // remove the trailing '/'
    int stlen = strlen(pathname);
    char* parent = new char[stlen];
    char* dirname = new char[stlen];
    memset(parent, 0, stlen);
    memset(dirname, 0, stlen);
    for (int i = stlen -1; i <= 0; i++){
        if (pathname[i] == '/'){
            memcpy(parent, pathname, i);
            parent[i] = '\0';
            memcpy(dirname, pathname + i, stlen - i);
            dirname[stlen - i] = '\0';
        }
    }

    vnode_t* pnode = vfs::resolve_path(parent);
    if (pnode == nullptr) return -ENOENT;
    if (pnode->type != VNODE_TYPE::VDIR) return -ENOTDIR;

    return pnode->mkfile(dirname, true);
}

int sys_pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const timespc* timeout, uint64_t* sigmask){
    task_t* ctask = task_scheduler::get_current_task();
    uint64_t sv = ctask->signal_mask;
    if (sigmask) ctask->signal_mask = *sigmask;

    if (nfds <= 0 || nfds > FD_SETSIZE) return -EINVAL;

    // Convert timeout to milliseconds
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_nsec / 1000000);
    }

    // Convert fd_sets to pollfd array
    pollfd pfds[FD_SETSIZE];
    int poll_count = 0;

    for (int fd = 0; fd < nfds; ++fd) {
        short events = 0;
        if (readfds && FD_ISSET(fd, readfds)) events |= POLLIN;
        if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
        // exceptfds are usually for exceptional conditions (e.g. OOB), but we'll ignore for now

        if (events) {
            pfds[poll_count].fd = fd;
            pfds[poll_count].events = events;
            pfds[poll_count].revents = 0;
            ++poll_count;
        }
    }

    int ready = sys_poll(pfds, poll_count, timeout_ms);
    if (ready < 0) {ctask->signal_mask = sv; return ready;}

    // Clear all sets
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds); // Not used here, but clear anyway

    // Reconstruct fd_sets
    for (int i = 0; i < poll_count; ++i) {
        int fd = pfds[i].fd;
        short revents = pfds[i].revents;

        if (readfds && (revents & POLLIN)) FD_SET(fd, readfds);
        if (writefds && (revents & POLLOUT)) FD_SET(fd, writefds);
        // No handling for exceptfds (unless you add POLLPRI logic)
    }

    ctask->signal_mask = sv; 
    return ready;
}

int sys_truncate_node(vnode_t* node, size_t length){
    return node->truncate(length);
}

int sys_truncate(const char *path, size_t length){
    vnode_t* node = vfs::resolve_path(path);
    if (vfs::resolve_path(path) == nullptr) return -ENOENT;

    return sys_truncate_node(node, length);
}

int sys_ftruncate(int fd, size_t length){
    task_t* ctask = task_scheduler::get_current_task();
    open_fd_t* ofd = ctask->get_fd(fd);
    if (ofd == nullptr) return -EBADFD;

    return sys_truncate_node(ofd->node, length);
}

int sys_chown(const char *pathname, int owner, int group){
    vnode_t* node = vfs::resolve_path(pathname);
    if (node == nullptr) return -ENOENT;

    node->uid = owner;
    node->gid = group;

    node->save_entry();
    return 0;
}
void register_fs_syscalls(){
    register_syscall(SYSCALL_WRITE, (syscall_handler_t)sys_write);
    register_syscall(SYSCALL_WRITEV, (syscall_handler_t)sys_writev);
    register_syscall(SYSCALL_READ, (syscall_handler_t)sys_read);
    register_syscall(SYSCALL_READV, (syscall_handler_t)sys_readv);
    register_syscall(SYSCALL_LSEEK, (syscall_handler_t)sys_lseek);
    register_syscall(SYSCALL_IOCTL, (syscall_handler_t)sys_ioctl);
    register_syscall(SYSCALL_OPEN, (syscall_handler_t)sys_open);
    register_syscall(SYSCALL_OPENAT, (syscall_handler_t)sys_openat);
    register_syscall(SYSCALL_CLOSE, (syscall_handler_t)sys_close);
    register_syscall(SYSCALL_DUP, (syscall_handler_t)sys_dup);
    register_syscall(SYSCALL_DUP2, (syscall_handler_t)sys_dup2);
    register_syscall(SYSCALL_FCNTL, (syscall_handler_t)sys_fcntl);
    register_syscall(SYSCALL_STAT, (syscall_handler_t)sys_stat);
    register_syscall(SYSCALL_FSTAT, (syscall_handler_t)sys_fstat);
    register_syscall(SYSCALL_LSTAT, (syscall_handler_t)sys_lstat);
    register_syscall(SYSCALL_NEWFSTATAT, (syscall_handler_t)sys_newfstatat);
    register_syscall(SYSCALL_POLL, (syscall_handler_t)sys_poll);
    register_syscall(SYSCALL_PIPE, (syscall_handler_t)pipe);
    register_syscall(SYSCALL_PIPE2, (syscall_handler_t)pipe);
    register_syscall(SYSCALL_FACCESSAT2, (syscall_handler_t)sys_faccessat2);
    register_syscall(SYSCALL_FACCESSAT, (syscall_handler_t)sys_faccessat);
    register_syscall(SYSCALL_ACCESS, (syscall_handler_t)sys_access);
    register_syscall(SYSCALL_GETDENTS64, (syscall_handler_t)sys_getdents64);
    register_syscall(SYSCALL_CHDIR, (syscall_handler_t)sys_chdir);
    register_syscall(SYSCALL_FCHDIR, (syscall_handler_t)sys_fchdir);
    register_syscall(SYSCALL_SENDFILE, (syscall_handler_t)sys_sendfile);
    register_syscall(SYSCALL_MKDIR, (syscall_handler_t)sys_mkdir);
    register_syscall(SYSCALL_MKDIRAT, (syscall_handler_t)sys_mkdirat);
    register_syscall(SYSCALL_CREAT, (syscall_handler_t)sys_creat);
    register_syscall(SYSCALL_PREAD64, (syscall_handler_t)sys_pread64);
    register_syscall(SYSCALL_PSELECT, (syscall_handler_t)sys_pselect);
    register_syscall(SYSCALL_TRUNCATE, (syscall_handler_t)sys_truncate);
    register_syscall(SYSCALL_FTRUNCATE, (syscall_handler_t)sys_ftruncate);
    register_syscall(SYSCALL_CHOWN, (syscall_handler_t)sys_chown);

}

/*
TODO: Release any memory used for string manipulation
*/