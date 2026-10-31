#ifndef _INFERNO_LOCK_TYPES_CTX_H_
#define _INFERNO_LOCK_TYPES_CTX_H_

/*
 *  What every thread descriptor (inferno_thread_t) holds for the lock layer, the
 *  first member of the descriptors of all the backends.  Internal: only the backends
 *  and their common core (emu/sync/lock-core.inc.h) use these fields.
 */
#include <inferno/lock.h>

#define INFERNO_THREAD_MAGIC	0x1f3a5c7eu	/* set by inferno_thread_init, cleared by destroy */

struct inferno_prim;

struct inferno_ctx {
	unsigned		magic;
	unsigned		id;			/* for messages */
	int			n;			/* tokens held */
	struct inferno_prim	*t[INFERNO_TOKEN_MAX];
	unsigned		seq[INFERNO_TOKEN_MAX];	/* acquisition order, kept across pauses */
	unsigned		nextseq;
	int			nshort;			/* short mutexes held */
	unsigned		rng;			/* for the debug mode */
};

#endif /* _INFERNO_LOCK_TYPES_CTX_H_ */
