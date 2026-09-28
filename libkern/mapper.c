#include <inferno/mapper.h>
#include <inferno/lock.h>
#include <inferno/platform.h>

struct map_range_search_state {
    uint16_t best_entry; /* best-fit available non mapped space */
    uint16_t best_base;  /* base page (within the best_entry) */
    uint16_t best_size;  /* adusted size (within the best_entry) */

    uint16_t page_cnt; /* number of pages to be mapped */
    uint16_t page_min; /* minimum page number (inclusive) target zone */
    uint16_t page_max; /* maximum page number (exclusive) target zone */
};

static Bhdr* arenas[ARENA_MAX_PAGES];
static struct free_entry unmapped[ARENA_MAX_FREENODES];

static uint16_t map_root;
static uint16_t free_head;

static Lock map_lock;

#define UNSIGNED_CMP(a, b) (((a) > (b)) - ((a) < (b)))

enum {
    RB_COLOR_BIT = FREENODE_NIL + 1,
    RB_PARENT_MASK = FREENODE_NIL,
    RB_RED = 0,
    RB_BLACK = RB_COLOR_BIT
};

#define rb_parent(i)   ((uint16_t)(unmapped[i].fe_parent & RB_PARENT_MASK))
#define rb_color(i)    ((uint16_t)(unmapped[i].fe_parent & RB_COLOR_BIT))
#define rb_is_red(i)   (rb_color(i) == RB_RED)
#define rb_is_black(i) (rb_color(i) == RB_BLACK)
#define rb_set_parent(i, p) \
    (unmapped[i].fe_parent = (uint16_t)((unmapped[i].fe_parent & RB_COLOR_BIT) | ((p) & RB_PARENT_MASK)))
#define rb_set_color(i, c) \
    (unmapped[i].fe_parent = (uint16_t)((unmapped[i].fe_parent & RB_PARENT_MASK) | (c)))

/* -------------------------------------------------------------- *
 * fe_amax maintenance
 * -------------------------------------------------------------- */

static uint16_t amax_of(uint16_t idx) {
    uint16_t m = unmapped[idx].fe_size;
    uint16_t l = unmapped[idx].fe_left;
    uint16_t r = unmapped[idx].fe_right;

    if (l != FREENODE_NIL && unmapped[l].fe_amax > m) {
        m = unmapped[l].fe_amax;
    }
    if (r != FREENODE_NIL && unmapped[r].fe_amax > m) {
        m = unmapped[r].fe_amax;
    }

    return m;
}

/* Walks toward the root recomputing fe_amax, stopping as soon as a
 * level's value doesn't change (its ancestors can't be affected
 * either). Called once per structural change (leaf insert, node
 * removal) - rotations are self-contained (see below) and never need
 * this beyond the two nodes they directly touch. */
static void propagate_amax(uint16_t idx) {
    while (idx != FREENODE_NIL) {
        uint16_t m = amax_of(idx);
        if (m == unmapped[idx].fe_amax) {
            break;
        }
        unmapped[idx].fe_amax = m;
        idx = rb_parent(idx);
    }
}

/* -------------------------------------------------------------- *
 * Rotations. A rotation rearranges the SAME set of nodes under x/y,
 * so amax_of() for the pair afterward reproduces exactly what was
 * already true of the pre-rotation subtree - nothing beyond x/y ever
 * needs to be touched.
 * -------------------------------------------------------------- */

static void rb_rotate_left(uint16_t x) {
    uint16_t y = unmapped[x].fe_right;
    uint16_t p = rb_parent(x);

    unmapped[x].fe_right = unmapped[y].fe_left;
    if (unmapped[y].fe_left != FREENODE_NIL) {
        rb_set_parent(unmapped[y].fe_left, x);
    }
    rb_set_parent(y, p);
    if (p == FREENODE_NIL) {
        map_root = y;
    } else if (x == unmapped[p].fe_left) {
        unmapped[p].fe_left = y;
    } else {
        unmapped[p].fe_right = y;
    }
    unmapped[y].fe_left = x;
    rb_set_parent(x, y);

    unmapped[x].fe_amax = amax_of(x);
    unmapped[y].fe_amax = amax_of(y);
}

