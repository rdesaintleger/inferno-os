#include <inferno/bhdr.h>
#include <inferno/pool.h>

/* Directions and subtree helpers */
#define AVLFREE_LEFT          0
#define AVLFREE_RIGHT         1
#define AVLFREE_OPPOSITE(d)   (1 - (d))

/* Access child node by direction */
#define AVLFREE_CHILD(node, dir) \
	((dir) == AVLFREE_LEFT ? (node)->bhf_left : (node)->bhf_right)

/*
 * Balance factor storage: packed into the low 3 bits of bh_magic, with
 * BAL_BIAS (4) as the raw value representing signed balance 0. The
 * algorithm below never converts back to a signed value -- it works
 * entirely in raw space, comparing against BAL_BIAS and BAL_BIAS+-1
 * instead of 0 and +-1. A +-1 delta on the signed balance is the same
 * bit pattern as a +-1 delta on the raw nibble (additive bias commutes
 * with addition), so BAL_ADD needs no mask. Only absolute writes
 * (BAL_RAW_SET) and same-type nibble copies (BAL_COPY) touch bh_magic
 * outside of a plain add.
 *
 * All base magics (MAGIC_A/F/L/E) end in a zero nibble by construction,
 * so ORing BAL_BIAS into a freshly-typed magic sets balance to 0 in a
 * single instruction (see pooladd).
 */
#define BAL_BIAS 4

#define BAL_RAW(b)          ((int)((b)->bh_magic & BFLAGS_MASK))
#define BAL_RAW_SET(b, r)   ((b)->bh_magic = ((b)->bh_magic & BMAGIC_MASK) | \
                             ((uint32_t)(r) & BFLAGS_MASK))
#define BAL_ADD(b, delta)   ((b)->bh_magic += (delta))
#define BAL_COPY(dst, src)  ((dst)->bh_magic = ((dst)->bh_magic & BMAGIC_MASK) | \
                             ((src)->bh_magic & BFLAGS_MASK))

/* Balance check on a raw nibble: unbalanced iff signed value is +-2 */
#define AVLFREE_RAW_UNBALANCED(r) ((r) > (BAL_BIAS + 1) || (r) < (BAL_BIAS - 1))

/* Set child pointer of a node according to direction */
static void avlfree_set_child(Bhdr* node, int dir, Bhdr* child) {
    if (dir == AVLFREE_LEFT) {
        node->bhf_left = child;
    } else {
        node->bhf_right = child;
    }
}

/* Replace old_node with new_node under parent (updates *root if parent is NULL) */
static void avlfree_replace_child(Bhdr** root, Bhdr* parent, Bhdr* old_node, Bhdr* new_node) {
    if (parent == NULL) {
        *root = new_node;
    } else if (parent->bhf_left == old_node) {
        parent->bhf_left = new_node;
    } else {
        parent->bhf_right = new_node;
    }

    if (new_node != NULL) {
        new_node->bhf_parent = parent;
    }
}

/* Generic rotation: dir = AVLFREE_RIGHT promotes right child (left rotation), dir = AVLFREE_LEFT promotes left child (right rotation) */
static Bhdr* avlfree_rotate(Bhdr** root, Bhdr* x, int dir) {
    int opp = AVLFREE_OPPOSITE(dir);
    Bhdr* y = AVLFREE_CHILD(x, dir);
    Bhdr* parent = x->bhf_parent;
    Bhdr* sub = AVLFREE_CHILD(y, opp);

    avlfree_set_child(x, dir, sub);
    if (sub != NULL) {
        sub->bhf_parent = x;
    }

    avlfree_set_child(y, opp, x);
    x->bhf_parent = y;
    y->bhf_parent = parent;

    avlfree_replace_child(root, parent, x, y);

    return y;
}

