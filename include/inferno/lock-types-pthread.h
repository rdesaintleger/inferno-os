#ifndef _INFERNO_LOCK_TYPES_PTHREAD_H_
#define _INFERNO_LOCK_TYPES_PTHREAD_H_

/* Types of <inferno/lock.h> for the POSIX threads backend (emu/sync/lock-pthread.c). */
#include <pthread.h>
#include <inferno/lock-types-ctx.h>

/*
 *  A mutex without a thread owner is a flag guarded by a pthread mutex plus a
 *  condition variable, so that, as the API requires, it can be released by a thread
 *  other than the one that took it.  A token and a short mutex are always released by
 *  the thread that took them: they are the pthread mutex itself.
 */
struct inferno_prim {
	int		flags;
	int		locked;
	int		nwait;
	pthread_mutex_t	m;
	pthread_cond_t	c;
};

#define INFERNO_PRIM_TOKEN	2	/* flags of a token (INFERNO_MUTEX_SHORT is 1) */

/* for statics, with no init call: INFERNO_MUTEX_INITIALIZER(INFERNO_MUTEX_SHORT), INFERNO_TOKEN_INITIALIZER */
#define INFERNO_MUTEX_INITIALIZER(flags) \
	{ { (flags), 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER } }
#define INFERNO_TOKEN_INITIALIZER \
	{ { INFERNO_PRIM_TOKEN, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }, NULL, 0 }

struct inferno_token {
	struct inferno_prim	p;	/* first: the core converts between the two */
	struct inferno_token	*held_next;	/* the tokens held by a thread; only its holder uses them */
	unsigned		held_seq;
};

struct inferno_mutex {
	struct inferno_prim	p;
};

struct inferno_seqlock {
	pthread_mutex_t	m;
	unsigned	seq;
};

struct inferno_ref {
	int	v;
};

struct inferno_thread {
	struct inferno_ctx	ctx;		/* first: the lock layer goes from the one to the other */
	pthread_mutex_t	m;
	pthread_cond_t	c;
	int		permits;	/* inferno_wake not consumed yet */
	int		pending;	/* interrupt not consumed yet */
	int		state;		/* what the thread is blocked in, so how to wake it */
	int		wake[2];	/* pipe that interrupts a poll; created by the first wait_fd */
};

#endif /* _INFERNO_LOCK_TYPES_PTHREAD_H_ */
