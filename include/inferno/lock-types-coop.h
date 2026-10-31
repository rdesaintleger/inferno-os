#ifndef _INFERNO_LOCK_TYPES_COOP_H_
#define _INFERNO_LOCK_TYPES_COOP_H_

/* Types of <inferno/lock.h> for the coop backend (emu/sync/lock-coop.c): one host thread. */

struct inferno_prim {
	int	flags;
	int	locked;
};

struct inferno_token {
	struct inferno_prim	p;	/* first: the core converts between the two */
};

struct inferno_mutex {
	struct inferno_prim	p;
};

struct inferno_seqlock {
	unsigned	seq;
};

struct inferno_ref {
	int	v;
};

struct inferno_thread {
	int	permits;
	int	pending;
};

#endif /* _INFERNO_LOCK_TYPES_COOP_H_ */
