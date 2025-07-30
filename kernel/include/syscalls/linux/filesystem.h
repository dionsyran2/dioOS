#pragma once

#define AT_FDCWD (-100)


#define _IOC(a,b,c,d) ( ((a)<<30) | ((b)<<8) | (c) | ((d)<<16) )
#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U

#define _IO(a,b) _IOC(_IOC_NONE,(a),(b),0)
#define _IOW(a,b,c) _IOC(_IOC_WRITE,(a),(b),sizeof(c))
#define _IOR(a,b,c) _IOC(_IOC_READ,(a),(b),sizeof(c))
#define _IOWR(a,b,c) _IOC(_IOC_READ|_IOC_WRITE,(a),(b),sizeof(c))

#define TCGETS          0x5401
#define TCSETS          0x5402
#define TCSETSW         0x5403
#define TCSETSF         0x5404
#define TCGETA          0x5405
#define TCSETA          0x5406
#define TCSETAW         0x5407
#define TCSETAF         0x5408
#define TCSBRK          0x5409
#define TCXONC          0x540A
#define TCFLSH          0x540B
#define TIOCEXCL        0x540C
#define TIOCNXCL        0x540D
#define TIOCSCTTY       0x540E
#define TIOCGPGRP       0x540F
#define TIOCSPGRP       0x5410
#define TIOCOUTQ        0x5411
#define TIOCSTI         0x5412
#define TIOCGWINSZ      0x5413
#define TIOCSWINSZ      0x5414
#define TIOCMGET        0x5415
#define TIOCMBIS        0x5416
#define TIOCMBIC        0x5417
#define TIOCMSET        0x5418
#define TIOCGSOFTCAR    0x5419
#define TIOCSSOFTCAR    0x541A
#define FIONREAD        0x541B
#define TIOCINQ         FIONREAD
#define TIOCLINUX       0x541C
#define TIOCCONS        0x541D
#define TIOCGSERIAL     0x541E
#define TIOCSSERIAL     0x541F
#define TIOCPKT         0x5420
#define FIONBIO         0x5421
#define TIOCNOTTY       0x5422
#define TIOCSETD        0x5423
#define TIOCGETD        0x5424
#define TCSBRKP         0x5425  /* Needed for POSIX tcsendbreak() */
#define TIOCSBRK        0x5427  /* BSD compatibility */
#define TIOCCBRK        0x5428  /* BSD compatibility */
#define TIOCGSID        0x5429  /* Return the session ID of FD */
#define TCGETS2         _IOR('T', 0x2A, struct termios2)
#define TCSETS2         _IOW('T', 0x2B, struct termios2)
#define TCSETSW2        _IOW('T', 0x2C, struct termios2)
#define TCSETSF2        _IOW('T', 0x2D, struct termios2)
#define TIOCGRS485      0x542E

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_DATA 3
#define SEEK_HOLE 4
/* open/fcntl.  */
#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02
#ifndef O_CREAT
# define O_CREAT	   0100	/* Not fcntl.  */
#endif
#ifndef O_EXCL
# define O_EXCL		   0200	/* Not fcntl.  */
#endif
#ifndef O_NOCTTY
# define O_NOCTTY	   0400	/* Not fcntl.  */
#endif
#ifndef O_TRUNC
# define O_TRUNC	  01000	/* Not fcntl.  */
#endif
#ifndef O_APPEND
# define O_APPEND	  02000
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK	  04000
#endif
#ifndef O_NDELAY
# define O_NDELAY	O_NONBLOCK
#endif
#ifndef O_SYNC
# define O_SYNC	       04010000
#endif
#define O_FSYNC		O_SYNC
#ifndef O_ASYNC
# define O_ASYNC	 020000
#endif
#ifndef __O_LARGEFILE
# define __O_LARGEFILE	0100000
#endif

#ifndef __O_DIRECTORY
# define __O_DIRECTORY	0200000
#endif
#ifndef __O_NOFOLLOW
# define __O_NOFOLLOW	0400000
#endif
#ifndef __O_CLOEXEC
# define __O_CLOEXEC   02000000
#endif
#ifndef __O_DIRECT
# define __O_DIRECT	 040000
#endif
#ifndef __O_NOATIME
# define __O_NOATIME   01000000
#endif
#ifndef __O_PATH
# define __O_PATH     010000000
#endif
#ifndef __O_DSYNC
# define __O_DSYNC	 010000
#endif
#ifndef __O_TMPFILE
# define __O_TMPFILE   (020000000 | __O_DIRECTORY)
#endif

#ifndef F_GETLK
# ifndef __USE_FILE_OFFSET64
#  define F_GETLK	5	/* Get record locking info.  */
#  define F_SETLK	6	/* Set record locking info (non-blocking).  */
#  define F_SETLKW	7	/* Set record locking info (blocking).  */
# else
#  define F_GETLK	F_GETLK64  /* Get record locking info.  */
#  define F_SETLK	F_SETLK64  /* Set record locking info (non-blocking).*/
#  define F_SETLKW	F_SETLKW64 /* Set record locking info (blocking).  */
# endif
#endif
#ifndef F_GETLK64
# define F_GETLK64	12	/* Get record locking info.  */
# define F_SETLK64	13	/* Set record locking info (non-blocking).  */
# define F_SETLKW64	14	/* Set record locking info (blocking).  */
#endif

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4

#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#  define R_OK	4		/* Test for read permission.  */
#  define W_OK	2		/* Test for write permission.  */
#  define X_OK	1		/* Test for execute permission.  */
#  define F_OK	0		/* Test for existence.  */

int sys_pread64(int fd, char* buf, size_t count, size_t off);

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

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */


    unsigned long  d_off;     /* Not an offset; see below */


    unsigned short d_reclen;  /* Length of this linux_dirent */


    char* d_name;  /* Filename (null-terminated) */

    char pad;

    char d_type;
};

struct linux_dirent64 {
    unsigned long d_ino;
    long d_off;    /* Not an offset; see getdents() */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[128]; /* Filename (null-terminated) */
};

#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

typedef long int __fd_mask;
#define __NFDBITS	(8 * (int) sizeof (__fd_mask))

#define FD_ZERO(p) \
    memset((p), 0, sizeof(fd_set))

#define FD_SET(fd, p) \
    ((p)->fds_bits[(fd) / __NFDBITS] |= (1UL << ((fd) % __NFDBITS)))

#define FD_CLR(fd, p) \
    ((p)->fds_bits[(fd) / __NFDBITS] &= ~(1UL << ((fd) % __NFDBITS)))

#define FD_ISSET(fd, p) \
    ((p)->fds_bits[(fd) / __NFDBITS] & (1UL << ((fd) % __NFDBITS)))