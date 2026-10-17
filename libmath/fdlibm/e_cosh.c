/* @(#)e_cosh.c 1.3 95/01/18 */
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
/* __ieee754_cosh(x)
 * Method :
 * mathematically cosh(x) if defined to be (exp(x)+exp(-x))/2
 *	1. Replace x by |x| (cosh(x) = cosh(-x)).
 *	2.
 *		                                        [ exp(x) - 1 ]^2
 *	    0        <= x <= ln2/2  :  cosh(x) := 1 + -------------------
 *			       			           2*exp(x)
 *
 *		                                  exp(x) +  1/exp(x)
 *	    ln2/2    <= x <= 22     :  cosh(x) := -------------------
 *			       			          2
 *	    22       <= x <= lnovft :  cosh(x) := exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  cosh(x) := exp(x/2)/2 * exp(x/2)
 *	    ln2ovft  <  x	    :  cosh(x) := huge*huge (overflow)
 *
 * Special cases:
 *	cosh(x) is |x| if x is +INF, -INF, or NaN.
 *	only cosh(0)=1 is exact for finite x.
 */
#include "fdlibm.h"
#include "e_cosh.k.h"
F64K(one, 1.0)
F64K(half, 0.5)
F64K(huge, 1.0e300)
float64_t fd_cosh(float64_t x)
{
	float64_t t, w;
	int32_t ix;
	uint32_t lx;
	/* High word of |x|. */
	ix = FHI(x);	/* ix = __HI(x); */
	ix &= 0x7fffffff;
	/* x is INF or NaN */
	if (ix >= 0x7ff00000) {
		return FMUL(x, x);	/* return x*x; */
	}
	/* |x| in [0,0.5*ln2], return 1+expm1(|x|)^2/(2*exp(|x|)) */
	if (ix < 0x3fd62e43) {
		t = fd_expm1(fd_fabs(x));
		w = FADD(one, t);	/* w = one+t; */
		if (ix < 0x3c800000) {
			return w;	/* cosh(tiny) = 1 */
		}
		return FADD(one, FDIV(FMUL(t, t), FADD(w, w)));	/* return one+(t*t)/(w+w); */
	}
	/* |x| in [0.5*ln2,22], return (exp(|x|)+1/exp(|x|)/2; */
	if (ix < 0x40360000) {
		t = fd_exp(fd_fabs(x));
		return FADD(FMUL(half, t), FDIV(half, t));	/* return half*t+half/t; */
	}
	/* |x| in [22, log(maxdouble)] return half*exp(|x|) */
	if (ix < 0x40862E42) {
		return FMUL(half, fd_exp(fd_fabs(x)));	/* return half*fd_exp(fd_fabs(x)); */
	}
	/* |x| in [log(maxdouble), overflowthresold] */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */
	if (ix < 0x408633CE || (ix == 0x408633ce) && (lx <= 0x8fb9f87d)) {	/* ix<0x408633CE || (ix==0x408633ce)&&(lx<=(unsigned)0x8fb9f87d) */
		w = fd_exp(FMUL(half, fd_fabs(x)));	/* w = fd_exp(half*fd_fabs(x)); */
		t = FMUL(half, w);	/* t = half*w; */
		return FMUL(t, w);	/* return t*w; */
	}
	/* |x| > overflowthresold, cosh(x) overflow */
	return FMUL(huge, huge);	/* return huge*huge; */
}
