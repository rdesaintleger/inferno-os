#ifndef _INFERNO_LOCK_CORE_INC_H_
#define _INFERNO_LOCK_CORE_INC_H_

#include "lock-core.h"

/*
 *  Tokens and mutexes over the primitives of a backend; see lock-core.h.
 *  Included once, at the end of the source of a backend: not a header for anyone else.
 *
 *  The tokens held by a thread are a list through the tokens (held_next, held_seq),
 *  the last taken first, which only their holder reads or writes.  A pause releases
 *  them all after writing {token, order of acquisition} in a save area, then takes
 *  them again in the canonical order (by address) and relists them in the order of
 *  acquisition.  The save area is enlarged before anything is released.
 */

#define TOKEN_WARN	1024	/* a thread that holds more tokens than this is suspect: said once */

void
inferno_lock_debug(unsigned seed, int permille)
{
	dbg_seed = seed;
	dbg_permille = permille;
}

static int
forced(Ctx *c)
{
	if(dbg_permille <= 0)
		return 0;
	c->rng = c->rng * 1664525u + 1013904223u;
	return (int)((c->rng >> 8) % 1000u) < dbg_permille;
}

static int
isheld(Ctx *c, inferno_token_t *t)
{
	inferno_token_t *x;

	for(x = c->held; x != NULL; x = x->held_next)
		if(x == t)
			return 1;
	return 0;
}

static void
push(Ctx *c, inferno_token_t *t, unsigned seq)
{
	t->held_next = c->held;
	t->held_seq = seq;
	c->held = t;
	if(++c->n == TOKEN_WARN)
		fprintf(stderr, "inferno_lock: thread %u holds %d tokens\n", c->id, c->n);
}

/* unlist, without releasing */
static void
unlist(Ctx *c, inferno_token_t *t)
{
	inferno_token_t **pp;

	for(pp = &c->held; *pp != NULL; pp = &(*pp)->held_next)
		if(*pp == t){
			*pp = t->held_next;
			c->n--;
			return;
		}
	die("put of a token not held");
}

static int
greater(struct inferno_saveent *a, struct inferno_saveent *b, int byseq)
{
	if(byseq)
		return (int)(a->seq - b->seq) > 0;
	return (uintptr_t)a->t > (uintptr_t)b->t;
}

/* canonical order: by address; or, to relist, by order of acquisition */
static void
sortents(struct inferno_saveent *e, int n, int byseq)
{
	struct inferno_saveent x;
	int gap, i, j;

	for(gap = n / 2; gap > 0; gap /= 2)
		for(i = gap; i < n; i++){
			x = e[i];
			for(j = i; j >= gap && greater(&e[j-gap], &x, byseq); j -= gap)
				e[j] = e[j-gap];
			e[j] = x;
		}
}

/* a pause, or a token taken, where it is forbidden */
static void
pausecheck(Ctx *c)
{
	if(c->nshort)
		die("pause with a short mutex held");
	if(c->nogo)
		die("pause in a zone without tokens");
}

/* room for need entries in the save area, BEFORE the tokens are released */
static struct inferno_saveent*
saveroom(Ctx *c, int need)
{
	struct inferno_saveent *e;
	int cap;

	if(need <= c->savecap)
		return c->save;
	cap = c->savecap * 2;
	if(cap < need)
		cap = need;
	e = lock_alloc != NULL? lock_alloc(cap * sizeof *e): malloc(cap * sizeof *e);
	if(e == NULL)
		die("out of memory for the save area of the tokens");
	ctx_free(c);
	c->save = e;
	c->savecap = cap;
	return e;
}

/* release all the tokens into e (room for c->n entries); returns how many */
static int
release_all(Ctx *c, struct inferno_saveent *e)
{
	inferno_token_t *t;
	int n;

	n = 0;
	while((t = c->held) != NULL){
		c->held = t->held_next;		/* before it is free for someone else */
		c->n--;
		e[n].t = t;
		e[n].seq = t->held_seq;
		n++;
		prim_put(&t->p);
	}
	return n;
}

/* take the n entries of e again, in canonical order, and relist them in order of acquisition */
static void
retake(Ctx *c, struct inferno_saveent *e, int n)
{
	int i;

	sortents(e, n, 0);
	for(i = 0; i < n; i++)
		prim_wait(&e[i].t->p);
	sortents(e, n, 1);
	for(i = 0; i < n; i++)
		push(c, e[i].t, e[i].seq);
}

/* the pause of this layer: release everything, to be taken back by pause_end */
static int
pause_begin(Ctx *c, struct inferno_saveent **e)
{
	pausecheck(c);
	*e = saveroom(c, c->n);
	return release_all(c, *e);
}

static void
pause_end(Ctx *c, struct inferno_saveent *e, int n)
{
	retake(c, e, n);
}

int
inferno_token_init(inferno_token_t *t)
{
	Prim *p = &t->p;

	memset(t, 0, sizeof *t);
	p->flags = F_TOKEN;
	return prim_init(p);
}

void
inferno_token_destroy(inferno_token_t *t)
{
	prim_destroy(&t->p);
}

int
inferno_token_held(inferno_token_t *t)
{
	return isheld(self(), t);
}

