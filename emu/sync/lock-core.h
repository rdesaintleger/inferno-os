/*
 *  Shared by the backends of the synchronization API (include/inferno/lock.h).
 *
 *  A backend defines struct Prim (starting with `flags') and the primitives
 *	static Ctx *self(void);			context of the calling thread
 *	static int prim_init(Prim*);
 *	static void prim_destroy(Prim*);
 *	static int prim_try(Prim*);		1 if taken, 0 if busy
 *	static void prim_wait(Prim*);		blocks until taken
 *	static void prim_put(Prim*);		releases, any thread may call it
 *  then includes lock-core.inc, which implements tokens and mutexes on top.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inferno/lock.h>

typedef struct Ctx Ctx;
typedef struct Prim Prim;

struct Ctx
{
	int		n;				/* tokens held */
	Prim		*t[INFERNO_TOKEN_MAX];
	unsigned	seq[INFERNO_TOKEN_MAX];		/* acquisition order, kept across pauses */
	unsigned	nextseq;
	int		nshort;				/* short mutexes held */
	unsigned	rng;				/* for the debug mode */
	void		*thread;			/* wait object attached by the thread */
};

#define F_TOKEN	2

static unsigned dbg_seed;
static int dbg_permille;

static void
die(const char *m)
{
	fprintf(stderr, "inferno_lock: %s\n", m);
	abort();
}

static void
ctx_init(Ctx *c, unsigned id)
{
	memset(c, 0, sizeof *c);
	c->rng = dbg_seed * 2654435761u + id * 40503u + 1;
}
