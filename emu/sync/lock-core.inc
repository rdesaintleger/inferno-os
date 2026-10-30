/* tokens and mutexes over the primitives of a backend; see lock-core.h */

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
heldidx(Ctx *c, Prim *p)
{
	int i;

	for(i = 0; i < c->n; i++)
		if(c->t[i] == p)
			return i;
	return -1;
}

static void
push(Ctx *c, Prim *p, unsigned seq)
{
	if(c->n >= INFERNO_TOKEN_MAX)
		die("too many tokens held");
	c->t[c->n] = p;
	c->seq[c->n] = seq;
	c->n++;
}

/* canonical order: by address */
static void
sortprims(Prim **v, unsigned *q, int n)
{
	int i, j;
	Prim *p;
	unsigned s;

	for(i = 1; i < n; i++){
		p = v[i];
		s = q[i];
		for(j = i; j > 0 && (unsigned long)v[j-1] > (unsigned long)p; j--){
			v[j] = v[j-1];
			q[j] = q[j-1];
		}
		v[j] = p;
		q[j] = s;
	}
}

static void
release_all(Ctx *c, inferno_token_saved_t *s)
{
	int i;
	Prim *p;

	s->n = c->n;
	for(i = 0; i < c->n; i++){
		s->t[i] = (inferno_token_t*)c->t[i];
		s->seq[i] = c->seq[i];
	}
	while(c->n > 0){
		p = c->t[--c->n];
		prim_put(p);
	}
}

/* take again what release_all took, plus ne more, in canonical order */
static void
retake(Ctx *c, inferno_token_saved_t *s, Prim **ex, unsigned *exq, int ne)
{
	Prim *v[INFERNO_TOKEN_MAX*2];
	unsigned q[INFERNO_TOKEN_MAX*2];
	int i, n;

	n = 0;
	for(i = 0; i < s->n; i++){
		v[n] = (Prim*)s->t[i];
		q[n++] = s->seq[i];
	}
	for(i = 0; i < ne; i++){
		v[n] = ex[i];
		q[n++] = exq[i];
	}
	if(n > INFERNO_TOKEN_MAX)
		die("too many tokens held");
	sortprims(v, q, n);
	for(i = 0; i < n; i++){
		prim_wait(v[i]);
		push(c, v[i], q[i]);
	}
}

int
inferno_token_init(inferno_token_t *t)
{
	Prim *p = (Prim*)t;

	memset(p, 0, sizeof *p);
	p->flags = F_TOKEN;
	return prim_init(p);
}

void
inferno_token_destroy(inferno_token_t *t)
{
	prim_destroy((Prim*)t);
}

int
inferno_token_held(inferno_token_t *t)
{
	return heldidx(self(), (Prim*)t) >= 0;
}

int
inferno_token_get(inferno_token_t *t)
{
	Prim *p = (Prim*)t;
	Ctx *c = self();
	inferno_token_saved_t s;
	unsigned q;

	if(heldidx(c, p) >= 0)
		die("token already held");
	if(!forced(c) && prim_try(p)){
		push(c, p, c->nextseq++);
		return 0;
	}
	if(c->nshort)
		die("token get would pause with a short mutex held");
	release_all(c, &s);
	q = c->nextseq++;
	retake(c, &s, &p, &q, 1);
	return 1;
}

int
inferno_token_try(inferno_token_t *t)
{
	Prim *p = (Prim*)t;
	Ctx *c = self();

	if(heldidx(c, p) >= 0)
		die("token already held");
	if(!prim_try(p))
		return 0;
	push(c, p, c->nextseq++);
	return 1;
}

void
inferno_token_put(inferno_token_t *t)
{
	Prim *p = (Prim*)t;
	Ctx *c = self();
	int i;

	i = heldidx(c, p);
	if(i < 0)
		die("put of a token not held");
	for(c->n--; i < c->n; i++){
		c->t[i] = c->t[i+1];
		c->seq[i] = c->seq[i+1];
	}
	prim_put(p);
}

int
inferno_token_get_all(int n, inferno_token_t **tk)
{
	Prim *v[INFERNO_TOKEN_MAX];
	unsigned q[INFERNO_TOKEN_MAX];
	Ctx *c = self();
	inferno_token_saved_t s;
	int i, j, m;

	m = 0;
	for(i = 0; i < n; i++){
		for(j = 0; j < m; j++)
			if(v[j] == (Prim*)tk[i])
				break;
		if(j < m)
			continue;		/* duplicate */
		if(heldidx(c, (Prim*)tk[i]) >= 0)
			die("token already held");
		if(m >= INFERNO_TOKEN_MAX)
			die("too many tokens held");
		v[m] = (Prim*)tk[i];
		q[m++] = c->nextseq++;
	}
	sortprims(v, q, m);
	if(c->n == 0){			/* nothing held, so nothing to lose */
		for(i = 0; i < m; i++){
			prim_wait(v[i]);
			push(c, v[i], q[i]);
		}
		return 0;
	}
	if(!forced(c)){
		for(i = 0; i < m; i++)
			if(!prim_try(v[i]))
				break;
		if(i == m){
			for(i = 0; i < m; i++)
				push(c, v[i], q[i]);
			return 0;
		}
		while(--i >= 0)
			prim_put(v[i]);
	}
	if(c->nshort)
		die("token get would pause with a short mutex held");
	release_all(c, &s);
	retake(c, &s, v, q, m);
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
	int i;

	for(i = c->n - 1; i >= 0; i--)
		if((int)(c->seq[i] - mark) >= 0)
			inferno_token_put((inferno_token_t*)c->t[i]);
}

void
inferno_token_suspend(inferno_token_saved_t *s)
{
	Ctx *c = self();

	if(c->nshort)
		die("suspend with a short mutex held");
	release_all(c, s);
}

void
inferno_token_resume(inferno_token_saved_t *s)
{
	retake(self(), s, 0, 0, 0);
	s->n = 0;
}

int
inferno_mutex_init(inferno_mutex_t *m, int flags)
{
	Prim *p = (Prim*)m;

	memset(p, 0, sizeof *p);
	p->flags = flags & INFERNO_MUTEX_SHORT;
	return prim_init(p);
}

void
inferno_mutex_destroy(inferno_mutex_t *m)
{
	prim_destroy((Prim*)m);
}

int
inferno_mutex_lock(inferno_mutex_t *m)
{
	Prim *p = (Prim*)m;
	Ctx *c = self();
	inferno_token_saved_t s;

	if(p->flags & INFERNO_MUTEX_SHORT){
		if(!prim_try(p))
			prim_wait(p);	/* its holder never pauses: bounded wait, tokens kept */
		c->nshort++;
		return 0;
	}
	if(!forced(c) && prim_try(p))
		return 0;
	if(c->nshort)
		die("mutex lock would pause with a short mutex held");
	release_all(c, &s);
	prim_wait(p);
	retake(c, &s, 0, 0, 0);
	return 1;
}

int
inferno_mutex_trylock(inferno_mutex_t *m)
{
	Prim *p = (Prim*)m;

	if(!prim_try(p))
		return 0;
	if(p->flags & INFERNO_MUTEX_SHORT)
		self()->nshort++;
	return 1;
}

void
inferno_mutex_unlock(inferno_mutex_t *m)
{
	Prim *p = (Prim*)m;
	Ctx *c;

	if(p->flags & INFERNO_MUTEX_SHORT){
		c = self();
		if(c->nshort <= 0)
			die("short mutex released by a thread that does not hold one");
		c->nshort--;
	}
	prim_put(p);
}
