/*
 *  Cost of the primitives without contention:
 *	cc -O2 -DBACKEND_PTHREAD bench.c lock-pthread.c -lpthread
 */
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <inferno/lock.h>

#define N	10000000L

static double
now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}

#define BENCH(name, body) do {\
	double t0, t1; long i_;\
	t0 = now();\
	for(i_ = 0; i_ < N; i_++){ body; }\
	t1 = now();\
	printf("%-44s %6.1f ns\n", name, (t1 - t0) / N);\
} while(0)

int
main(void)
{
	pthread_mutex_t pm;
	inferno_mutex_t m, sm;
	inferno_token_t t, t2;
	inferno_seqlock_t sl;
	inferno_ref_t r;
	volatile int sink = 0;
	unsigned s;

	pthread_mutex_init(&pm, NULL);
	inferno_mutex_init(&m, 0);
	inferno_mutex_init(&sm, INFERNO_MUTEX_SHORT);
	inferno_token_init(&t);
	inferno_token_init(&t2);
	inferno_seqlock_init(&sl);
	inferno_ref_init(&r, 1);

	printf("uncontended, one thread, %ld iterations\n", N);
	BENCH("pthread_mutex_lock + unlock", pthread_mutex_lock(&pm); pthread_mutex_unlock(&pm));
	BENCH("inferno_mutex_lock + unlock", inferno_mutex_lock(&m); inferno_mutex_unlock(&m));
	BENCH("inferno_mutex (short) lock + unlock", inferno_mutex_lock(&sm); inferno_mutex_unlock(&sm));
	BENCH("inferno_token_get + put", inferno_token_get(&t); inferno_token_put(&t));
	inferno_token_get(&t2);
	BENCH("inferno_token_get + put, one other held", inferno_token_get(&t); inferno_token_put(&t));
	inferno_token_put(&t2);
	BENCH("inferno_seqlock read_begin + read_retry", s = inferno_seqlock_read_begin(&sl); sink += inferno_seqlock_read_retry(&sl, s));
	BENCH("inferno_seqlock write_begin + write_end", inferno_seqlock_write_begin(&sl); inferno_seqlock_write_end(&sl));
	BENCH("inferno_ref_inc", inferno_ref_inc(&r));
	return sink;
}
