/* @(#)s_scalbn.c 1.3 95/01/18 */
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
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */
#include "fdlibm.h"
#include "s_scalbn.k.h"
F64K(two54, 1.80143985094819840000e+16)
F64K(twom54, 5.55111512312578270212e-17)
F64K(huge, 1.0e+300)
F64K(tiny, 1.0e-300)
float64_t fd_scalbn(float64_t x, int32_t n)
{
	int32_t k, hx, lx;
	hx = FHI(x);	/* hx = __HI(x); */
	lx = FLO(x);	/* lx = __LO(x); */
	k = (hx & 0x7ff00000) >> 20;	/* extract exponent */
	if (k == 0) {
		/* 0 or subnormal x */
		if ((lx | (hx & 0x7fffffff)) == 0) {
			return x;	/* +-0 */
		}
		x = FMUL(x, two54);	/* x *= two54; */
		hx = FHI(x);	/* hx = __HI(x); */
		k = ((hx & 0x7ff00000) >> 20) - 54;
		if (n < -50000) {
			return FMUL(tiny, x);	/* return tiny*x; */	/*underflow*/
		}
	}
	if (k == 0x7ff) {
		return FADD(x, x);	/* return x+x; */	/* NaN or Inf */
	}
	k = k + n;
	if (k > 0x7fe) {
		return FMUL(huge, fd_copysign(huge, x));	/* return huge*fd_copysign(huge,x); */	/* overflow */
	}
	if (k > 0) {	/* normal result */
		FSETHI(x, (hx & 0x800fffff) | (k << 20));	/* __HI(x) = (hx&0x800fffff)|(k<<20); */
		return x;
	}
	if (k <= -54) {
		if (n > 50000) {	/* in case integer overflow in n+k */
			return FMUL(huge, fd_copysign(huge, x));	/* return huge*fd_copysign(huge,x); */	/*overflow*/
		} else {
			return FMUL(tiny, fd_copysign(tiny, x));	/* return tiny*fd_copysign(tiny,x); */	/*underflow*/
		}
	}
	k += 54;	/* subnormal result */
	FSETHI(x, (hx & 0x800fffff) | (k << 20));	/* __HI(x) = (hx&0x800fffff)|(k<<20); */
	return FMUL(x, twom54);	/* return x*twom54; */
}
