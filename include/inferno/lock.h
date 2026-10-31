#ifndef _INFERNO_LOCK_H_
#define _INFERNO_LOCK_H_

/*
 *  synchronization
 */
typedef
struct Lock {
	int	val;
} Lock;

typedef struct QLock QLock;
struct QLock
{
	Lock	use;			/* to access Qlock structure */
	struct Proc	*head;			/* next process waiting for object */
	struct Proc	*tail;			/* last process waiting for object */
	int	locked;			/* flag */
};

typedef struct RWlock RWlock;
struct RWlock
{
	Lock	l;			/* Lock modify lock */
	QLock	x;			/* Mutual exclusion lock */
	QLock	k;			/* Lock for waiting writers */
	int	readers;		/* Count of readers in lock */
};

extern	void	lock(Lock*);
extern	void	unlock(Lock*);

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);

extern	int	canrlock(RWlock*);
extern	void	rlock(RWlock*);
extern	void	runlock(RWlock*);
extern	void	wlock(RWlock*);
extern	void	wunlock(RWlock*);

/*
 *  New synchronization API (C99, no assembler, no C11 atomics).
 *
 *  The types below are incomplete here: this header only declares them and the
 *  functions that take pointers to them, and does not depend on the host.  The
 *  code that embeds one of them in a structure, or defines one, includes
 *  <inferno/lock-types.h>, which selects the complete definitions of the backend
 *  of the build (INFERNO_LOCK_BACKEND_PTHREAD, INFERNO_LOCK_BACKEND_COOP, ...).
 *  Nothing outside the backend uses their fields.
 *
 *  Tokens.  A successful inferno_token_get gives exclusivity as long as the
 *  thread runs.  Every pause point (a get that has to wait, a slow
 *  inferno_mutex_lock, inferno_token_suspend, a blocking host call between
 *  suspend and resume) releases ALL the tokens of the thread; when the thread
 *  resumes it takes them all again before going on.  Functions that can pause
 *  return 0 on the fast path and 1 when the thread was paused: what was read
 *  under the tokens already held must then be revalidated.
 */
#define INFERNO_TOKEN_MAX	16	/* tokens held at the same time by a thread */

typedef struct inferno_token inferno_token_t;
typedef struct inferno_mutex inferno_mutex_t;
typedef struct inferno_seqlock inferno_seqlock_t;
typedef struct inferno_ref inferno_ref_t;

/* what inferno_token_suspend takes away, for inferno_token_resume */
typedef struct inferno_token_saved {
	int		n;
	inferno_token_t	*t[INFERNO_TOKEN_MAX];
	unsigned	seq[INFERNO_TOKEN_MAX];
} inferno_token_saved_t;

/* tokens */
extern	int	inferno_token_init(inferno_token_t*);
extern	void	inferno_token_destroy(inferno_token_t*);
extern	int	inferno_token_get(inferno_token_t*);		/* 0 fast, 1 paused */
extern	int	inferno_token_try(inferno_token_t*);		/* 1 taken, 0 busy; never pauses */
extern	void	inferno_token_put(inferno_token_t*);
extern	int	inferno_token_get_all(int, inferno_token_t**);	/* the set, canonical order */
extern	int	inferno_token_held(inferno_token_t*);		/* does this thread hold it? */
extern	unsigned	inferno_token_mark(void);		/* for error unwinding */
extern	void	inferno_token_release_to(unsigned);		/* put the tokens taken since the mark */
extern	void	inferno_token_suspend(inferno_token_saved_t*);	/* release all */
extern	void	inferno_token_resume(inferno_token_saved_t*);	/* take them again */

/*
 *  Mutex: no thread owner (a thread can take it and another release it).  The
 *  slow path of inferno_mutex_lock is a pause point (returns 1).  A mutex
 *  created with INFERNO_MUTEX_SHORT is for very short sections: its holder
 *  never pauses (checked), so waiting for it does not release the tokens, and
 *  it is released by the thread that took it.
 */
#define INFERNO_MUTEX_SHORT	1

extern	int	inferno_mutex_init(inferno_mutex_t*, int);
extern	void	inferno_mutex_destroy(inferno_mutex_t*);
extern	int	inferno_mutex_lock(inferno_mutex_t*);		/* 0 fast, 1 paused */
extern	int	inferno_mutex_trylock(inferno_mutex_t*);	/* 1 taken, 0 busy */
extern	void	inferno_mutex_unlock(inferno_mutex_t*);

