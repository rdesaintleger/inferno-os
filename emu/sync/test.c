/*
 *  Tests of the synchronization API of include/inferno/lock.h, for any backend:
 *	cc -DBACKEND_PTHREAD test.c lock-pthread.c -lpthread
 *	cc -DBACKEND_COOP test.c lock-coop.c
 *  Prints "PASS name" per test and exits 0 when all passed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inferno/lock-types.h>
#ifdef BACKEND_COOP
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "lock-coop.h"
#endif
#ifdef BACKEND_PTHREAD
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

static int failed;
static inferno_thread_t mainth;		/* the main thread, attached in main */

#define CHECK(x) do { if(!(x)){ printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); failed++; } } while(0)

static void
report(const char *name, int before)
{
	printf("%s %s\n", failed == before ? "PASS" : "FAIL", name);
}

#define H(t) inferno_token_held(&(t))

static void
t_token(void)
{
	inferno_token_t a, b, c, *set[3];
	inferno_token_saved_t s;
	unsigned m0, m1;

	inferno_token_init(&a); inferno_token_init(&b); inferno_token_init(&c);
	CHECK(inferno_token_get(&a) == 0);
	CHECK(H(a) && !H(b));
	CHECK(inferno_token_get(&b) == 0);
	inferno_token_put(&a);
	CHECK(!H(a) && H(b));
	inferno_token_put(&b);
	CHECK(!H(b));

	CHECK(inferno_token_try(&a) == 1);
	inferno_token_put(&a);

	/* marks: what was taken since is put back */
	m0 = inferno_token_mark();
	inferno_token_get(&a);
	m1 = inferno_token_mark();
	inferno_token_get(&b);
	inferno_token_get(&c);
	inferno_token_release_to(m1);
	CHECK(H(a) && !H(b) && !H(c));
	inferno_token_release_to(m0);
	CHECK(!H(a));

	/* get_all, in any order, duplicates ignored */
	set[0] = &c; set[1] = &a; set[2] = &c;
	CHECK(inferno_token_get_all(3, set) == 0);
	CHECK(H(a) && H(c) && !H(b));
	inferno_token_put(&a);
	inferno_token_put(&c);

	/* suspend / resume keep the acquisition order for the marks */
	inferno_token_get(&a);
	m1 = inferno_token_mark();
	inferno_token_get(&b);
	inferno_token_suspend(&s);
	CHECK(s.n == 2 && !H(a) && !H(b));
	inferno_token_resume(&s);
	CHECK(H(a) && H(b));
	inferno_token_release_to(m1);
	CHECK(H(a) && !H(b));
	inferno_token_put(&a);

	inferno_token_destroy(&a); inferno_token_destroy(&b); inferno_token_destroy(&c);
}

static void
t_debug(void)
{
	inferno_token_t a, b;
	inferno_mutex_t m;
	int i;

	inferno_token_init(&a); inferno_token_init(&b);
	inferno_mutex_init(&m, 0);
	inferno_lock_debug(1, 1000);		/* always pause */
	CHECK(inferno_token_get(&a) == 1);
	CHECK(inferno_token_get(&b) == 1);
	CHECK(H(a) && H(b));			/* all taken again */
	inferno_token_put(&a);
	inferno_token_put(&b);
	CHECK(inferno_mutex_lock(&m) == 1);
	inferno_mutex_unlock(&m);
	inferno_lock_debug(1, 0);
	CHECK(inferno_token_get(&a) == 0);
	inferno_token_put(&a);
	inferno_lock_debug(7, 500);		/* about half */
	{
		int n = 0;
		for(i = 0; i < 1000; i++){
			n += inferno_token_get(&a);
			inferno_token_put(&a);
		}
		CHECK(n > 300 && n < 700);
	}
	inferno_lock_debug(0, 0);
	inferno_token_destroy(&a); inferno_token_destroy(&b);
	inferno_mutex_destroy(&m);
}

static void
t_mutex(void)
{
	inferno_mutex_t m, sm;

	inferno_mutex_init(&m, 0);
	inferno_mutex_init(&sm, INFERNO_MUTEX_SHORT);
	CHECK(inferno_mutex_lock(&m) == 0);
	CHECK(inferno_mutex_trylock(&m) == 0);
	inferno_mutex_unlock(&m);
	CHECK(inferno_mutex_trylock(&m) == 1);
	inferno_mutex_unlock(&m);
	CHECK(inferno_mutex_lock(&sm) == 0);
	CHECK(inferno_mutex_trylock(&sm) == 0);
	inferno_mutex_unlock(&sm);
	inferno_mutex_destroy(&m);
	inferno_mutex_destroy(&sm);
}

