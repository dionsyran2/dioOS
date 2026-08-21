#pragma once
#include <stdint.h>
#include <stddef.h>

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#define __FD_ZERO(s) \
  do {									      \
    unsigned int __i;							      \
    fd_set *__arr = (s);						      \
    for (__i = 0; __i < sizeof (fd_set) / sizeof (__fd_mask); ++__i)	      \
      __FDS_BITS (__arr)[__i] = 0;					      \
  } while (0)
#define __FD_SET(d, s) \
  ((void) (__FDS_BITS (s)[__FD_ELT(d)] |= __FD_MASK(d)))
#define __FD_CLR(d, s) \
  ((void) (__FDS_BITS (s)[__FD_ELT(d)] &= ~__FD_MASK(d)))
#define __FD_ISSET(d, s) \
  ((__FDS_BITS (s)[__FD_ELT (d)] & __FD_MASK (d)) != 0)
# define __FDS_BITS(set) ((set)->fds_bits)
typedef long int __fd_mask;
#define __NFDBITS	(8 * (int) sizeof (__fd_mask))
#define	__FD_ELT(d)	((d) / __NFDBITS)
#define	__FD_MASK(d)	((__fd_mask) (1UL << ((d) % __NFDBITS)))

#define	__FD_MASK(d)	((__fd_mask) (1UL << ((d) % __NFDBITS)))

#define	FD_SET(fd, fdsetp)	__FD_SET (fd, fdsetp)
#define	FD_CLR(fd, fdsetp)	__FD_CLR (fd, fdsetp)
#define	FD_ISSET(fd, fdsetp)	__FD_ISSET (fd, fdsetp)
#define	FD_ZERO(fdsetp)		__FD_ZERO (fdsetp)

struct pselect6_sigmask {
    uint64_t *ss;     // Pointer to the signal mask in userspace
    size_t ss_len;    // Size of the signal mask
};

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;
