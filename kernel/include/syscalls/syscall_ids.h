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

#define SYS_ioctl           16
#define SYS_pread64         17

#define SYS_readv           19
#define SYS_writev          20
#define SYS_access          21

#define SYS_getpid          39

#define SYS_exit            60

#define SYS_uname           63

#define SYS_getcwd          79

#define SYS_getuid          102

#define SYS_getgid          104
#define SYS_setuid          105
#define SYS_setgid          106
#define SYS_geteuid         107

#define SYS_getegid         108

#define SYS_setreuid        113
#define SYS_setregid        114

#define SYS_setresuid       117
#define SYS_getresuid       118
#define SYS_setresgid       119
#define SYS_getresgid       120

#define SYS_arch_prctl      158

#define SYS_set_tid_address 218

#define SYS_clock_gettime   228

#define SYS_exit_group      231

#define SYS_openat          257

#define SYS_newfstatat      262

#define SYS_getrandom       318