static void
t_seqlock(void)
{
	inferno_seqlock_t l;
	unsigned s;
	int v = 1, copy, tries = 0;

	inferno_seqlock_init(&l);
	do{
		s = inferno_seqlock_read_begin(&l);
		copy = v;
		tries++;
	}while(inferno_seqlock_read_retry(&l, s));
	CHECK(copy == 1 && tries == 1);
	inferno_seqlock_write_begin(&l);
	v = 2;
	inferno_seqlock_write_end(&l);
	do{
		s = inferno_seqlock_read_begin(&l);
		copy = v;
	}while(inferno_seqlock_read_retry(&l, s));
	CHECK(copy == 2);
	inferno_seqlock_destroy(&l);
}

static void
t_ref(void)
{
	inferno_ref_t r;

	inferno_ref_init(&r, 1);
	CHECK(inferno_ref_inc(&r) == 2);
	CHECK(inferno_ref_dec(&r) == 1);
	CHECK(inferno_ref_dec(&r) == 0);
	CHECK(inferno_ref_get(&r) == 0);
}

#ifdef BACKEND_PTHREAD

#define NTHREAD	4

/* every thread that uses the API has a descriptor */
#define ATTACH(me)	do { inferno_thread_init(&(me)); inferno_thread_attach(&(me)); } while(0)
#define DETACH(me)	do { inferno_thread_detach(); inferno_thread_destroy(&(me)); } while(0)

/* a pthread-specific helper: run n copies of f, wait for them */
static void
spawn(int n, void *(*f)(void*))
{
	pthread_t th[NTHREAD];
	int i;

	for(i = 0; i < n; i++)
		pthread_create(&th[i], NULL, f, (void*)(long)i);
	for(i = 0; i < n; i++)
		pthread_join(th[i], NULL);
}

static void
nap(void)
{
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	nanosleep(&ts, NULL);
}

/* tokens taken in all orders by several threads: exclusivity, no deadlock, no lost update */
static inferno_token_t T[3];
static long cnt[3];
#define NITER	20000

static void*
tokworker(void *arg)
{
	inferno_thread_t me;

	ATTACH(me);
	unsigned r = (unsigned)(long)arg * 7919u + 1;
	int i, x, y, pr;
	long vx, a;

	for(i = 0; i < NITER; i++){
		r = r * 1103515245u + 12345u;
		x = (r >> 8) % 3;
		y = (r >> 12) % 3;
		if(x == y)
			y = (x + 1) % 3;
		inferno_token_get(&T[x]);
		vx = cnt[x];
		pr = inferno_token_get(&T[y]);
		if(pr == 0 && cnt[x] != vx){
			printf("FAIL: data under a token held changed without a pause\n");
			failed++;
		}
		a = cnt[x];
		if(i % 16 == 0)
			sched_yield();
		if(cnt[x] != a){
			printf("FAIL: exclusivity lost on token %d\n", x);
			failed++;
		}
		cnt[x] = a + 1;
		a = cnt[y];
		if(i % 16 == 8)
			sched_yield();
		cnt[y] = a + 1;
		inferno_token_put(&T[y]);
		inferno_token_put(&T[x]);
	}
	DETACH(me);
	return NULL;
}

static void
t_tokens_threads(int permille)
{
	int i;

	for(i = 0; i < 3; i++){
		inferno_token_init(&T[i]);
		cnt[i] = 0;
	}
	inferno_lock_debug(42, permille);
	spawn(NTHREAD, tokworker);
	inferno_lock_debug(0, 0);
	CHECK(cnt[0] + cnt[1] + cnt[2] == 2L * NTHREAD * NITER);
	for(i = 0; i < 3; i++)
		inferno_token_destroy(&T[i]);
}

