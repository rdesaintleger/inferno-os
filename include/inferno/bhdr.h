#ifndef _INFERNO_BHDR_H_
#define _INFERNO_BHDR_H_

#include <stddef.h>
#include <stdint.h>

typedef struct Bchk Bchk;
typedef struct Bhdr Bhdr;
typedef struct Blead  Blead;
typedef struct Blink  Blink;
typedef struct Balloc Balloc;
typedef struct Bfree  Bfree;

typedef struct Bwalk  Bwalk;

typedef union Balign Balign;

enum {
    MAGIC_A = 0x00a110c0, /* Allocated block */
    MAGIC_F = 0xbadc0c00, /* Free block */
    MAGIC_L = 0xdeadbab0, /* start of arena */
    MAGIC_E = 0xdeadbee0  /* end of arena */
};

/* allocated flags */
enum {
    BF_SCRAMBLE    = 0x01,  /* scramble memory on free */
    BF_COLLECTABLE = 0x02,  /* GC can collect and free this block */
    BF_IMMUTABLE   = 0x04,  /* GC can collect this block but it won't free it */
    BF_MEMZERO     = 0x20,  /* ensure memory is reset after allocation (non persistent) */
};

/* arena flags */
enum {
    BF_MAPPED   = 0x01,  /* 32 bits mapped arena (implies arena is chunked) */
    BF_MAXIMIZE = 0x10,  /* maximize arena allocation (internal, not persistent) */
};

enum {
    CHUNK_PAGESIZE = 128*1024,
};

#define BFLAGS_MASK ((uint32_t) 0xf)
#define BMAGIC_MASK (~BFLAGS_MASK)
#define BMAGIC(b) ((b)->bh_magic & BMAGIC_MASK)

union Balign {
    uintptr_t p;

    double d;
    uint64_t l;
};

struct Blead {
    /* leader block definitions */
    Bhdr*    bh_nextchain; /* pointer to next arena block in pool chain (can be NULL) */
    Bhdr*    bh_prevchain; /* pointer to prev arena block in pool chain (can be NULL) */
    Bhdr*    bh_trail;     /* pointer to arena sentinel block */
    Bwalk*   bh_walkers;   /* pointer to first arena walker */
    size_t   bh_freecnt;   /* free blocks counter for this arena (allows compaction if > 1, 0 for direct arenas) */
    uint32_t bh_mapoffset; /* 32 bits mapping offset for first valid data in arena */

    Balign data; /* start of arena raw data */
};

struct Blink {
    /* allocated / free block common definitions */
    Bhdr* bh_lead;  /* arena leader bloc */
};

struct Balloc {
    Blink  bh_link;

    Balign data; /* start of allocated data */
};

struct Bfree {
    /* free block definitions (bh_magic flags are used for balance factor) */
    Blink bh_link;
    Bhdr* bh_left;   /* AVL tree left pointer   */
    Bhdr* bh_right;  /* AVL tree right pointer  */
    Bhdr* bh_parent; /* AVL tree parent pointer */
    Bhdr* bh_succ;   /* AVL tree next bloc of same size */
    Bhdr* bh_pred;   /* AVL tree pred bloc of same size */

    Balign data; /* start of free aera */
};

struct Bhdr {
    size_t   bh_size;  /* block size, include header (exclude Bchk pred pointer)*/
    uint32_t bh_magic; /* bloc type, leave 4 bits for persistent flags/state (see above) */

    union {
        Balloc a;
        Bfree  f;
        Blead  l;

        Balign data; /* start of block (untyped/generic) */
    } u;
};

struct Bchk {
    /*
     * this extra definition is for chunked arena blocks (except for leader)
     * Only usage is for chunked free block merge.
     * This structure is obtained using pointer arithmetic on the Bhdr pointer (behavior of chunk vs direct does not change for other Bhdr pointers).
     */
    Bhdr* bc_pred;
    Bhdr  bc_self;
};

struct Bwalk {
    /*
     * walker structure. purpose of this is to to make block iterators immune to compaction.
     * compaction will check if bw_ptr is moving and will upgrade it if needed. Typical use is
     * in the GC wich keep a persistent block cursor between calls.
     */
    Bwalk* bw_succ; /* next walker in list */
    Bwalk* bw_pred; /* pred walker in list */
    Bhdr*  bw_ptr;  /* walker cursor (allocated blocks only, updated by compaction) */
};

/* for generic / untyped / end of arena blocks */
#define bh_data u.data

/* this is only valid for allocated blocks */
#define bha_lead u.a.bh_link.bh_lead
#define bha_data u.a.data

/* this is only valid for arena free blocks */
#define bhf_left   u.f.bh_left
#define bhf_right  u.f.bh_right
#define bhf_parent u.f.bh_parent
#define bhf_succ   u.f.bh_succ
#define bhf_pred   u.f.bh_pred
#define bhf_lead   u.f.bh_link.bh_lead
#define bhf_data   u.f.data

/* this is only valid for arena leader */
#define bhl_nextchain u.l.bh_nextchain
#define bhl_prevchain u.l.bh_prevchain
#define bhl_trail     u.l.bh_trail
#define bhl_freecnt   u.l.bh_freecnt
#define bhl_walkers   u.l.bh_walkers
#define bhl_mapoffset u.l.bh_mapoffset
#define bhl_data      u.l.data

#define BALIGN_SZ    sizeof(Balign)

#define BALIGN_SIZE16(base, ptr, size, offset) \
    ((size_t)(size) + \
     ((16 - (((uintptr_t)(ptr) - (uintptr_t)(base) + \
              (uintptr_t)(size) + (uintptr_t)(offset)) & 15)) & 15))

#define BCEIL(s, pad)    BFLOOR((s) + ((pad) - 1), pad)
#define BFLOOR(s, pad)   (((s) / (pad)) * (pad))

#define BHDRSIZE \
    ((size_t)(offsetof(Bhdr, bha_data)))

#define BHDR2BCHK(bp) \
    ((Bchk *)((uint8_t *)(bp) - offsetof(Bchk, bc_self)))

#define BHDR2DATA(bp) \
    ((void *)((uint8_t *)(bp) + BHDRSIZE))

#define DATA2BHDR(b, dp, blockfault) \
    do {                                                         \
        void *_dp = (void *)(dp);                                \
        Bhdr *_b = (b) = (Bhdr *)((uint8_t *)_dp - BHDRSIZE);    \
        if (BMAGIC(_b) != MAGIC_A)                               \
            blockfault(_dp, "alloc:D2B");                        \
    } while (0)

#define BHDR2CHKSUCC(b) \
    ((Bchk *)((uint8_t *)(b) + (b)->bh_size))

void bwalk_link(Bhdr*, Bwalk*);

void bwalk_unlink(Bhdr*, Bwalk*);

#endif /* _INFERNO_BHDR_H_ */