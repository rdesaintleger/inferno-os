/*
 *  Backend `coop' of include/inferno/lock.h: a single host thread, no host
 *  wait.  The "threads" are contexts selected by hand, which makes it a
 *  deterministic oracle for the tests, and the model of a port to a host
 *  without threads (WASM).  When the running context would have to wait, it
 *  calls inferno_coop_wait, which must let the holder make progress (typically
 *  select the holder's context, run a step, select back).
 */
#define INFERNO_COOP_MAXCTX	8

extern	void	inferno_coop_select(int);		/* the context that runs from now on */
extern	int	inferno_coop_current(void);
extern	void	(*inferno_coop_wait)(void);		/* NULL: waiting aborts (deadlock) */

/*
 *  Waits of the coop backend (inferno_wait, inferno_sleep, inferno_wait_fd): when
 *  nothing is available the running context pauses (tokens released) and calls
 *  inferno_coop_wait in a loop.  Time is virtual: it passes by 1 ms each time that
 *  hook returns while a timed wait is blocked, and by inferno_coop_advance; a
 *  sleep with no hook just takes the clock to its deadline.
 */
extern	long	inferno_coop_now(void);			/* virtual milliseconds */
extern	void	inferno_coop_advance(long);
extern	int	(*inferno_coop_fdready)(int fd, int events);	/* events ready, 0 if none */