/* get_all against incremental gets, in opposite orders */
static void*
allworker(void *arg)
{
	inferno_thread_t me;

	ATTACH(me);
	inferno_token_t *set[3];
	int i;

	for(i = 0; i < NITER/4; i++){
		if((long)arg & 1){
			set[0] = &T[0]; set[1] = &T[1]; set[2] = &T[2];
		}else{
			set[0] = &T[2]; set[1] = &T[1]; set[2] = &T[0];
		}
		inferno_token_get_all(3, set);
		cnt[0]++; cnt[1]++; cnt[2]++;
		if(i % 8 == 0)
			sched_yield();
		inferno_token_put(&T[0]); inferno_token_put(&T[1]); inferno_token_put(&T[2]);
	}
	DETACH(me);
	return NULL;
}

static void
t_get_all_threads(void)
{
	int i;

	for(i = 0; i < 3; i++){
		inferno_token_init(&T[i]);
		cnt[i] = 0;
	}
	inferno_lock_debug(5, 200);
	spawn(NTHREAD, allworker);
	inferno_lock_debug(0, 0);
	for(i = 0; i < 3; i++)
		CHECK(cnt[i] == (long)NTHREAD * (NITER/4));
	for(i = 0; i < 3; i++)
		inferno_token_destroy(&T[i]);
}

/* short mutex and seqlock under contention */
static inferno_mutex_t SM;
static inferno_seqlock_t SL;
static long shared_a, shared_b;

static void*
shortworker(void *arg)
{
	inferno_thread_t me;

	ATTACH(me);
	int i;
	long a, b;
	unsigned s;

	for(i = 0; i < NITER; i++){
		if((long)arg == 0){
			inferno_seqlock_write_begin(&SL);
			shared_a++;
			shared_b++;
			inferno_seqlock_write_end(&SL);
		}else{
			do{
				s = inferno_seqlock_read_begin(&SL);
				a = shared_a;
				b = shared_b;
			}while(inferno_seqlock_read_retry(&SL, s));
			if(a != b){
				printf("FAIL: seqlock reader saw a torn value\n");
				failed++;
			}
		}
		inferno_mutex_lock(&SM);
		cnt[0]++;
		inferno_mutex_unlock(&SM);
	}
	DETACH(me);
	return NULL;
}

static void
t_short_seq_threads(void)
{
	inferno_mutex_init(&SM, INFERNO_MUTEX_SHORT);
	inferno_seqlock_init(&SL);
	cnt[0] = 0;
	spawn(NTHREAD, shortworker);
	CHECK(cnt[0] == (long)NTHREAD * NITER);
	CHECK(shared_a == NITER && shared_b == NITER);
	inferno_mutex_destroy(&SM);
	inferno_seqlock_destroy(&SL);
}

/* a mutex has no owner: taken by a thread, released by another */
static inferno_mutex_t XM;

static void*
locker(void *arg)
{
	inferno_thread_t me;

	ATTACH(me);
	(void)arg;
	inferno_mutex_lock(&XM);
	DETACH(me);
	return NULL;
}

static void
t_mutex_transfer(void)
{
	pthread_t th;

	inferno_mutex_init(&XM, 0);
	pthread_create(&th, NULL, locker, NULL);
	pthread_join(th, NULL);
	CHECK(inferno_mutex_trylock(&XM) == 0);
	inferno_mutex_unlock(&XM);
	CHECK(inferno_mutex_trylock(&XM) == 1);
	inferno_mutex_unlock(&XM);
	inferno_mutex_destroy(&XM);
}

/* a thread that waits on a mutex releases its tokens, and takes them back */
static inferno_token_t PA;
static inferno_mutex_t PM;
static volatile int p_state, p_ret, p_held;

static void*
pausethr(void *arg)
{
	inferno_thread_t me;

	ATTACH(me);
	(void)arg;
	inferno_token_get(&PA);
	p_state = 1;
	p_ret = inferno_mutex_lock(&PM);	/* held by main: slow path */
	p_held = inferno_token_held(&PA);
	inferno_mutex_unlock(&PM);
	inferno_token_put(&PA);
	p_state = 2;
	DETACH(me);
	return NULL;
}

