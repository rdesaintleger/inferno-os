#include <stddef.h>

#include <inferno/callback.h>

void callback_register(CallbackEntry** list, CallbackEntry* s) {
    if ((s->state & CALLBACK_REGISTERED) == 0) {
        s->next = *list;
        s->state |= CALLBACK_REGISTERED;

        *list = s;
    }
}

void callback_unregister(CallbackEntry** list, CallbackEntry* s) {
    if ((s->state & CALLBACK_REGISTERED) == CALLBACK_REGISTERED) {
        /* pred avoids repeated dereferences on non-optimizing compilers */
        CallbackEntry* pred = *list;

        while (pred != NULL) {
            if (pred == s) {
                *list = s->next;
                s->next = NULL;
                s->state &= ~CALLBACK_REGISTERED;
                break;
            }

            list = &pred->next;
            pred = *list;
        }
    }
}

void callback_notify(CallbackEntry* head, void* data) {
    for (CallbackEntry* cb = head; cb != NULL;) {
        /* allow caller to unregister itself */
        CallbackEntry* next = cb->next;

        if ((cb->state & ~CALLBACK_REGISTERED) == 0) {
            cb->callback(cb->ctx, data);
        }

        cb = next;
    }
}

void disable_callback(CallbackEntry* s) {
    unsigned int cnt = s->state >> 1;

    if (cnt < 0x7ffffff) {
        cnt += 1;
    }

    s->state = (cnt << 1) | (s->state & CALLBACK_REGISTERED);
}

void enable_callback(CallbackEntry* s) {
    int cnt = s->state >> 1;

    if (cnt > 0) {
        cnt -= 1;
    }

    s->state = (cnt << 1) | (s->state & CALLBACK_REGISTERED);
}