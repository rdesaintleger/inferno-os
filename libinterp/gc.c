#include <inferno/heapprof.h>

#include "lib9.h"
#include "interp.h"
#include "pool.h"

enum {
	Quanta = 50,		/* Allocated blocks to sweep each time slice usually */
	MaxQuanta = 15 * Quanta,
	PTRHASH = (1 << 5)
};

static int quanta = Quanta;
static int gce, gct = 1;

int	nprop;
int	gchalt;
int	mutator = 0;
int	gccolor = 3;

uint32_t	gcnruns;
uint32_t	gcsweeps;
uint32_t	gcbroken;
uint32_t	gchalted;
uint32_t	gcepochs;
uint64_t	gcdestroys;
uint64_t	gcinspects;

static	int	marker = 1;
static	int	sweeper = 2;
static Bhdr* base = nil;
static Bhdr* sptr = nil;

static Bhdr* limit;
static Bhdr* ptr;

static	int	visit;
extern	Pool* heapmem;
static Heap* ptrs = NULL;

void
ptradd(Heap* v) {
	v->gc_succ = ptrs;
	v->gc_pred = NULL;

	if (ptrs != NULL) {
		ptrs->gc_pred = v;
	}

	ptrs = v;
}

void
ptrdel(Heap* v) {
	/*
	 * assume that provided Heap is in ptr list.
	 */
	if (ptrs == v) {
		ptrs = v->gc_succ;
	}

	if (v->gc_succ != NULL) {
		v->gc_succ->gc_pred = v->gc_pred;
	}

	if (v->gc_pred != NULL) {
		v->gc_pred->gc_succ = v->gc_succ;
	}
}

static void
ptrmark(void) {
	for (Heap* h = ptrs; h != NULL; h = h->gc_succ) {
		Setmark(h);
	}
}

void
noptrs(Type* t, void* vw) {
	USED(t);
	USED(vw);
}

static int markdepth;

/* code simpler with a depth search compared to a width search*/
void
markheap(Type* t, void* vw) {
	Heap* h;
	uchar* p;
	int i, c, m;
	WORD** w, ** q;
	Type* t1;

	if (t == nil || t->np == 0)
		return;

	markdepth++;
	w = (WORD**)vw;
	p = t->map;
	for (i = 0; i < t->np; i++) {
		c = *p++;
		if (c != 0) {
			q = w;
			for (m = 0x80; m != 0; m >>= 1) {
				if ((c & m) && *q != H) {
					h = D2H(*q);
					Setmark(h);
					if (h->color == propagator && --visit >= 0 && markdepth < 64) {
						gce--;
						h->color = mutator;
						if ((t1 = h->t) != nil)
							t1->mark(t1, H2D(void*, h));
					}
				}
				q++;
			}
		}
		w += 8;
	}
	markdepth--;
}

/*
 * This routine should be modified to be incremental, but how?
 */
void
markarray(Type* t, void* vw) {
	int i;
	Heap* h;
	uchar* v;
	Array* a;

	USED(t);

	a = vw;
	t = a->t;
	if (a->root != H) {
		h = D2H(a->root);
		Setmark(h);
	}

	if (t->np == 0)
		return;

	v = a->data;
	for (i = 0; i < a->len; i++) {
		markheap(t, v);
		v += t->size;
	}
	visit -= a->len;
}

void
marklist(Type* t, void* vw) {
	List* l;
	Heap* h;

	USED(t);
	l = vw;
	markheap(l->t, l->data);
	while (visit > 0) {
		l = l->tail;
		if (l == H)
			return;
		h = D2H(l);
		Setmark(h);
		markheap(l->t, l->data);
		visit--;
	}
	l = l->tail;
	if (l != H) {
		D2H(l)->color = propagator;
		nprop = 1;
	}
}

static void
rootset(Prog* root) {
	Heap* h;
	Type* t;
	Frame* f;
	Module* m;
	Stkext* sx;
	Modlink* ml;
	uchar* fp, * sp, * ex, * mp;

	mutator = gccolor % 3;
	marker = (gccolor - 1) % 3;
	sweeper = (gccolor - 2) % 3;

	while (root != nil) {
		ml = root->R.M;
		h = D2H(ml);
		Setmark(h);
		mp = ml->MP;
		if (mp != H) {
			h = D2H(mp);
			Setmark(h);
		}

		sp = root->R.SP;
		ex = root->R.EX;
		while (ex != nil) {
			sx = (Stkext*)ex;
			fp = sx->reg.tos.fu;
			while (fp != sp) {
				f = (Frame*)fp;
				t = f->t;
				if (t == nil)
					t = sx->reg.TR;
				fp += t->size;
				t->mark(t, f);
				ml = f->mr;
				if (ml != nil) {
					h = D2H(ml);
					Setmark(h);
					mp = ml->MP;
					if (mp != H) {
						h = D2H(mp);
						Setmark(h);
					}
				}
			}
			ex = sx->reg.EX;
			sp = sx->reg.SP;
		}

		root = root->next;
	}

	for (m = modules; m != nil; m = m->link) {
		if (m->origmp != H) {
			h = D2H(m->origmp);
			Setmark(h);
		}
	}

	ptrmark();
}