static void
t_pause_releases(void)
{
	pthread_t th;
	int i, got;

	inferno_token_init(&PA);
	inferno_mutex_init(&PM, 0);
	p_state = p_ret = p_held = 0;
	inferno_mutex_lock(&PM);
	pthread_create(&th, NULL, pausethr, NULL);
	for(i = 0; i < 5000 && p_state != 1; i++)	/* it holds the token now */
		nap();
	got = 0;
	for(i = 0; i < 5000 && !got; i++){	/* the token must come free while it waits */
		got = inferno_token_try(&PA);
		if(!got)
			nap();
	}
	CHECK(got);
	if(got)
		inferno_token_put(&PA);
	inferno_mutex_unlock(&PM);
	pthread_join(th, NULL);
	CHECK(p_ret == 1);
	CHECK(p_held == 1);
	inferno_token_destroy(&PA);
	inferno_mutex_destroy(&PM);
}

/* pausing with a short mutex held is a bug that must be caught */
static void
t_short_pause_aborts(void)
{
	pid_t pid;
	int st;
	inferno_mutex_t sm;
	inferno_token_t a;
	inferno_token_saved_t s;

	pid = fork();
	if(pid == 0){
		if(freopen("/dev/null", "w", stderr) == NULL)
			_exit(1);
		inferno_mutex_init(&sm, INFERNO_MUTEX_SHORT);
		inferno_token_init(&a);
		inferno_mutex_lock(&sm);
		inferno_token_suspend(&s);	/* must abort */
		_exit(0);
	}
	waitpid(pid, &st, 0);
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
}

/* an interrupt or a wake is never lost, in lockstep, whatever the interleaving */
#define NPING	3000

static struct {
	inferno_thread_t th;
	int res[NPING];
} pw;

static void*
pingworker(void *arg)
{
	int i;

	(void)arg;
	inferno_thread_attach(&pw.th);
	for(i = 0; i < NPING; i++){
		pw.res[i] = inferno_wait_timed(5000);
		inferno_wake(&mainth);
	}
	return NULL;
}

static void
t_wait_pingpong(void)
{
	pthread_t th;
	int i, intr;

	inferno_thread_init(&pw.th);
	pthread_create(&th, NULL, pingworker, NULL);
	for(i = 0; i < NPING; i++){
		intr = (i * 7) % 3 == 0;
		if(i % 5 == 0)
			sched_yield();
		if(intr)
			inferno_thread_interrupt(&pw.th);
		else
			inferno_wake(&pw.th);
		if(inferno_wait_timed(5000) != INFERNO_WAIT_WOKEN){
			printf("FAIL: the worker did not answer at step %d\n", i);
			failed++;
			break;
		}
	}
	pthread_join(th, NULL);
	for(i = 0; i < NPING; i++){
		intr = (i * 7) % 3 == 0;
		if(pw.res[i] != (intr? INFERNO_WAIT_INTR: INFERNO_WAIT_WOKEN)){
			printf("FAIL: step %d gave %d\n", i, pw.res[i]);
			failed++;
			break;
		}
	}
	inferno_thread_destroy(&pw.th);
}

/* descriptors */
static void
t_wait_fd_basic(void)
{
	int p[2], ready = 0;
	char b = 'x';

	CHECK(pipe(p) == 0);
	inferno_thread_interrupt(&mainth);
	CHECK(inferno_wait_fd(p[0], INFERNO_FD_READ, -1, &ready) == INFERNO_WAIT_INTR);
	CHECK(inferno_wait_fd(p[0], INFERNO_FD_READ, 20, &ready) == INFERNO_WAIT_TIMEOUT);
	CHECK(inferno_wait_fd(p[0], INFERNO_FD_READ, 0, &ready) == INFERNO_WAIT_TIMEOUT);
	CHECK(write(p[1], &b, 1) == 1);
	CHECK(inferno_wait_fd(p[0], INFERNO_FD_READ, -1, &ready) == INFERNO_WAIT_READY);
	CHECK(ready == INFERNO_FD_READ);
	CHECK(inferno_wait_fd(p[1], INFERNO_FD_WRITE, -1, &ready) == INFERNO_WAIT_READY);
	CHECK(ready == INFERNO_FD_WRITE);
	/* the wake pipe holds no stale byte: an interrupt does not leak into the next wait */
	inferno_thread_interrupt(&mainth);
	CHECK(inferno_wait_fd(p[0], INFERNO_FD_READ, -1, &ready) == INFERNO_WAIT_INTR);
	CHECK(inferno_wait_fd(p[1], INFERNO_FD_WRITE, 5, &ready) == INFERNO_WAIT_READY);
	close(p[0]);
	close(p[1]);
}

