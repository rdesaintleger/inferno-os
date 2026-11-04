#ifndef _INFERNO_POOL_H_
#define _INFERNO_POOL_H_

#include <inferno/bhdr.h>
#include <inferno/lock.h>

typedef struct Pool Pool;

struct Pool {
    char* name;
    int pnum;
    size_t maxsize;
    int chunk;
    int monitor;
    size_t ressize; /* restricted size */
    size_t cursize;
    size_t arenasize;
    uint32_t hw;
    inferno_mutex_t l;	/* short mutex: init by whoever creates the pool (allocinit) */
    Bhdr* root;
    Bhdr* chain;
    size_t nalloc;
    size_t nfree;
    int nbrk;
    void (*move)(void*, void*);
};

#ifdef POOLPARANOID
typedef struct Poolfault Poolfault;
typedef struct Poolctx Poolctx;

/* what the walk of a pool found */
struct Poolfault {
    const char* msg;  /* what is wrong */
    Bhdr* blk;        /* block (or arena leader) where it was found */
    Bhdr* prev;       /* block before it in the arena (NULL for a leader) */
    Bhdr* lead;       /* arena leader */
    Bhdr* other;      /* second block involved (link target...) or NULL */
};

/* the native function of the VM that just returned */
struct Poolctx {
    const char* mod;  /* module name */
    const char* fn;   /* function name */
    void* addr;       /* address of the C function (gdb: info symbol addr) */
    int entry;        /* 1: checked before the call, 0: after it returned */
};

/* libkern/poolcheck.c: returns 0 if the pool is consistent, else fills *f (pool unlocked) */
extern int poolcheck(Pool*, Poolfault*);

/* libkern/poolcheck.c: prints the report and calls abort(); does not return */
extern void poolreport(Pool*, const Poolfault*, const Poolctx* now, const Poolctx* last);

/* emu/port/alloc.c: checks every pool, aborts on corruption (also callable from gdb) */
extern void poolparanoid(const Poolctx*);
extern void poolparanoid_now(void);
#endif

#endif /* _INFERNO_POOL_H_ */