/* Rebalance a node with a raw balance nibble of BAL_BIAS+-2. Returns 1 if subtree height decreased, 0 otherwise. */
static int avlfree_rebalance(Bhdr** root, Bhdr* q, int dir) {
    /* raw value of "balance == dir's sign" and of "balance == opposite
     * sign" -- branchless: dir is 0 or 1, so this is a shift+add, no
     * conditional. dirraw/oppraw replace the old +-s pair entirely. */
    int dirraw = (BAL_BIAS - 1) + (dir << 1);   /* RIGHT->(BAL_BIAS + 1), LEFT->(BAL_BIAS - 1) */
    int oppraw = (BAL_BIAS + 1) - (dir << 1);   /* RIGHT->(BAL_BIAS - 1), LEFT->(BAL_BIAS + 1) */
    int opp = AVLFREE_OPPOSITE(dir);
    Bhdr* c = AVLFREE_CHILD(q, dir);
    int craw = BAL_RAW(c);

    /* c's balance is always in {-1,0,1} here (only q was unbalanced),
     * so "same sign as dir, or zero" reduces to "not the opposite raw
     * value" -- a single equality test, cheaper than the original
     * multiply-and-compare. */
    if (craw != oppraw) {
        /* Single rotation (LL or RR) */
        if (craw == BAL_BIAS) {
            BAL_RAW_SET(q, dirraw);
            BAL_RAW_SET(c, oppraw);
            avlfree_rotate(root, q, dir);
            return 0; /* Subtree height unchanged */
        }
        BAL_RAW_SET(q, BAL_BIAS);
        BAL_RAW_SET(c, BAL_BIAS);
        avlfree_rotate(root, q, dir);
        return 1; /* Subtree height decreased */
    } else {
        /* Double rotation (LR or RL) */
        Bhdr* g = AVLFREE_CHILD(c, opp);
        int graw = BAL_RAW(g);
        BAL_RAW_SET(q, (graw == dirraw) ? oppraw : BAL_BIAS);
        BAL_RAW_SET(c, (graw == oppraw) ? dirraw : BAL_BIAS);
        BAL_RAW_SET(g, BAL_BIAS);
        avlfree_rotate(root, c, opp);
        avlfree_rotate(root, q, dir);
        return 1;
    }
}

/* Bottom-up balance factor fix after insertion */
static void avlfree_fix_insert(Bhdr** root, Bhdr* node) {
    Bhdr* parent = node->bhf_parent;

    while (parent != NULL) {
        int raw;

        BAL_ADD(parent, (node == parent->bhf_left) ? -1 : 1);
        raw = BAL_RAW(parent);

        if (AVLFREE_RAW_UNBALANCED(raw)) {
            int heavy_dir = (raw > BAL_BIAS) ? AVLFREE_RIGHT : AVLFREE_LEFT;
            avlfree_rebalance(root, parent, heavy_dir);
            break; /* Single rebalance restores tree height on insertion */
        }

        if (raw == BAL_BIAS) {
            break; /* Subtree height did not increase */
        }

        node = parent;
        parent = node->bhf_parent;
    }
}

/* Bottom-up balance factor fix after deletion */
static void avlfree_fix_delete(Bhdr** root, Bhdr* q, int shrank_dir) {
    while (q != NULL) {
        Bhdr* parent = q->bhf_parent;
        int next_shrank_dir = (parent != NULL && parent->bhf_left == q) ? AVLFREE_LEFT : AVLFREE_RIGHT;
        int raw;

        BAL_ADD(q, (shrank_dir == AVLFREE_LEFT) ? 1 : -1);
        raw = BAL_RAW(q);

        if (AVLFREE_RAW_UNBALANCED(raw)) {
            int heavy_dir = (raw > BAL_BIAS) ? AVLFREE_RIGHT : AVLFREE_LEFT;
            int height_decreased = avlfree_rebalance(root, q, heavy_dir);

            if (!height_decreased) {
                break; /* Stop if subtree height remains unchanged */
            }
        } else if (raw != BAL_BIAS) {
            break; /* Balance is +-1: subtree height unchanged */
        }

        q = parent;
        shrank_dir = next_shrank_dir;
    }
}

