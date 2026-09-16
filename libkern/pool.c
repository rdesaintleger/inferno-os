#include <inferno/memprof.h>
#include <inferno/pool.h>

#include <inferno/protos/lib9.h>

#include <stdlib.h>
#include <string.h>

char* poolname(Pool* p) {
	return p->name;
}

Bhdr* poolchain(Pool* p) {
	return p->chain;
}

/* defined in alloc.c */
extern void* poolalloc(Pool*, size_t);
extern void (*poolfault)(void *, char *);

/* defined in avlfree.c */
extern void pooladd(Pool*, Bhdr*, Bhdr*);
extern void pooldel(Pool*, Bhdr*);

static Bhdr* arenaappendalloc(Bhdr* b, Bhdr* lead, uint32_t flags) {
	/* reset block type */
	b->bh_magic = MAGIC_A;
	b->bha_lead = lead;

	/* clear memory if requested */
	if (flags & BF_MEMZERO) {
		void *v = &b->bha_data;
		size_t size = (size_t) ((uintptr_t) BHDR2CHKSUCC(b) - (uintptr_t) v);

		memset(&b->bha_data, 0, size);
	}

	return b;
}

static int arena_flags_match(Bhdr* lead, uint32_t flags) {
	return (lead->bh_magic & BFLAGS_MASK) == (flags & BFLAGS_MASK);
}

static Bhdr* chunksplitblock(Pool* p, Bhdr* q, Bhdr* lead, size_t size) {
	Bchk* t, *n;
	size_t frag;

	/* ensure future free block conversion will have its data member aligned */
	size = BALIGN_SIZE16(lead, q, size, offsetof(Bchk, bc_self.bha_data));

	/* assume that size < q->bh_size */
	frag = q->bh_size - size;

	/* ensure it will be able to fit a free (chunked) block */
	if (frag >= offsetof(Bchk, bc_self.bhf_data)) {
		/* take current next block */
		n = BHDR2CHKSUCC(q);

		/* split existing bloc */
		q->bh_size = size;
		t = BHDR2CHKSUCC(q);

		t->bc_self.bh_size = (size_t)((uintptr_t)n -(uintptr_t)&t->bc_self);

		t->bc_pred = q;
		n->bc_pred = &t->bc_self;

		pooladd(p, &t->bc_self, lead);
	}

	return q;
}

static Bhdr* chunkfindflagged(Bhdr* t, uint32_t flags) {
	Bhdr* c = t;

	do {
		if (arena_flags_match(c->bhf_lead, flags)) {
			return c;
		}
		c = c->bhf_succ;
	} while (c != t);

	return NULL;
}

static Bhdr* chunkallocfromfree(Pool* p, size_t size, uint32_t flags) {
	/*
	 * note that provided size in this fonction must include the overhead of the allocated block header (bhdr_a_overhead)
	 */
	Bhdr* q, * t, * m, * lead;
	size_t aligned;

	t = p->root;
	q = NULL;
	while (t) {
		if (t->bh_size == size) {
			m = chunkfindflagged(t, flags);

			if (m != NULL) {
				lead = m->bhf_lead;

				pooldel(p, m);

				return arenaappendalloc(m, lead, flags);
			}

			t = t->bhf_right;
			continue;
		}

		if (size < t->bh_size) {
			/* this may fit, check with requested header alignment */
			aligned = BALIGN_SIZE16(t->bhf_lead, t, size, offsetof(Bchk, bc_self.bha_data));

			if (aligned <= t->bh_size) {
				m = chunkfindflagged(t, flags);
				if (m != NULL) {
					q = m;
				}
			}
			t = t->bhf_left;
			continue;
		}

		t = t->bhf_right;
	}

	if (q == NULL) {
		return NULL;
	}

	lead = q->bhf_lead;

	pooldel(p, q);

	return arenaappendalloc(chunksplitblock(p, q, lead, size), lead, flags);
}

