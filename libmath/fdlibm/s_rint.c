/* @(#)s_rint.c 1.3 95/01/18 */
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
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */
#include "fdlibm.h"
#include "s_rint.k.h"
F64KA(TWO52, 2, 4.50359962737049600000e+15, -4.50359962737049600000e+15)
float64_t fd_rint(float64_t x)
{
	int32_t i0, j0, sx;
	uint32_t i, i1;
	float64_t w, t;
	i0 = FHI(x);	/* i0 = __HI(x); */
	sx = (i0 >> 31) & 1;
	i1 = (uint32_t)FLO(x);	/* i1 = __LO(x); */
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	if (j0 < 20) {
		if (j0 < 0) {
			if (((i0 & 0x7fffffff) | i1) == 0) {
				return x;
			}
			i1 |= (i0 & 0x0fffff);
			i0 &= 0xfffe0000;
			i0 |= ((i1 | -i1) >> 12) & 0x80000;
			FSETHI(x, i0);	/* __HI(x)=i0; */
			w = FADD(TWO52[sx], x);	/* w = TWO52[sx]+x; */
			t = FSUB(w, TWO52[sx]);	/* t = w-TWO52[sx]; */
			i0 = FHI(t);	/* i0 = __HI(t); */
			FSETHI(t, (i0 & 0x7fffffff) | (sx << 31));	/* __HI(t) = (i0&0x7fffffff)|(sx<<31); */
			return t;
		} else {
			i = (uint32_t)(0x000fffff >> j0);	/* i = (0x000fffff)>>j0; */
			if (((i0 & i) | i1) == 0) {
				return x;	/* x is integral */
			}
			i >>= 1;
			if (((i0 & i) | i1) != 0) {
				if (j0 == 19) {
					i1 = (uint32_t)0x40000000;	/* i1 = 0x40000000; */
				} else {
					i0 = (int32_t)((i0 & (~i)) | (0x20000 >> j0));	/* i0 = (i0&(~i))|((0x20000)>>j0); */
				}
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
		i >>= 1;
		if ((i1 & i) != 0) {
			i1 = (i1 & (~i)) | (0x40000000 >> (j0 - 20));	/* i1 = (i1&(~i))|((0x40000000)>>(j0-20)); */
		}
	}
	FSETHI(x, i0);	/* __HI(x) = i0; */
	FSETLO(x, i1);	/* __LO(x) = i1; */
	w = FADD(TWO52[sx], x);	/* w = TWO52[sx]+x; */
	return FSUB(w, TWO52[sx]);	/* return w-TWO52[sx]; */
}