/*
 *  Seqlock: one writer at a time, readers copy values (never follow a
 *  pointer) and retry if told to:
 *	do { s = inferno_seqlock_read_begin(l); ...copy...; } while(inferno_seqlock_read_retry(l, s));
 *  inferno_seqlock_read_retry must always be called, it ends the read section.
 *  The writer must not block in its section.
 */
extern	int	inferno_seqlock_init(inferno_seqlock_t*);
extern	void	inferno_seqlock_destroy(inferno_seqlock_t*);
extern	void	inferno_seqlock_write_begin(inferno_seqlock_t*);
extern	void	inferno_seqlock_write_end(inferno_seqlock_t*);
extern	unsigned	inferno_seqlock_read_begin(inferno_seqlock_t*);
extern	int	inferno_seqlock_read_retry(inferno_seqlock_t*, unsigned);

/* reference counts: the new value is returned */
extern	void	inferno_ref_init(inferno_ref_t*, int);
extern	int	inferno_ref_get(inferno_ref_t*);
extern	int	inferno_ref_inc(inferno_ref_t*);
extern	int	inferno_ref_dec(inferno_ref_t*);

/*
 *  Debug: with permille > 0 each token get and each mutex lock takes its slow
 *  path (and returns 1) with that probability, pseudo-randomly from seed, so
 *  that callers' invariants are really tested.  Call it before starting
 *  threads.  Pausing with a short mutex held always aborts.
 */
extern	void	inferno_lock_debug(unsigned seed, int permille);

/*
 *  Waiting and interruption, with no host signal.
 *
 *  Each thread has a wait object (inferno_thread_t), created and destroyed by
 *  whoever owns the thread's descriptor (it can be embedded in it) and attached
 *  by the thread itself.  Every wait below is interruptible by
 *  inferno_thread_interrupt, called from any thread, and is a pause point for
 *  tokens: when the thread has to block it releases all its tokens first and takes
 *  them again when the wait ends.
 *
 *  An interrupt is level-triggered: it stays set until a wait consumes it (that
 *  wait returns INFERNO_WAIT_INTR) or inferno_thread_clear_interrupt is called, so
 *  that it cannot be lost between the moment the interrupter decides to interrupt
 *  and the moment the thread starts to wait: the flag is tested and the wait is
 *  entered under the same internal lock.  The caller that wants an interrupt to
 *  concern a given region only (the usual case) decides, under a lock of its own,
 *  whether the thread is in that region before interrupting, and clears the flag
 *  when it leaves the region under the same lock.
 *
 *  inferno_wake grants one permit to inferno_wait / inferno_wait_timed (a counting
 *  semaphore, one waiter), which consume it.  Sleeping and waiting for a descriptor
 *  do not consume permits.
 */
#define INFERNO_WAIT_WOKEN	0
#define INFERNO_WAIT_INTR	1
#define INFERNO_WAIT_TIMEOUT	2
#define INFERNO_WAIT_READY	3	/* inferno_wait_fd: the descriptor is ready */

#define INFERNO_FD_READ		1
#define INFERNO_FD_WRITE	2

typedef struct inferno_thread inferno_thread_t;

extern	int	inferno_thread_init(inferno_thread_t*);
extern	void	inferno_thread_destroy(inferno_thread_t*);
extern	void	inferno_thread_attach(inferno_thread_t*);	/* the calling thread is this one */
extern	void	inferno_thread_detach(void);
extern	inferno_thread_t*	inferno_thread_current(void);

extern	int	inferno_wait(void);				/* until a permit or an interrupt */
extern	int	inferno_wait_timed(long ms);			/* ... or the timeout (ms >= 0) */
extern	int	inferno_sleep(long ms);				/* TIMEOUT, or INTR */
extern	int	inferno_wait_fd(int fd, int events, long ms, int *ready);
							/* READY (with *ready), TIMEOUT or INTR; ms < 0: no limit */
extern	void	inferno_wake(inferno_thread_t*);
extern	void	inferno_thread_interrupt(inferno_thread_t*);
extern	int	inferno_thread_clear_interrupt(void);		/* was it set? */

#endif /* _INFERNO_LOCK_H_ */