static Bhdr* chunkallocnewarena(size_t request, size_t limit, Bchk** free, uint32_t flags) {
	Bhdr* lead;
	Bchk* data, * trail;
	size_t leadsize, dataoffset, tailsize, alloc, minsize, size, overhead;

	*free = NULL;

	/* pad lead size so the first allocated block has its data aligned */
	dataoffset = offsetof(Bchk, bc_self.bha_data);

	/* compute real lead/trail size */
	leadsize = BALIGN_SIZE16(0, 0, offsetof(Bhdr, bhl_data), dataoffset);
	tailsize = offsetof(Bchk, bc_self.bh_data);

	/* arena headers overhead outside requested allocation */
	/* data offset not used here since its part of minsize Bhdr */
	overhead = leadsize + tailsize;

	/* mnimum allocation size must be able to fit a free Bhdr (not Bchk) */
	minsize = offsetof(Bhdr, bhf_data);
	size = minsize; /* default to minsize */

	if (request > size) {
		/* pad requested size (not needed for minsize) */
		size = BCEIL(request, BALIGN_SZ);
	}

	if (flags & BF_MAPPED) {
		/* ensure first data will be page aligned without header */
		overhead += dataoffset;

		/* mapping is requested, constraint requested size to pagesize */
		size = BCEIL(size, CHUNK_PAGESIZE);
	} else {
		/* original pool semantic : size is for Bhdr, alloc is for Bchk */
		overhead += offsetof(Bchk, bc_self);

		if (flags & BF_MAXIMIZE) {
			/* compute minimal allocation */
			alloc = overhead + minsize;

			if (alloc > limit) {
				/* cannot satisfy request */
				return NULL;
			}

			/* maximize arena allocation to given limit */
			/* assume limit is correctly rounded (no need to round minsize) */
			size = limit - overhead;

			if (size < request) {
				/* cannot satisfy request */
				return NULL;
			}
		}
	}

	alloc = overhead + size;

	if (alloc > limit) {
		/* request is too large, cannot allocate a new chunked arena */
		return NULL;
	}

	lead = (Bhdr*)malloc(alloc); // XXX to be changed with host malloc later

	if (lead == NULL) {
		return NULL;
	}

	lead->bh_size = leadsize;

	/* place trailer at the end */
	trail = (Bchk*)((uint8_t*)lead + (alloc - tailsize));

	/* retrieve free member according to lead size */
	data = BHDR2CHKSUCC(lead);

	/* compute chunk chain (preds and sizes) */
	data->bc_pred = lead;
	trail->bc_pred = &data->bc_self;

	data->bc_self.bh_size = (size_t)((uintptr_t)trail - (uintptr_t)&data->bc_self);
	trail->bc_self.bh_size = offsetof(Bhdr, bh_data);

	/* initialize leader */
	lead->bh_magic = MAGIC_L;
	lead->bhl_freecnt = 0;
	lead->bhl_mapoffset = 0;
	lead->bhl_walkers = NULL;
	lead->bhl_trail = &trail->bc_self;

	/* initialize trailer */
	trail->bc_self.bh_magic = MAGIC_E;

	/* set the free block pointer */
	*free = data;

	/* and return new leader */
	return lead;
}

int poolcompact(Pool* pool) {
	Bhdr* lead, * ptr, * end, * next;
	Bhdr* last;
	Bchk* chk;
	size_t compacted;

	Bwalk* walkers;

	if (pool->move == NULL) {
		return 0;
	}

	lead = pool->chain;
	compacted = 0;

	while ((lead != NULL) && (lead->bhl_freecnt <= 1)) {
		/* skip leader when it has only one freeblock */
		lead = lead->bhl_nextchain;
	}

	if (lead != NULL) {
		last = lead;
		ptr = &BHDR2CHKSUCC(last)->bc_self;
		end = ptr;
	}

	while (lead != NULL) {
		next = &BHDR2CHKSUCC(ptr)->bc_self;

		switch (BMAGIC(ptr)) {
		case MAGIC_A:
			if (ptr != end) {
				memmove(end, ptr, ptr->bh_size);

				/* update chk pred pointer */
				BHDR2BCHK(end)->bc_pred = last;

				walkers = lead->bhl_walkers;

				while (walkers != NULL) {
					if (ptr == walkers->bw_ptr) {
						walkers->bw_ptr = end;
					}

					walkers = walkers->bw_succ;
				}

				/* call the move callback */
				/* Note: ptr data may be invalid goal is to make underlying move aware of the data move offset */
				pool->move(BHDR2DATA(ptr), BHDR2DATA(end));
				compacted++;
			}

			last = end;
			end = &BHDR2CHKSUCC(end)->bc_self;
			break;
		case MAGIC_E:
			if (ptr != end) {
				chk = BHDR2BCHK(ptr);
				chk->bc_pred = end;

				/* assume that enough room is left for a free block (since there was a free bloc before) */
				BHDR2BCHK(end)->bc_pred = last;

				end->bh_size = (size_t)((uintptr_t)chk - (uintptr_t)end);

				pooladd(pool, end, lead);
			}

			do {
				lead = lead->bhl_nextchain;
			} while ((lead != NULL) && (lead->bhl_freecnt <= 1));

			if (lead != NULL) {
				last = lead;
				ptr = &BHDR2CHKSUCC(last)->bc_self;
				end = ptr;
			}
			continue;
		case MAGIC_F:
			pooldel(pool, ptr);
			break;
		default:
			break;
		}

		ptr = next;
	}

	return compacted > 0;
}