static void
nogocheck(Ctx *c)
{
	if(c->nogo)
		die("token taken in a zone without tokens");
}

int
inferno_token_get(inferno_token_t *t)
{
	Ctx *c = self();
	struct inferno_saveent *e;
	int n;

	nogocheck(c);
	if(isheld(c, t))
		die("token already held");
	if(!forced(c) && prim_try(&t->p)){
		push(c, t, c->nextseq++);
		return 0;
	}
	pausecheck(c);
	e = saveroom(c, c->n + 1);
	n = release_all(c, e);
	e[n].t = t;
	e[n].seq = c->nextseq++;
	retake(c, e, n + 1);
	return 1;
}

int
inferno_token_try(inferno_token_t *t)
{
	Ctx *c = self();

	nogocheck(c);
	if(isheld(c, t))
		die("token already held");
	if(!prim_try(&t->p))
		return 0;
	push(c, t, c->nextseq++);
	return 1;
}

void
inferno_token_put(inferno_token_t *t)
{
	Ctx *c = self();

	unlist(c, t);
	prim_put(&t->p);
}

int
inferno_token_get_all(int n, inferno_token_t **tk)
{
	Ctx *c = self();
	struct inferno_saveent *e;
	int i, m, k;

	nogocheck(c);
	e = saveroom(c, c->n + n);
	for(i = 0; i < n; i++){
		if(isheld(c, tk[i]))
			die("token already held");
		e[i].t = tk[i];
		e[i].seq = 0;
	}
	sortents(e, n, 0);
	m = 0;				/* drop the duplicates, now next to each other */
	for(i = 0; i < n; i++)
		if(m == 0 || e[m-1].t != e[i].t)
			e[m++] = e[i];
	for(i = 0; i < m; i++)
		e[i].seq = c->nextseq++;
	if(c->n == 0){			/* nothing held, so nothing to lose */
		for(i = 0; i < m; i++)
			prim_wait(&e[i].t->p);
		for(i = 0; i < m; i++)
			push(c, e[i].t, e[i].seq);
		return 0;
	}
	if(!forced(c)){
		for(i = 0; i < m; i++)
			if(!prim_try(&e[i].t->p))
				break;
		if(i == m){
			for(i = 0; i < m; i++)
				push(c, e[i].t, e[i].seq);
			return 0;
		}
		while(--i >= 0)
			prim_put(&e[i].t->p);
	}
	pausecheck(c);
	k = release_all(c, e + m);
	retake(c, e, m + k);
	return 1;
}

unsigned
inferno_token_mark(void)
{
	return self()->nextseq;
}

void
inferno_token_release_to(unsigned mark)
{
	Ctx *c = self();

	while(c->held != NULL && (int)(c->held->held_seq - mark) >= 0)
		inferno_token_put(c->held);
}

size_t
inferno_token_save_size(void)
{
	int n = self()->n;

	return offsetof(struct inferno_token_save, e) + (n > 0? n: 1) * sizeof(struct inferno_saveent);
}

void
inferno_token_suspend(inferno_token_save_t *s)
{
	Ctx *c = self();

	pausecheck(c);
	s->n = release_all(c, s->e);
}

void
inferno_token_resume(inferno_token_save_t *s)
{
	retake(self(), s->e, s->n);
	s->n = 0;
}

void
inferno_token_nogo_begin(void)
{
	self()->nogo++;
}

void
inferno_token_nogo_end(void)
{
	Ctx *c = self();

	if(c->nogo <= 0)
		die("nogo_end without nogo_begin");
	c->nogo--;
}

int
inferno_mutex_init(inferno_mutex_t *m, int flags)
{
	Prim *p = &m->p;

	memset(p, 0, sizeof *p);
	p->flags = flags & INFERNO_MUTEX_SHORT;
	return prim_init(p);
}

void
inferno_mutex_destroy(inferno_mutex_t *m)
{
	prim_destroy(&m->p);
}

int
inferno_mutex_lock(inferno_mutex_t *m)
{
	Prim *p = &m->p;
	Ctx *c = self();
	struct inferno_saveent *e;
	int n;

	if(p->flags & INFERNO_MUTEX_SHORT){
		if(!prim_try(p))
			prim_wait(p);	/* its holder never pauses: bounded wait, tokens kept */
		c->nshort++;
		return 0;
	}
	if(!forced(c) && prim_try(p))
		return 0;
	n = pause_begin(c, &e);
	prim_wait(p);
	pause_end(c, e, n);
	return 1;
}

int
inferno_mutex_trylock(inferno_mutex_t *m)
{
	Prim *p = &m->p;

	if(!prim_try(p))
		return 0;
	if(p->flags & INFERNO_MUTEX_SHORT)
		self()->nshort++;
	return 1;
}

void
inferno_mutex_unlock(inferno_mutex_t *m)
{
	Prim *p = &m->p;
	Ctx *c;

	if(p->flags & INFERNO_MUTEX_SHORT){
		c = self();
		if(c->nshort <= 0)
			die("short mutex released by a thread that does not hold one");
		c->nshort--;
	}
	prim_put(p);
}

#endif /* _INFERNO_LOCK_CORE_INC_H_ */
