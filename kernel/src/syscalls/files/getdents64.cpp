#include <syscalls/syscalls.h>

struct linux_dirent {
    unsigned long d_ino;
    long d_off;    /* Not an offset; see getdents() */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[128]; /* Filename (null-terminated) */
};

enum DT_TYPE{
    DT_UNKNOWN = 0,
    DT_FIFO = 1,
    DT_CHR = 2,
    DT_DIR = 4,
    DT_BLK = 6,
    DT_REG = 8,
    DT_LNK = 10,
    DT_SOCK = 12,
    DT_WHT = 14
};

long getdents64(unsigned int fd, linux_dirent* dirp, unsigned int count){
    task_t* self = task_scheduler::get_current_task();
    file_t* file = self->fd_table->get_file(fd);

    if (file == nullptr) return -EBADF;
    if (!S_ISDIR(file->node->attributes.mode)) return -ENOTDIR;
    if (count < sizeof(linux_dirent)) return -EINVAL;

    linux_dirent dirent;
    int offset = 0;

    dentry_t* entries = nullptr;
    int cnt = file->node->get_listing(entries, file->offset, count / sizeof(linux_dirent));

    if (cnt <= 0) return cnt;

    if (file->offset >= cnt){
        file->offset = 0;
        return 0;
    }

    for (int i = 0; i < cnt; i++){
        if ((offset + sizeof(linux_dirent)) >= count){
            break;
        }

        dentry_t *dentry = &entries[i];
        vnode_t *node = dentry->fetch_vnode();

        if (!node) continue;

        memset(&dirent, 0, sizeof(dirent));

        dirent.d_ino = node->inode;
        dirent.d_reclen = sizeof(linux_dirent);
        if (S_ISBLK(node->attributes.mode)){
            dirent.d_type = DT_BLK;
        } else if (S_ISCHR(node->attributes.mode)){
            dirent.d_type = DT_CHR;
        } else if (S_ISDIR(node->attributes.mode)){
            dirent.d_type = DT_DIR;
        } else if (S_ISLNK(node->attributes.mode)){
            dirent.d_type = DT_LNK;
        } else if (S_ISFIFO(node->attributes.mode)){
            dirent.d_type = DT_FIFO;
        } else if (S_ISREG(node->attributes.mode)){
            dirent.d_type = DT_REG;
        } else if (S_ISSOCK(node->attributes.mode)){
            dirent.d_type = DT_SOCK;
        } else {
            dirent.d_type = DT_UNKNOWN;
        }

        delete node;
        
        strcpy(dirent.d_name, dentry->name);

        self->write_to_userspace((void*)((uint64_t)dirp + offset), &dirent, sizeof(linux_dirent));

        offset += sizeof(linux_dirent);

        file->offset++;
    }

    delete[] entries;

    return offset;
}

REGISTER_SYSCALL(SYS_getdents64, getdents64);