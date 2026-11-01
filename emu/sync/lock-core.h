#ifndef _INFERNO_LOCK_CORE_H_
#define _INFERNO_LOCK_CORE_H_
/*
 *  Shared by the backends of the synchronization API (include/inferno/lock.h).
 *
 *  The types are those of <inferno/lock.h> (struct inferno_prim: flags, locked and
 *  `impl', the backend's own data, allocated by prim_init and freed by prim_destroy).
 *  A backend defines here (in its .c file) the primitives
 *	static Ctx *self(void);			context of the calling thread
 *	static int prim_init(Prim*);
 *	static void prim_destroy(Prim*);
 *	static int prim_try(Prim*);		1 if taken, 0 if busy
 *	static void prim_wait(Prim*);		blocks until taken
 *	static void prim_put(Prim*);		releases, any thread may call it
 *  then includes lock-core.inc.h, which implements tokens and mutexes on top.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inferno/lock.h>
#include <inferno/platform.h>

typedef struct inferno_ctx Ctx;
typedef struct inferno_prim Prim;

/*
 *  What the core needs from a backend, declared here so that lock-core.inc.h
 *  analyses and compiles on its own (an editor, a syntax check): the backend
 *  defines them, static, before it includes lock-core.inc.h.
 */
static Ctx *self(void);
static int prim_init(Prim*);
static void prim_destroy(Prim*);
static int prim_try(Prim*);
static void prim_wait(Prim*);
static void prim_put(Prim*);

/* a token starts with its Prim: the core goes from one to the other */
typedef char token_prim_first[offsetof(struct inferno_token, p) == 0 ? 1 : -1];
typedef char thread_ctx_first[offsetof(struct inferno_thread, ctx) == 0 ? 1 : -1];
#define TOKEN(prim)	((inferno_token_t*)(prim))

#define F_TOKEN	INFERNO_PRIM_TOKEN

static unsigned dbg_seed;
static int dbg_permille;

/*
 *  Nothing here calls the host: memory and fatal errors go through
 *  <inferno/platform.h> (inferno_malloc, inferno_free, inferno_panic, whose format
 *  is Plan 9's: `%ud', not `%u'), and the lock layer takes no lock of its own to do
 *  so, so that the formatting primitives may themselves rely on it.
 */
static void
die(const char *m)
{
	inferno_panic("inferno_lock: %s", m);
	for(;;)
		;			/* inferno_panic does not return */
}

/* a diagnostic that is not fatal; the one place to change when the platform has one */
static void
lock_warn(const char *fmt, ...)
{
	va_list a;

	va_start(a, fmt);
	vfprintf(stderr, fmt, a);
	va_end(a);
}

/* zeroed memory of the backends' own data, from the platform */
static inline void*
lock_zalloc(size_t n)
{
	size_t got;
	void *p;

	p = inferno_malloc(n, &got);
	if(p != NULL)
		memset(p, 0, n);
	return p;
}

static inline void
lock_zfree(void *p, size_t n)
{
	inferno_free(p, n);
}

static void
ctx_init(Ctx *c, unsigned id)
{
	memset(c, 0, sizeof *c);
	c->magic = INFERNO_THREAD_MAGIC;
	c->id = id;
	c->rng = dbg_seed * 2654435761u + id * 40503u + 1;
	c->save = c->inl;
	c->savecap = INFERNO_SAVE_INLINE;
}

/* memory of the save areas: the platform's, or what inferno_lock_set_allocator gave */
static void *(*lock_alloc)(size_t);		/* NULL: the platform's */
static void (*lock_release)(void*);

void
inferno_lock_set_allocator(void *(*alloc)(size_t), void (*release)(void*))
{
	lock_alloc = alloc;
	lock_release = release;
}

static void
ctx_free(Ctx *c)
{
	if(c->save != c->inl){
		if(lock_release != NULL)
			lock_release(c->save);
		else
			lock_zfree(c->save, c->savecap * sizeof *c->save);
		c->save = c->inl;
		c->savecap = INFERNO_SAVE_INLINE;
	}
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