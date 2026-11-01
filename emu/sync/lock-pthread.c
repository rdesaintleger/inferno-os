/*
 *  Backend of include/inferno/lock.h on POSIX threads.  What it keeps for itself
 *  (pthread mutexes and condition variables, the wake pipe) is allocated by the
 *  init functions behind the `impl' pointers of the types, which are those of the
 *  header.
 */
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include "lock-core.h"

typedef struct inferno_thread Thread;
enum { Tidle, Tcond, Tfd };

/* behind inferno_thread.impl: everything about one thread's waits, under m */
struct pthr {
	pthread_mutex_t	m;
	pthread_cond_t	c;
	int		state;		/* what the thread is blocked in, so how to wake it */
	int		wake[2];	/* pipe that interrupts a poll; created by the first wait_fd */
};

/* the impl of a descriptor that another thread names, checked */
static struct pthr*
thread_impl(Thread *t)
{
	thread_ctx(t);
	return t->impl;
}

static pthread_key_t key;
static pthread_once_t keyonce = PTHREAD_ONCE_INIT;
static pthread_mutex_t idlock = PTHREAD_MUTEX_INITIALIZER;
static unsigned nctx;

static void
mkkey(void)
{
	if(pthread_key_create(&key, NULL) != 0)
		die("cannot create the thread key");
}

static volatile int keyready;	/* the key exists: spares a pthread_once on every call */

static Thread*
selfthread(void)
{
	Thread *t;

	if(!keyready){
		pthread_once(&keyonce, mkkey);
		keyready = 1;
	}
	t = pthread_getspecific(key);
	if(t == NULL || t->ctx.magic != INFERNO_THREAD_MAGIC)
		thread_ctx(t);		/* dies, with the right message */
	return t;
}

static Ctx*
self(void)
{
	return &selfthread()->ctx;
}

/*
 *  What a token or a mutex keeps for this backend, behind Prim.impl.  A token or a
 *  short mutex is released by its holder: the pthread mutex is enough, and it has
 *  no condition variable.  A mutex without an owner is the flag Prim.locked, guarded
 *  by the pthread mutex, plus the condition variable, so that another thread than
 *  the one that took it can release it.
 */
struct pprim {
	pthread_mutex_t	m;
	pthread_cond_t	c;
	int		nwait;
};

#define RAW(p)	((p)->flags & (INFERNO_PRIM_TOKEN | INFERNO_MUTEX_SHORT))

static struct pprim*
pp(Prim *p)
{
	if(p->impl == NULL)
		die("token or mutex not initialized (or destroyed)");
	return p->impl;
}

static int
prim_init(Prim *p)
{
	struct pprim *x;

	x = lock_zalloc(sizeof *x);
	if(x == NULL)
		return -1;
	if(pthread_mutex_init(&x->m, NULL) != 0){
		lock_zfree(x, sizeof *x);
		return -1;
	}
	if(!RAW(p) && pthread_cond_init(&x->c, NULL) != 0){
		pthread_mutex_destroy(&x->m);
		lock_zfree(x, sizeof *x);
		return -1;
	}
	p->impl = x;
	return 0;
}

static void
prim_destroy(Prim *p)
{
	struct pprim *x = p->impl;

	if(x == NULL)
		return;
	if(!RAW(p))
		pthread_cond_destroy(&x->c);
	pthread_mutex_destroy(&x->m);
	lock_zfree(x, sizeof *x);
	p->impl = NULL;
}

static int
prim_try(Prim *p)
{
	struct pprim *x = pp(p);
	int r;

	if(RAW(p))
		return pthread_mutex_trylock(&x->m) == 0;

	pthread_mutex_lock(&x->m);
	r = !p->locked;
	if(r)
		p->locked = 1;
	pthread_mutex_unlock(&x->m);
	return r;
}

static void
prim_wait(Prim *p)
{
	struct pprim *x = pp(p);

	pthread_mutex_lock(&x->m);
	if(RAW(p))
		return;
	x->nwait++;
	while(p->locked)
		pthread_cond_wait(&x->c, &x->m);
	x->nwait--;
	p->locked = 1;
	pthread_mutex_unlock(&x->m);
}

