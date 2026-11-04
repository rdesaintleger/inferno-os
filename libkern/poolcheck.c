/*
 * Paranoid pool mode: CRC of the block headers and walk of the arenas.
 * Everything here is compiled only when POOLPARANOID is defined (see
 * include/inferno/poolcheck.h).
 */
#include <inferno/poolcheck.h>

#ifdef POOLPARANOID

#include <inferno/pool.h>
#include <inferno/platform.h>
#include <inferno/protos/lib9.h>

#include <stdlib.h>
#include <string.h>

/* CRC-32 (IEEE 802.3, reflected), 4 bits at a time: small and without any initialization */
static const uint32_t crctab[16] = {
	0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
	0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
	0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t bhdr_crc_of(size_t size, uint32_t magic) {
	uint8_t buf[sizeof(size_t) + sizeof(uint32_t)];
	uint32_t crc = 0xffffffffu;
	size_t i;

	magic &= BMAGIC_MASK;
	memcpy(buf, &size, sizeof(size_t));
	memcpy(buf + sizeof(size_t), &magic, sizeof(uint32_t));

	for (i = 0; i < sizeof(buf); i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 15];
		crc = (crc >> 4) ^ crctab[crc & 15];
	}

	return ~crc;
}

uint32_t bhdr_crc(const Bhdr* b) {
	return bhdr_crc_of(b->bh_size, b->bh_magic);
}

/* ---------------------------------------------------------------------- *
 * walk
 * ---------------------------------------------------------------------- */

/* is ptr (a block header) inside an arena of the pool? The chain must have been validated. */
static int inarena(Pool* p, const void* ptr) {
	Bhdr* lead;

	for (lead = p->chain; lead != NULL; lead = lead->bhl_nextchain) {
		if ((uintptr_t)ptr > (uintptr_t)lead && (uintptr_t)ptr < (uintptr_t)lead->bhl_trail) {
			return 1;
		}
	}

	return 0;
}

#define FAULT(m, b, pv, o) \
	do { f->msg = (m); f->blk = (b); f->prev = (pv); f->lead = lead; f->other = (o); return 1; } while (0)

/* first pass: the chain of arenas. Returns 0 when it is safe to follow it. */
static int checkchain(Pool* p, Poolfault* f) {
	Bhdr* lead, * prevlead = NULL;
	int n = 0;

	for (lead = p->chain; lead != NULL; lead = lead->bhl_nextchain) {
		Bhdr* trail;

		if (++n > p->nbrk) {
			FAULT("chain has more arenas than the pool counts (nbrk)", lead, NULL, NULL);
		}
		if (BMAGIC(lead) != MAGIC_L) {
			FAULT("arena leader: bad magic", lead, NULL, NULL);
		}
		if (lead->bh_crc != bhdr_crc(lead)) {
			FAULT("arena leader: bad CRC", lead, NULL, NULL);
		}
		if (lead->bhl_prevchain != prevlead) {
			FAULT("arena leader: bad bhl_prevchain", lead, NULL, prevlead);
		}

		trail = lead->bhl_trail;
		if ((uintptr_t)trail <= (uintptr_t)lead || (uintptr_t)trail - (uintptr_t)lead > p->arenasize) {
			FAULT("arena leader: bhl_trail out of range", lead, NULL, trail);
		}

		prevlead = lead;
	}

	if (n != p->nbrk) {
		lead = NULL;
		FAULT("number of arenas differs from the pool counter (nbrk)", NULL, NULL, NULL);
	}

	return 0;
}

