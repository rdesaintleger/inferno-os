#ifndef _INFERNO_LOCK_TYPES_PTHREAD_H_
#define _INFERNO_LOCK_TYPES_PTHREAD_H_

/* Types of <inferno/lock.h> for the POSIX threads backend (emu/sync/lock-pthread.c). */
#include <pthread.h>

/*
 *  A token or a mutex is a flag guarded by a pthread mutex plus a condition
 *  variable, so that, as the API requires, it can be released by a thread other
 *  than the one that took it.
 */
struct inferno_prim {
	int		flags;
	int		locked;
	int		nwait;
	pthread_mutex_t	m;
	pthread_cond_t	c;
};

struct inferno_token {
	struct inferno_prim	p;	/* first: the core converts between the two */
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
	pthread_mutex_t	m;
	pthread_cond_t	c;
	int		permits;	/* inferno_wake not consumed yet */
	int		pending;	/* interrupt not consumed yet */
	int		state;		/* what the thread is blocked in, so how to wake it */
	int		wake[2];	/* pipe that interrupts a poll; created by the first wait_fd */
};

#endif /* _INFERNO_LOCK_TYPES_PTHREAD_H_ */
