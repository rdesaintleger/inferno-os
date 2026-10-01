#ifndef _INFERNO_DPTR_H_
#define _INFERNO_DPTR_H_

#include <inferno/bhdr.h>
#include <inferno/mapper.h>

/*
 * DREF: a Dis-visible heap reference, typed - every array/list/chan/
 * string/ref/module field or local, declared with its real C type via
 * DREF() so the compiler keeps checking it exactly as it does today.
 * DPTR: the resolved, host-usable form of a DREF (a real pointer, safe
 * to dereference directly). The compiler (limbo/types.c, sizetype())
 * already commits every DREF-kind value to exactly IBY2WD (4) bytes;
 * on a 64-bit host that's already wrong (xec.c stores a full host
 * pointer there today), which is why -m32 is currently required.
 *
 * Plan (agreed):
 *  1. THIS header: declare every stored heap reference via DREF(),
 *     and convert at every read/write boundary via DREF2DPTR/
 *     DPTR2DREF. Zero functional change while DIS_MAPPED is 0 - in
 *     this phase DREF(T, x) is plainly `T* x`, so nothing about field
 *     types changes for the compiler; this step is a pure mechanical
 *     sweep, safe to do file by file.
 *  2. Handle the things that have no natural DREF representation this
 *     way: lea's result (a raw address, not a HEAP2DPTR/DPTR2HEAP-
 *     tracked reference - see xec.c: `W(d) = (WORD)R.s;`, already a
 *     silent truncation today) and the PC in a Frame (currently an
 *     Inst*; code is meant to move outside mapped space entirely, so
 *     PC becomes a plain uint32_t index instead of going through the
 *     mapper at all - a different mechanism, not covered here).
 *  3. Once 1+2 are functionally complete, flip DIS_MAPPED on (can be
 *     done early, purely to profile which DPTR2DREF call sites are
 *     hot) and optimize: push conversions toward DREF2DPTR, and
 *     minimize/eliminate DPTR2DREF outside of the initial allocation
 *     site. Deliberately not designed now, before there's real data
 *     on which sites are actually hot.
 *  4. Ship with DIS_MAPPED on.
 */

#ifndef DIS_MAPPED
#define DIS_MAPPED 0
#endif

#if !DIS_MAPPED

#define DREF(DisType, disname) DisType* disname

/* H (interp.h) is already the "no value" sentinel used throughout
 * (checktype, freeptrs) for exactly these slots - DREF reuses it
 * rather than introducing a second nil convention. */
#define DREF_NIL ((void*)H)

/* stored reference -> usable pointer: already the right type in this
 * phase, the cast is a documentation-only no-op. */
#define DREF2DPTR(DisType, ref) ((DisType*)(ref))

/* usable pointer -> value to store: identity, arena unused. */
#define DPTR2DREF(ptr, arena) (ptr)

#else /* DIS_MAPPED */

#define DREF(DisType, disname) uint32_t disname

/*
 * TODO(mapped-refs): confirm this can never collide with a real
 * mapped address. 0xffffffff lands on page 0x7fff (ARENA_PAGESHIFT=17
 * => addr>>17), which IS a valid page index (0..ARENA_MAX_PAGES-1) -
 * not automatically excluded by the mapper today. Needs either a
 * reserved/never-mapped top page, or a different sentinel value.
 */
#define DREF_NIL ((uint32_t)0xffffffffU)

#define DREF2DPTR(DisType, ref) ((DisType*)arena_lookup64((ref), NULL))
#define DPTR2DREF(ptr, arena)   arena_lookup32((arena), (ptr))

#endif /* DIS_MAPPED */

#include <interp.h> /* for DPTR2HPTR */
#include <pool.h> /* for poolfault  */

/*
 * Arena lookup for a DPTR that is a HEAP2DPTR/DPTR2HEAP-style block
 * start (the ADT case): DPTR2HPTR(dp) undoes HPTR2DPTR (interp.h) to
 * recover the exact poolalloc() block start, which DATA2BHDR requires
 * (HEAP2DPTR/DPTR2HEAP/DPTR2HPTR/HPTR2DPTR stay untouched by this
 * header). Deliberately not cached/optimized (step 3's job, once real
 * call sites are known) - this is the naive, always-correct version
 * used for the step 1 sweep.
 */
static inline Bhdr* dptr_arena(void* dp) {
    Bhdr* b;

    DATA2BHDR(b, DPTR2HPTR(dp), poolfault);
    return b->bha_lead;
}
#define DPTR2ARENA(dp) dptr_arena(dp)

#endif /* _INFERNO_DPTR_H_ */