static void
prim_put(Prim *p)
{
	struct pprim *x = pp(p);

	if(RAW(p)){
		pthread_mutex_unlock(&x->m);
		return;
	}
	pthread_mutex_lock(&x->m);
	p->locked = 0;
	if(x->nwait)
		pthread_cond_signal(&x->c);
	pthread_mutex_unlock(&x->m);
}

#include "lock-core.inc.h"

/*
 *  Seqlock.  Portable form: the reader holds the lock from read_begin to
 *  read_retry, which therefore never asks for a retry.  The callers do not
 *  change when a lock-free backend replaces this one.
 */
static pthread_mutex_t*
seqm(inferno_seqlock_t *l)
{
	if(l->impl == NULL)
		die("seqlock not initialized (or destroyed)");
	return l->impl;
}

int
inferno_seqlock_init(inferno_seqlock_t *l)
{
	pthread_mutex_t *m;

	l->seq = 0;
	m = lock_zalloc(sizeof *m);
	if(m == NULL)
		return -1;
	if(pthread_mutex_init(m, NULL) != 0){
		lock_zfree(m, sizeof *m);
		l->impl = NULL;
		return -1;
	}
	l->impl = m;
	return 0;
}

void
inferno_seqlock_destroy(inferno_seqlock_t *l)
{
	if(l->impl == NULL)
		return;
	pthread_mutex_destroy(l->impl);
	lock_zfree(l->impl, sizeof(pthread_mutex_t));
	l->impl = NULL;
}

void
inferno_seqlock_write_begin(inferno_seqlock_t *l)
{
	pthread_mutex_lock(seqm(l));
	l->seq++;
}

void
inferno_seqlock_write_end(inferno_seqlock_t *l)
{
	l->seq++;
	pthread_mutex_unlock(seqm(l));
}

unsigned
inferno_seqlock_read_begin(inferno_seqlock_t *l)
{
	pthread_mutex_lock(seqm(l));
	return l->seq;
}