static void rb_rotate_right(uint16_t x) {
    uint16_t y = unmapped[x].fe_left;
    uint16_t p = rb_parent(x);

    unmapped[x].fe_left = unmapped[y].fe_right;
    if (unmapped[y].fe_right != FREENODE_NIL) {
        rb_set_parent(unmapped[y].fe_right, x);
    }
    rb_set_parent(y, p);
    if (p == FREENODE_NIL) {
        map_root = y;
    } else if (x == unmapped[p].fe_right) {
        unmapped[p].fe_right = y;
    } else {
        unmapped[p].fe_left = y;
    }
    unmapped[y].fe_right = x;
    rb_set_parent(x, y);

    unmapped[x].fe_amax = amax_of(x);
    unmapped[y].fe_amax = amax_of(y);
}

/* -------------------------------------------------------------- *
 * Insertion
 * -------------------------------------------------------------- */

static void rb_insert_fixup(uint16_t z) {
    while (rb_parent(z) != FREENODE_NIL && rb_is_red(rb_parent(z))) {
        uint16_t p = rb_parent(z);
        uint16_t g = rb_parent(p);

        if (p == unmapped[g].fe_left) {
            uint16_t u = unmapped[g].fe_right;
            if (u != FREENODE_NIL && rb_is_red(u)) {
                rb_set_color(p, RB_BLACK);
                rb_set_color(u, RB_BLACK);
                rb_set_color(g, RB_RED);
                z = g;
            } else {
                if (z == unmapped[p].fe_right) {
                    z = p;
                    rb_rotate_left(z);
                    p = rb_parent(z);
                    g = rb_parent(p);
                }
                rb_set_color(p, RB_BLACK);
                rb_set_color(g, RB_RED);
                rb_rotate_right(g);
            }
        } else {
            uint16_t u = unmapped[g].fe_left;
            if (u != FREENODE_NIL && rb_is_red(u)) {
                rb_set_color(p, RB_BLACK);
                rb_set_color(u, RB_BLACK);
                rb_set_color(g, RB_RED);
                z = g;
            } else {
                if (z == unmapped[p].fe_left) {
                    z = p;
                    rb_rotate_right(z);
                    p = rb_parent(z);
                    g = rb_parent(p);
                }
                rb_set_color(p, RB_BLACK);
                rb_set_color(g, RB_RED);
                rb_rotate_left(g);
            }
        }
    }
    rb_set_color(map_root, RB_BLACK);
}

static void map_insert(uint16_t idx) {
    uint16_t x = map_root, y = FREENODE_NIL;
    int went_left = 0;

    while (x != FREENODE_NIL) {
        int c = UNSIGNED_CMP(unmapped[idx].fe_base, unmapped[x].fe_base);
        if (c == 0)
            inferno_panic("map_insert: duplicate entry(%6ud-%6ud) for page %6ud", idx, x, unmapped[x].fe_base);
        y = x;
        went_left = (c < 0);
        x = went_left ? unmapped[x].fe_left : unmapped[x].fe_right;
    }

    unmapped[idx].fe_left = FREENODE_NIL;
    unmapped[idx].fe_right = FREENODE_NIL;
    rb_set_parent(idx, y);
    rb_set_color(idx, RB_RED);
    unmapped[idx].fe_amax = unmapped[idx].fe_size;

    if (y == FREENODE_NIL) map_root = idx;
    else if (went_left) unmapped[y].fe_left = idx;
    else unmapped[y].fe_right = idx;

    propagate_amax(y);
    rb_insert_fixup(idx);
}

/* -------------------------------------------------------------- *
 * Removal
 * -------------------------------------------------------------- */

static void rb_transplant(uint16_t u, uint16_t v) {
    uint16_t p = rb_parent(u);

    if (p == FREENODE_NIL) {
        map_root = v;
    } else if (u == unmapped[p].fe_left) {
        unmapped[p].fe_left = v;
    } else {
        unmapped[p].fe_right = v;
    }
    if (v != FREENODE_NIL) {
        rb_set_parent(v, p);
    }
}

static uint16_t rb_minimum(uint16_t x) {
    while (unmapped[x].fe_left != FREENODE_NIL) {
        x = unmapped[x].fe_left;
    }
    return x;
}

