/*
 *  Backend of include/inferno/lock.h without any host thread or wait: see
 *  lock-coop.h.  Nothing here is atomic because nothing runs concurrently.
 */
#include "lock-core.h"
#include "lock-coop.h"

struct Prim
{
	int	flags;
	int	locked;
};

typedef char prim_fits[sizeof(struct Prim) <= sizeof(inferno_token_t) ? 1 : -1];
typedef char prim_fits2[sizeof(struct Prim) <= sizeof(inferno_mutex_t) ? 1 : -1];

void (*inferno_coop_wait)(void);

static Ctx ctxs[INFERNO_COOP_MAXCTX];
static int inited, cur;

static Ctx*
self(void)
{
	int i;

	if(!inited){
		for(i = 0; i < INFERNO_COOP_MAXCTX; i++)
			ctx_init(&ctxs[i], i);
		inited = 1;
	}
	return &ctxs[cur];
}

void
inferno_coop_select(int id)
{
	if(id < 0 || id >= INFERNO_COOP_MAXCTX)
		die("bad context");
	self();
	cur = id;
}

int
inferno_coop_current(void)
{
	return cur;
}

static void
waitturn(void)
{
	int me = cur;

	if(inferno_coop_wait == NULL)
		die("coop: would wait with nothing to run (deadlock)");
	inferno_coop_wait();
	cur = me;
}

static int
prim_init(Prim *p)
{
	(void)p;
	return 0;
}

static void
prim_destroy(Prim *p)
{
	(void)p;
}

static int
prim_try(Prim *p)
{
	if(p->locked)
		return 0;
	p->locked = 1;
	return 1;
}

static void
prim_wait(Prim *p)
{
	long spins = 0;

	while(p->locked){
		if(++spins > 1000000)
			die("coop: wait does not end (deadlock)");
		waitturn();
	}
	p->locked = 1;
}

static void
prim_put(Prim *p)
{
	p->locked = 0;
}

#include "lock-core.inc"

/*
 *  Seqlock with a real retry, as a lock-free backend would have: since
 *  contexts only switch in waitturn, a writer (which never waits) cannot be
 *  seen in the middle of its section except by a test that selects a context
 *  by hand there, which is how the retry is tested.
 */
struct SeqLock
{
	unsigned	seq;
};

typedef char seq_fits[sizeof(struct SeqLock) <= sizeof(inferno_seqlock_t) ? 1 : -1];

int
inferno_seqlock_init(inferno_seqlock_t *l)
{
	((struct SeqLock*)l)->seq = 0;
	return 0;
}

void
inferno_seqlock_destroy(inferno_seqlock_t *l)
{
	(void)l;
}

void
inferno_seqlock_write_begin(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;

	if(s->seq & 1)
		die("seqlock: two writers");
	s->seq++;
}

void
inferno_seqlock_write_end(inferno_seqlock_t *l)
{
	((struct SeqLock*)l)->seq++;
}

unsigned
inferno_seqlock_read_begin(inferno_seqlock_t *l)
{
	struct SeqLock *s = (struct SeqLock*)l;
	long spins = 0;

	while(s->seq & 1){		/* writer inside */
		if(++spins > 1000000)
			die("coop: seqlock writer does not finish");
		waitturn();
	}
	return s->seq;
}

int
inferno_seqlock_read_retry(inferno_seqlock_t *l, unsigned seq)
{
	return ((struct SeqLock*)l)->seq != seq;
}

void
inferno_ref_init(inferno_ref_t *r, int v)
{
	r->v = v;
}

int
inferno_ref_get(inferno_ref_t *r)
{
	return r->v;
}

int
inferno_ref_inc(inferno_ref_t *r)
{
	return ++r->v;
}

int
inferno_ref_dec(inferno_ref_t *r)
{
	return --r->v;
}