int
inferno_seqlock_read_retry(inferno_seqlock_t *l, unsigned seq)
{
	(void)seq;
	pthread_mutex_unlock(seqm(l));
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
	return &stripe[((uintptr_t)r >> 4) % NSTRIPE];
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

/*
 *  Waiting and interruption.  Everything about one thread's wait (the flag of
 *  an interrupt, the permits, what it is blocked in) is under the mutex of its impl, and a wait
 *  tests the flag and blocks under that same lock (a condition wait releases it
 *  atomically; for a descriptor, the interrupter writes a byte to a pipe that
 *  the poll includes), so an interrupt cannot fall between the test and the block.
 */
int
inferno_thread_init(inferno_thread_t *th)
{
	Thread *t = th;
	struct pthr *x;
	unsigned id;

	memset(t, 0, sizeof *t);
	x = lock_zalloc(sizeof *x);
	if(x == NULL)
		return -1;
	x->wake[0] = x->wake[1] = -1;
	if(pthread_mutex_init(&x->m, NULL) != 0){
		lock_zfree(x, sizeof *x);
		return -1;
	}
	if(pthread_cond_init(&x->c, NULL) != 0){
		pthread_mutex_destroy(&x->m);
		lock_zfree(x, sizeof *x);
		return -1;
	}
	pthread_mutex_lock(&idlock);
	id = nctx++;
	pthread_mutex_unlock(&idlock);
	ctx_init(&t->ctx, id);
	t->impl = x;
	return 0;
}

void
inferno_thread_destroy(inferno_thread_t *th)
{
	Thread *t = th;
	struct pthr *x;

	if(thread_ctx(t)->n || t->ctx.nshort)
		lock_warn("inferno_lock: thread %u destroyed holding %d tokens and %d short mutexes\n",
			t->ctx.id, t->ctx.n, t->ctx.nshort);
	x = t->impl;
	ctx_free(&t->ctx);
	t->ctx.magic = 0;
	t->impl = NULL;
	if(x->wake[0] >= 0){
		close(x->wake[0]);
		close(x->wake[1]);
	}
	pthread_cond_destroy(&x->c);
	pthread_mutex_destroy(&x->m);
	lock_zfree(x, sizeof *x);
}

void
inferno_thread_attach(inferno_thread_t *th)
{
	thread_ctx(th);
	pthread_once(&keyonce, mkkey);
	pthread_setspecific(key, th);
}

void
inferno_thread_detach(void)
{
	Thread *t;

	pthread_once(&keyonce, mkkey);
	t = pthread_getspecific(key);
	if(t != NULL && (t->ctx.n || t->ctx.nshort))
		lock_warn("inferno_lock: thread %u detached holding %d tokens and %d short mutexes\n",
			t->ctx.id, t->ctx.n, t->ctx.nshort);
	pthread_setspecific(key, NULL);
}

inferno_thread_t*
inferno_thread_current(void)
{
	pthread_once(&keyonce, mkkey);
	return pthread_getspecific(key);
}

static void
deadline(struct timespec *ts, long ms)
{
	clock_gettime(CLOCK_REALTIME, ts);	/* a condition wait takes this clock everywhere */
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (ms % 1000) * 1000000L;
	if(ts->tv_nsec >= 1000000000L){
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000L;
	}
}

static void
drain(Thread *t)
{
	struct pthr *X = t->impl;
	char b[16];

	if(X->wake[0] >= 0)
		while(read(X->wake[0], b, sizeof b) > 0)
			;
}

static int
mkwake(Thread *t)
{
	struct pthr *X = t->impl;
	int i;

	if(pipe(X->wake) < 0){
		X->wake[0] = X->wake[1] = -1;
		return -1;
	}
	for(i = 0; i < 2; i++){
		fcntl(X->wake[i], F_SETFL, fcntl(X->wake[i], F_GETFL) | O_NONBLOCK);
		fcntl(X->wake[i], F_SETFD, FD_CLOEXEC);
	}
	return 0;
}

/* ms < 0: no limit; usepermit: a permit of inferno_wake ends the wait */
static int
blockon(long ms, int usepermit)
{
	Thread *t = selfthread();
	struct pthr *X = t->impl;
	Ctx *c = &t->ctx;
	struct inferno_saveent *sv = NULL;
	int sn = 0;
	struct timespec dl;
	int r, rc, paused, timedout;

	paused = timedout = 0;
	if(ms > 0)
		deadline(&dl, ms);
	if(forced(c)){
		sn = pause_begin(c, &sv);
		paused = 1;
	}
	pthread_mutex_lock(&X->m);
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
		if(timedout || ms == 0){
			r = INFERNO_WAIT_TIMEOUT;
			break;
		}
		if(!paused){			/* about to block: pause point */
			pthread_mutex_unlock(&X->m);
			sn = pause_begin(c, &sv);
			paused = 1;
			pthread_mutex_lock(&X->m);
			continue;		/* test again, we were not holding the lock */
		}
		X->state = Tcond;
		if(ms > 0)
			rc = pthread_cond_timedwait(&X->c, &X->m, &dl);
		else
			rc = pthread_cond_wait(&X->c, &X->m);
		X->state = Tidle;
		if(rc == ETIMEDOUT)
			timedout = 1;
	}
	pthread_mutex_unlock(&X->m);
	if(paused)
		pause_end(c, sv, sn);
	return r;
}

int
inferno_wait(void)
{
	return blockon(-1, 1);
}

int
inferno_wait_timed(long ms)
{
	return blockon(ms, 1);
}

int
inferno_sleep(long ms)
{
	return blockon(ms, 0);
}

