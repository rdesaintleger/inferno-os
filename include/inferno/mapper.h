#ifndef _INFERNO_MAPPER_H_
#define _INFERNO_MAPPER_H_

#include <stdint.h>
#include <stddef.h>
#include <inferno/bhdr.h>

/*
 * Arena mapper: a purely software 32-bit virtual address space for
 * chunked (BF_MAPPED) arenas. No mmap()/VirtualAlloc(), no hardware
 * mapping of any kind.
 */

enum {
    ARENA_PAGESHIFT = 17,
    ARENA_PAGESIZE = 1U << ARENA_PAGESHIFT,
    ARENA_PAGEMASK = ARENA_PAGESIZE - 1,
    ARENA_MAX_PAGES = 1U << (32 - ARENA_PAGESHIFT),
    ARENA_MAX_FREENODES = ARENA_MAX_PAGES >> 1,
    ARENA_PAGE_NIL = ((ARENA_MAX_PAGES - 1) << 1) | 1,
    FREENODE_NIL = ((ARENA_MAX_FREENODES - 1) << 1) | 1
};

/*
 * Offset from an arena's Blead to the first possible bha_data byte of
 * the first real (Balloc) block in the arena - accounts for the
 * hidden bc_pred prefix every non-leader chunked block carries (see
 * struct Bchk in bhdr.h). Used once per arena_map_fixed()/
 * arena_map_range()/arena_unmap() to derive/recover bhl_mapoffset, so
 * the hot arena_lookup64()/arena_lookup32() path only ever does a
 * single add/sub - it never recomputes this.
 */
#define ARENA_DATAOFFSET(lead) ((lead)->bh_size + offsetof(Bchk, bc_self.bha_data))

/*
 * Free virtual-address range: a single Red-Black tree, keyed by
 * fe_base (always unique). fe_amax augments every node with the
 * largest fe_size found anywhere in its own subtree (self included),
 * enabling an O(log n) best-fit search constrained to an address
 * window - same technique as Linux's rb_subtree_gap (mm/mmap.c).
 *
 * fe_parent packs the parent index in bits 0-14 and the color in bit
 * 15 (RB_COLOR_BIT/RB_PARENT_MASK, private to mapper.c).
 */
struct free_entry {
    uint16_t fe_left;
    uint16_t fe_right;
    uint16_t fe_parent;

    uint16_t fe_base; /* first free page; the tree's key */
    uint16_t fe_size; /* run length in pages */

    uint16_t fe_amax; /* largest fe_size anywhere in this node's subtree, self included */
};

void      arena_initmapper(void);
int       arena_map_fixed(Bhdr* arena, uint16_t base);
int       arena_map_range(Bhdr* arena, uint32_t addr_min, uint32_t addr_max); /* best fit within [addr_min, addr_max] */
void      arena_unmap(Bhdr* arena);
void*     arena_lookup64(uint32_t address, Bhdr** arena); /* arena is an output if non NULL pointer provided */
uint32_t  arena_lookup32(Bhdr* arena, void* v);

#endif /* _INFERNO_MAPPER_H_ */
