/*
 * system- and machine-specific declarations for emu:
 * signal handling primitive, and
 * implementation of the current-process variable `up'.
 */

#define KSTACK (32 * 1024)

#ifndef USE_PTHREADS
static __inline Proc *getup(void) {
	Proc *p;
	__asm__(	"movl	%%esp, %%eax\n\t"
			: "=a" (p)
	);
	return *(Proc **)((uintptr)p & ~(KSTACK - 1));
}
#else
extern	Proc*	getup(void);
#endif

#define	up	(getup())

typedef sigjmp_buf osjmpbuf;
#define	ossetjmp(buf)	sigsetjmp(buf, 1)
