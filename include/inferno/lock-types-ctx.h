#ifndef _INFERNO_LOCK_TYPES_CTX_H_
#define _INFERNO_LOCK_TYPES_CTX_H_

/*
 *  What every thread descriptor (inferno_thread_t) holds for the lock layer, the
 *  first member of the descriptors of all the backends.  Internal: only the backends
 *  and their common core (emu/sync/lock-core.inc.h) use these fields.
 */
#include <inferno/lock.h>

#define INFERNO_THREAD_MAGIC	0x1f3a5c7eu	/* set by inferno_thread_init, cleared by destroy */
#define INFERNO_SAVE_INLINE	8		/* entries of the save area that need no allocation */

struct inferno_prim;
struct inferno_token;

/* a token held, as remembered while a pause has released it */
struct inferno_saveent {
	struct inferno_token	*t;
	unsigned		seq;
};

/* the save area of inferno_token_suspend */
struct inferno_token_save {
	int			n;
	struct inferno_saveent	e[1];		/* n entries, really */
};

struct inferno_ctx {
	unsigned		magic;
	unsigned		id;			/* for messages */
	struct inferno_token	*held;			/* the tokens held, the last taken first */
	int			n;			/* how many */
	unsigned		nextseq;		/* order of acquisition, kept across pauses */
	int			nshort;			/* short mutexes held */
	int			nogo;			/* depth of the zones that take no token */
	unsigned		rng;			/* for the debug mode */
	struct inferno_saveent	*save;			/* save area of the pauses of this layer */
	int			savecap;
	struct inferno_saveent	inl[INFERNO_SAVE_INLINE];
};

#endif /* _INFERNO_LOCK_TYPES_CTX_H_ */
