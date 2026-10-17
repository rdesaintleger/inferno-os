/* @(#)s_ilogb.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* ilogb(double x)
 * return the binary exponent of non-zero x
 * ilogb(0) = 0x80000001
 * ilogb(inf/NaN) = 0x7fffffff (no signal is raised)
 */
#include "fdlibm.h"
#include "s_ilogb.k.h"
int32_t fd_ilogb(float64_t x)
{
	int32_t hx, lx, ix;
	hx = FHI(x) & 0x7fffffff;	/* hx = (__HI(x))&0x7fffffff; */	/* high word of x */
	if (hx < 0x00100000) {
		lx = FLO(x);	/* lx = __LO(x); */
		if ((hx | lx) == 0) {
			return (int32_t)0x80000001;	/* return 0x80000001; */	/* ilogb(0) = 0x80000001 */
		} else if (hx == 0) {	/* subnormal x */
			for (ix = -1043; lx > 0; lx <<= 1) {
				ix -= 1;
			}
		} else {
			for (ix = -1022, hx <<= 11; hx > 0; hx <<= 1) {
				ix -= 1;
			}
		}
		return ix;
	} else if (hx < 0x7ff00000) {
		return (hx >> 20) - 1023;
	} else {
		return 0x7fffffff;
	}
}
