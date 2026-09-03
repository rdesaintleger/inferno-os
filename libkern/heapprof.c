#include <inferno/heapprof.h>

static CallbackEntry *monitors = NULL;

void heapprof_register(CallbackEntry* s) {
    callback_register(&monitors, s);
}

void heapprof_unregister(CallbackEntry* s) {
    callback_unregister(&monitors, s);
}

void heapprof_notify(int v, void *base, size_t size) {
    HeapProfEvent event = {
        .v = v,
        .base = base,
        .size = size
    };

    callback_notify(monitors, &event);
}