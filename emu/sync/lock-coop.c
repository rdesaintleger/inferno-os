/*
 *  Backend of include/inferno/lock.h without any host thread or wait: see
 *  lock-coop.h.  Nothing here is atomic because nothing runs concurrently.
 */
#include "lock-core.h"
#include "lock-coop.h"

void (*inferno_coop_wait)(void);

typedef struct inferno_thread Thread;

static Thread *cur;		/* the context that runs */
static unsigned nctx;

static Thread*
selfthread(void)
{
	thread_ctx(cur);
	return cur;
}

static Ctx*
self(void)
{
	return &selfthread()->ctx;
}

void
inferno_coop_select(inferno_thread_t *th)
{
	thread_ctx(th);
	cur = th;
}

inferno_thread_t*
inferno_coop_current(void)
{
	return cur;
}

static void
waitturn(void)
{
	Thread *me = cur;

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

#include "lock-core.inc.h"

/*
 *  Seqlock with a real retry, as a lock-free backend would have: since
 *  contexts only switch in waitturn, a writer (which never waits) cannot be
 *  seen in the middle of its section except by a test that selects a context
 *  by hand there, which is how the retry is tested.
 */
int
inferno_seqlock_init(inferno_seqlock_t *l)
{
	l->seq = 0;
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
	struct inferno_seqlock *s = l;

	if(s->seq & 1)
		die("seqlock: two writers");
	s->seq++;
}

void
inferno_seqlock_write_end(inferno_seqlock_t *l)
{
	l->seq++;
}

unsigned
inferno_seqlock_read_begin(inferno_seqlock_t *l)
{
	struct inferno_seqlock *s = l;
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
	return l->seq != seq;
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

/*
 *  Waiting and interruption on one host thread: the contexts take turns by hand.
 */
static long clock_ms;
int (*inferno_coop_fdready)(int, int);

long
inferno_coop_now(void)
{
	return clock_ms;
}

void
inferno_coop_advance(long ms)
{
	clock_ms += ms;
}

int
inferno_thread_init(inferno_thread_t *th)
{
	memset(th, 0, sizeof *th);
	ctx_init(&th->ctx, nctx++);
	return 0;
}

void
inferno_thread_destroy(inferno_thread_t *th)
{
	if(thread_ctx(th)->n || th->ctx.nshort)
		fprintf(stderr, "inferno_lock: context %u destroyed holding %d tokens and %d short mutexes\n",
			th->ctx.id, th->ctx.n, th->ctx.nshort);
	ctx_free(&th->ctx);
	th->ctx.magic = 0;
	if(cur == th)
		cur = NULL;
}

void
inferno_thread_attach(inferno_thread_t *th)
{
	inferno_coop_select(th);
}

void
inferno_thread_detach(void)
{
	cur = NULL;
}

inferno_thread_t*
inferno_thread_current(void)
{
	return cur;
}

/* ms < 0: no limit; fd >= 0: wait for a descriptor instead of a permit */
static int
blockon(long ms, int usepermit, int fd, int events, int *ready)
{
	Thread *t = selfthread();
	Ctx *c = &t->ctx;
	struct inferno_saveent *sv = NULL;
	int sn = 0;
	long dl = clock_ms + ms;
	int r, paused, k;
	long spins = 0;

	paused = 0;
	if(forced(c)){
		sn = pause_begin(c, &sv);
		paused = 1;
	}
	for(;;){
		if(t->pending){
			t->pending = 0;
			r = INFERNO_WAIT_INTR;
			break;
		}
		if(usepermit && t->permits > 0){
			t->permits--;
			r = INFERNO_WAIT_WOKEN;
			break;
		}
		if(fd >= 0 && inferno_coop_fdready != NULL && (k = inferno_coop_fdready(fd, events)) != 0){
			if(ready != NULL)
				*ready = k & events;
			r = INFERNO_WAIT_READY;
			break;
		}
		if(ms == 0 || (ms > 0 && clock_ms >= dl)){
			r = INFERNO_WAIT_TIMEOUT;
			break;
		}
		if(!paused){
			sn = pause_begin(c, &sv);
			paused = 1;
		}
		if(inferno_coop_wait == NULL){
			if(ms > 0){		/* nothing else can run: the time goes by */
				clock_ms = dl;
				continue;
			}
			die("coop: would wait with nothing to run (deadlock)");
		}
		if(++spins > 1000000)
			die("coop: wait does not end (deadlock)");
		waitturn();
		if(ms > 0 && clock_ms < dl)
			clock_ms++;
	}
	if(paused)
		pause_end(c, sv, sn);
	return r;
}

int
inferno_wait(void)
{
	return blockon(-1, 1, -1, 0, NULL);
}

int
inferno_wait_timed(long ms)
{
	return blockon(ms, 1, -1, 0, NULL);
}

int
inferno_sleep(long ms)
{
	return blockon(ms, 0, -1, 0, NULL);
}

int
inferno_wait_fd(int fd, int events, long ms, int *ready)
{
	return blockon(ms, 0, fd, events, ready);
}

void
inferno_wake(inferno_thread_t *th)
{
	th->permits++;
}

void
inferno_thread_interrupt(inferno_thread_t *th)
{
	th->pending = 1;
}

int
inferno_thread_clear_interrupt(void)
{
	Thread *t = selfthread();
	int r = t->pending;

	t->pending = 0;
	return r;
}
