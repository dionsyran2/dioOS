#define SIGHUP 1 
#define SIGINT 2 
#define SIGQUIT 3 
#define SIGILL 4   
#define SIGTRAP 5  
#define SIGABRT 6  
#define SIGIOT 6   
#define SIGBUS 7    
#define SIGFPE 8 
#define SIGKILL 9 
#define SIGUSR1 10 
#define SIGSEGV 11 
#define SIGUSR2 12 
#define SIGPIPE 13 
#define SIGALRM 14  
#define SIGTERM 15 
#define SIGSTKFLT 16 
#define SIGCHLD 17 
#define SIGCONT 18 
#define SIGSTOP 19 
#define SIGTSTP 20 
#define SIGTTIN 21 
#define SIGTTOU 22 
#define SIGURG 23 
#define SIGXCPU 24 
#define SIGXFSZ 25 
#define SIGVTALRM 26 
#define SIGPROF 27 
#define SIGWINCH 28  
#define SIGIO 29 
#define SIGPOLL 29 
#define SIGPWR 30 
#define SIGSYS 31 
#define SIGUNUSED 31 



/*
 * si_code values
 * Digital reserves positive values for kernel-generated signals.
 */
#define SI_USER		0		/* sent by kill, sigsend, raise */
#define SI_KERNEL	0x80		/* sent by the kernel from somewhere */
#define SI_QUEUE	-1		/* sent by sigqueue */
#define SI_TIMER	-2		/* sent by timer expiration */
#define SI_MESGQ	-3		/* sent by real time mesq state change */
#define SI_ASYNCIO	-4		/* sent by AIO completion */
#define SI_SIGIO	-5		/* sent by queued SIGIO */
#define SI_TKILL	-6		/* sent by tkill system call */
#define SI_DETHREAD	-7		/* sent by execve() killing subsidiary threads */
#define SI_ASYNCNL	-60		/* sent by glibc async name lookup completion */

#define SI_FROMUSER(siptr)	((siptr)->si_code <= 0)
#define SI_FROMKERNEL(siptr)	((siptr)->si_code > 0)

#define SA_NODEFER   0x40000000

/*
 * SIGILL si_codes
 */
#define ILL_ILLOPC	1	/* illegal opcode */
#define ILL_ILLOPN	2	/* illegal operand */
#define ILL_ILLADR	3	/* illegal addressing mode */
#define ILL_ILLTRP	4	/* illegal trap */
#define ILL_PRVOPC	5	/* privileged opcode */
#define ILL_PRVREG	6	/* privileged register */
#define ILL_COPROC	7	/* coprocessor error */
#define ILL_BADSTK	8	/* internal stack error */
#define ILL_BADIADDR	9	/* unimplemented instruction address */
#define __ILL_BREAK	10	/* illegal break */
#define __ILL_BNDMOD	11	/* bundle-update (modification) in progress */
#define NSIGILL		11

/*
 * SIGFPE si_codes
 */
#define FPE_INTDIV	1	/* integer divide by zero */
#define FPE_INTOVF	2	/* integer overflow */
#define FPE_FLTDIV	3	/* floating point divide by zero */
#define FPE_FLTOVF	4	/* floating point overflow */
#define FPE_FLTUND	5	/* floating point underflow */
#define FPE_FLTRES	6	/* floating point inexact result */
#define FPE_FLTINV	7	/* floating point invalid operation */
#define FPE_FLTSUB	8	/* subscript out of range */
#define __FPE_DECOVF	9	/* decimal overflow */
#define __FPE_DECDIV	10	/* decimal division by zero */
#define __FPE_DECERR	11	/* packed decimal error */
#define __FPE_INVASC	12	/* invalid ASCII digit */
#define __FPE_INVDEC	13	/* invalid decimal digit */
#define FPE_FLTUNK	14	/* undiagnosed floating-point exception */
#define FPE_CONDTRAP	15	/* trap on condition */
#define NSIGFPE		15

/*
 * SIGSEGV si_codes
 */
#define SEGV_MAPERR	1	/* address not mapped to object */
#define SEGV_ACCERR	2	/* invalid permissions for mapped object */
#define SEGV_BNDERR	3	/* failed address bound checks */
#ifdef __ia64__
# define __SEGV_PSTKOVF	4	/* paragraph stack overflow */
#else
# define SEGV_PKUERR	4	/* failed protection key checks */
#endif
#define SEGV_ACCADI	5	/* ADI not enabled for mapped object */
#define SEGV_ADIDERR	6	/* Disrupting MCD error */
#define SEGV_ADIPERR	7	/* Precise MCD exception */
#define SEGV_MTEAERR	8	/* Asynchronous ARM MTE error */
#define SEGV_MTESERR	9	/* Synchronous ARM MTE exception */
#define NSIGSEGV	9

/*
 * SIGBUS si_codes
 */
#define BUS_ADRALN	1	/* invalid address alignment */
#define BUS_ADRERR	2	/* non-existent physical address */
#define BUS_OBJERR	3	/* object specific hardware error */
/* hardware memory error consumed on a machine check: action required */
#define BUS_MCEERR_AR	4
/* hardware memory error detected in process but not consumed: action optional*/
#define BUS_MCEERR_AO	5
#define NSIGBUS		5

/*
 * SIGTRAP si_codes
 */
#define TRAP_BRKPT	1	/* process breakpoint */
#define TRAP_TRACE	2	/* process trace trap */
#define TRAP_BRANCH     3	/* process taken branch trap */
#define TRAP_HWBKPT     4	/* hardware breakpoint/watchpoint */
#define TRAP_UNK	5	/* undiagnosed trap */
#define TRAP_PERF	6	/* perf event with sigtrap=1 */
#define NSIGTRAP	6

/*
 * There is an additional set of SIGTRAP si_codes used by ptrace
 * that are of the form: ((PTRACE_EVENT_XXX << 8) | SIGTRAP)
 */

/*
 * Flags for si_perf_flags if SIGTRAP si_code is TRAP_PERF.
 */
#define TRAP_PERF_FLAG_ASYNC (1u << 0)

/*
 * SIGCHLD si_codes
 */
#define CLD_EXITED	1	/* child has exited */
#define CLD_KILLED	2	/* child was killed */
#define CLD_DUMPED	3	/* child terminated abnormally */
#define CLD_TRAPPED	4	/* traced child has trapped */
#define CLD_STOPPED	5	/* child has stopped */
#define CLD_CONTINUED	6	/* stopped child has continued */
#define NSIGCHLD	6
