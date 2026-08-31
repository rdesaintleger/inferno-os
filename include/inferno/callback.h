#ifndef _INFERNO_CALLBACK_H_
#define _INFERNO_CALLBACK_H_

typedef struct CallbackEntry CallbackEntry;

#define CALLBACK_REGISTERED 0x01

struct CallbackEntry {
    void (*callback)(void* ctx, void* data);
    void* ctx;

    CallbackEntry* next;
    unsigned int state; /* private callback state, initialize to 0 */
};

extern void callback_register(CallbackEntry** list, CallbackEntry* s);
extern void callback_unregister(CallbackEntry** list, CallbackEntry* s);
extern void callback_notify(CallbackEntry* head, void* data);

extern void disable_callback(CallbackEntry* s);
extern void enable_callback(CallbackEntry* s);

#endif /* _INFERNO_CALLBACK_H_ */