static void rb_remove_fixup(uint16_t x, uint16_t xp) {
    while (x != map_root && (x == FREENODE_NIL || rb_is_black(x))) {
        if (x == unmapped[xp].fe_left) {
            uint16_t w = unmapped[xp].fe_right;
            if (rb_is_red(w)) {
                rb_set_color(w, RB_BLACK);
                rb_set_color(xp, RB_RED);
                rb_rotate_left(xp);
                w = unmapped[xp].fe_right;
            }
            if ((unmapped[w].fe_left == FREENODE_NIL || rb_is_black(unmapped[w].fe_left)) &&
                (unmapped[w].fe_right == FREENODE_NIL || rb_is_black(unmapped[w].fe_right))) {
                rb_set_color(w, RB_RED);
                x = xp;
                xp = rb_parent(x);
            } else {
                if (unmapped[w].fe_right == FREENODE_NIL || rb_is_black(unmapped[w].fe_right)) {
                    if (unmapped[w].fe_left != FREENODE_NIL) {
                        rb_set_color(unmapped[w].fe_left, RB_BLACK);
                    }
                    rb_set_color(w, RB_RED);
                    rb_rotate_right(w);
                    w = unmapped[xp].fe_right;
                }
                rb_set_color(w, rb_color(xp));
                rb_set_color(xp, RB_BLACK);
                if (unmapped[w].fe_right != FREENODE_NIL) {
                    rb_set_color(unmapped[w].fe_right, RB_BLACK);
                }
                rb_rotate_left(xp);
                x = map_root;
                xp = FREENODE_NIL;
            }
        } else {
            uint16_t w = unmapped[xp].fe_left;
            if (rb_is_red(w)) {
                rb_set_color(w, RB_BLACK);
                rb_set_color(xp, RB_RED);
                rb_rotate_right(xp);
                w = unmapped[xp].fe_left;
            }
            if ((unmapped[w].fe_right == FREENODE_NIL || rb_is_black(unmapped[w].fe_right)) &&
                (unmapped[w].fe_left == FREENODE_NIL || rb_is_black(unmapped[w].fe_left))) {
                rb_set_color(w, RB_RED);
                x = xp;
                xp = rb_parent(x);
            } else {
                if (unmapped[w].fe_left == FREENODE_NIL || rb_is_black(unmapped[w].fe_left)) {
                    if (unmapped[w].fe_right != FREENODE_NIL) {
                        rb_set_color(unmapped[w].fe_right, RB_BLACK);
                    }
                    rb_set_color(w, RB_RED);
                    rb_rotate_left(w);
                    w = unmapped[xp].fe_left;
                }
                rb_set_color(w, rb_color(xp));
                rb_set_color(xp, RB_BLACK);
                if (unmapped[w].fe_left != FREENODE_NIL) {
                    rb_set_color(unmapped[w].fe_left, RB_BLACK);
                }
                rb_rotate_right(xp);
                x = map_root;
                xp = FREENODE_NIL;
            }
        }
    }
    if (x != FREENODE_NIL) {
        rb_set_color(x, RB_BLACK);
    }
}

static void map_remove(uint16_t idx) {
    uint16_t y = idx;
    uint16_t x;
    uint16_t xp;
    uint16_t y_orig_color = rb_color(y);

    if (unmapped[idx].fe_left == FREENODE_NIL) {
        x = unmapped[idx].fe_right;
        xp = rb_parent(idx);
        rb_transplant(idx, x);
        propagate_amax(xp);
    } else if (unmapped[idx].fe_right == FREENODE_NIL) {
        x = unmapped[idx].fe_left;
        xp = rb_parent(idx);
        rb_transplant(idx, x);
        propagate_amax(xp);
    } else {
        y = rb_minimum(unmapped[idx].fe_right);
        y_orig_color = rb_color(y);
        x = unmapped[y].fe_right;
        if (rb_parent(y) == idx) {
            xp = y;
        } else {
            xp = rb_parent(y);
            rb_transplant(y, x);
            unmapped[y].fe_right = unmapped[idx].fe_right;
            if (unmapped[y].fe_right != FREENODE_NIL) {
                rb_set_parent(unmapped[y].fe_right, y);
            }
            propagate_amax(xp);
        }
        rb_transplant(idx, y);
        unmapped[y].fe_left = unmapped[idx].fe_left;
        if (unmapped[y].fe_left != FREENODE_NIL) {
            rb_set_parent(unmapped[y].fe_left, y);
        }
        rb_set_color(y, rb_color(idx));
        unmapped[y].fe_amax = amax_of(y);
        propagate_amax(rb_parent(y));
    }

    if (y_orig_color == RB_BLACK) {
        rb_remove_fixup(x, xp);
    }
}

/* -------------------------------------------------------------- *
 * Traversal
 * -------------------------------------------------------------- */

