/* @(#)e_atanh.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 */
/* __ieee754_atanh(x)
 * Method :
 *    1.Reduced x to positive by atanh(-x) = -atanh(x)
 *    2.For x>=0.5
 *                  1              2x                          x
 *	atanh(x) = --- * log(1 + -------) = 0.5 * log1p(2 * --------)
 *                  2             1 - x                      1 - x
 *
 * 	For x<0.5
 *	atanh(x) = 0.5*log1p(2x+2x*x/(1-x))
 *
 * Special cases:
 *	atanh(x) is NaN if |x| > 1 with signal;
 *	atanh(NaN) is that NaN with no signal;
 *	atanh(+-1) is +-INF with signal.
 *
 */
#include "fdlibm.h"
#include "e_atanh.k.h"
F64K(one, 1.0)
F64K(huge, 1e300)
F64K(zero, 0.0)
F64K(k_0p5, 0.5)

float64_t fd_atanh(float64_t x)
{
	float64_t t;
	int32_t hx, ix;
	uint32_t lx;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */	/* low word */
	ix = hx & 0x7fffffff;
	if ((ix | ((lx | (-lx)) >> 31)) > 0x3ff00000) {	/* |x|>1 */
		return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */
	}
	if (ix == 0x3ff00000) {
		return FDIV(x, zero);	/* return x/zero; */
	}
	if (ix < 0x3e300000 && FGT(FADD(huge, x), zero)) {	/* ix<0x3e300000&&(huge+x)>zero */
		return x;	/* x<2**-28 */
	}
	FSETHI(x, ix);	/* __HI(x) = ix; */	/* x <- |x| */
	if (ix < 0x3fe00000) {
		/* x < 0.5 */
		t = FADD(x, x);	/* t = x+x; */
		t = FMUL(k_0p5, fd_log1p(FADD(t, FDIV(FMUL(t, x), FSUB(one, x)))));	/* t = 0.5*fd_log1p(t+t*x/(one-x)); */
	} else {
		t = FMUL(k_0p5, fd_log1p(FDIV(FADD(x, x), FSUB(one, x))));	/* t = 0.5*fd_log1p((x+x)/(one-x)); */
	}
	if (hx >= 0) {
		return t;
	} else {
		return FNEG(t);	/* return -t; */
	}
}
