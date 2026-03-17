/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_FCNTL_H
#define _LINUX_FCNTL_H
/*
#include <asm/fcntl.h>
#include <linux/openat2.h>
*/
#include <syscalls/files/bits/fcntl.h>

#define F_LINUX_SPECIFIC_BASE	1024

#define F_SETLEASE	(F_LINUX_SPECIFIC_BASE + 0)
#define F_GETLEASE	(F_LINUX_SPECIFIC_BASE + 1)

/*
 * Cancel a blocking posix lock; internal use only until we expose an
 * asynchronous lock api to userspace:
 */
#define F_CANCELLK	(F_LINUX_SPECIFIC_BASE + 5)

/* Create a file descriptor with FD_CLOEXEC set. */
#define F_DUPFD_CLOEXEC	(F_LINUX_SPECIFIC_BASE + 6)

/*
 * Request nofications on a directory.
 * See below for events that may be notified.
 */
#define F_NOTIFY	(F_LINUX_SPECIFIC_BASE+2)

/*
 * Set and get of pipe page size array
 */
#define F_SETPIPE_SZ	(F_LINUX_SPECIFIC_BASE + 7)
#define F_GETPIPE_SZ	(F_LINUX_SPECIFIC_BASE + 8)

/*
 * Set/Get seals
 */
#define F_ADD_SEALS	(F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS	(F_LINUX_SPECIFIC_BASE + 10)

/*
 * Types of seals
 */
#define F_SEAL_SEAL	0x0001	/* prevent further seals from being set */
#define F_SEAL_SHRINK	0x0002	/* prevent file from shrinking */
#define F_SEAL_GROW	0x0004	/* prevent file from growing */
#define F_SEAL_WRITE	0x0008	/* prevent writes */
#define F_SEAL_FUTURE_WRITE	0x0010  /* prevent future writes while mapped */
/* (1U << 31) is reserved for signed error codes */

/*
 * Set/Get write life time hints. {GET,SET}_RW_HINT operate on the
 * underlying inode, while {GET,SET}_FILE_RW_HINT operate only on
 * the specific file.
 */
#define F_GET_RW_HINT		(F_LINUX_SPECIFIC_BASE + 11)
#define F_SET_RW_HINT		(F_LINUX_SPECIFIC_BASE + 12)
#define F_GET_FILE_RW_HINT	(F_LINUX_SPECIFIC_BASE + 13)
#define F_SET_FILE_RW_HINT	(F_LINUX_SPECIFIC_BASE + 14)

/*
 * Valid hint values for F_{GET,SET}_RW_HINT. 0 is "not set", or can be
 * used to clear any hints previously set.
 */
#define RWH_WRITE_LIFE_NOT_SET	0
#define RWH_WRITE_LIFE_NONE	1
#define RWH_WRITE_LIFE_SHORT	2
#define RWH_WRITE_LIFE_MEDIUM	3
#define RWH_WRITE_LIFE_LONG	4
#define RWH_WRITE_LIFE_EXTREME	5

/*
 * The originally introduced spelling is remained from the first
 * versions of the patch set that introduced the feature, see commit
 * v4.13-rc1~212^2~51.
 */
#define RWF_WRITE_LIFE_NOT_SET	RWH_WRITE_LIFE_NOT_SET

/*
 * Types of directory notifications that may be requested.
 */
#define DN_ACCESS	0x00000001	/* File accessed */
#define DN_MODIFY	0x00000002	/* File modified */
#define DN_CREATE	0x00000004	/* File created */
#define DN_DELETE	0x00000008	/* File removed */
#define DN_RENAME	0x00000010	/* File renamed */
#define DN_ATTRIB	0x00000020	/* File changed attibutes */
#define DN_MULTISHOT	0x80000000	/* Don't remove notifier */

/*
 * The constants AT_REMOVEDIR and AT_EACCESS have the same value.  AT_EACCESS is
 * meaningful only to faccessat, while AT_REMOVEDIR is meaningful only to
 * unlinkat.  The two functions do completely different things and therefore,
 * the flags can be allowed to overlap.  For example, passing AT_REMOVEDIR to
 * faccessat would be undefined behavior and thus treating it equivalent to
 * AT_EACCESS is valid undefined behavior.
 */
#define AT_FDCWD		-100    /* Special value used to indicate
                                           openat should use the current
                                           working directory. */
#define AT_SYMLINK_NOFOLLOW	0x100   /* Do not follow symbolic links.  */
#define AT_EACCESS		0x200	/* Test access permitted for
                                           effective IDs, not real IDs.  */
#define AT_REMOVEDIR		0x200   /* Remove directory instead of
                                           unlinking file.  */
#define AT_SYMLINK_FOLLOW	0x400   /* Follow symbolic links.  */
#define AT_NO_AUTOMOUNT		0x800	/* Suppress terminal automount traversal */
#define AT_EMPTY_PATH		0x1000	/* Allow empty relative pathname */

#define AT_STATX_SYNC_TYPE	0x6000	/* Type of synchronisation required from statx() */
#define AT_STATX_SYNC_AS_STAT	0x0000	/* - Do whatever stat() does */
#define AT_STATX_FORCE_SYNC	0x2000	/* - Force the attributes to be sync'd with the server */
#define AT_STATX_DONT_SYNC	0x4000	/* - Don't sync attributes with the server */

#define AT_RECURSIVE		0x8000	/* Apply to the entire subtree */



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

#ifndef S_IRUSR
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
#endif

#define S_IFSOCK    0140000
#define S_IFDIR	    0040000
#define S_IFCHR	    0020000
#define S_IFBLK	    0060000
#define S_IFREG	    0100000
#define S_IFIFO	    0010000
#define S_IFLNK	    0120000


#endif /* _LINUX_FCNTL_H */
