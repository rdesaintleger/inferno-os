#include <inferno/memprof.h>

static CallbackEntry *monitors = NULL;

void memprof_register(CallbackEntry* s) {
    callback_register(&monitors, s);
}

void memprof_unregister(CallbackEntry* s) {
    callback_unregister(&monitors, s);
}

void memprof_notify(int v, void *base, size_t size) {
    MemProfEvent event = {
        .v = v,
        .base = base,
        .size = size
    };

    callback_notify(monitors, &event);
}