static int chunkgrowpool(Pool* p, size_t size, Bhdr** bp, uint32_t flags) {
	Bhdr* lead, * b;
	Bchk* data;
	size_t limit, alloc, chunk;

	*bp = NULL;

	if (p->maxsize > p->arenasize) {
		chunk = BCEIL(size, p->chunk);
		limit = p->maxsize - p->arenasize;

		/* unlock the pool during arena allocation */
		unlock(&p->l);

		lead = chunkallocnewarena(chunk, limit, &data, flags);

		if (lead == NULL && chunk != size) {
			/* try to allocate an arena which fits remaining space in pool (including headers) */
			lead = chunkallocnewarena(size, limit, &data, flags | BF_MAXIMIZE);
		}

		lock(&p->l);

		if (lead != NULL) {
			alloc = (size_t)((uintptr_t)BHDR2CHKSUCC(lead->bhl_trail) - (uintptr_t)lead);

			if ((p->maxsize - alloc) < p->arenasize) {
				/* pool size has changed, cancel this allocation and retry */
				free(lead); // XXX to be changed with host free later

				return 1;
			}

			p->nbrk++;
			p->arenasize += alloc;

			lead->bhl_nextchain = p->chain;
			lead->bhl_prevchain = NULL;

			if (p->chain != NULL) {
				p->chain->bhl_prevchain = lead;
			}

			p->chain = lead;

			b = arenaappendalloc(chunksplitblock(p, &data->bc_self, lead, size), lead, flags);
			*bp = b;
			return 0;
		}
	}

	if (poolcompact(p)) {
		return 1;
	}

	HOSTED_API(print)("arena %s too large: size %lud cursize %lud arenasize %lud maxsize %lud\n",
		p->name, size, p->cursize, p->arenasize, p->maxsize);

	return 0;
}

static int chunkalloc(Pool* p, size_t size, Bhdr** bp, uint32_t flags) {
	Bhdr* b;

	if (size < offsetof(Bhdr, bhf_data)) {
		/* apply minimal allocation size (size of free block header) */
		size = offsetof(Bhdr, bhf_data);
	}

	b = chunkallocfromfree(p, size, flags);
	*bp = b;

	return b != NULL ? 0 : chunkgrowpool(p, size, bp, flags);
}

void* dopoolalloc(Pool* p, size_t asize) {
	Bhdr* b;
	void* v;
	size_t osize, size;
	int retry, monitor;

	if (asize >= p->maxsize) { /* for sanity and to avoid overflow */
		return NULL;
	}

	size = asize;
	osize = size;

	/* add overhead for allocated block header */
	size += offsetof(Bhdr, bha_data);

	/*
	 * no need to pad size. underlying allocator ensure that allocated
	 * data pointer is alwayd aligned to 16 and will adjust size of freeblocks according to this
	 * also minimal allocation size is updated by underlying allocator.
	 */

	lock(&p->l);

	do {
		p->nalloc++;

		/* XXX need to handle flags (later) */
		retry = chunkalloc(p, size, &b, 0);
	} while (retry);

	if (b != NULL) {
		/* if b is not null, ignore retry */
		p->cursize += b->bh_size;
		if (p->cursize > p->hw) {
			p->hw = p->cursize;
		}

		v = BHDR2DATA(b);
	}

	monitor = p->monitor;
	unlock(&p->l);

	if (b == NULL) {
		return NULL;
	}

	if (monitor)
		memprof_notify(p->pnum, v, size);

	return v;
}

static void bh_merge(Bhdr* b, Bhdr* c) {
	Bchk* chk;

	/* merge c with b */
	chk = BHDR2CHKSUCC(c);

	/* compute new size using pointer arithmetic (+ bh_size not accurate) */
	b->bh_size = (size_t)((uintptr_t)chk - (uintptr_t)b);
	chk->bc_pred = b;
}