/*
 *  The bug of the signal protocol: the interrupter acts between the moment the
 *  thread has announced that it is going to wait and the moment it is really in
 *  the host call.  Here the interrupt lands anywhere in that interval and the
 *  wait must always return INTR.
 */
#define NFD	2000

static struct {
	inferno_thread_t th;
	int p[2];
	volatile int armed;
	int res[NFD];
} fw;

static void*
fdworker(void *arg)
{
	int i, ready;

	(void)arg;
	inferno_thread_attach(&fw.th);
	for(i = 0; i < NFD; i++){
		fw.armed = i + 1;
		fw.res[i] = inferno_wait_fd(fw.p[0], INFERNO_FD_READ, 5000, &ready);
		inferno_wake(&mainth);
	}
	return NULL;
}

static void
t_wait_fd_race(void)
{
	pthread_t th;
	int i;

	inferno_thread_init(&fw.th);
	if(pipe(fw.p) != 0)
		return;
	fw.armed = 0;
	pthread_create(&th, NULL, fdworker, NULL);
	for(i = 0; i < NFD; i++){
		while(fw.armed != i + 1)
			sched_yield();
		if(i % 3 == 0)
			sched_yield();
		inferno_thread_interrupt(&fw.th);
		if(inferno_wait_timed(5000) != INFERNO_WAIT_WOKEN){
			printf("FAIL: the worker did not answer at step %d\n", i);
			failed++;
			break;
		}
	}
	pthread_join(th, NULL);
	for(i = 0; i < NFD; i++)
		if(fw.res[i] != INFERNO_WAIT_INTR){
			printf("FAIL: wait %d returned %d, the interrupt was lost\n", i, fw.res[i]);
			failed++;
			break;
		}
	close(fw.p[0]);
	close(fw.p[1]);
	inferno_thread_destroy(&fw.th);
}

/* waiting releases the tokens and takes them back: wait, sleep and descriptor */
static struct {
	inferno_thread_t th;
	inferno_token_t tok;
	int kind;
	volatile int armed;
	int res, held;
} tw;

static void*
tokwaiter(void *arg)
{
	int ready, p[2] = {-1, -1};

	(void)arg;
	inferno_thread_attach(&tw.th);
	inferno_token_get(&tw.tok);
	if(tw.kind == 2 && pipe(p) != 0)
		return NULL;
	tw.armed = 1;
	if(tw.kind == 0)
		tw.res = inferno_wait();
	else if(tw.kind == 1)
		tw.res = inferno_sleep(400);
	else
		tw.res = inferno_wait_fd(p[0], INFERNO_FD_READ, 400, &ready);
	tw.held = inferno_token_held(&tw.tok);
	inferno_token_put(&tw.tok);
	if(tw.kind == 2){
		close(p[0]);
		close(p[1]);
	}
	return NULL;
}

static void
t_wait_releases_tokens(void)
{
	pthread_t th;
	int kind, i, got;

	for(kind = 0; kind < 3; kind++){
		inferno_thread_init(&tw.th);
		inferno_token_init(&tw.tok);
		tw.kind = kind;
		tw.armed = 0;
		tw.res = -1;
		tw.held = 0;
		pthread_create(&th, NULL, tokwaiter, NULL);
		for(i = 0; i < 5000 && !tw.armed; i++)
			nap();
		got = 0;
		for(i = 0; i < 5000 && !got; i++){	/* released while it waits */
			got = inferno_token_try(&tw.tok);
			if(!got)
				nap();
		}
		CHECK(got);
		if(got)
			inferno_token_put(&tw.tok);
		if(kind == 0)
			inferno_wake(&tw.th);
		pthread_join(th, NULL);
		CHECK(tw.held == 1);
		CHECK(tw.res == (kind == 0? INFERNO_WAIT_WOKEN: INFERNO_WAIT_TIMEOUT));
		inferno_token_destroy(&tw.tok);
		inferno_thread_destroy(&tw.th);
	}
}

/* a sleeping thread that is interrupted gets out at once */
static struct { inferno_thread_t th; volatile int armed; int res; } sw;

static void*
sleeper(void *arg)
{
	(void)arg;
	inferno_thread_attach(&sw.th);
	sw.armed = 1;
	sw.res = inferno_sleep(20000);
	return NULL;
}

