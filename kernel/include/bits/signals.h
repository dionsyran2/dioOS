#pragma once

#define	SIGINT		2	/* Interactive attention signal.  */
#define	SIGILL		4	/* Illegal instruction.  */
#define	SIGABRT		6	/* Abnormal termination.  */
#define	SIGFPE		8	/* Erroneous arithmetic operation.  */
#define	SIGSEGV		11	/* Invalid access to storage.  */
#define	SIGTERM		15	/* Termination request.  */

/* Historical signals specified by POSIX. */
#define	SIGHUP		1	/* Hangup.  */
#define	SIGQUIT		3	/* Quit.  */
#define	SIGTRAP		5	/* Trace/breakpoint trap.  */
#define	SIGKILL		9	/* Killed.  */
#define	SIGPIPE		13	/* Broken pipe.  */
#define	SIGALRM		14	/* Alarm clock.  */

#define SIGURG		23	/* Urgent data is available at a socket.  */
#define SIGSTOP		19	/* Stop, unblockable.  */
#define SIGTSTP		20	/* Keyboard stop.  */
#define SIGCONT		18	/* Continue.  */
#define SIGCHLD		17	/* Child terminated or stopped.  */
#define SIGTTIN		21	/* Background read from control terminal.  */
#define SIGTTOU		22	/* Background write to control terminal.  */
#define SIGPOLL		29	/* Pollable event occurred (System V).  */
#define SIGXFSZ		25	/* File size limit exceeded.  */
#define SIGXCPU		24	/* CPU time limit exceeded.  */
#define SIGVTALRM	26	/* Virtual timer expired.  */
#define SIGPROF		27	/* Profiling timer expired.  */
#define SIGUSR1		10	/* User-defined signal 1.  */
#define SIGUSR2		12	/* User-defined signal 2.  */
#define	SIGIOT		SIGABRT	/* IOT instruction, abort() on a PDP-11.  */



#define NSIG    64

typedef struct {
	unsigned long sig[NSIG / 64];
} sigset_t;

union sigval {
        int sival_int;
        void *sival_ptr;
};

#define SI_MAX_SIZE 128
// 3 ints (signo, errno, code) + 1 pad int = 4 ints. 128/4 - 4 = 28.
#define SI_PAD_SIZE ((SI_MAX_SIZE / sizeof(int)) - 4) 

typedef struct siginfo {
    int si_signo;   // Signal number
    int si_errno;   // If non-zero, an errno value associated with this signal
    int si_code;    // Signal code
    int __pad0;     // 64-bit explicit padding for 8-byte alignment

    union {
        int _pad[SI_PAD_SIZE]; // Forces the whole struct to be exactly 128 bytes

        // kill()
        struct {
            int _pid;     // Sender's PID
            int _uid;     // Sender's UID
        } _kill;

        // POSIX.1b timers
        struct {
            int _tid;       // Timer ID
            int _overrun;   // Overrun count
            union sigval _sigval; // Same as below
        } _timer;

        // POSIX.1b signals
        struct {
            int _pid;     // Sender's PID
            int _uid;     // Sender's UID
            union sigval _sigval;
        } _rt;

        // SIGCHLD
        struct {
            int _pid;     // Which child
            int _uid;     // Sender's UID
            int _status;    // Exit value or signal
            long _utime; // User time consumed
            long _stime; // System time consumed
        } _sigchld;

        // SIGILL, SIGFPE, SIGSEGV, SIGBUS
        struct {
            void *_addr;    // Faulting instruction/memory reference
            short _addr_lsb;// Valid LSB of the reported address
        } _sigfault;

        // SIGPOLL
        struct {
            long _band;     // POLL_IN, POLL_OUT, POLL_MSG
            int _fd;
        } _sigpoll;

        // SIGSYS (Bad system call)
        struct {
            void *_call_addr; // Calling user instruction
            int _syscall;     // Triggering system call number
            unsigned int _arch; // AUDIT_ARCH_* of syscall
        } _sigsys;

    } _sifields;
} siginfo_t;


struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t*, void*);
    };
    uint64_t sa_flags;
    void (*sa_restorer)(void);
    uint64_t sa_mask;
};


/*
 * SA_FLAGS values:
 *
 * SA_NOCLDSTOP flag to turn off SIGCHLD when children stop.
 * SA_NOCLDWAIT flag on SIGCHLD to inhibit zombies.
 * SA_SIGINFO delivers the signal with SIGINFO structs.
 * SA_ONSTACK indicates that a registered stack_t will be used.
 * SA_RESTART flag to get restarting signals (which were the default long ago)
 * SA_NODEFER prevents the current signal from being masked in the handler.
 * SA_RESETHAND clears the handler when the signal is delivered.
 * SA_UNSUPPORTED is a flag bit that will never be supported. Kernels from
 * before the introduction of SA_UNSUPPORTED did not clear unknown bits from
 * sa_flags when read using the oldact argument to sigaction and rt_sigaction,
 * so this bit allows flag bit support to be detected from userspace while
 * allowing an old kernel to be distinguished from a kernel that supports every
 * flag bit.
 * SA_EXPOSE_TAGBITS exposes an architecture-defined set of tag bits in
 * siginfo.si_addr.
 *
 * SA_ONESHOT and SA_NOMASK are the historical Linux names for the Single
 * Unix names RESETHAND and NODEFER respectively.
 */
#ifndef SA_NOCLDSTOP
#define SA_NOCLDSTOP	0x00000001
#endif
#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT	0x00000002
#endif
#ifndef SA_SIGINFO
#define SA_SIGINFO	0x00000004
#endif

/* 0x00000008 used on alpha, mips, parisc */
/* 0x00000010 used on alpha, parisc */
/* 0x00000020 used on alpha, parisc, sparc */
/* 0x00000040 used on alpha, parisc */
/* 0x00000080 used on parisc */
/* 0x00000100 used on sparc */
/* 0x00000200 used on sparc */
#define SA_UNSUPPORTED	0x00000400
#define SA_EXPOSE_TAGBITS	0x00000800

/* 0x00010000 used on mips */
/* 0x00800000 used for internal SA_IMMUTABLE */
/* 0x01000000 used on x86 */
/* 0x02000000 used on x86 */
/*
 * New architectures should not define the obsolete
 *	SA_RESTORER	0x04000000
 */
#ifndef SA_ONSTACK
#define SA_ONSTACK	0x08000000
#endif
#ifndef SA_RESTART
#define SA_RESTART	0x10000000
#endif
#ifndef SA_NODEFER
#define SA_NODEFER	0x40000000
#endif
#ifndef SA_RESETHAND
#define SA_RESETHAND	0x80000000
#endif

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#ifndef SIG_BLOCK
#define SIG_BLOCK          0	/* for blocking signals */
#endif
#ifndef SIG_UNBLOCK
#define SIG_UNBLOCK        1	/* for unblocking signals */
#endif
#ifndef SIG_SETMASK
#define SIG_SETMASK        2	/* for setting the signal mask */
#endif