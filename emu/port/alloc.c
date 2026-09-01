#include <inferno/memprof.h>

#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "error.h"

enum
{
	MAXPOOL		= 4
};

#define RESERVED	512*1024

struct
{
	int	n;
	Pool	pool[MAXPOOL];
	/* Lock l; */
} table = {
	3,
	{
		{ "main",  0, 	32*1024*1024, 31,  512*1024, 0, 31*1024*1024 },
		{ "heap",  1, 	32*1024*1024, 31,  512*1024, 0, 31*1024*1024 },
		{ "image", 2,   64*1024*1024+256, 31, 4*1024*1024, 1, 63*1024*1024 },
	}
};

Pool*	mainmem = &table.pool[0];
Pool*	heapmem = &table.pool[1];
Pool*	imagmem = &table.pool[2];

static void _auditmemloc(char *, void *);
void (*auditmemloc)(char *, void *) = _auditmemloc;
static void _poolfault(void *, char *);
void (*poolfault)(void *, char *) = _poolfault;

/*	non tracing
 *
enum {
	Npadlong	= 0,
	MallocOffset = 0,
	ReallocOffset = 0
};
 *
 */

/* tracing */
enum {
	Npadlong	= 2,
	MallocOffset = 0,
	ReallocOffset = 1
};

int
memusehigh(void)
{
	return 	mainmem->cursize > mainmem->ressize ||
			heapmem->cursize > heapmem->ressize ||
			0 && imagmem->cursize > imagmem->ressize;
}

int
memlow(void)
{
	return heapmem->cursize > (heapmem->maxsize)/2;
}

int
poolsetsize(char *s, int size)
{
	int i;

	for(i = 0; i < table.n; i++) {
		if(strcmp(table.pool[i].name, s) == 0) {
			table.pool[i].maxsize = size;
			table.pool[i].ressize = size-RESERVED;
			if(size < RESERVED)
				panic("not enough memory");
			return 1;
		}
	}
	return 0;
}

ulong
poolmaxsize(void)
{
	int i;
	ulong total;

	total = 0;
	for(i = 0; i < nelem(table.pool); i++)
		total += table.pool[i].maxsize;
	return total;
}

int
poolread(char *va, int count, ulong offset)
{
	Pool *p;
	int n, i, signed_off;

	n = 0;
	signed_off = offset;
	for(i = 0; i < table.n; i++) {
		p = &table.pool[i];
		n += HOSTED_API(snprint)(va+n, count-n, "%11lud %11lud %11lud %11lud %11lud %11d %11lud %s\n",
			p->cursize,
			p->maxsize,
			p->hw,
			p->nalloc,
			p->nfree,
			p->nbrk,
			poolmax(p),
			p->name);

		if(signed_off > 0) {
			signed_off -= n;
			if(signed_off < 0) {
				memmove(va, va+n+signed_off, -signed_off);
				n = -signed_off;
			}
			else
				n = 0;
		}

	}
	return n;
}

void*
smalloc(size_t size)
{
	void *v;

	for(;;){
		v = HOSTED_API(malloc)(size);
		if(v != nil)
			break;
		osenter();
		osmillisleep(100);
		osleave();
	}
	return v;
}

void*
kmalloc(size_t size)
{
	void *v;

	v = dopoolalloc(mainmem, size+Npadlong*sizeof(ulong));
	if(v != nil){
		if(Npadlong){
			v = (ulong*)v+Npadlong;
		}
		memset(v, 0, size);
		memprof_notify(0, v, size);
	}
	return v;
}



void*
HOSTED_API(malloc)(size_t size)
{
	void *v;

	v = poolalloc(mainmem, size+Npadlong*sizeof(ulong));
	if(v != nil){
		if(Npadlong){
			v = (ulong*)v+Npadlong;
		}
		memset(v, 0, size);
		memprof_notify(0, v, size);
	} else 
		HOSTED_API(print)("malloc failed\n");
	return v;
}

void*
HOSTED_API(mallocz)(ulong size, int clr)
{
	void *v;

	v = poolalloc(mainmem, size+Npadlong*sizeof(ulong));
	if(v != nil){
		if(Npadlong){
			v = (ulong*)v+Npadlong;
		}
		if(clr)
			memset(v, 0, size);
		memprof_notify(0, v, size);
	} else 
		HOSTED_API(print)("mallocz failed\n");
	return v;
}

void
HOSTED_API(free)(void *v)
{
	Bhdr *b;

	if(v != nil) {
		if(Npadlong)
			v = (ulong*)v-Npadlong;
		D2B(b, v, poolfault);
		memprof_notify(1<<8|0, (ulong*)v+Npadlong, b->bh_size);
		poolfree(mainmem, v);
	}
}

void*
HOSTED_API(realloc)(void *v, size_t size)
{
	void *nv;

	if(size == 0)
		return HOSTED_API(malloc)(size);	/* temporary change until realloc calls can be checked */
	if(v != nil)
		v = (ulong*)v-Npadlong;
	if(Npadlong!=0 && size!=0)
		size += Npadlong*sizeof(ulong);
	nv = poolrealloc(mainmem, v, size);
	if(nv != nil) {
		nv = (ulong*)nv+Npadlong;
	} else 
		HOSTED_API(print)("realloc failed\n");
	return nv;
}