static int
okbhdr(Bhdr* b) {
	if (b == nil)
		return 0;
	switch (b->bh_magic) {
	case MAGIC_A:
	case MAGIC_F:
	case MAGIC_L:
	case MAGIC_E:
	case MAGIC_I:
		return 1;
	}
	return 0;
}

/* XXX from heap.c, need to make a shared function*/
static void
heapfree(Heap* h) {
	void* d = D2P(H2D(void*, h));

	poolfree(heapmem, d);
	HOSTED_API(free)(h);
}

void
rungc(Prog* p) {
	Type* t;
	Heap* h;
	Bhdr* b;

	Heap* freehead; /* head of free list */
	Heap** freetail; /* tail of free list */
	void* d; /* temporary heap data pointer */

	gcnruns++;
	if (gchalt) {
		gchalted++;
		return;
	}

	if (base == nil) {
		gcsweeps++;
		b = poolchain(heapmem);
		base = b;
		ptr = b;
		limit = B2LIMIT(b);
	} else if (sptr != nil) {
		/* we stopped on allocated data, restore heap ref count */
		ptr = sptr;
		sptr = nil;
		d = P2D(B2D(ptr)); /* retrieve the real data pointer (in heapmem) */
		h = D2H(d); /* retrieve the heap pointer (in mainmem) */

		h->ref--;

		if (h->ref == 0) {
			/*
			 * gc is hybrid, meaning that if ref == 0, no-one except gc has a reference
			 * to this object. schedule object remove by forcing color
			 */
			h->color = sweeper;
		}
	}

	/* Chain broken ? */
	if (!okbhdr(ptr)) {
		base = nil;
		gcbroken++;
		return;
	}

	freehead = NULL;
	freetail = &freehead;

	for (visit = quanta; visit > 0; ) {
		if (ptr->bh_magic == MAGIC_A) {
			visit--;

			/*
			 * XXX suboptimal: use macro to retrieve real data pointer
			 * then convert this pointer to a Heap pointer
			 */
			d = P2D(B2D(ptr)); /* retrieve the real data pointer (in heapmem) */
			h = D2H(d); /* retrieve the heap pointer (in mainmem) */

			if (visit <= 0) {
				/* quanta has expired, stay on current Bhdr. Increment ref count to prevent bloc to be freed */
				sptr = ptr;
				h->ref++;
				break;
			}

			gct++;
			gcinspects++;
			t = h->t;
			if (h->color == propagator) {
				gce--;
				h->color = mutator;
				if (t != nil)
					t->mark(t, H2D(void*, h));
			} else
				if (h->color == sweeper) {
					/* make a queue of heap pointers to be freed */
					*freetail = h;
					h->ref++; /* ensure this object will not be freed outside this scan */
					h->gc_collect = NULL;
					freetail = &h->gc_collect;
				}
		}
		ptr = B2NB(ptr);
		if (ptr >= limit) {
			base = base->bh_link;
			if (base == nil)
				break;
			ptr = base;
			limit = B2LIMIT(base);
		}
	}

	while (freehead != NULL) {
		h = freehead;
		t = h->t;
		freehead = freehead->gc_collect;

		gce++;
		heapprof_notify(2, h, 0);
		if (t != nil) {
			gclock();
			t->free(h, 1);
			gcunlock();
			freetype(t);
		}
		gcdestroys++;
		heapfree(h);
	}

	quanta = (MaxQuanta + Quanta) / 2 + ((MaxQuanta - Quanta) / 20) * ((100 * gce) / gct);
	if (quanta < Quanta)
		quanta = Quanta;
	if (quanta > MaxQuanta)
		quanta = MaxQuanta;

	if (base != nil)		/* Completed this iteration ? */
		return;
	if (nprop == 0) {	/* Completed the epoch ? */
		gcepochs++;
		gccolor++;
		rootset(p);
		gce = 0;
		gct = 1;
		return;
	}
	nprop = 0;
}
