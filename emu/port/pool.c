#include <inferno/memprof.h>

#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "error.h"

const size_t btail_overhead = BCEIL(BTAIL_SIZE, BALIGN_SZ);
const size_t bhdr_a_overhead = BCEIL(BHDR_A_SIZE, BALIGN_SZ) + btail_overhead;
const size_t bhdr_f_overhead = BCEIL(BHDR_F_SIZE, BALIGN_SZ) + btail_overhead;
const size_t bhdr_l_overhead = BCEIL(BHDR_L_SIZE, BALIGN_SZ) + btail_overhead;
const size_t bhdr_e_overhead = BCEIL(BHDR_E_SIZE, BALIGN_SZ);
const size_t arena_overhead = bhdr_l_overhead + bhdr_e_overhead;

char*
poolname(Pool *p)
{
	return p->name;
}

Bhdr*
poolchain(Pool *p)
{
	return p->chain;
}

void
pooldel(Pool *p, Bhdr *t)
{
	Bhdr *s, *f, *rp, *q;

	if(t->bh_parent == nil && p->root != t) {
		t->bh_prev->bh_fwd = t->bh_fwd;
		t->bh_fwd->bh_prev = t->bh_prev;
		return;
	}

	if(t->bh_fwd != t) {
		f = t->bh_fwd;
		s = t->bh_parent;
		f->bh_parent = s;
		if(s == nil)
			p->root = f;
		else {
			if(s->bh_left == t)
				s->bh_left = f;
			else
				s->bh_right = f;
		}

		rp = t->bh_left;
		f->bh_left = rp;
		if(rp != nil)
			rp->bh_parent = f;
		rp = t->bh_right;
		f->bh_right = rp;
		if(rp != nil)
			rp->bh_parent = f;

		t->bh_prev->bh_fwd = t->bh_fwd;
		t->bh_fwd->bh_prev = t->bh_prev;
		return;
	}

	if(t->bh_left == nil)
		rp = t->bh_right;
	else {
		if(t->bh_right == nil)
			rp = t->bh_left;
		else {
			f = t;
			rp = t->bh_right;
			s = rp->bh_left;
			while(s != nil) {
				f = rp;
				rp = s;
				s = rp->bh_left;
			}
			if(f != t) {
				s = rp->bh_right;
				f->bh_left = s;
				if(s != nil)
					s->bh_parent = f;
				s = t->bh_right;
				rp->bh_right = s;
				if(s != nil)
					s->bh_parent = rp;
			}
			s = t->bh_left;
			rp->bh_left = s;
			s->bh_parent = rp;
		}
	}
	q = t->bh_parent;
	if(q == nil)
		p->root = rp;
	else {
		if(t == q->bh_left)
			q->bh_left = rp;
		else
			q->bh_right = rp;
	}
	if(rp != nil)
		rp->bh_parent = q;
}

void
pooladd(Pool *p, Bhdr *q)
{
	int size;
	Bhdr *tp, *t;

	q->bh_magic = MAGIC_F;

	q->bh_left = nil;
	q->bh_right = nil;
	q->bh_parent = nil;
	q->bh_fwd = q;
	q->bh_prev = q;

	t = p->root;
	if(t == nil) {
		p->root = q;
		return;
	}

	size = q->bh_size;

	tp = nil;
	while(t != nil) {
		if(size == t->bh_size) {
			q->bh_prev = t->bh_prev;
			q->bh_prev->bh_fwd = q;
			q->bh_fwd = t;
			t->bh_prev = q;
			return;
		}
		tp = t;
		if(size < t->bh_size)
			t = t->bh_left;
		else
			t = t->bh_right;
	}

	q->bh_parent = tp;
	if(size < tp->bh_size)
		tp->bh_left = q;
	else
		tp->bh_right = q;
}