int
inferno_wait_fd(int fd, int events, long ms, int *ready)
{
	Thread *t = selfthread();
	struct pthr *X = t->impl;
	Ctx *c = &t->ctx;
	struct inferno_saveent *sv = NULL;
	int sn = 0;
	struct pollfd pfd[2];
	struct timespec dl, now;
	int r, rc, to, paused, ev;

	paused = 0;
	if(ms > 0)
		deadline(&dl, ms);
	pthread_mutex_lock(&X->m);
	if(t->pending){
		t->pending = 0;
		pthread_mutex_unlock(&X->m);
		return INFERNO_WAIT_INTR;
	}
	if(X->wake[0] < 0 && mkwake(t) < 0){
		pthread_mutex_unlock(&X->m);
		die("cannot create the wake pipe");
	}
	X->state = Tfd;			/* from here an interrupt writes the pipe */
	pthread_mutex_unlock(&X->m);
	sn = pause_begin(c, &sv);
	paused = 1;

	ev = 0;
	if(events & INFERNO_FD_READ)
		ev |= POLLIN;
	if(events & INFERNO_FD_WRITE)
		ev |= POLLOUT;
	for(;;){
		pfd[0].fd = fd;
		pfd[0].events = ev;
		pfd[0].revents = 0;
		pfd[1].fd = X->wake[0];
		pfd[1].events = POLLIN;
		pfd[1].revents = 0;
		if(ms < 0)
			to = -1;
		else if(ms == 0)
			to = 0;
		else{
			clock_gettime(CLOCK_REALTIME, &now);
			to = (dl.tv_sec - now.tv_sec) * 1000 + (dl.tv_nsec - now.tv_nsec) / 1000000;
			if(to < 0)
				to = 0;
		}
		rc = poll(pfd, 2, to);
		if(rc < 0 && errno == EINTR)
			continue;		/* a host signal, not ours: wait again */
		pthread_mutex_lock(&X->m);
		if(t->pending){
			t->pending = 0;
			drain(t);
			r = INFERNO_WAIT_INTR;
		}else if(rc > 0 && pfd[0].revents != 0){
			r = INFERNO_WAIT_READY;
			if(ready != NULL){
				*ready = 0;
				if(pfd[0].revents & (POLLIN|POLLHUP|POLLERR|POLLNVAL))
					*ready |= events & INFERNO_FD_READ;
				if(pfd[0].revents & (POLLOUT|POLLERR|POLLNVAL))
					*ready |= events & INFERNO_FD_WRITE;
			}
		}else if(rc > 0){		/* the pipe, but the flag was consumed meanwhile */
			drain(t);
			pthread_mutex_unlock(&X->m);
			continue;
		}else if(rc == 0)
			r = INFERNO_WAIT_TIMEOUT;
		else
			r = INFERNO_WAIT_INTR;	/* poll failed: let the caller's own call report it */
		X->state = Tidle;
		pthread_mutex_unlock(&X->m);
		break;
	}
	if(paused)
		pause_end(c, sv, sn);
	return r;
}

void
inferno_wake(inferno_thread_t *th)
{
	Thread *t = th;
	struct pthr *X = thread_impl(t);

	pthread_mutex_lock(&X->m);
	t->permits++;
	if(X->state == Tcond)
		pthread_cond_signal(&X->c);
	pthread_mutex_unlock(&X->m);
}

void
inferno_thread_interrupt(inferno_thread_t *th)
{
	Thread *t = th;
	struct pthr *X = thread_impl(t);
	char b = 'i';
	ssize_t n;

	pthread_mutex_lock(&X->m);
	t->pending = 1;
	if(X->state == Tcond)
		pthread_cond_broadcast(&X->c);
	else if(X->state == Tfd)
		n = write(X->wake[1], &b, 1), (void)n;	/* full pipe: already woken */
	pthread_mutex_unlock(&X->m);
}

int
inferno_thread_clear_interrupt(void)
{
	Thread *t = selfthread();
	struct pthr *X = t->impl;
	int r;

	pthread_mutex_lock(&X->m);
	r = t->pending;
	t->pending = 0;
	drain(t);
	pthread_mutex_unlock(&X->m);
	return r;
}
