/* @(#)s_ceil.c 1.3 95/01/18 */
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
/*
 * ceil(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to ceil(x).
 */
#include "fdlibm.h"
#include "s_ceil.k.h"
F64K(huge, 1.0e300)
F64K(k_0p0, 0.0)

float64_t fd_ceil(float64_t x)
{
	int32_t i0, i1, j0;
	uint32_t i, j;
	i0 = FHI(x);	/* i0 = __HI(x); */
	i1 = FLO(x);	/* i1 = __LO(x); */
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	if (j0 < 20) {
		if (j0 < 0) {
			/* raise inexact if x != 0 */
			if (FGT(FADD(huge, x), k_0p0)) {	/* huge+x>0.0 */
				/* return 0*sign(x) if |x|<1 */
				if (i0 < 0) {
					i0 = (int32_t)0x80000000;	/* i0=0x80000000; */
					i1 = 0;
				} else if ((i0 | i1) != 0) {
					i0 = 0x3ff00000;
					i1 = 0;
				}
			}
		} else {
			i = (uint32_t)(0x000fffff >> j0);	/* i = (0x000fffff)>>j0; */
			if (((i0 & i) | i1) == 0) {
				return x;	/* x is integral */
			}
			if (FGT(FADD(huge, x), k_0p0)) {	/* huge+x>0.0 */
				/* raise inexact flag */
				if (i0 > 0) {
					i0 += 0x00100000 >> j0;	/* i0 += (0x00100000)>>j0; */
				}
				i0 &= (~i);
				i1 = 0;
			}
		}
	} else if (j0 > 51) {
		if (j0 == 0x400) {
			return FADD(x, x);	/* return x+x; */	/* inf or NaN */
		} else {
			return x;	/* x is integral */
		}
	} else {
		i = 0xffffffff >> (j0 - 20);	/* i = ((unsigned)(0xffffffff))>>(j0-20); */
		if ((i1 & i) == 0) {
			return x;	/* x is integral */
		}
		if (FGT(FADD(huge, x), k_0p0)) {	/* huge+x>0.0 */
			/* raise inexact flag */
			if (i0 > 0) {
				if (j0 == 20) {
					i0 += 1;
				} else {
					j = (uint32_t)(i1 + (1 << (52 - j0)));	/* j = i1 + (1<<(52-j0)); */
					if (j < i1) {
						i0 += 1;	/* got a carry */
					}
					i1 = (int32_t)j;	/* i1 = j; */
				}
			}
			i1 &= (~i);
		}
	}
	FSETHI(x, i0);	/* __HI(x) = i0; */
	FSETLO(x, i1);	/* __LO(x) = i1; */
	return x;
}
