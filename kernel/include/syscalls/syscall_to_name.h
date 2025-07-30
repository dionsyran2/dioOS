#pragma once
#include <cstr.h>

#define ERRNO_NAME(err)                \
    ((err) == 1 ? "EPERM" :            \
    (err) == 2 ? "ENOENT" :            \
    (err) == 3 ? "ESRCH" :             \
    (err) == 4 ? "EINTR" :             \
    (err) == 5 ? "EIO" :               \
    (err) == 6 ? "ENXIO" :             \
    (err) == 9 ? "EBADF" :             \
    (err) == 11 ? "EAGAIN" :           \
    (err) == 12 ? "ENOMEM" :           \
    (err) == 13 ? "EACCES" :           \
    (err) == 14 ? "EFAULT" :           \
    (err) == 17 ? "EEXIST" :           \
    (err) == 20 ? "ENOTDIR" :          \
    (err) == 21 ? "EISDIR" :           \
    (err) == 22 ? "EINVAL" :           \
    (err) == 24 ? "EMFILE" :           \
    (err) == 28 ? "ENOSPC" :           \
    (err) == 30 ? "EROFS" :            \
    (err) == 32 ? "EPIPE" :            \
    (err) == 38 ? "ENOSYS" :           \
    "UNKNOWN_ERRNO")

inline const char* syscall_to_name(int sysnum){
    switch(sysnum){
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 4: return "stat";
        case 5: return "fstat";
        case 6: return "lstat";
        case 7: return "poll";
        case 8: return "lseek";
        case 9: return "mmap";
        case 10: return "mprotect";
        case 11: return "munmap";
        case 12: return "brk";
        case 13: return "rt_sigaction";
        case 14: return "rt_sigprocmask";
        case 15: return "rt_sigreturn";
        case 16: return "ioctl";
        case 17: return "pread64";
        case 18: return "pwrite64";
        case 19: return "readv";
        case 20: return "writev";
        case 21: return "access";
        case 22: return "pipe";
        case 23: return "select";
        case 27: return "mincore";
        case 32: return "dup";
        case 33: return "dup2";
        case 39: return "getpid";
        case 40: return "sendfile";
        case 56: return "clone";
        case 57: return "fork";
        case 59: return "execve";
        case 61: return "wait4";
        case 62: return "kill";
        case 63: return "uname";
        case 72: return "fcntl";
        case 76: return "truncate";
        case 77: return "ftruncate";
        case 79: return "getcwd";
        case 80: return "chdir";
        case 81: return "fchdir";
        case 83: return "mkdir";
        case 85: return "creat";
        case 96: return "gettimeofday";
        case 102: return "getuid";
        case 104: return "getgid";
        case 105: return "setuid";
        case 106: return "setgid";
        case 107: return "geteuid";
        case 108: return "getegid";
        case 109: return "setpgid";
        case 110: return "getppid";
        case 111: return "getprgp";
        case 117: return "setresuid";
        case 118: return "getresuid";
        case 119: return "setresgid";
        case 120: return "getresgid";
        case 121: return "getpgid";
        case 131: return "sigaltstack";
        case 158: return "arch_prctl";
        case 186: return "gettid";
        case 200: return "tkill";
        case 201: return "time";
        case 202: return "futex";
        case 204: return "sched_getaffinity";
        case 217: return "getdents64";
        case 218: return "set_tid_address";
        case 228: return "clock_gettime";
        case 234: return "tgkill";
        case 257: return "openat";
        case 258: return "mkdirat";
        case 262: return "fstatat";
        case 269: return "faccessat";
        case 270: return "pselect6";
        case 293: return "pipe2";
        case 302: return "prlimit64";
        case 318: return "getrandom";
        case 439: return "faccessat2";
        default: return toString((uint64_t)sysnum);
    }
}

inline void syscall_args(int sysnum, int out[]){
    switch(sysnum){
        case 0: out[0] = 1; out[1] = 1; out[2] = 1; break; 
        case 1: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 2: out[0] = 2; out[1] = 1; out[2] = 1; break;
        case 3: out[0] = 1; break;
        case 4: out[0] = 2; out[1] = 1;break;
        case 5: out[0] = 1; out[1] = 1; break;
        case 6: out[0] = 2; out[1] = 1;break;
        case 7: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 8: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 9: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; out[4] = 1; out[5] = 1; break;
        case 10: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 11: out[0] = 1; out[1] = 1; break;
        case 12: out[0] = 1;break;
        case 13: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 14: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 15: break;
        case 16: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 17: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 18: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 19: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 20: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 21: out[0] = 1; out[1] = 1; break;
        case 22: out[0] = 1;break;
        case 23: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; out[4] = 1; break;
        case 27: out[0] = 1; out[1] = 3; out[2] = 1; break;
        case 32: out[0] = 1;break;
        case 33: out[0] = 1; out[1] = 1; break;
        case 40: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 56: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 59: out[0] = 2; out[1] = 1; out[2] = 1; break;
        case 61: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 62: out[0] = 1; out[1] = 1; break;
        case 63: out[0] = 1;break;
        case 72: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 76: out[0] = 2; out[1] = 3; break;
        case 77: out[0] = 3; out[1] = 3; break;
        case 79: out[0] = 2; out[1] = 1; break;
        case 80: out[0] = 2; break;
        case 81: out[0] = 1; break;
        case 83: out[0] = 2; out[1] = 1; break;
        case 85: out[0] = 2; out[1] = 1; break;
        case 96: out[0] = 1; out[1] = 1; break;
        case 102: break;
        case 104: break;
        case 105: out[0] = 1;break;
        case 106: out[0] = 1;break;
        case 107: break;
        case 108: break;
        case 109: out[0] = 1;break;
        case 117: out[0] = 3; out[1] = 3; out[2] = 3; break;
        case 118: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 119: out[0] = 3; out[1] = 3; out[2] = 3; break;
        case 120: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 131: out[0] = 1; out[1] = 1;break;
        case 158: out[0] = 1; out[1] = 1; break;
        case 200: out[0] = 3; out[1] = 3; break;
        case 201: out[0] = 1; break;
        case 202: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; break;
        case 204: out[0] = 3; out[1] = 3; out[2] = 1; break;
        case 218: out[0] = 1; break;
        case 217: out[0] = 1; out[1] = 1; out[2] = 1; break;
        case 228: out[0] = 1; out[1] = 1; break;
        case 234: out[0] = 3; out[1] = 3; out[2] = 3; break;
        case 257: out[0] = 1; out[1] = 2; out[2] = 1; out[3] = 1; break;
        case 258: out[0] = 3; out[1] = 2; out[2] = 1; break;
        case 262: out[0] = 3; out[1] = 2; out[2] = 1; out[3] = 3; break;
        case 269: out[0] = 1; out[1] = 2; out[2] = 1; break;
        case 270: out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; out[4] = 1; out[5] = 1; break;
        case 293: out[0] = 1; out[1] = 1; break;
        case 318: out[0] = 1; out[1] = 3; out[2] = 3; break;
        case 439: out[0] = 1; out[1] = 2; out[2] = 1; out[3] = 1; break;

        default: /*out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; out[4] = 1; out[5] = 1;*/ break;
    }
}