void*
dopoolalloc(Pool *p, size_t asize)
{
	Bhdr *q, *t;
	size_t alloc, ns, frag;
	size_t osize, size;

	if(asize >= p->maxsize)	/* for sanity and to avoid overflow */
		return nil;
	size = asize;
	osize = size;
	size = BCEIL(size, BALIGN_SZ) + bhdr_a_overhead;

	if (size < bhdr_f_overhead) {
		/* each 'allocated' block must be able to be mutated to 'free' */
		size = bhdr_f_overhead;
	}

	lock(&p->l);
	p->nalloc++;

	t = p->root;
	q = nil;
	while(t) {
		if(t->bh_size == size) {
			t = t->bh_fwd;
			pooldel(p, t);
			t->bh_magic = MAGIC_A;
			p->cursize += t->bh_size;
			if(p->cursize > p->hw)
				p->hw = p->cursize;
			unlock(&p->l);
			if(p->monitor)
				memprof_notify(p->pnum, B2D(t), size);
			return B2D(t);
		}
		if(size < t->bh_size) {
			q = t;
			t = t->bh_left;
		}
		else
			t = t->bh_right;
	}
	if(q != nil) {
		pooldel(p, q);
		q->bh_magic = MAGIC_A;
		frag = q->bh_size - size;
		if(frag < bhdr_f_overhead) {
			p->cursize += q->bh_size;
			if(p->cursize > p->hw)
				p->hw = p->cursize;
			unlock(&p->l);
			if(p->monitor)
				memprof_notify(p->pnum, B2D(q), size);
			return B2D(q);
		}
		/* Split */
		ns = q->bh_size - size;
		q->bh_size = size;
		B2T(q)->bt_hdr = q;
		t = B2NB(q);
		t->bh_size = ns;
		B2T(t)->bt_hdr = t;
		pooladd(p, t);
		p->cursize += q->bh_size;
		if(p->cursize > p->hw)
			p->hw = p->cursize;
		unlock(&p->l);
		if(p->monitor)
			memprof_notify(p->pnum, B2D(q), size);
		return B2D(q);
	}

	ns = p->chunk;
	if(size > ns)
		ns = BCEIL(size, ns);

	alloc = ns+arena_overhead;
	p->arenasize += alloc;
	if(p->arenasize > p->maxsize) {
		p->arenasize -= alloc;

		/* compute remaining space (paranoid) */
		ns = p->arenasize > p->maxsize ? 0 : p->maxsize-p->arenasize;

		/* substract overhead and compute proper alignment */
		ns = ns < arena_overhead ? 0 : BFLOOR(ns - arena_overhead, BALIGN_SZ);

		if (ns < size) {
			if(poolcompact(p)) {
				unlock(&p->l);
				return poolalloc(p, osize);
			}

			unlock(&p->l);
			HOSTED_API(print)("arena %s too large: size %d cursize %lud arenasize %lud maxsize %lud\n",
			 p->name, size, p->cursize, p->arenasize, p->maxsize);
			return nil;
		}

		alloc = ns+arena_overhead;
		p->arenasize += alloc;
	}

	p->nbrk++;
	t = (Bhdr *) malloc(alloc); // XXX to be changed with host malloc later
	if(t == NULL) {
		p->nbrk--;
		p->arenasize -= alloc;
		unlock(&p->l);
		return nil;
	}
	
	t->bh_magic = MAGIC_L;		/* Make a leader */
	t->bh_size = bhdr_l_overhead;
	t->bh_limit = ns+t->bh_size;
	t->bh_link = p->chain;
	p->chain = t;
	B2T(t)->bt_hdr = t;
	t = B2NB(t);

	t->bh_magic = MAGIC_A;		/* Make the block we are going to return */
	t->bh_size = size;

	ns -= size;			/* Free the rest */

	if(ns >= bhdr_f_overhead) {
		B2T(t)->bt_hdr = t;

		q = B2NB(t);
		q->bh_size = ns;
		B2T(q)->bt_hdr = q;
		pooladd(p, q);
	} else {
		t->bh_size += ns;

		B2T(t)->bt_hdr = t;
		q = t;
	}

	B2NB(q)->bh_magic = MAGIC_E;	/* Mark the end of the chunk */

	p->cursize += t->bh_size;
	if(p->cursize > p->hw)
		p->hw = p->cursize;
	unlock(&p->l);
	if(p->monitor)
		memprof_notify(p->pnum, B2D(t), size);
	return B2D(t);
}

void *
poolalloc(Pool *p, ulong asize)
{
	Prog *prog;

	if(p->cursize > p->ressize && (prog = currun()) != nil && prog->flags&Prestricted)
		return nil;
	return dopoolalloc(p, asize);
}

void
poolfree(Pool *p, void *v)
{
	Bhdr *b, *c;
	extern Bhdr *ptr;

	D2B(b, v, poolfault);
	if(p->monitor)
		memprof_notify(p->pnum|(1<<8), v, b->bh_size);

	lock(&p->l);
	p->nfree++;
	p->cursize -= b->bh_size;
	c = B2NB(b);
	if(c->bh_magic == MAGIC_F) {	/* Join forward */
		if(c == ptr)
			ptr = b;
		pooldel(p, c);
		c->bh_magic = 0;
		b->bh_size += c->bh_size;
		B2T(b)->bt_hdr = b;
	}

	c = B2PT(b)->bt_hdr;
	if(c->bh_magic == MAGIC_F) {	/* Join backward */
		if(b == ptr)
			ptr = c;
		pooldel(p, c);
		b->bh_magic = 0;
		c->bh_size += b->bh_size;
		b = c;
		B2T(b)->bt_hdr = b;
	}
	pooladd(p, b);
	unlock(&p->l);
}

