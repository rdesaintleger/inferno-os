#include <inferno/bhdr.h>
#include <inferno/pool.h>

/* -------------------------------------------------------------- *
 * Red-Black tree keyed by bh_size, augmented with a 3-bit arena-flag
 * presence mask (ARENA_FLAGS_MASK) packed alongside the RB color bit
 * in the low nibble of bh_magic (BFLAGS_MASK).
 * -------------------------------------------------------------- */

#define RB_COLOR_BIT ((uint32_t) 0x08) /* last bit of the low nibble */
#define RB_RED       ((uint32_t) 0x00)
#define RB_BLACK     RB_COLOR_BIT

enum {
    ARENA_FLAGS_MASK = BFLAGS_MASK & ~RB_COLOR_BIT
};

#define rb_color(n)        ((n)->bh_magic & RB_COLOR_BIT)
#define rb_is_red(n)       (rb_color(n) == RB_RED)
#define rb_is_black(n)     (rb_color(n) == RB_BLACK)
#define rb_set_color(n, c) \
    ((n)->bh_magic = ((n)->bh_magic & ~RB_COLOR_BIT) | (c))

/* NULL is conventionally black, like a classic RB sentinel leaf */
#define rb_node_is_red(n)   ((n) != NULL && rb_is_red(n))
#define rb_node_is_black(n) ((n) == NULL || rb_is_black(n))

#define rb_fmask(n) ((uint32_t)((n)->bh_magic & ARENA_FLAGS_MASK))
#define rb_set_fmask(n, m) \
    ((n)->bh_magic = ((n)->bh_magic & ~(uint32_t)ARENA_FLAGS_MASK) | ((m) & ARENA_FLAGS_MASK))

#define arena_flags_of(t) ((uint32_t)((t)->bhf_lead->bh_magic & ARENA_FLAGS_MASK))

/* -------------------------------------------------------------- *
 * Flag-mask maintenance
 * -------------------------------------------------------------- */

/* OR of arena_flags_of() over every node in t's duplicate (same
 * bh_size) circular list, t included. Duplicates hang off a single
 * tree node via bhf_succ/bhf_pred and are otherwise invisible to the
 * tree structure - this is the "self" contribution a tree node brings
 * to its own subtree mask. */
static uint32_t dupchain_mask(Bhdr* t) {
    uint32_t m = 0;
    Bhdr* c = t;

    do {
        m |= arena_flags_of(c);
        c = c->bhf_succ;
    } while (c != t);

    return m;
}

static uint32_t fmask_of(Bhdr* node) {
    uint32_t m = dupchain_mask(node);

    if (node->bhf_left != NULL) {
        m |= rb_fmask(node->bhf_left);
    }
    if (node->bhf_right != NULL) {
        m |= rb_fmask(node->bhf_right);
    }

    return m;
}

/* Recomputes node's own mask (dup-chain + children) and walks toward
 * the root redoing the same for every ancestor, stopping as soon as a
 * level's value doesn't change (higher ancestors can't be affected
 * either). Unlike mapper.c's propagate_amax, this also recomputes
 * node itself - needed because a node's own contribution can change
 * on its own (a duplicate joining/leaving its circular list) without
 * any structural tree change happening at that node. */
static void fixup_fmask(Bhdr* node) {
    while (node != NULL) {
        uint32_t m = fmask_of(node);

        if (m == rb_fmask(node)) {
            break;
        }

        rb_set_fmask(node, m);
        node = node->bhf_parent;
    }
}

/* -------------------------------------------------------------- *
 * Rotations. Self-contained like mapper.c's: a rotation rearranges
 * the SAME set of nodes under x/y, so recomputing the mask for the
 * pair afterward reproduces exactly what was already true beforehand
 * - nothing beyond x/y ever needs to be touched here.
 * -------------------------------------------------------------- */

static void rb_rotate_left(Pool* p, Bhdr* x) {
    Bhdr* y = x->bhf_right;
    Bhdr* pp = x->bhf_parent;

    x->bhf_right = y->bhf_left;
    if (y->bhf_left != NULL) {
        y->bhf_left->bhf_parent = x;
    }
    y->bhf_parent = pp;
    if (pp == NULL) {
        p->root = y;
    } else if (x == pp->bhf_left) {
        pp->bhf_left = y;
    } else {
        pp->bhf_right = y;
    }
    y->bhf_left = x;
    x->bhf_parent = y;

    rb_set_fmask(x, fmask_of(x));
    rb_set_fmask(y, fmask_of(y));
}