void pooladd(Pool* p, Bhdr* q, Bhdr* lead) {
    size_t size;
    Bhdr* tp, * t;

    /* MAGIC_F's low nibble is 0 by construction, so ORing BAL_BIAS sets
     * type and balance==0 in a single write. */
    q->bh_magic = MAGIC_F | BAL_BIAS;
    q->bhf_lead = lead;

    /* Increment free blocks counter for arena compaction */
    q->bhf_lead->bhl_freecnt++;

    q->bhf_left = NULL;
    q->bhf_right = NULL;
    q->bhf_parent = NULL;
    q->bhf_succ = q;
    q->bhf_pred = q;

    t = p->root;
    if (t == NULL) {
        p->root = q;
        return;
    }

    size = q->bh_size;

    tp = NULL;
    while (t != NULL) {
        if (size == t->bh_size) {
            /* Duplicate size: insert into circular list without modifying tree structure */
            q->bhf_pred = t->bhf_pred;
            q->bhf_pred->bhf_succ = q;
            q->bhf_succ = t;
            t->bhf_pred = q;
            return;
        }
        tp = t;
        if (size < t->bh_size) {
            t = t->bhf_left;
        } else {
            t = t->bhf_right;
        }
    }

    q->bhf_parent = tp;
    if (size < tp->bh_size) {
        tp->bhf_left = q;
    } else {
        tp->bhf_right = q;
    }

    /* Bottom-up rebalancing after insertion */
    avlfree_fix_insert(&p->root, q);
}

void pooldel(Pool* p, Bhdr* t) {
    Bhdr* q_fix = NULL;
    int shrank_dir = AVLFREE_RIGHT;

    /* Decrement free blocks counter */
    t->bhf_lead->bhl_freecnt--;

    /* Case 1: Duplicate size block handling */
    if (t->bhf_succ != t) {
        if (t->bhf_parent == NULL && p->root != t) {
            /* Secondary duplicate node: remove from circular list */
            t->bhf_pred->bhf_succ = t->bhf_succ;
            t->bhf_succ->bhf_pred = t->bhf_pred;
            return;
        }
        /* Main tree node: promote successor from duplicate list without tree layout changes */
        Bhdr* f = t->bhf_succ;
        f->bhf_left = t->bhf_left;
        if (f->bhf_left != NULL) {
            f->bhf_left->bhf_parent = f;
        }
        f->bhf_right = t->bhf_right;
        if (f->bhf_right != NULL) {
            f->bhf_right->bhf_parent = f;
        }
        BAL_COPY(f, t);

        avlfree_replace_child(&p->root, t->bhf_parent, t, f);

        t->bhf_pred->bhf_succ = t->bhf_succ;
        t->bhf_succ->bhf_pred = t->bhf_pred;
        return;
    }

    /* Case 2: Standard AVL node removal */
    if (t->bhf_left == NULL || t->bhf_right == NULL) {
        /* 0 or 1 child */
        Bhdr* child = (t->bhf_left != NULL) ? t->bhf_left : t->bhf_right;
        q_fix = t->bhf_parent;
        shrank_dir = (q_fix != NULL && q_fix->bhf_left == t) ? AVLFREE_LEFT : AVLFREE_RIGHT;

        avlfree_replace_child(&p->root, q_fix, t, child);
    } else {
        /* 2 children: extract in-order successor (smallest node in right subtree) */
        Bhdr* rp = t->bhf_right;
        while (rp->bhf_left != NULL) {
            rp = rp->bhf_left;
        }

        Bhdr* f = rp->bhf_parent;
        if (f != t) {
            q_fix = f;
            shrank_dir = AVLFREE_LEFT;
            avlfree_replace_child(&p->root, f, rp, rp->bhf_right);

            rp->bhf_right = t->bhf_right;
            rp->bhf_right->bhf_parent = rp;
        } else {
            q_fix = rp;
            shrank_dir = AVLFREE_RIGHT;
        }

        rp->bhf_left = t->bhf_left;
        rp->bhf_left->bhf_parent = rp;
        BAL_COPY(rp, t);

        avlfree_replace_child(&p->root, t->bhf_parent, t, rp);
    }

    /* Case 3: Bottom-up rebalancing if subtree height decreased */
    if (q_fix != NULL) {
        avlfree_fix_delete(&p->root, q_fix, shrank_dir);
    }
}