static uint16_t rb_last(uint16_t root) {
    if (root == FREENODE_NIL) {
        return FREENODE_NIL;
    }
    while (unmapped[root].fe_right != FREENODE_NIL) {
        root = unmapped[root].fe_right;
    }
    return root;
}

static uint16_t rb_prev(uint16_t idx) {
    if (unmapped[idx].fe_left != FREENODE_NIL) {
        idx = unmapped[idx].fe_left;
        while (unmapped[idx].fe_right != FREENODE_NIL) {
            idx = unmapped[idx].fe_right;
        }
        return idx;
    }

    {
        uint16_t p = rb_parent(idx);
        while (p != FREENODE_NIL && idx == unmapped[p].fe_left) {
            idx = p;
            p = rb_parent(idx);
        }
        return p;
    }
}

/* Single descent: smallest fe_base >= target_key, or FREENODE_NIL if
 * every entry's base is smaller. Combined with rb_prev(succ) (or
 * rb_last() when succ is NIL), this replaces a two-descent neighbor
 * search for arena_unmap's coalescing: one descent plus an O(height)
 * hop, never two full descents. */
static uint16_t rb_lower_bound(uint16_t root, uint16_t target_key) {
    uint16_t x = root;
    uint16_t succ = FREENODE_NIL;

    while (x != FREENODE_NIL) {
        if (UNSIGNED_CMP(unmapped[x].fe_base, target_key) >= 0) {
            succ = x;
            x = unmapped[x].fe_left;
        } else {
            x = unmapped[x].fe_right;
        }
    }
    return succ;
}

/* -------------------------------------------------------------- *
 * Free-node pool
 * -------------------------------------------------------------- */

static void free_mapnode(uint16_t idx) {
    struct free_entry* entry = &unmapped[idx];

    entry->fe_left = free_head;
    entry->fe_right = FREENODE_NIL;
    entry->fe_parent = FREENODE_NIL;
    entry->fe_base = ARENA_PAGE_NIL;
    entry->fe_size = 0;
    entry->fe_amax = 0;

    free_head = idx;
}

static uint16_t alloc_mapnode(void) {
    uint16_t node = free_head;

    if (node != FREENODE_NIL) {
        free_head = unmapped[node].fe_left;
        unmapped[node].fe_left = FREENODE_NIL;
    }

    return node;
}

/* -------------------------------------------------------------- *
 * Public API
 * -------------------------------------------------------------- */

void arena_initmapper(void) {
    uint16_t first;

    for (uint16_t i = 0; i < ARENA_MAX_PAGES; i++) {
        arenas[i] = NULL;
    }

    free_head = FREENODE_NIL;
    for (uint16_t idx = ARENA_MAX_FREENODES - 1; idx < ARENA_MAX_FREENODES; idx--) {
        free_mapnode(idx);
    }

    map_root = FREENODE_NIL;

    first = alloc_mapnode();
    unmapped[first].fe_base = 0;
    unmapped[first].fe_size = ARENA_MAX_PAGES;
    map_insert(first);

    /* XXX initialize lock after Lock subsystem refactor */
}

void* arena_lookup64(uint32_t address, Bhdr** arena) {
    void* v = NULL;
    Bhdr* h;
    uint32_t offset;
    uint16_t page = (uint16_t)(address >> ARENA_PAGESHIFT);

    lock(&map_lock);
    h = arenas[page];

    if (h != NULL) {
        offset = address - h->bhl_mapoffset;
        v = (void*)((uintptr_t)h + (uintptr_t)offset);
    }

    unlock(&map_lock);

    if (arena != NULL) {
        *arena = h;
    }

    return v;
}

uint32_t arena_lookup32(Bhdr* arena, void* v) {
    uint32_t offset = (uint32_t)((uintptr_t)v - (uintptr_t)arena);
    return arena->bhl_mapoffset + offset;
}

