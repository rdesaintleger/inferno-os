#ifndef _POOL_H_
#define _POOL_H_

#include <stddef.h>
#include <stdint.h>

typedef struct Pool Pool;
typedef struct Bhdr Bhdr;
typedef struct Btail Btail;

enum
{
    MAGIC_A = 0xa110c,    /* Allocated block */
    MAGIC_F = 0xbadc0c0a, /* Free block */
    MAGIC_E = 0xdeadbabe, /* End of arena */
    MAGIC_I = 0xabba      /* Block is immutable (hidden from gc) */
};

struct Bhdr
{
    uint32_t magic;
    size_t size;
    union
    {
        uint8_t data[1];
        struct
        {
            Bhdr *bhl;
            Bhdr *bhr;
            Bhdr *bhp;
            Bhdr *bhv;
            Bhdr *bhf;
        } s;
#define clink u.l.link
#define csize u.l.size
        struct
        {
            Bhdr *link;
            size_t size;
        } l;
    } u;
};

struct Btail
{
    Bhdr *hdr;
};

struct Pool
{
	char*	name;
	int	pnum;
	ulong	maxsize;
	int	quanta;
	int	chunk;
	int	monitor;
	ulong	ressize;	/* restricted size */
	ulong	cursize;
	ulong	arenasize;
	ulong	hw;
	Lock	l;
	Bhdr*	root;
	Bhdr*	chain;
	ulong	nalloc;
	ulong	nfree;
	int	nbrk;
	int	lastfree;
	void	(*move)(void*, void*);
};

#define B2D(bp) \
    ((void *)((uint8_t *)(bp) + offsetof(Bhdr, u.data)))

#define D2B(b, dp)                                                          \
    do                                                                      \
    {                                                                       \
        void *_dp = (void *)(dp);                                           \
        Bhdr *_b = (b) = (Bhdr *)((uint8_t *)_dp - offsetof(Bhdr, u.data)); \
        if (_b->magic != MAGIC_A && _b->magic != MAGIC_I)                   \
            poolfault(_dp, "alloc:D2B");                                    \
    } while (0)

#define B2NB(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->size))

#define B2PT(b) \
    ((Btail *)((uint8_t *)(b) - sizeof(Btail)))

#define B2T(b) \
    ((Btail *)((uint8_t *)(b) + (b)->size - sizeof(Btail)))

#define B2LIMIT(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->csize))
#define BHDRSIZE \
    ((size_t)(offsetof(Bhdr, u.data) + sizeof(Btail)))

#endif /* _POOL_H_ */