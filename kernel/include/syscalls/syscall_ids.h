#pragma once

#define SYS_read            0
#define SYS_write           1
#define SYS_open            2
#define SYS_close           3
#define SYS_stat            4
#define SYS_fstat           5
#define SYS_lstat           6
#define SYS_poll            7

#define SYS_mmap            9
#define SYS_mprotect        10
#define SYS_munmap          11
#define SYS_brk             12
#define SYS_sigaction       13
#define SYS_sigprocmask     14
#define SYS_sigreturn       15
#define SYS_ioctl           16
#define SYS_pread64         17

#define SYS_readv           19
#define SYS_writev          20
#define SYS_access          21

#define SYS_dup             32
#define SYS_dup2            33

#define SYS_getpid          39

#define SYS_clone           56
#define SYS_fork            57
#define SYS_vfork           58
#define SYS_execve          59
#define SYS_exit            60
#define SYS_wait4           61
#define SYS_kill            62

#define SYS_uname           63

#define SYS_fcntl           72

#define SYS_getcwd          79
#define SYS_chdir           80
#define SYS_fchdir          81

#define SYS_readlink        89
#define SYS_chmod           90
#define SYS_fchmod          91
#define SYS_chown           92
#define SYS_fchown          93
#define SYS_lchown          94

#define SYS_getuid          102

#define SYS_getgid          104
#define SYS_setuid          105
#define SYS_setgid          106
#define SYS_geteuid         107
#define SYS_getegid         108
#define SYS_setpgid         109
#define SYS_getppid         110
#define SYS_getpgrp         111
#define SYS_setsid          112
#define SYS_setreuid        113
#define SYS_setregid        114
#define SYS_getgroups       115
#define SYS_setgroups       116
#define SYS_setresuid       117
#define SYS_getresuid       118
#define SYS_setresgid       119
#define SYS_getresgid       120
#define SYS_getpgid         121

#define SYS_getsid          124

#define SYS_arch_prctl      158

#define SYS_time            201

#define SYS_getdents64      217
#define SYS_set_tid_address 218

#define SYS_clock_gettime   228

#define SYS_exit_group      231

#define SYS_openat          257

#define SYS_newfstatat      262

#define SYS_readlinkat      267

#define SYS_pselect6        270

#define SYS_getrandom       318