static void
t_sleep_interrupted(void)
{
	pthread_t th;
	struct timespec t0, t1;
	long ms;

	inferno_thread_init(&sw.th);
	sw.armed = 0;
	pthread_create(&th, NULL, sleeper, NULL);
	while(!sw.armed)
		sched_yield();
	nap();
	clock_gettime(CLOCK_MONOTONIC, &t0);
	inferno_thread_interrupt(&sw.th);
	pthread_join(th, NULL);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
	CHECK(sw.res == INFERNO_WAIT_INTR);
	CHECK(ms < 2000);
	inferno_thread_destroy(&sw.th);
}

#endif /* BACKEND_PTHREAD */

/* permits and interrupts of the calling thread alone */
static void
t_wait_basic(void)
{
	int r, ready = 0;

	CHECK(inferno_wait_timed(0) == INFERNO_WAIT_TIMEOUT);	/* nothing: no block */
	inferno_wake(&mainth);
	CHECK(inferno_wait_timed(1000) == INFERNO_WAIT_WOKEN);	/* the permit is consumed */
	CHECK(inferno_wait_timed(0) == INFERNO_WAIT_TIMEOUT);

	inferno_thread_interrupt(&mainth);			/* level: not lost before the wait */
	CHECK(inferno_wait() == INFERNO_WAIT_INTR);
	CHECK(inferno_wait_timed(5) == INFERNO_WAIT_TIMEOUT);	/* and consumed by it */

	inferno_thread_interrupt(&mainth);
	CHECK(inferno_sleep(1000) == INFERNO_WAIT_INTR);
	CHECK(inferno_sleep(5) == INFERNO_WAIT_TIMEOUT);

	inferno_wake(&mainth);					/* sleeping does not take permits */
	CHECK(inferno_sleep(5) == INFERNO_WAIT_TIMEOUT);
	CHECK(inferno_wait_timed(0) == INFERNO_WAIT_WOKEN);

	inferno_thread_interrupt(&mainth);
	CHECK(inferno_thread_clear_interrupt() == 1);
	CHECK(inferno_thread_clear_interrupt() == 0);
	CHECK(inferno_wait_timed(5) == INFERNO_WAIT_TIMEOUT);

	inferno_lock_debug(3, 1000);				/* forced pause: same results */
	inferno_wake(&mainth);
	CHECK(inferno_wait_timed(1000) == INFERNO_WAIT_WOKEN);
	inferno_thread_interrupt(&mainth);
	CHECK(inferno_sleep(1000) == INFERNO_WAIT_INTR);
	inferno_lock_debug(0, 0);
	(void)r;
	(void)ready;
}

/* a thread that is not attached, or whose descriptor is gone, is a bug that must be caught */
static int
aborts(void (*f)(void))
{
	pid_t pid;
	int st;

	pid = fork();
	if(pid == 0){
		if(freopen("/dev/null", "w", stderr) == NULL)
			_exit(1);
		f();
		_exit(0);
	}
	waitpid(pid, &st, 0);
	return WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
}

static void
use_detached(void)
{
	inferno_token_t t;

	inferno_token_init(&t);
	inferno_thread_detach();
	inferno_token_get(&t);
}

static void
use_destroyed(void)
{
	inferno_token_t t;
	inferno_thread_t me;

	inferno_token_init(&t);
	inferno_thread_init(&me);
	inferno_thread_attach(&me);
	inferno_thread_destroy(&me);
	inferno_token_get(&t);
}

static void
t_attach_checked(void)
{
	CHECK(inferno_thread_current() == &mainth);
	CHECK(aborts(use_detached));
	CHECK(aborts(use_destroyed));
	CHECK(inferno_thread_current() == &mainth);
}

#ifdef BACKEND_COOP

/*
 *  Deterministic scenarios: context 0 and 1 are two "threads".  inferno_coop_wait
 *  runs step(), one move of whoever holds what the waiter needs.
 */
static inferno_thread_t cth[2];		/* the two contexts */
static inferno_token_t CT[2];		/* canonical order: CT[0] first */
#define CA	CT[0]
#define CB	CT[1]
static int stepno, step_freed_b;
static void (*scenario)(void);

static void
wait_hook(void)
{
	inferno_thread_t *me = inferno_coop_current();

	stepno++;
	scenario();
	inferno_coop_select(me);
}

