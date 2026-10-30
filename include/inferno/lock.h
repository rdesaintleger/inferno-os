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
 *  The types below are opaque: they only reserve storage so that they can be
 *  embedded in other structures; a backend (emu/sync/lock-pthread.c,
 *  emu/sync/lock-coop.c, ...) gives them a meaning and checks at compile time
 *  that its representation fits.  Nothing here depends on the host.
 *
 *  Tokens.  A successful inferno_token_get gives exclusivity as long as the
 *  thread runs.  Every pause point (a get that has to wait, a slow
 *  inferno_mutex_lock, inferno_token_suspend, a blocking host call between
 *  suspend and resume) releases ALL the tokens of the thread; when the thread
 *  resumes it takes them all again before going on.  Functions that can pause
 *  return 0 on the fast path and 1 when the thread was paused: what was read
 *  under the tokens already held must then be revalidated.
 */
#define INFERNO_OPAQUE(n)\
	union { long long ll_; long double ld_; void *p_; char b_[n]; }

#define INFERNO_TOKEN_MAX	16	/* tokens held at the same time by a thread */

typedef struct inferno_token { INFERNO_OPAQUE(128) u; } inferno_token_t;
typedef struct inferno_mutex { INFERNO_OPAQUE(128) u; } inferno_mutex_t;
typedef struct inferno_seqlock { INFERNO_OPAQUE(128) u; } inferno_seqlock_t;
typedef struct inferno_ref { int v; } inferno_ref_t;

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

#endif /* _INFERNO_LOCK_H_ */
