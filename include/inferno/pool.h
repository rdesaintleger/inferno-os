#ifndef _INFERNO_POOL_H_
#define _INFERNO_POOL_H_

#include <stddef.h>
#include <stdint.h>

typedef struct Bhdr Bhdr;
typedef struct Btail Btail;
typedef union Balign Balign;

enum {
    MAGIC_A = 0xa110c,    /* Allocated block */
    MAGIC_F = 0xbadc0c0a, /* Free block */
    MAGIC_E = 0xdeadbabe, /* End of arena */
    MAGIC_I = 0xabba      /* Block is immutable (hidden from gc) */
};

union Balign {
    uintptr_t p;

    double d;
    uint64_t l;
};

struct Bhdr {
    uint32_t bh_magic;
    size_t bh_size;
    union {
        Balign data;
        struct {
            Bhdr* bhl;
            Bhdr* bhr;
            Bhdr* bhp;
            Bhdr* bhv;
            Bhdr* bhf;
        } s;
        struct {
            Bhdr* link;
            size_t limit;
        } l;
    } u;
};

#define bh_left   u.s.bhl
#define bh_right  u.s.bhr
#define bh_fwd    u.s.bhf
#define bh_prev   u.s.bhv
#define bh_parent u.s.bhp

#define bh_link u.l.link
#define bh_limit u.l.limit

struct Btail {
    Bhdr* hdr;
};

#define B2D(bp) \
    ((void *)((uint8_t *)(bp) + offsetof(Bhdr, u.data)))

#define D2B(b, dp)                                                          \
    do                                                                      \
    {                                                                       \
        void *_dp = (void *)(dp);                                           \
        Bhdr *_b = (b) = (Bhdr *)((uint8_t *)_dp - offsetof(Bhdr, u.data)); \
        if (_b->bh_magic != MAGIC_A && _b->bh_magic != MAGIC_I)             \
            poolfault(_dp, "alloc:D2B");                                    \
    } while (0)

#define B2NB(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->bh_size))

#define B2PT(b) \
    ((Btail *)((uint8_t *)(b) - sizeof(Btail)))

#define B2T(b) \
    ((Btail *)((uint8_t *)(b) + (b)->bh_size - sizeof(Btail)))

#define B2LIMIT(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->bh_limit))

#define BHDRSIZE \
    ((size_t)(offsetof(Bhdr, u.data) + sizeof(Btail)))

typedef struct Pool Pool;

struct Pool {
    char* name;
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
    Bhdr* root;
    Bhdr* chain;
    ulong	nalloc;
    ulong	nfree;
    int	nbrk;
    int	lastfree;
    void	(*move)(void*, void*);
};

#endif /* _INFERNO_POOL_H_ */