/*
 *  ctx 0 holds A (the lowest address); ctx 1 holds B and asks for A: B is released
 *  and ctx 1 waits for A first, as the canonical order says; A is then handed over
 */
static void
sc_a_holder(void)
{
	inferno_coop_select(&cth[0]);
	if(stepno == 1){
		step_freed_b = inferno_token_try(&CB);	/* proves ctx 1 released B */
		if(step_freed_b)
			inferno_token_put(&CB);
		inferno_token_put(&CA);
	}
}

static void
t_coop_pause(void)
{
	int r;

	inferno_token_init(&CA);
	inferno_token_init(&CB);
	inferno_coop_select(&cth[0]);
	CHECK(inferno_token_get(&CA) == 0);
	inferno_coop_select(&cth[1]);
	CHECK(inferno_token_get(&CB) == 0);
	scenario = sc_a_holder;
	inferno_coop_wait = wait_hook;
	stepno = 0;
	r = inferno_token_get(&CA);		/* ctx 1 waits: pause point */
	inferno_coop_wait = NULL;
	CHECK(r == 1);
	CHECK(step_freed_b == 1);
	CHECK(inferno_coop_current() == &cth[1]);
	CHECK(inferno_token_held(&CA) && inferno_token_held(&CB));
	inferno_token_put(&CA);
	inferno_token_put(&CB);
	inferno_coop_select(&cth[0]);
	CHECK(!inferno_token_held(&CA));
	inferno_coop_select(&cth[0]);
}

/* the same with a mutex taken by ctx 0 and released by ctx 1: no owner */
static inferno_mutex_t CM;

static void
sc_unlock_other(void)
{
	inferno_coop_select(&cth[1]);		/* not the one that took it */
	inferno_mutex_unlock(&CM);
}

static void
t_coop_mutex_noowner(void)
{
	inferno_mutex_init(&CM, 0);
	inferno_coop_select(&cth[0]);
	CHECK(inferno_mutex_lock(&CM) == 0);
	scenario = sc_unlock_other;
	inferno_coop_wait = wait_hook;
	stepno = 0;
	CHECK(inferno_mutex_lock(&CM) == 1);	/* ctx 0 again: waits, ctx 1 releases */
	inferno_coop_wait = NULL;
	inferno_mutex_unlock(&CM);
	inferno_coop_select(&cth[0]);
}

/* the seqlock asks for a retry when a write happened in the read section */
static void
t_coop_seqlock_retry(void)
{
	inferno_seqlock_t l;
	unsigned s;
	int v = 1, copy, tries = 0;

	inferno_seqlock_init(&l);
	do{
		s = inferno_seqlock_read_begin(&l);
		copy = v;
		if(tries++ == 0){			/* another context writes now */
			inferno_seqlock_write_begin(&l);
			v = 2;
			inferno_seqlock_write_end(&l);
		}
	}while(inferno_seqlock_read_retry(&l, s));
	CHECK(tries == 2 && copy == 2);
}

/* with no hook, a wait is a deadlock and must be reported */
static void
t_coop_deadlock_aborts(void)
{
	pid_t pid;
	int st;
	inferno_mutex_t m;

	pid = fork();
	if(pid == 0){
		if(freopen("/dev/null", "w", stderr) == NULL)
			_exit(1);
		inferno_mutex_init(&m, 0);
		inferno_mutex_lock(&m);
		inferno_mutex_lock(&m);		/* must abort */
		_exit(0);
	}
	waitpid(pid, &st, 0);
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
}

/* the wait of ctx 0 pauses: tokens come free, the others run, the wake comes from ctx 1 */
static inferno_token_t WT;
static int wsteps, w_freed;

static void
wake_scenario(void)
{
	inferno_coop_select(&cth[1]);
	wsteps++;
	if(wsteps == 2){
		w_freed = inferno_token_try(&WT);	/* ctx 0 released it */
		if(w_freed)
			inferno_token_put(&WT);
		inferno_wake(&cth[0]);
	}
}

static void
t_coop_wait(void)
{
	int r;

	inferno_coop_select(&cth[0]);
	inferno_thread_attach(&cth[0]);
	inferno_thread_attach(&cth[1]);
	inferno_coop_select(&cth[0]);
	inferno_token_init(&WT);
	inferno_token_get(&WT);
	scenario = wake_scenario;
	inferno_coop_wait = wait_hook;
	wsteps = 0;
	r = inferno_wait();
	inferno_coop_wait = NULL;
	CHECK(r == INFERNO_WAIT_WOKEN);
	CHECK(w_freed == 1);
	CHECK(inferno_token_held(&WT));
	inferno_token_put(&WT);
	CHECK(inferno_coop_current() == &cth[0]);
}