void *
poolrealloc(Pool *p, void *v, ulong size)
{
	Bhdr *b;
	void *nv;
	int osize;

	if(size >= p->maxsize)	/* for sanity and to avoid overflow */
		return nil;
	if(size == 0){
		poolfree(p, v);
		return nil;
	}
	SET(osize);
	if(v != nil){
		lock(&p->l);
		D2B(b, v, poolfault);
		osize = b->bh_size - bhdr_a_overhead;
		unlock(&p->l);
		if(osize >= size)
			return v;
	}
	nv = poolalloc(p, size);
	if(nv != nil && v != nil){
		memmove(nv, v, osize);
		poolfree(p, v);
	}
	return nv;
}

ulong
poolmsize(Pool *p, void *v)
{
	Bhdr *b;
	ulong size;

	if(v == nil)
		return 0;
	lock(&p->l);
	D2B(b, v, poolfault);
	size = b->bh_size - bhdr_a_overhead;
	unlock(&p->l);
	return size;
}

ulong
poolmax(Pool *p)
{
	Bhdr *t;
	ulong size;

	lock(&p->l);
	size = p->maxsize - p->cursize;
	t = p->root;
	if(t != nil) {
		while(t->bh_right != nil)
			t = t->bh_right;
		if(size < t->bh_size)
			size = t->bh_size;
	}
	if(size >= bhdr_a_overhead)
		size -= bhdr_a_overhead;
	unlock(&p->l);
	return size;
}

/*
void
pooldump(Pool *p)
{
	Bhdr *b, *base, *limit, *ptr;

	b = p->chain;
	if(b == nil)
		return;
	base = b;
	ptr = b;
	limit = B2LIMIT(b);

	while(base != nil) {
		HOSTED_API(print)("\tbase #%.8lux ptr #%.8lux", base, ptr);
		if(ptr->bh_magic == MAGIC_A || ptr->bh_magic == MAGIC_I)
			HOSTED_API(print)("\tA%.5d\n", ptr->bh_size);
		else if(ptr->bh_magic == MAGIC_L)
			HOSTED_API(print)("\tE\tL#%.8lux\tS#%.8lux\n", ptr->bh_link, ptr->bh_limit);
		else if(ptr->bh_magic == MAGIC_E)
			HOSTED_API(print)("\tE\tL#%.8lux\tS#%.8lux\n", nil, nil);
		else
			HOSTED_API(print)("\tF%.5d\tL#%.8lux\tR#%.8lux\tF#%.8lux\tP#%.8lux\tT#%.8lux\n",
				ptr->bh_size, ptr->bh_left, ptr->bh_right, ptr->bh_fwd, ptr->bh_prev, ptr->bh_parent);
		ptr = B2NB(ptr);
		if(ptr >= limit) {
			HOSTED_API(print)("link to #%.8lux\n", base->bh_link);
			base = base->bh_link;
			if(base == nil)
				break;
			ptr = base;
			limit = B2LIMIT(base);
		}
	}
}
*/

void
poolsetcompact(Pool *p, void (*move)(void*, void*))
{
	p->move = move;
}

int
poolcompact(Pool *pool)
{
	Bhdr *base, *limit, *ptr, *end, *next;
	int compacted, nb;

	if(pool->move == nil || pool->lastfree == pool->nfree)
		return 0;

	pool->lastfree = pool->nfree;

	base = pool->chain;
	ptr = B2NB(base);	/* First Block in arena has bh_link */
	limit = B2LIMIT(base);
	compacted = 0;

	pool->root = nil;
	end = ptr;
	while(base != nil) {
		next = B2NB(ptr);
		if(ptr->bh_magic == MAGIC_A || ptr->bh_magic == MAGIC_I) {
			if(ptr != end) {
				memmove(end, ptr, ptr->bh_size);
				pool->move(B2D(ptr), B2D(end));
				compacted = 1;
			}
			end = B2NB(end);
		}
		if(next >= limit) {
			nb = (uchar*)limit - (uchar*)end;
			if(nb > 0){
				if(nb < bhdr_f_overhead){
					HOSTED_API(print)("poolcompact: leftover too small\n");
					abort();
				}
				end->bh_size = nb;
				B2T(end)->bt_hdr = end;
				pooladd(pool, end);
			}
			base = base->bh_link;
			if(base == nil)
				break;
			ptr = B2NB(base);
			end = ptr;	/* could do better by copying between chains */
			limit = B2LIMIT(base);
		} else
			ptr = next;
	}

	return compacted;
}