void arena_unmap(Bhdr* arena) {
    uintptr_t dataOffset;
    uint16_t start;
    uint16_t end;
    uint16_t pred;
    uint16_t succ;
    uint16_t next;
    uint16_t size = 0;
    uint16_t freed;

    lock(&map_lock);

    dataOffset = ARENA_DATAOFFSET(arena);
    start = (uint16_t)(arena_lookup32(arena, (void*)((uintptr_t)arena + dataOffset)) >> ARENA_PAGESHIFT);
    end = (uint16_t)(arena_lookup32(arena, (void*)((uintptr_t)BHDR2BCHK(arena->bhl_trail) - 1)) >> ARENA_PAGESHIFT);

    if ((arena->bh_magic & BF_MAPPED) == 0) {
        inferno_panic("arena_unmap: not mapped %p", arena);
    }

    /* The underlying host memory is about to be freed by the caller
     * regardless of what happens below - arenas[] must never keep
     * pointing at it, even if the free-node pool turns out to be
     * exhausted (in which case the virtual space is simply lost, not
     * a dangling pointer, which would be worse). */
    for (uint16_t page = start; page <= end; page++) {
        arenas[page] = NULL;
        size++;
    }

    arena->bh_magic &= ~BF_MAPPED;

    next = start + size;
    succ = rb_lower_bound(map_root, next);
    pred = (succ != FREENODE_NIL) ? rb_prev(succ) : rb_last(map_root);
    freed = FREENODE_NIL;

    if ((succ != FREENODE_NIL) && (unmapped[succ].fe_base == next)) {
        map_remove(succ);
        size += unmapped[succ].fe_size;
        freed = succ;
    }

    if ((pred != FREENODE_NIL) && ((unmapped[pred].fe_base + unmapped[pred].fe_size) == start)) {
        map_remove(pred);
        size += unmapped[pred].fe_size;
        start = unmapped[pred].fe_base;

        if (freed == FREENODE_NIL) {
            freed = pred;
        } else {
            free_mapnode(pred);
        }
    }

    if (freed == FREENODE_NIL) {
        freed = alloc_mapnode();
    }

    if (freed != FREENODE_NIL) {
        unmapped[freed].fe_base = start;
        unmapped[freed].fe_size = size;
        map_insert(freed);
    }

    unlock(&map_lock);
}

static int arena_pagecount(Bhdr* arena, uint16_t* out_pages) {
    uintptr_t dataOffset = ARENA_DATAOFFSET(arena);
    uintptr_t size = (uintptr_t)BHDR2BCHK(arena->bhl_trail) - ((uintptr_t) arena + dataOffset);
    uintptr_t pages;

    if (size & (uintptr_t)ARENA_PAGEMASK) {
        return 1; /* no partial page allowed */
    }

    pages = size >> ARENA_PAGESHIFT;

    if (pages >= (uintptr_t)ARENA_MAX_PAGES) {
        /* arena size is above 4Gb */
        return 1;
    }

    *out_pages = (uint16_t)pages;
    return 0;
}

static int arena_map_fixed_internal(Bhdr* arena, uint16_t base) {
    uint16_t pages;
    uint16_t entry;
    uint16_t succ;
    uint16_t before;
    uint16_t after;
    uint16_t newnode;
    uint16_t mapend;

    if (arena->bh_magic & BF_MAPPED) {
        return 1;
    }

    if (base >= ARENA_MAX_PAGES) {
        return 1;
    }
    if (arena_pagecount(arena, &pages) != 0 || pages == 0) {
        return 1;
    }
    if (pages > (ARENA_MAX_PAGES - base)) {
        return 1;
    }

    mapend = base + pages;
    succ = rb_lower_bound(map_root, base);
    entry = (succ != FREENODE_NIL && unmapped[succ].fe_base == base)
        ? succ
        : ((succ != FREENODE_NIL) ? rb_prev(succ) : rb_last(map_root));

    if (entry == FREENODE_NIL) {
        return 1;
    }
    if (unmapped[entry].fe_base + unmapped[entry].fe_size < mapend) {
        return 1;
    }

    before = base - unmapped[entry].fe_base;
    after = (unmapped[entry].fe_base + unmapped[entry].fe_size) - (mapend);

    newnode = FREENODE_NIL;

    if (before != 0 && after != 0) {
        /* Double split: secure the missing node before touching
         * anything irreversible. */
        newnode = alloc_mapnode();
        if (newnode == FREENODE_NIL) {
            return 1;
        }
    }

    /* From here on, nothing can fail: commit. */

    if (before == 0 && after == 0) {
        map_remove(entry);
        free_mapnode(entry);
    } else if (before == 0) {
        map_remove(entry);
        unmapped[entry].fe_base = mapend;
        unmapped[entry].fe_size = after;
        map_insert(entry);
    } else if (after == 0) {
        unmapped[entry].fe_size = before;
        propagate_amax(entry);
    } else {
        unmapped[entry].fe_size = before;
        propagate_amax(entry);
        unmapped[newnode].fe_base = mapend;
        unmapped[newnode].fe_size = after;
        map_insert(newnode);
    }

    arena->bhl_mapoffset = ((uint32_t)base << ARENA_PAGESHIFT) - (uint32_t)ARENA_DATAOFFSET(arena);
    arena->bh_magic |= BF_MAPPED;

    for (uint16_t page = base; page < mapend; page++) {
        arenas[page] = arena;
    }

    return 0;
}

