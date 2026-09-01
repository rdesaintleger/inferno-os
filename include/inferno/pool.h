#ifndef _INFERNO_POOL_H_
#define _INFERNO_POOL_H_

#include <inferno/bhdr.h>

typedef struct Pool Pool;

struct Pool {
    char* name;
    int pnum;
    size_t maxsize;
    int quanta;
    int chunk;
    int monitor;
    size_t ressize; /* restricted size */
    size_t cursize;
    size_t arenasize;
    uint32_t hw;
    Lock l;
    Bhdr* root;
    Bhdr* chain;
    size_t nalloc;
    size_t nfree;
    int nbrk;
    int lastfree;
    void (*move)(void*, void*);
};

#endif /* _INFERNO_POOL_H_ */