ulong
HOSTED_API(msize)(void *v)
{
	if(v == nil)
		return 0;
	return poolmsize(mainmem, (ulong*)v-Npadlong)-Npadlong*sizeof(ulong);
}

void*
HOSTED_API(calloc)(size_t n, size_t szelem)
{
	return HOSTED_API(malloc)(n*szelem);
}

static void
_poolfault(void *v, char *msg)
{
	auditmemloc(msg, v);
	panic("%s %lux", msg, v);
}

static void
dumpvl(char *msg, ulong *v, int n)
{
	int i, l;

	l = HOSTED_API(print)("%s at %p: ", msg, v);
	for (i = 0; i < n; i++) {
		if (l >= 60) {
			HOSTED_API(print)("\n");
			l = HOSTED_API(print)("    %p: ", v);
		}
		l += HOSTED_API(print)(" %lux", *v++);
	}
	HOSTED_API(print)("\n");
}

static void
corrupted(char *str, char *msg, Pool *p, Bhdr *b, void *v)
{
	HOSTED_API(print)("%s(%p): pool %s CORRUPT: %s at %p'%lud(magic=%lux)\n",
		str, v, p->name, msg, b, b->bh_size, b->bh_magic);
	dumpvl("bad Bhdr", (ulong *)((ulong)b & ~3)-4, 10);
}

static void
_auditmemloc(char *str, void *v)
{
	Pool *p;
	Bhdr *bc, *ec, *b, *nb, *fb = nil;
	char *fmsg, *msg;
	ulong fsz;

	SET(fsz);
	SET(fmsg);
	for (p = &table.pool[0]; p < &table.pool[nelem(table.pool)]; p++) {
		lock(&p->l);
		for (bc = p->chain; bc != nil; bc = bc->bh_link) {
			if (bc->bh_magic != MAGIC_L) {
				unlock(&p->l);
				corrupted(str, "chain hdr!=MAGIC_L", p, bc, v);
				goto nextpool;
			}
			ec = B2LIMIT(bc);
			if (((Bhdr*)v >= bc) && ((Bhdr*)v < ec))
				goto found;
		}
		unlock(&p->l);
nextpool:	;
	}
	HOSTED_API(print)("%s: %p not in pools\n", str, v);
	return;

found:
	for (b = bc; b < ec; b = nb) {
		switch(b->bh_magic) {
		case MAGIC_F:
			msg = "free blk";
			break;
		case MAGIC_I:
			msg = "immutable block";
			break;
		case MAGIC_A:
			msg = "block";
			break;
		case MAGIC_L:
			msg = "arena leader block";
			break;
		case MAGIC_E:
			msg = "arena end block";
			break;
		default:
			unlock(&p->l);
			corrupted(str, "bad magic", p, b, v);
			goto badchunk;
		}
		if (b->bh_size <= 0 || (b->bh_size & p->quanta)) {
			unlock(&p->l);
			corrupted(str, "bad size", p, b, v);
			goto badchunk;
		}
		if (fb != nil)
			break;
		nb = B2NB(b);
		if ((Bhdr*)v < nb) {
			fb = b;
			fsz = b->bh_size;
			fmsg = msg;
		}
	}
	unlock(&p->l);
	if (b >= ec) {
		if (b > ec)
			corrupted(str, "chain size mismatch", p, b, v);
		else if (b->bh_magic != MAGIC_E)
			corrupted(str, "chain end!=MAGIC_E", p, b, v);
	}
badchunk:
	if (fb != nil) {
		HOSTED_API(print)("%s: %p in %s:", str, v, p->name);
		if (fb == v)
			HOSTED_API(print)(" is %s '%lux\n", fmsg, fsz);
		else
			HOSTED_API(print)(" in %s at %p'%lux\n", fmsg, fb, fsz);
		dumpvl("area", (ulong *)((ulong)v & ~3)-4, 20);
	}
}

char *
poolaudit(char*(*audit)(int, Bhdr *))
{
	Pool *p;
	Bhdr *bc, *ec, *b;
	char *r = nil;

	for (p = &table.pool[0]; p < &table.pool[nelem(table.pool)]; p++) {
		lock(&p->l);
		for (bc = p->chain; bc != nil; bc = bc->bh_link) {
			if (bc->bh_magic != MAGIC_L) {
				unlock(&p->l);
				return "bad chain hdr";
			}
			ec = B2LIMIT(bc);
			for (b = bc; b < ec; b = B2NB(b)) {
				if (b->bh_size <= 0 || (b->bh_size & p->quanta))
					r = "bad size in bhdr";
				else
					switch(b->bh_magic) {
					case MAGIC_E:
						r = "unexpected MAGIC_E";
						break;
					case MAGIC_L:
						if (b != bc) {
							r = "unexpected MAGIC_L";
							break;
						}
					case MAGIC_F:
					case MAGIC_A:
					case MAGIC_I:
						r = audit(p->pnum, b);
						break;
					default:
						r = "bad magic";
					}
				if (r != nil) {
					unlock(&p->l);
					return r;
				}
			}
			if (b != ec || b->bh_magic != MAGIC_E) {
				unlock(&p->l);
				return "bad chain ending";
			}
		}
		unlock(&p->l);
	}
	return r;
}
