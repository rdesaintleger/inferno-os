#ifndef _INFERNO_LOCK_CORE_H_
#define _INFERNO_LOCK_CORE_H_
/*
 *  Shared by the backends of the synchronization API (include/inferno/lock.h).
 *
 *  A backend defines (in its <inferno/lock-types-*.h>) struct inferno_prim, which
 *  starts with `flags', and here (in its .c file) the primitives
 *	static Ctx *self(void);			context of the calling thread
 *	static int prim_init(Prim*);
 *	static void prim_destroy(Prim*);
 *	static int prim_try(Prim*);		1 if taken, 0 if busy
 *	static void prim_wait(Prim*);		blocks until taken
 *	static void prim_put(Prim*);		releases, any thread may call it
 *  then includes lock-core.inc.h, which implements tokens and mutexes on top.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inferno/lock-types.h>

typedef struct inferno_ctx Ctx;
typedef struct inferno_prim Prim;

/* a token starts with its Prim: the core goes from one to the other */
typedef char token_prim_first[offsetof(struct inferno_token, p) == 0 ? 1 : -1];
typedef char thread_ctx_first[offsetof(struct inferno_thread, ctx) == 0 ? 1 : -1];
#define TOKEN(prim)	((inferno_token_t*)(prim))

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
	c->magic = INFERNO_THREAD_MAGIC;
	c->id = id;
	c->rng = dbg_seed * 2654435761u + id * 40503u + 1;
}

/* the context of an attached descriptor, checked */
static Ctx*
thread_ctx(inferno_thread_t *t)
{
	if(t == NULL)
		die("no thread attached: the thread must call inferno_thread_attach");
	if(t->ctx.magic != INFERNO_THREAD_MAGIC)
		die("bad thread descriptor (not initialized, or destroyed)");
	return &t->ctx;
}

#endif /* _INFERNO_LOCK_CORE_H_ */