static void rb_rotate_right(Pool* p, Bhdr* x) {
    Bhdr* y = x->bhf_left;
    Bhdr* pp = x->bhf_parent;

    x->bhf_left = y->bhf_right;
    if (y->bhf_right != NULL) {
        y->bhf_right->bhf_parent = x;
    }
    y->bhf_parent = pp;
    if (pp == NULL) {
        p->root = y;
    } else if (x == pp->bhf_right) {
        pp->bhf_right = y;
    } else {
        pp->bhf_left = y;
    }
    y->bhf_right = x;
    x->bhf_parent = y;

    rb_set_fmask(x, fmask_of(x));
    rb_set_fmask(y, fmask_of(y));
}

/* -------------------------------------------------------------- *
 * Insertion
 * -------------------------------------------------------------- */

static void rb_insert_fixup(Pool* p, Bhdr* z) {
    while (z->bhf_parent != NULL && rb_is_red(z->bhf_parent)) {
        Bhdr* pn = z->bhf_parent;
        Bhdr* g = pn->bhf_parent;

        if (pn == g->bhf_left) {
            Bhdr* u = g->bhf_right;

            if (rb_node_is_red(u)) {
                rb_set_color(pn, RB_BLACK);
                rb_set_color(u, RB_BLACK);
                rb_set_color(g, RB_RED);
                z = g;
            } else {
                if (z == pn->bhf_right) {
                    z = pn;
                    rb_rotate_left(p, z);
                    pn = z->bhf_parent;
                    g = pn->bhf_parent;
                }
                rb_set_color(pn, RB_BLACK);
                rb_set_color(g, RB_RED);
                rb_rotate_right(p, g);
            }
        } else {
            Bhdr* u = g->bhf_left;

            if (rb_node_is_red(u)) {
                rb_set_color(pn, RB_BLACK);
                rb_set_color(u, RB_BLACK);
                rb_set_color(g, RB_RED);
                z = g;
            } else {
                if (z == pn->bhf_left) {
                    z = pn;
                    rb_rotate_right(p, z);
                    pn = z->bhf_parent;
                    g = pn->bhf_parent;
                }
                rb_set_color(pn, RB_BLACK);
                rb_set_color(g, RB_RED);
                rb_rotate_left(p, g);
            }
        }
    }
    rb_set_color(p->root, RB_BLACK);
}

void pooladd(Pool* p, Bhdr* q, Bhdr* lead) {
    Bhdr* x = p->root;
    Bhdr* y = NULL;
    size_t size;

    /* MAGIC_F's low nibble is 0 by construction: color defaults to
     * RB_RED and fmask to 0 in the same write, both fixed up below. */
    q->bh_magic = MAGIC_F;
    q->bhf_lead = lead;
    lead->bhl_freecnt++;

    q->bhf_left = NULL;
    q->bhf_right = NULL;
    q->bhf_parent = NULL;
    q->bhf_succ = q;
    q->bhf_pred = q;

    size = q->bh_size;

    while (x != NULL) {
        if (size == x->bh_size) {
            /* Duplicate size: join the circular list, tree shape
             * unaffected - no rotation, no color/rebalance work. */
            q->bhf_pred = x->bhf_pred;
            q->bhf_pred->bhf_succ = q;
            q->bhf_succ = x;
            x->bhf_pred = q;

            fixup_fmask(x);
            return;
        }
        y = x;
        x = (size < x->bh_size) ? x->bhf_left : x->bhf_right;
    }

    q->bhf_parent = y;
    if (y == NULL) {
        p->root = q;
    } else if (size < y->bh_size) {
        y->bhf_left = q;
    } else {
        y->bhf_right = q;
    }

    rb_set_color(q, RB_RED);
    rb_set_fmask(q, dupchain_mask(q)); /* leaf: no children yet */

    fixup_fmask(y);
    rb_insert_fixup(p, q);
}

/* -------------------------------------------------------------- *
 * Removal
 * -------------------------------------------------------------- */

static void rb_transplant(Pool* p, Bhdr* u, Bhdr* v) {
    Bhdr* pu = u->bhf_parent;

    if (pu == NULL) {
        p->root = v;
    } else if (u == pu->bhf_left) {
        pu->bhf_left = v;
    } else {
        pu->bhf_right = v;
    }
    if (v != NULL) {
        v->bhf_parent = pu;
    }
}

static Bhdr* rb_minimum(Bhdr* x) {
    while (x->bhf_left != NULL) {
        x = x->bhf_left;
    }
    return x;
}

