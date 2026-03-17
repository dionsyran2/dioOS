#include <syscalls/files/files.h>
#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>

struct linux_dirent64 {
    unsigned long d_ino;
    long d_off;    /* Not an offset; see getdents() */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[128]; /* Filename (null-terminated) */
};

int64_t sys_getdents64(int fd, void* dirp, size_t count){
    task_t* self = task_scheduler::get_current_task();
    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    if (ofd->node->type != VDIR) return -ENOTDIR;
    if (count < sizeof(linux_dirent64)) return -EINVAL;

    linux_dirent64 dirent;
    int offset = 0;
    
    vnode_t* tnode = nullptr;
    int cnt = ofd->node->read_dir(&tnode);

    if (cnt <= 0) return cnt;

    if (ofd->offset >= cnt){
        ofd->offset = 0;
        while(tnode != nullptr){
            vnode_t* prev = tnode;
            tnode = tnode->next;
        }
        return 0;
    }

    if (ofd->offset){
        for (int i = 0; i < ofd->offset; i++){
            tnode = tnode->next;
            if (tnode == nullptr) break;
        }
    }

    while(tnode != nullptr){
        if ((offset + sizeof(linux_dirent64)) >= count){
            break;
        }

        vnode_t* node = tnode;

        memset(&dirent, 0, sizeof(dirent));
        
        dirent.d_ino = (unsigned long)node;
        dirent.d_reclen = sizeof(linux_dirent64);
        switch (node->type){
            case VBLK:
                dirent.d_type = DT_BLK;
                break;
            case VCHR:
                dirent.d_type = DT_CHR;
                break;
            case VDIR:
                dirent.d_type = DT_DIR;
                break;
            case VLNK:
                dirent.d_type = DT_LNK;
                break;
            case VPIPE:
                dirent.d_type = DT_FIFO;
                break;
            case VREG:
                dirent.d_type = DT_REG;
                break;
            case VSOC:
                dirent.d_type = DT_SOCK;
                break;
            default:
                dirent.d_type = DT_UNKNOWN;
                break;
        }

        strcpy(dirent.d_name, node->name);
        
        self->write_memory((void*)((uint64_t)dirp + offset), &dirent, sizeof(linux_dirent64));
        
        offset += sizeof(linux_dirent64);


        ofd->offset++;
        vnode_t* prev = tnode;
        tnode = tnode->next;
    }

    return offset;
}

REGISTER_SYSCALL(SYS_getdents64, sys_getdents64);