/* virtual time: a sleep with nothing to run takes the clock to its deadline; interrupts end it */
static void
intr_scenario(void)
{
	inferno_coop_select(&cth[1]);
	inferno_thread_interrupt(&cth[0]);
}

static void
t_coop_sleep(void)
{
	long t0;

	inferno_coop_select(&cth[0]);
	t0 = inferno_coop_now();
	CHECK(inferno_sleep(100) == INFERNO_WAIT_TIMEOUT);
	CHECK(inferno_coop_now() - t0 == 100);
	scenario = intr_scenario;
	inferno_coop_wait = wait_hook;
	CHECK(inferno_sleep(100000) == INFERNO_WAIT_INTR);
	inferno_coop_wait = NULL;
	CHECK(inferno_coop_now() - t0 < 1000);
	inferno_coop_select(&cth[0]);
}

/* descriptors: readiness is decided by a hook of the test */
static int fdcalls;

static int
fakefd(int fd, int events)
{
	(void)fd;
	return ++fdcalls >= 3? events: 0;
}

static void
nop_scenario(void)
{
	inferno_coop_select(&cth[1]);
}

static void
t_coop_fd(void)
{
	int ready = 0;

	inferno_coop_select(&cth[0]);
	inferno_coop_fdready = fakefd;
	scenario = nop_scenario;
	inferno_coop_wait = wait_hook;
	fdcalls = 0;
	CHECK(inferno_wait_fd(7, INFERNO_FD_READ, -1, &ready) == INFERNO_WAIT_READY);
	CHECK(ready == INFERNO_FD_READ && fdcalls == 3);
	fdcalls = -1000;
	CHECK(inferno_wait_fd(7, INFERNO_FD_READ, 5, &ready) == INFERNO_WAIT_TIMEOUT);
	inferno_coop_wait = NULL;
	inferno_coop_fdready = NULL;
	inferno_coop_select(&cth[0]);
}

#endif /* BACKEND_COOP */

int
main(void)
{
	int b;

	inferno_thread_init(&mainth);
	inferno_thread_attach(&mainth);
#ifdef BACKEND_COOP
	inferno_thread_init(&cth[0]);
	inferno_thread_init(&cth[1]);
#endif

#define RUN(name, call) do { b = failed; call; report(name, b); } while(0)
	RUN("token", t_token());
	RUN("debug", t_debug());
	RUN("mutex", t_mutex());
	RUN("seqlock", t_seqlock());
	RUN("ref", t_ref());
	RUN("wait-basic", t_wait_basic());
	RUN("attach-checked", t_attach_checked());
#ifdef BACKEND_COOP
	RUN("coop-pause", t_coop_pause());
	RUN("coop-mutex-noowner", t_coop_mutex_noowner());
	RUN("coop-seqlock-retry", t_coop_seqlock_retry());
	RUN("coop-deadlock-aborts", t_coop_deadlock_aborts());
	RUN("coop-wait", t_coop_wait());
	RUN("coop-sleep", t_coop_sleep());
	RUN("coop-fd", t_coop_fd());
#endif
#ifdef BACKEND_PTHREAD
	RUN("short-pause-aborts", t_short_pause_aborts());
	RUN("tokens-threads", t_tokens_threads(0));
	RUN("tokens-threads-forced-pause", t_tokens_threads(300));
	RUN("get-all-threads", t_get_all_threads());
	RUN("short-seqlock-threads", t_short_seq_threads());
	RUN("mutex-transfer", t_mutex_transfer());
	RUN("pause-releases-tokens", t_pause_releases());
	RUN("wait-pingpong", t_wait_pingpong());
	RUN("wait-fd-basic", t_wait_fd_basic());
	RUN("wait-fd-race", t_wait_fd_race());
	RUN("wait-releases-tokens", t_wait_releases_tokens());
	RUN("sleep-interrupted", t_sleep_interrupted());
#endif
	if(failed){
		printf("%d checks failed\n", failed);
		return 1;
	}
	printf("all lock tests passed\n");
	return 0;
}