static void rb_remove_fixup(Pool* p, Bhdr* x, Bhdr* xp) {
    while (x != p->root && rb_node_is_black(x)) {
        if (x == xp->bhf_left) {
            Bhdr* w = xp->bhf_right;

            if (rb_node_is_red(w)) {
                rb_set_color(w, RB_BLACK);
                rb_set_color(xp, RB_RED);
                rb_rotate_left(p, xp);
                w = xp->bhf_right;
            }
            if (rb_node_is_black(w->bhf_left) && rb_node_is_black(w->bhf_right)) {
                rb_set_color(w, RB_RED);
                x = xp;
                xp = x->bhf_parent;
            } else {
                if (rb_node_is_black(w->bhf_right)) {
                    if (w->bhf_left != NULL) {
                        rb_set_color(w->bhf_left, RB_BLACK);
                    }
                    rb_set_color(w, RB_RED);
                    rb_rotate_right(p, w);
                    w = xp->bhf_right;
                }
                rb_set_color(w, rb_color(xp));
                rb_set_color(xp, RB_BLACK);
                if (w->bhf_right != NULL) {
                    rb_set_color(w->bhf_right, RB_BLACK);
                }
                rb_rotate_left(p, xp);
                x = p->root;
                xp = NULL;
            }
        } else {
            Bhdr* w = xp->bhf_left;

            if (rb_node_is_red(w)) {
                rb_set_color(w, RB_BLACK);
                rb_set_color(xp, RB_RED);
                rb_rotate_right(p, xp);
                w = xp->bhf_left;
            }
            if (rb_node_is_black(w->bhf_right) && rb_node_is_black(w->bhf_left)) {
                rb_set_color(w, RB_RED);
                x = xp;
                xp = x->bhf_parent;
            } else {
                if (rb_node_is_black(w->bhf_left)) {
                    if (w->bhf_right != NULL) {
                        rb_set_color(w->bhf_right, RB_BLACK);
                    }
                    rb_set_color(w, RB_RED);
                    rb_rotate_left(p, w);
                    w = xp->bhf_left;
                }
                rb_set_color(w, rb_color(xp));
                rb_set_color(xp, RB_BLACK);
                if (w->bhf_left != NULL) {
                    rb_set_color(w->bhf_left, RB_BLACK);
                }
                rb_rotate_right(p, xp);
                x = p->root;
                xp = NULL;
            }
        }
    }
    if (x != NULL) {
        rb_set_color(x, RB_BLACK);
    }
}

void pooldel(Pool* p, Bhdr* t) {
    Bhdr* y;
    Bhdr* x;
    Bhdr* xp;
    uint32_t y_orig_color;

    t->bhf_lead->bhl_freecnt--;

    if (t->bhf_succ != t) {
        if (t->bhf_parent == NULL && p->root != t) {
            /* Secondary duplicate: unlink from the circular list. The
             * tree structure is unaffected, but the primary node's
             * dup-chain contribution just shrank - its mask (and its
             * ancestors') must be recomputed. The primary is the one
             * chain member linked into the tree (has a parent, or is
             * the root); secondaries have neither. */
            Bhdr* primary = t->bhf_succ;

            while (primary->bhf_parent == NULL && p->root != primary) {
                primary = primary->bhf_succ;
            }

            t->bhf_pred->bhf_succ = t->bhf_succ;
            t->bhf_succ->bhf_pred = t->bhf_pred;

            fixup_fmask(primary);
            return;
        }

        /* Primary (tree-linked) node with duplicates: promote the
         * next one into the tree slot. Its own mask depends on its
         * new children and its own (now shorter) duplicate chain, so
         * it must be recomputed - never copied from t. */
        {
            Bhdr* f = t->bhf_succ;

            f->bhf_left = t->bhf_left;
            if (f->bhf_left != NULL) {
                f->bhf_left->bhf_parent = f;
            }
            f->bhf_right = t->bhf_right;
            if (f->bhf_right != NULL) {
                f->bhf_right->bhf_parent = f;
            }
            rb_set_color(f, rb_color(t)); /* color is structural, must be preserved */

            /* f takes t's place: its stored mask must be what t's
             * ancestors currently see (t's old mask), otherwise
             * fixup_fmask would compare against a meaningless value
             * (a secondary duplicate's stored mask is never
             * maintained) and could stop too early. */
            rb_set_fmask(f, rb_fmask(t));

            rb_transplant(p, t, f);

            t->bhf_pred->bhf_succ = t->bhf_succ;
            t->bhf_succ->bhf_pred = t->bhf_pred;

            fixup_fmask(f);
        }
        return;
    }

    /* Standard RB node removal (no duplicates) */
    y = t;
    y_orig_color = rb_color(y);

    if (t->bhf_left == NULL) {
        x = t->bhf_right;
        xp = t->bhf_parent;
        rb_transplant(p, t, x);
        fixup_fmask(xp);
    } else if (t->bhf_right == NULL) {
        x = t->bhf_left;
        xp = t->bhf_parent;
        rb_transplant(p, t, x);
        fixup_fmask(xp);
    } else {
        y = rb_minimum(t->bhf_right);
        y_orig_color = rb_color(y);
        x = y->bhf_right;

        if (y->bhf_parent == t) {
            xp = y;
        } else {
            xp = y->bhf_parent;
            rb_transplant(p, y, x);
            y->bhf_right = t->bhf_right;
            y->bhf_right->bhf_parent = y;
        }

        rb_transplant(p, t, y);
        y->bhf_left = t->bhf_left;
        y->bhf_left->bhf_parent = y;
        rb_set_color(y, rb_color(t));

        /* y now sits where t was: what t's ancestors currently see is
         * t's old mask, so y inherits it before propagating (same
         * reason as the promotion path above). Two walks are needed,
         * both on the fully relinked structure:
         * - from xp, for the nodes between y's old position and y
         *   (their subtree lost y);
         * - from y itself, unconditionally: the first walk may stop
         *   early (unchanged value) before ever reaching y, yet y
         *   replaces t with a different duplicate chain and different
         *   children, so y and t's old ancestors must be recomputed
         *   regardless. */
        rb_set_fmask(y, rb_fmask(t));
        if (xp != y) {
            fixup_fmask(xp);
        }
        fixup_fmask(y);
    }

    if (y_orig_color == RB_BLACK) {
        rb_remove_fixup(p, x, xp);
    }
}

