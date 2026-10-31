/*
 *  Backend of include/inferno/lock.h on POSIX threads.  A token or a mutex is a
 *  flag guarded by a pthread mutex plus a condition variable, so that, as the
 *  API requires, it can be released by a thread other than the one that took it.
 */
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include "lock-core.h"

struct Prim
{
	int		flags;
	int		locked;
	int		nwait;
	pthread_mutex_t	m;
	pthread_cond_t	c;
};

struct Thread
{
	pthread_mutex_t	m;
	pthread_cond_t	c;
	int		permits;	/* inferno_wake not consumed yet */
	int		pending;	/* interrupt not consumed yet */
	int		state;		/* what the thread is blocked in, so how to wake it */
	int		wake[2];	/* pipe that interrupts a poll; created by the first wait_fd */
};

enum { Tidle, Tcond, Tfd };

struct SeqLock
{
	pthread_mutex_t	m;
	unsigned	seq;
};

typedef char prim_fits[sizeof(struct Prim) <= sizeof(inferno_token_t) ? 1 : -1];
typedef char prim_fits2[sizeof(struct Prim) <= sizeof(inferno_mutex_t) ? 1 : -1];
typedef char thread_fits[sizeof(struct Thread) <= sizeof(inferno_thread_t) ? 1 : -1];
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

/*
 *  Waiting and interruption.  Everything about one thread's wait (the flag of
 *  an interrupt, the permits, what it is blocked in) is under t->m, and a wait
 *  tests the flag and blocks under that same lock (a condition wait releases it
 *  atomically; for a descriptor, the interrupter writes a byte to a pipe that
 *  the poll includes), so an interrupt cannot fall between the test and the block.
 */
typedef struct Thread Thread;

int
inferno_thread_init(inferno_thread_t *th)
{
	Thread *t = (Thread*)th;

	memset(t, 0, sizeof *t);
	t->wake[0] = t->wake[1] = -1;
	if(pthread_mutex_init(&t->m, NULL) != 0)
		return -1;
	if(pthread_cond_init(&t->c, NULL) != 0){
		pthread_mutex_destroy(&t->m);
		return -1;
	}
	return 0;
}

void
inferno_thread_destroy(inferno_thread_t *th)
{
	Thread *t = (Thread*)th;

	if(t->wake[0] >= 0){
		close(t->wake[0]);
		close(t->wake[1]);
	}
	pthread_cond_destroy(&t->c);
	pthread_mutex_destroy(&t->m);
}

void
inferno_thread_attach(inferno_thread_t *th)
{
	self()->thread = th;
}

void
inferno_thread_detach(void)
{
	self()->thread = NULL;
}

inferno_thread_t*
inferno_thread_current(void)
{
	return self()->thread;
}

static Thread*
mythread(Ctx *c)
{
	if(c->thread == NULL)
		die("wait by a thread that is not attached (inferno_thread_attach)");
	return c->thread;
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
	char b[16];

	if(t->wake[0] >= 0)
		while(read(t->wake[0], b, sizeof b) > 0)
			;
}

static int
mkwake(Thread *t)
{
	int i;

	if(pipe(t->wake) < 0){
		t->wake[0] = t->wake[1] = -1;
		return -1;
	}
	for(i = 0; i < 2; i++){
		fcntl(t->wake[i], F_SETFL, fcntl(t->wake[i], F_GETFL) | O_NONBLOCK);
		fcntl(t->wake[i], F_SETFD, FD_CLOEXEC);
	}
	return 0;
}

/* ms < 0: no limit; usepermit: a permit of inferno_wake ends the wait */
static int
blockon(long ms, int usepermit)
{
	Ctx *c = self();
	Thread *t = mythread(c);
	inferno_token_saved_t s;
	struct timespec dl;
	int r, rc, paused, timedout;

	paused = timedout = 0;
	if(ms > 0)
		deadline(&dl, ms);
	if(forced(c)){
		if(c->nshort)
			die("wait would pause with a short mutex held");
		release_all(c, &s);
		paused = 1;
	}
	pthread_mutex_lock(&t->m);
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
			pthread_mutex_unlock(&t->m);
			if(c->nshort)
				die("wait would pause with a short mutex held");
			release_all(c, &s);
			paused = 1;
			pthread_mutex_lock(&t->m);
			continue;		/* test again, we were not holding the lock */
		}
		t->state = Tcond;
		if(ms > 0)
			rc = pthread_cond_timedwait(&t->c, &t->m, &dl);
		else
			rc = pthread_cond_wait(&t->c, &t->m);
		t->state = Tidle;
		if(rc == ETIMEDOUT)
			timedout = 1;
	}
	pthread_mutex_unlock(&t->m);
	if(paused)
		retake(c, &s, 0, 0, 0);
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
	Ctx *c = self();
	Thread *t = mythread(c);
	inferno_token_saved_t s;
	struct pollfd pfd[2];
	struct timespec dl, now;
	int r, rc, to, paused, ev;

	paused = 0;
	if(ms > 0)
		deadline(&dl, ms);
	pthread_mutex_lock(&t->m);
	if(t->pending){
		t->pending = 0;
		pthread_mutex_unlock(&t->m);
		return INFERNO_WAIT_INTR;
	}
	if(t->wake[0] < 0 && mkwake(t) < 0){
		pthread_mutex_unlock(&t->m);
		die("cannot create the wake pipe");
	}
	t->state = Tfd;			/* from here an interrupt writes the pipe */
	pthread_mutex_unlock(&t->m);
	if(c->nshort)
		die("wait would pause with a short mutex held");
	release_all(c, &s);
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
		pfd[1].fd = t->wake[0];
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
		pthread_mutex_lock(&t->m);
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
			pthread_mutex_unlock(&t->m);
			continue;
		}else if(rc == 0)
			r = INFERNO_WAIT_TIMEOUT;
		else
			r = INFERNO_WAIT_INTR;	/* poll failed: let the caller's own call report it */
		t->state = Tidle;
		pthread_mutex_unlock(&t->m);
		break;
	}
	if(paused)
		retake(c, &s, 0, 0, 0);
	return r;
}

void
inferno_wake(inferno_thread_t *th)
{
	Thread *t = (Thread*)th;

	pthread_mutex_lock(&t->m);
	t->permits++;
	if(t->state == Tcond)
		pthread_cond_signal(&t->c);
	pthread_mutex_unlock(&t->m);
}

void
inferno_thread_interrupt(inferno_thread_t *th)
{
	Thread *t = (Thread*)th;
	char b = 'i';
	ssize_t n;

	pthread_mutex_lock(&t->m);
	t->pending = 1;
	if(t->state == Tcond)
		pthread_cond_broadcast(&t->c);
	else if(t->state == Tfd)
		n = write(t->wake[1], &b, 1), (void)n;	/* full pipe: already woken */
	pthread_mutex_unlock(&t->m);
}

int
inferno_thread_clear_interrupt(void)
{
	Thread *t = mythread(self());
	int r;

	pthread_mutex_lock(&t->m);
	r = t->pending;
	t->pending = 0;
	drain(t);
	pthread_mutex_unlock(&t->m);
	return r;
}