void poolfree(Pool* p, void* v) {
	Bhdr* b, * lead, * pred, * succ;
	Bchk* chk;

	int monitor;
	size_t size, alloc;

	DATA2BHDR(b, v, poolfault);

	lock(&p->l);

	/* capture values for profiling */
	monitor = p->monitor;
	size = b->bh_size;

	p->nfree++;
	p->cursize -= b->bh_size;

	/* save leader */
	lead = b->bha_lead;

	/* join forward */
	chk = BHDR2CHKSUCC(b);
	succ = &chk->bc_self;
	if (BMAGIC(succ) == MAGIC_F) {
		/* delete next block from free tree */
		pooldel(p, succ);

		bh_merge(b, succ); /* proceed with merge */
	}

	/* join backward */
	chk = BHDR2BCHK(b);
	pred = chk->bc_pred;
	if (BMAGIC(pred) == MAGIC_F) {
		/* delete pred block from free tree */
		pooldel(p, pred);

		/* update size of current block */
		bh_merge(pred, b);

		b = pred;
	}

	/* take the first block following lead */
	chk = BHDR2CHKSUCC(lead);

	/* check if we merged or free the first arena block */
	if (b == &chk->bc_self) {
		/* take the block following the freed one */
		chk = BHDR2CHKSUCC(b);

		/* check if it is the last block */
		if (lead->bhl_trail == &chk->bc_self) {
			/* no more usage of this arena, it can be freed */
			if (lead->bhl_prevchain != NULL) {
                lead->bhl_prevchain->bhl_nextchain = lead->bhl_nextchain;
            } else {
                p->chain = lead->bhl_nextchain;
			}

            if (lead->bhl_nextchain != NULL) {
                lead->bhl_nextchain->bhl_prevchain = lead->bhl_prevchain;
			}

			alloc = (size_t)((uintptr_t)BHDR2CHKSUCC(lead->bhl_trail) - (uintptr_t)lead);

			p->arenasize -= alloc;
			p->nbrk--;

			free(lead); // XXX to be changed with host free later

			b = NULL;
		}
	}

	if (b != NULL) {
		pooladd(p, b, lead);
	}

	unlock(&p->l);

	if (monitor) {
		memprof_notify(p->pnum | (1 << 8), v, size);
	}
}

static int pooltrygrowinplace(Pool* p, Bhdr* b, size_t size) {
	Bhdr* succ, * lead;
	Bchk* chk;
	size_t oldsize;

	chk = BHDR2CHKSUCC(b);
	succ = &chk->bc_self;

	/* is next block free ? */
	if (BMAGIC(succ) != MAGIC_F) {
		return 0;
	}

	/* has next block enough room ? */
	if (b->bh_size + succ->bh_size < size) {
		return 0;
	}

	oldsize = b->bh_size;
	lead = succ->bhf_lead;

	pooldel(p, succ);
	bh_merge(b, succ);

	b = chunksplitblock(p, b, lead, size);

	p->cursize += b->bh_size - oldsize;

	if (p->cursize > p->hw) {
		p->hw = p->cursize;
	}

	return 1;
}

void*
poolrealloc(Pool* p, void* v, size_t asize) {
	Bhdr* b;
	void* nv;
	size_t osize, size;

	/* for sanity and to avoid overflow */
	if (asize >= p->maxsize) {
		return NULL;
	}

	if (asize == 0) {
		poolfree(p, v);
		return NULL;
	}

	osize = 0;
	size = asize + offsetof(Bhdr, bha_data);

	if (v != NULL) {
		lock(&p->l);

		DATA2BHDR(b, v, poolfault);
		osize = b->bh_size - offsetof(Bhdr, bha_data);

		if (osize >= asize) {
			p->cursize -= b->bh_size; /* remove old size */
			b = chunksplitblock(p, b, b->bha_lead, size); /* shrink the current block */
			p->cursize += b->bh_size; /* consume new size */

			unlock(&p->l);
			return v;
		}

		if (pooltrygrowinplace(p, b, size)) {
			unlock(&p->l);
			return v;
		}

		unlock(&p->l);
	}

	nv = poolalloc(p, asize);

	if (nv != NULL && v != NULL) {
		memmove(nv, v, osize);
		poolfree(p, v);
	}

	return nv;
}

size_t poolmsize(Pool* p, void* v) {
	size_t size;
	Bhdr* b;

	if (v == NULL)
		return 0;

	DATA2BHDR(b, v, poolfault);
	size = b->bh_size - offsetof(Bhdr, bha_data);

	return size;
}

size_t poolmax(Pool* p) {
	size_t size, overhead;
	Bhdr* t;

	overhead = offsetof(Bhdr, bha_data);

	lock(&p->l);

	size = p->maxsize - p->cursize;
	t = p->root;

	if (t != NULL) {
		while (t->bhf_right != NULL) {
			t = t->bhf_right;
		}

		if (size < t->bh_size) {
			size = t->bh_size;
		}
	}

	unlock(&p->l);

	if (size >= overhead)
		size -= overhead;
	return size;
}

void poolsetcompact(Pool* p, void (*move)(void*, void*)) {
	p->move = move;
}

void bwalk_link(Bhdr* lead, Bwalk* w) {
	w->bw_pred = NULL;
	w->bw_succ = lead->bhl_walkers;

	if (w->bw_succ != NULL) {
		w->bw_succ->bw_pred = w;
	}

	lead->bhl_walkers = w;
}

void bwalk_unlink(Bhdr* lead, Bwalk* w) {
	if (w->bw_pred != NULL) {
		w->bw_pred->bw_succ = w->bw_succ;
	} else {
		lead->bhl_walkers = w->bw_succ;
	}

	if (w->bw_succ != NULL) {
		w->bw_succ->bw_pred = w->bw_pred;
	}
}
