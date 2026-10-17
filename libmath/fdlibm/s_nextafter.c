/* @(#)s_nextafter.c 1.3 95/01/18 */
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
/* IEEE functions
 *	nextafter(x,y)
 *	return the next machine floating-point number of x in the
 *	direction toward y.
 *   Special cases:
 */
#include "fdlibm.h"
#include "s_nextafter.k.h"
float64_t fd_nextafter(float64_t x, float64_t y)
{
	int32_t hx, hy, ix, iy;
	uint32_t lx, ly;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */	/* low word of x */
	hy = FHI(y);	/* hy = __HI(y); */	/* high word of y */
	ly = (uint32_t)FLO(y);	/* ly = __LO(y); */	/* low word of y */
	ix = hx & 0x7fffffff;	/* |x| */
	iy = hy & 0x7fffffff;	/* |y| */
	if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0) || ((iy >= 0x7ff00000) && ((iy - 0x7ff00000) | ly) != 0)) {	/* x is nan */ /* y is nan */
		return FADD(x, y);	/* return x+y; */
	}
	if (FEQ(x, y)) {	/* x==y */
		return x;	/* x=y, return x */
	}
	if ((ix | lx) == 0) {
		/* x == 0 */
		FSETHI(x, hy & 0x80000000);	/* __HI(x) = hy&0x80000000; */	/* return +-minsubnormal */
		FSETLO(x, 1);	/* __LO(x) = 1; */
		y = FMUL(x, x);	/* y = x*x; */
		if (FEQ(y, x)) {	/* y==x */
			return y;
		} else {
			return x;	/* raise underflow flag */
		}
	}
	if (hx >= 0) {
		/* x > 0 */
		if (hx > hy || ((hx == hy) && (lx > ly))) {
			/* x > y, x -= ulp */
			if (lx == 0) {
				hx -= 1;
			}
			lx -= 1;
		} else {
			/* x < y, x += ulp */
			lx += 1;
			if (lx == 0) {
				hx += 1;
			}
		}
	} else {
		/* x < 0 */
		if (hy >= 0 || hx > hy || ((hx == hy) && (lx > ly))) {
			/* x < y, x -= ulp */
			if (lx == 0) {
				hx -= 1;
			}
			lx -= 1;
		} else {
			/* x > y, x += ulp */
			lx += 1;
			if (lx == 0) {
				hx += 1;
			}
		}
	}
	hy = hx & 0x7ff00000;
	if (hy >= 0x7ff00000) {
		return FADD(x, x);	/* return x+x; */	/* overflow */
	}
	if (hy < 0x00100000) {
		/* underflow */
		y = FMUL(x, x);	/* y = x*x; */
		if (FNE(y, x)) {	/* y!=x */
			/* raise underflow flag */
			FSETHI(y, hx);	/* __HI(y) = hx; */
			FSETLO(y, lx);	/* __LO(y) = lx; */
			return y;
		}
	}
	FSETHI(x, hx);	/* __HI(x) = hx; */
	FSETLO(x, lx);	/* __LO(x) = lx; */
	return x;
}