/* -------------------------------------------------------------- *
 * Combined (size, flags) search - exact flag match, best (smallest
 * sufficient) size. Same spirit as mapper.c's map_range_search: the
 * tree's own key (bh_size) already prunes by size via ordering, but
 * once flags enter the picture a single-path descent is no longer
 * enough (the flag-matching candidate may sit on the "wrong" side of
 * an otherwise-better-sized but non-matching node) - both children
 * must be considered whenever they might still hold a match; spatial
 * pruning is replaced by the flag-presence mask.
 * -------------------------------------------------------------- */

/* Per-block acceptance test beyond the flags. The default is a plain
 * size check; pool.c-side integration defines it to also honor the
 * address-dependent header alignment padding (BALIGN_SIZE16) that
 * chunksplitblock will apply - that padding depends on the block's own
 * address, so it must be evaluated per block, never per tree node. It
 * doesn't affect BST ordering or the flag mask, only which members of
 * a (size, flags)-eligible set are actually usable. */
#define POOL_BLOCK_FITS(c, size) \
    ((c)->bh_size == (size) || \
     BALIGN_SIZE16((c)->bhf_lead, (c), (size), offsetof(Bchk, bc_self.bha_data)) <= (c)->bh_size)

static Bhdr* dupchain_find(Bhdr* node, size_t size, uint32_t iflags) {
    Bhdr* c;

    if (node->bh_size < size) {
        return NULL;
    }

    c = node;
    do {
        if (arena_flags_of(c) == iflags && POOL_BLOCK_FITS(c, size)) {
            return c;
        }
        c = c->bhf_succ;
    } while (c != node);

    return NULL;
}

static void pool_search(Bhdr* node, size_t size, uint32_t iflags, Bhdr** best) {
    Bhdr* m;

    if (node == NULL) {
        return;
    }

    if (*best != NULL && (*best)->bh_size == size) {
        return; /* exact-size match already found, nothing can beat it */
    }

    if ((rb_fmask(node) & iflags) != iflags) {
        return; /* not every requested flag is present anywhere in this subtree */
    }

    m = dupchain_find(node, size, iflags);
    if (m != NULL && (*best == NULL || node->bh_size < (*best)->bh_size)) {
        *best = m;
    }

    /* Left: every element there has bh_size <= node->bh_size, so only
     * worth visiting if that upper bound still leaves room for a
     * sufficient candidate. */
    if (size <= node->bh_size) {
        pool_search(node->bhf_left, size, iflags, best);
    }

    /* Right: every element there has bh_size >= node->bh_size, so only
     * worth visiting if node's own size could still be improved upon
     * (otherwise every candidate over there is provably no better). */
    if (*best == NULL || node->bh_size < (*best)->bh_size) {
        pool_search(node->bhf_right, size, iflags, best);
    }
}

Bhdr* poolfindbest(Pool* p, size_t size, uint32_t iflags) {
    Bhdr* best = NULL;

    pool_search(p->root, size, iflags, &best);
    return best;
}