/* second pass: one arena, from the leader to the trailer */
static int checkarena(Pool* p, Bhdr* lead, Poolfault* f) {
	Bhdr* b, * prev, * trail, * sp, * pp;
	size_t nfree = 0, size;

	trail = lead->bhl_trail;
	prev = lead;
	b = &BHDR2CHKSUCC(lead)->bc_self;

	while (b != trail) {
		if ((uintptr_t)b <= (uintptr_t)prev || (uintptr_t)b >= (uintptr_t)trail) {
			FAULT("block address outside the arena (size of the previous block?)", prev, NULL, b);
		}

		if (b->bh_crc != bhdr_crc(b)) {
			FAULT("block header: bad CRC", b, prev, NULL);
		}

		if (BHDR2BCHK(b)->bc_pred != prev) {
			FAULT("Bchk: bc_pred is not the previous block", b, prev, BHDR2BCHK(b)->bc_pred);
		}

		size = b->bh_size;
		if (size < offsetof(Bhdr, bha_data) || size > (size_t)((uintptr_t)trail - (uintptr_t)b)) {
			FAULT("block header: bad size", b, prev, NULL);
		}

		switch (BMAGIC(b)) {
		case MAGIC_A:
			if (b->bha_lead != lead) {
				FAULT("allocated block: bad arena pointer", b, prev, b->bha_lead);
			}
			break;

		case MAGIC_F:
			nfree++;

			if (b->bhf_lead != lead) {
				FAULT("free block: bad arena pointer", b, prev, b->bhf_lead);
			}

			sp = b->bhf_succ;
			pp = b->bhf_pred;
			if (!inarena(p, sp) || BMAGIC(sp) != MAGIC_F) {
				FAULT("free block: bhf_succ is not a free block of the pool", b, prev, sp);
			}
			if (!inarena(p, pp) || BMAGIC(pp) != MAGIC_F) {
				FAULT("free block: bhf_pred is not a free block of the pool", b, prev, pp);
			}
			if (sp->bhf_pred != b) {
				FAULT("free block: bhf_succ->bhf_pred is not the block", b, prev, sp);
			}
			if (pp->bhf_succ != b) {
				FAULT("free block: bhf_pred->bhf_succ is not the block", b, prev, pp);
			}
			if (sp->bh_size != b->bh_size || pp->bh_size != b->bh_size) {
				FAULT("free block: duplicate list with different sizes", b, prev, sp);
			}

			if (b->bhf_left != NULL) {
				if (!inarena(p, b->bhf_left) || BMAGIC(b->bhf_left) != MAGIC_F) {
					FAULT("free block: bhf_left is not a free block of the pool", b, prev, b->bhf_left);
				}
				if (b->bhf_left->bhf_parent != b || b->bhf_left->bh_size > b->bh_size) {
					FAULT("free block: bad tree link (left)", b, prev, b->bhf_left);
				}
			}
			if (b->bhf_right != NULL) {
				if (!inarena(p, b->bhf_right) || BMAGIC(b->bhf_right) != MAGIC_F) {
					FAULT("free block: bhf_right is not a free block of the pool", b, prev, b->bhf_right);
				}
				if (b->bhf_right->bhf_parent != b || b->bhf_right->bh_size < b->bh_size) {
					FAULT("free block: bad tree link (right)", b, prev, b->bhf_right);
				}
			}
			if (b->bhf_parent != NULL) {
				Bhdr* t = b->bhf_parent;

				if (!inarena(p, t) || BMAGIC(t) != MAGIC_F) {
					FAULT("free block: bhf_parent is not a free block of the pool", b, prev, t);
				}
				if (t->bhf_left != b && t->bhf_right != b) {
					FAULT("free block: bhf_parent does not point back", b, prev, t);
				}
			}
			break;

		default:
			FAULT("block header: bad magic (MAGIC_L/MAGIC_E inside the arena?)", b, prev, NULL);
		}

		prev = b;
		b = &BHDR2CHKSUCC(b)->bc_self;
	}

	if (BMAGIC(trail) != MAGIC_E) {
		FAULT("arena trailer: bad magic", trail, prev, NULL);
	}
	if (trail->bh_crc != bhdr_crc(trail)) {
		FAULT("arena trailer: bad CRC", trail, prev, NULL);
	}
	if (BHDR2BCHK(trail)->bc_pred != prev) {
		FAULT("arena trailer: Bchk bc_pred is not the previous block", trail, prev, BHDR2BCHK(trail)->bc_pred);
	}
	if (nfree != lead->bhl_freecnt) {
		FAULT("arena leader: free counter (bhl_freecnt) differs from the free blocks found", lead, NULL, NULL);
	}

	return 0;
}

/* for gdb, when the faulting thread holds p->l: set poolcheck_nolock = 1 then call poolparanoid_now() */
int poolcheck_nolock;

int poolcheck(Pool* p, Poolfault* f) {
	Bhdr* lead = NULL;
	int bad;

	memset(f, 0, sizeof(*f));

	if (!poolcheck_nolock) {
		inferno_mutex_lock(&p->l);
	}

	bad = checkchain(p, f);

	if (!bad && p->root != NULL) {
		lead = NULL;
		if (!inarena(p, p->root) || BMAGIC(p->root) != MAGIC_F || p->root->bhf_parent != NULL) {
			f->msg = "pool root is not a free block without parent";
			f->blk = p->root;
			bad = 1;
		}
	}

	for (lead = p->chain; !bad && lead != NULL; lead = lead->bhl_nextchain) {
		bad = checkarena(p, lead, f);
	}

	if (!poolcheck_nolock) {
		inferno_mutex_unlock(&p->l);
	}

	return bad;
}

