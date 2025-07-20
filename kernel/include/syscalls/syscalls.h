#pragma once
#define LOG_SYSCALLS
#include <stdint.h>
#include <stddef.h>
#include "termios.h"
#include <scheduling/task/scheduler.h>
#include <syscalls/linux/filesystem.h>
#include <syscalls/linux/thread.h>
#include <kerrno.h>


void register_fs_syscalls();
void register_thread_syscalls();
void register_mem_syscalls();
void register_sig_syscalls();

#define SYSCALL_READ                    0
#define SYSCALL_WRITE					1
#define SYSCALL_OPEN                    2
#define SYSCALL_CLOSE                   3
#define SYSCALL_STAT					4
#define SYSCALL_FSTAT					5
#define SYSCALL_LSTAT                   6
#define SYSCALL_POLL					7
#define SYSCALL_LSEEK                   8
#define SYSCALL_MMAP                    9
#define SYSCALL_MPROTECT                10
#define SYSCALL_UNMAP					11
#define SYSCALL_BRK                     12
#define SYSCALL_SIGACTION               13
#define SYSCALL_SIGPROCMASK             14
#define SYSCALL_SIGRETURN				15
#define SYSCALL_IOCTL                   16
#define SYSCALL_READV					19
#define SYSCALL_WRITEV                  20
#define SYSCALL_ACCESS                  21
#define SYSCALL_PIPE                    22
#define SYSCALL_SELECT                  23
#define SYSCALL_MINCORE					27
#define SYSCALL_DUP						32
#define SYSCALL_DUP2					33
#define SYSCALL_NANOSLEEP				35
#define SYSCALL_GETPID					39
#define SYSCALL_SENDFILE				40
#define SYSCALL_CLONE					56
#define SYSCALL_FORK                    57
#define SYSCALL_EXECVE                  59
#define SYSCALL_WAIT4                   61
#define SYSCALL_KILL					62
#define SYSCALL_UNAME                   63
#define SYSCALL_FNCNTL					72
#define SYSCALL_GETCWD					79
#define SYSCALL_CHDIR					80
#define SYSCALL_FCHDIR					81
#define SYSCALL_MKDIR					83
#define SYSCALL_GETRLIMIT				97
#define SYSCALL_GETUID                  102
#define SYSCALL_GETGID                  104
#define SYSCALL_SETUID                  105
#define SYSCALL_SETGID                  106
#define SYSCALL_GETEUID                 107
#define SYSCALL_GETEGID                 108
#define SYSCALL_SETPGID					109
#define SYSCALL_GETPPID					110
#define SYSCALL_GETPGID					121
#define SYSCALL_SIGALTSTACK				131
#define SYSCALL_ARCH_PRCTL              158
#define SYSCALL_GETTID                  186
#define SYSCALL_TKILL					200
#define SYSCALL_FUTEX					202
#define SYSCALL_GETAFFINITY				204
#define SYSCALL_GETDENTS64				217
#define SYSCALL_SET_TID_ADDR            218
#define SYSCALL_GETTIME                 228
#define SYSCALL_EXIT_GROUP              231
#define SYSCALL_OPENAT					257
#define SYSCALL_MKDIRAT					258
#define SYSCALL_NEWFSTATAT				262
#define SYSCALL_FACCESSAT               269
#define SYSCALL_PSELECT					270
#define SYSCALL_PRLIMIT					302
#define SYSCALL_FACCESSAT2				439


#define SIG_BLOCK    0
#define SIG_UNBLOCK  1
#define SIG_SETMASK  2




#define F_SETOWN_EX    0x406
#define F_GETOWN_EX    0x407

struct f_owner_ex {
    int type;   // F_OWNER_TID, F_OWNER_PID, or F_OWNER_PGRP
    int pid;
};

struct iovec {
    void *iov_base;
    size_t iov_len;
};

struct winsize {
    unsigned short ws_row;    // number of rows (lines) in the terminal
    unsigned short ws_col;    // number of columns in the terminal
    unsigned short ws_xpixel; // width in pixels (may be zero or not supported)
    unsigned short ws_ypixel; // height in pixels (may be zero or not supported)
};

struct timespc{
    uint64_t tv_sec;
    uint32_t tv_nsec;
};

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

typedef unsigned long long rlim_t;

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000


#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   29
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED SIGSYS

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200

struct stat {
	unsigned long st_dev;
	unsigned long st_ino;
	unsigned long st_nlink;

	unsigned int st_mode;
	unsigned int st_uid;
	unsigned int st_gid;
	int __pad0;
	unsigned long st_rdev;
	long st_size;
	long st_blksize;
	long st_blocks;

	struct timespc st_atim;
	struct timespc st_mtim;
	struct timespc st_ctim;
	long __unused[3];
};



struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
#ifdef _GNU_SOURCE
	char domainname[65];
#else
	char __domainname[65];
#endif
};

typedef uint64_t (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                      uint64_t arg4, uint64_t arg5, uint64_t arg6);

#define MAX_SYSCALLS 512

void setup_syscalls();

int register_syscall(int num, syscall_handler_t handler);
extern "C" void syscall_entry();

extern "C" uint64_t next_syscall_stack;
extern "C" uint64_t user_syscall_stack;