int arena_map_fixed(Bhdr* arena, uint16_t base) {
    int result;

    lock(&map_lock);
    result = arena_map_fixed_internal(arena, base);
    unlock(&map_lock);

    return result;
}

static void map_range_search(uint16_t node, struct map_range_search_state *state) {
    uint16_t candidate_base, candidate_end, candidate_size;
    uint16_t raw_base, raw_end;

    if (node == FREENODE_NIL) {
        return; /* given node is not valid */
    }

    if (state->best_entry != FREENODE_NIL && state->best_size == state->page_cnt) {
        return; /* the best possible entry has been found, do not do anything */
    }

    if (unmapped[node].fe_amax < state->page_cnt) {
        return; /* size pruning: nothing in this subtree is big enough */
    }

    /* current node bounds */
    raw_base = candidate_base = unmapped[node].fe_base;
    raw_end = candidate_end = candidate_base + unmapped[node].fe_size;

    /* current node clamping against mapping window */
    candidate_base = raw_base < state->page_min ? state->page_min : raw_base;
    candidate_end = raw_end > state->page_max ? state->page_max : raw_end;

    if (candidate_base < candidate_end) {
        /* compute clamped size */
        candidate_size = candidate_end - candidate_base;

        /* check if enough room is remaining in this node */
        if (state->page_cnt <= candidate_size) {
            /* check if this is the actual best fit */
            if (state->best_entry == FREENODE_NIL || candidate_size < state->best_size) {
                state->best_entry = node;
                state->best_base = candidate_base;
                state->best_size = candidate_size;
            }
        }
    }

    /* Left spatial pruning: free entries are disjoint and sorted by
     * fe_base, so every entry in the left subtree ends at or before
     * this node's own fe_base. If this node already starts at or
     * before page_min, the whole left subtree ends at or before
     * page_min too - not worth exploring. */
    if (raw_base > state->page_min) {
        map_range_search(unmapped[node].fe_left, state);
    }

    /* Right spatial pruning: symmetrically, every entry in the right
     * subtree starts at or after this node's own end. If this node
     * already ends at or after page_max, the whole right subtree
     * starts at or after page_max too. */
    if (raw_end < state->page_max) {
        map_range_search(unmapped[node].fe_right, state);
    }
}

int arena_map_range(Bhdr* arena, uint32_t addr_min, uint32_t addr_max) {
    struct map_range_search_state search;
    int result;

    uint32_t addr_diff;
    uint16_t psize;

    search.best_entry = FREENODE_NIL;
    search.best_base = 0;

    if (arena->bh_magic & BF_MAPPED) {
        return 1;
    }

    if (arena_pagecount(arena, &search.page_cnt) != 0 || search.page_cnt == 0) {
        return 1;
    }

    search.page_min = (uint16_t)(addr_min >> ARENA_PAGESHIFT);
    if (addr_min & ARENA_PAGEMASK) {
        search.page_min++; /* never overflows a uint16_t: max is 32767+1=32768 */
    }

    if (search.page_min >= ARENA_MAX_PAGES) {
        return 1;
    }

    addr_min = (uint32_t)search.page_min << ARENA_PAGESHIFT; /* page-align addr_min */

    /* Note: addr_max is inclusive */
    if (addr_max <= addr_min) {
        return 1;
    }

    addr_diff = addr_max - addr_min;

    if (addr_diff < 0xffffffffU) {
        psize = (uint16_t)((addr_diff + 1) >> ARENA_PAGESHIFT);
    } else {
        /* should never occur (request whole 4Gb space mapping) */
        psize = ARENA_MAX_PAGES;
    }

    if (search.page_cnt > psize) {
        return 1;
    }

    search.page_max = search.page_min + psize;

    lock(&map_lock);
    map_range_search(map_root, &search);

    if (search.best_entry == FREENODE_NIL) {
        unlock(&map_lock);
        return 1;
    }

    result = arena_map_fixed_internal(arena, search.best_base);

    unlock(&map_lock);

    return result;
}
