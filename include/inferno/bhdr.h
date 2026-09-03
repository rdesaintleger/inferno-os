#ifndef _INFERNO_BHDR_H_
#define _INFERNO_BHDR_H_

#include <stddef.h>
#include <stdint.h>

typedef struct Bhdr Bhdr;
typedef struct Btail Btail;
typedef union Balign Balign;

enum {
    MAGIC_A = 0xa110c,    /* Allocated block */
    MAGIC_F = 0xbadc0c0a, /* Free block */
    MAGIC_L = 0xdeadbabe, /* start of arena */
    MAGIC_E = 0xdeadbeef, /* end of arena */
    MAGIC_I = 0xabba      /* Block is immutable (hidden from gc) */
};

union Balign {
    uintptr_t p;

    double d;
    uint64_t l;
};

struct Bhdr {
    uint32_t bh_magic;
    size_t bh_size; /* block size, include header and tail */
    union {
        Balign data; /* aligned block raw data */
        struct {
            /* Exec memory subsystem compat */
            //uint32_t mc_next;  /* next mapped memory chunk */
            //uint32_t mc_bytes; /* size of memory chunk */

            /* host free metadata */
            Bhdr* bhl;
            Bhdr* bhr;
            Bhdr* bhp;
            Bhdr* bhv;
            Bhdr* bhf;

            Balign data; /* start of free space */
        } s;
        struct {
            Bhdr* link;   /* next arena */
            size_t limit; /* size of this arena minus the MAGIC_E block */
            Balign data;  /* first aligned Bhdr */
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
#define bh_first u.l.data

struct Btail {
    Bhdr* bt_hdr;   /* pointer to bloc  */
    Bhdr* bt_arena; /* pointer to arena */

    Balign bt_next; /* next aligned header (should be a block) */
};

#define BTAIL_SIZE \
    ((size_t)(offsetof(Btail, bt_next)))

#define B2NB(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->bh_size))

#define B2LIMIT(b) \
    ((Bhdr *)((uint8_t *)(b) + (b)->bh_limit))

#define B2PT(b) \
    ((Btail *)((uint8_t *)(b) - BTAIL_SIZE))

#define B2T(b) B2PT(B2NB(b))

#define BALIGN_SZ    sizeof(Balign)
#define BCEIL(s, pad)    BFLOOR((s) + ((pad) - 1), pad)
#define BFLOOR(s, pad)   (((s) / (pad)) * (pad))

#define BHDR_E_SIZE \
    ((size_t)(offsetof(Bhdr, u.data)))

#define BHDR_L_SIZE \
    ((size_t)(offsetof(Bhdr, u.l.data)))

#define BHDR_F_SIZE \
    ((size_t)(offsetof(Bhdr, u.s.data)))

#define BHDR_A_SIZE BHDR_E_SIZE
#define BHDR_I_SIZE BHDR_E_SIZE

#define B2D(bp) \
    ((void *)((uint8_t *)(bp) + BHDR_A_SIZE))

#define D2B(b, dp, blockfault) \
    do {                                                         \
        void *_dp = (void *)(dp);                                \
        Bhdr *_b = (b) = (Bhdr *)((uint8_t *)_dp - BHDR_A_SIZE); \
        if (_b->bh_magic != MAGIC_A && _b->bh_magic != MAGIC_I)  \
            blockfault(_dp, "alloc:D2B");                        \
    } while (0)

#endif /* _INFERNO_BHDR_H_ */