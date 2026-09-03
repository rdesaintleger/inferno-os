#ifndef _INFERNO_HEAPPROF_H_
#define _INFERNO_HEAPPROF_H_

#include <stddef.h>

#include <inferno/callback.h>

typedef struct HeapProfEvent HeapProfEvent;

struct HeapProfEvent {
    void *base;
    size_t size;

    int v;
};

extern void heapprof_register(CallbackEntry*);
extern void heapprof_unregister(CallbackEntry*);
extern void heapprof_notify(int v, void *base, size_t size);

#endif /* _INFERNO_HEAPPROF_H_ */
