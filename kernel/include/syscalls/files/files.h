#pragma once
#include <stdint.h>
#include <stddef.h>

enum DIRENT_TYPE{
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

// ACCESS
long sys_access(const char *ustring, int mode);

// CWD
long sys_chdir(const char *fn);
long sys_fchdir(int fd);
long sys_getcwd(char* buf, size_t size);

// FD
long sys_open(const char* ustring, int flags, int mode);
long sys_close(int fd);
long sys_dup(int oldfd);
long sys_dup2(int oldfd, int newfd);


long sys_creat(const char *fn, int mode);
long sys_mkdir(char *fn, unsigned long mode);
long sys_mkdirat(int dirfd, char *fn, unsigned long mode);


// ctl
uint64_t sys_fcntl(int fd, uint64_t op, uint64_t arg);

long sys_pread64(int fd, char* buffer, size_t count, size_t off);
long sys_readv(int fd, const struct iovec *iov_src, int iovcnt);
long sys_read(int fd, char* user_buffer, size_t count);

long sys_write(int fd, const void* data, size_t len);
long sys_writev(int fd, const struct iovec *iov_src, int iovcnt);