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
