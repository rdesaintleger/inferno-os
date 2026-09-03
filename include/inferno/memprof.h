#ifndef _INFERNO_MEMPROF_H_
#define _INFERNO_MEMPROF_H_

#include <stddef.h>

#include <inferno/callback.h>

typedef struct MemProfEvent MemProfEvent;

struct MemProfEvent {
    void *base;
    size_t size;

    int v;
};

extern void memprof_register(CallbackEntry*);
extern void memprof_unregister(CallbackEntry*);
extern void memprof_notify(int v, void *base, size_t size);

#endif /* _INFERNO_MEMPROF_H_ */