/* ---------------------------------------------------------------------- *
 * report
 * ---------------------------------------------------------------------- */

static void dumpwords(const char* what, const void* v, int n) {
	const uint32_t* w = (const uint32_t*)((uintptr_t)v & ~(uintptr_t)3);
	int i;

	HOSTED_API(print)("  %s %p:", what, (void*)w);
	for (i = 0; i < n; i++) {
		if (i && i % 8 == 0) {
			HOSTED_API(print)("\n    %p:", (void*)(w + i));
		}
		HOSTED_API(print)(" %.8lux", (ulong)w[i]);
	}
	HOSTED_API(print)("\n");
}

/* does flipping a single bit of the header (or of its crc) explain the CRC mismatch? */
static void bitflips(const Bhdr* b) {
	uint32_t stored = b->bh_crc, mag = b->bh_magic & BMAGIC_MASK;
	size_t size = b->bh_size;
	unsigned i;

	if (stored == bhdr_crc_of(size, mag)) {
		return;
	}
	for (i = 0; i < 8 * sizeof(size_t); i++) {
		if (bhdr_crc_of(size ^ ((size_t)1 << i), mag) == stored) {
			HOSTED_API(print)("  hint: a single bit flip in bh_size (bit %ud) explains the CRC\n", i);
			return;
		}
	}
	for (i = 4; i < 32; i++) {
		if (bhdr_crc_of(size, mag ^ ((uint32_t)1 << i)) == stored) {
			HOSTED_API(print)("  hint: a single bit flip in bh_magic (bit %ud) explains the CRC\n", i);
			return;
		}
	}
	{
		uint32_t x = stored ^ bhdr_crc_of(size, mag);

		if ((x & (x - 1)) == 0) {
			HOSTED_API(print)("  hint: only one bit of bh_crc differs\n");
		}
	}
}

static void printctx(const char* what, const Poolctx* c) {
	if (c == NULL || c->fn == NULL) {
		HOSTED_API(print)("  %s: (none)\n", what);
		return;
	}
	HOSTED_API(print)("  %s: %s.%s (C function %p; gdb: info symbol %p)\n",
		what, c->mod != NULL ? c->mod : "?", c->fn, c->addr, c->addr);
}

void poolreport(Pool* p, const Poolfault* f, const Poolctx* now, const Poolctx* last) {
	Bhdr* b = f->blk, * pv = f->prev;

	HOSTED_API(print)("\n*** POOLPARANOID: pool %s is corrupted: %s\n", p->name, f->msg);
	HOSTED_API(print)("  block %p, previous block %p, arena %p, other %p\n",
		(void*)b, (void*)pv, (void*)f->lead, (void*)f->other);

	if (b != NULL) {
		HOSTED_API(print)("  header: size %lud magic %lux crc %lux (computed %lux)\n",
			(ulong)b->bh_size, (ulong)b->bh_magic, (ulong)b->bh_crc, (ulong)bhdr_crc(b));
		bitflips(b);
		dumpwords("Bchk+header", (uint8_t*)b - offsetof(Bchk, bc_self), 12);
	}

	if (pv != NULL && pv != f->lead) {
		HOSTED_API(print)("  previous block: magic %lux size %lud, its data is [%p, %p)\n",
			(ulong)pv->bh_magic, (ulong)pv->bh_size,
			(void*)((uint8_t*)pv + offsetof(Bhdr, bha_data)), (void*)((uint8_t*)pv + pv->bh_size));
		HOSTED_API(print)("  suspect: write past the end of the previous block (heap overflow), or write after free\n");
	}

	if (f->other != NULL && inarena(p, f->other)) {
		dumpwords("other", f->other, 8);
	}

	if (now != NULL && now->entry) {
		HOSTED_API(print)("  already corrupted BEFORE the native call below (the culprit ran since the last clean check):\n");
		printctx("about to call", now);
	} else {
		HOSTED_API(print)("  corrupted DURING or after the native call below, which has just returned:\n");
		printctx("returned     ", now);
	}
	printctx("last clean   ", last);
	HOSTED_API(print)("  the stack of the abort() below leads to poolparanoid() called by mcall() in xec.c\n\n");

	abort();
}

#endif /* POOLPARANOID */
