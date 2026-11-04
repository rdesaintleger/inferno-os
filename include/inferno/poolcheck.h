#ifndef _INFERNO_POOLCHECK_H_
#define _INFERNO_POOLCHECK_H_

/*
 * Paranoid pool mode (debugging aid, off by default).
 *
 * When POOLPARANOID is defined:
 *  - every Bhdr carries a CRC-32 of its bh_size and of the type part of bh_magic (the
 *    low nibble of bh_magic, which holds flags/colour that change in place, is not covered),
 *    stored in bh_crc right after bh_magic (this changes the layout of Bhdr);
 *  - after every native call made by the VM (xec.c, mcall) every pool is walked from the
 *    arena leader to the arena trailer: CRC of each block, Bchk back link to the previous
 *    block, arena of each block, links of the free tree, free counter of the arena;
 *  - on corruption, the pool, the block, the suspected overflowing block and the native
 *    function that just returned are printed, then abort() is called.
 *
 * The layout of Bhdr is shared by libkern, libinterp and emu: the switch must be the same
 * for all of them, so it is set here rather than on the command line of one library.
 * Either uncomment the define below or add -DPOOLPARANOID to CFLAGS of mkfiles/mkfile-*,
 * then do a clean build (mk does not track headers: ./makemk.sh && mk nukedist && ...).
 */
/* #define POOLPARANOID 1 */

#ifdef POOLPARANOID

/* scan the pools once every POOLPARANOID_STRIDE native calls (1: every call) */
#ifndef POOLPARANOID_STRIDE
#define POOLPARANOID_STRIDE 1
#endif

#endif /* POOLPARANOID */

#endif /* _INFERNO_POOLCHECK_H_ */
