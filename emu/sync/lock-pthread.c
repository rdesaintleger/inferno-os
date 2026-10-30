/*
 *  Backend of include/inferno/lock.h on POSIX threads.  A token or a mutex is a
 *  flag guarded by a pthread mutex plus a condition variable, so that, as the
 *  API requires, it can be released by a thread other than the one that took it.
 */
#include <pthread.h>
#include "lock-core.h"

struct Prim
{
	int		flags;
	int		locked;
	int		nwait;
	pthread_mutex_t	m;
	pthread_cond_t	c;
};

struct SeqLock
{
	pthread_mutex_t	m;
	unsigned	seq;
};

typedef char prim_fits[sizeof(struct Prim) <= sizeof(inferno_token_t) ? 1 : -1];
typedef char prim_fits2[sizeof(struct Prim) <= sizeof(inferno_mutex_t) ? 1 : -1];
typedef char seq_fits[sizeof(struct SeqLock) <= sizeof(inferno_seqlock_t) ? 1 : -1];

static pthread_key_t key;
static pthread_once_t keyonce = PTHREAD_ONCE_INIT;
static pthread_mutex_t idlock = PTHREAD_MUTEX_INITIALIZER;
static unsigned nctx;

static void
ctxfree(void *p)
{
	Ctx *c = p;

	if(c->n || c->nshort)
		fprintf(stderr, "inferno_lock: thread ended holding %d tokens and %d short mutexes\n", c->n, c->nshort);
	free(c);
}

static void
mkkey(void)
{
	if(pthread_key_create(&key, ctxfree) != 0)
		die("cannot create the thread key");
}

static Ctx*
self(void)
{
	Ctx *c;
	unsigned id;

	pthread_once(&keyonce, mkkey);
	c = pthread_getspecific(key);
	if(c == NULL){
		c = malloc(sizeof *c);
		if(c == NULL)
			die("out of memory");
		pthread_mutex_lock(&idlock);
		id = nctx++;
		pthread_mutex_unlock(&idlock);
		ctx_init(c, id);
		pthread_setspecific(key, c);
	}
	return c;
}

static int
prim_init(Prim *p)
{
	if(pthread_mutex_init(&p->m, NULL) != 0)
		return -1;
	if(pthread_cond_init(&p->c, NULL) != 0){
		pthread_mutex_destroy(&p->m);
		return -1;
	}
	return 0;
}

static void
prim_destroy(Prim *p)
{
	pthread_cond_destroy(&p->c);
	pthread_mutex_destroy(&p->m);
}

static int
prim_try(Prim *p)
{
	int r;

	pthread_mutex_lock(&p->m);
	r = !p->locked;
	if(r)
		p->locked = 1;
	pthread_mutex_unlock(&p->m);
	return r;
}

static void
prim_wait(Prim *p)
{
	pthread_mutex_lock(&p->m);
	p->nwait++;
	while(p->locked)
		pthread_cond_wait(&p->c, &p->m);
	p->nwait--;
	p->locked = 1;
	pthread_mutex_unlock(&p->m);
}

static void
prim_put(Prim *p)
{
	pthread_mutex_lock(&p->m);
	p->locked = 0;
	if(p->nwait)
		pthread_cond_signal(&p->c);
	pthread_mutex_unlock(&p->m);
}

#include "lock-core.inc"

/*
 *  Seqlock.  Portable form: the reader holds the lock from read_begin to
 *  read_retry, which therefore never asks for a retry.  The callers do not
 *  change when a lock-free backend replaces this one.
 */
int
inferno_seqlock_init(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;

	s->seq = 0;
	return pthread_mutex_init(&s->m, NULL) == 0 ? 0 : -1;
}

void
inferno_seqlock_destroy(inferno_seqlock_t *l)
{
	pthread_mutex_destroy(&((struct SeqLock*)l)->m);
}

void
inferno_seqlock_write_begin(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;

	pthread_mutex_lock(&s->m);
	s->seq++;
}

void
inferno_seqlock_write_end(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;

	s->seq++;
	pthread_mutex_unlock(&s->m);
}

unsigned
inferno_seqlock_read_begin(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;

	pthread_mutex_lock(&s->m);
	return s->seq;
}

int
inferno_seqlock_read_retry(inferno_seqlock_t *l, unsigned seq)
{
	(void)seq;
	pthread_mutex_unlock(&((struct SeqLock*)l)->m);
	return 0;
}

/* reference counts: a stripe of mutexes chosen by address, no storage in the object */
#define NSTRIPE	64
static pthread_mutex_t stripe[NSTRIPE];
static pthread_once_t stripeonce = PTHREAD_ONCE_INIT;

static void
mkstripes(void)
{
	int i;

	for(i = 0; i < NSTRIPE; i++)
		pthread_mutex_init(&stripe[i], NULL);
}

static pthread_mutex_t*
stripefor(inferno_ref_t *r)
{
	pthread_once(&stripeonce, mkstripes);
	return &stripe[((unsigned long)r >> 4) % NSTRIPE];
}

void
inferno_ref_init(inferno_ref_t *r, int v)
{
	r->v = v;
}

int
inferno_ref_get(inferno_ref_t *r)
{
	pthread_mutex_t *m = stripefor(r);
	int v;

	pthread_mutex_lock(m);
	v = r->v;
	pthread_mutex_unlock(m);
	return v;
}

int
inferno_ref_inc(inferno_ref_t *r)
{
	pthread_mutex_t *m = stripefor(r);
	int v;

	pthread_mutex_lock(m);
	v = ++r->v;
	pthread_mutex_unlock(m);
	return v;
}

int
inferno_ref_dec(inferno_ref_t *r)
{
	pthread_mutex_t *m = stripefor(r);
	int v;

	pthread_mutex_lock(m);
	v = --r->v;
	pthread_mutex_unlock(m);
	return v;
}
