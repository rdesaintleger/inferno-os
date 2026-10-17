/* @(#)e_sinh.c 1.3 95/01/18 */
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
/* __ieee754_sinh(x)
 * Method :
 * mathematically sinh(x) if defined to be (exp(x)-exp(-x))/2
 *	1. Replace x by |x| (sinh(-x) = -sinh(x)).
 *	2.
 *		                                    E + E/(E+1)
 *	    0        <= x <= 22     :  sinh(x) := --------------, E=expm1(x)
 *			       			        2
 *
 *	    22       <= x <= lnovft :  sinh(x) := exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  sinh(x) := exp(x/2)/2 * exp(x/2)
 *	    ln2ovft  <  x	    :  sinh(x) := x*shuge (overflow)
 *
 * Special cases:
 *	sinh(x) is |x| if x is +INF, -INF, or NaN.
 *	only sinh(0)=0 is exact for finite x.
 */
#include "fdlibm.h"
#include "e_sinh.k.h"
F64K(one, 1.0)
F64K(shuge, 1.0e307)
F64K(k_0p5, 0.5)
F64K(k_2p0, 2.0)

float64_t fd_sinh(float64_t x)
{
	float64_t t, w, h;
	int32_t ix, jx;
	uint32_t lx;
	/* High word of |x|. */
	jx = FHI(x);	/* jx = __HI(x); */
	ix = jx & 0x7fffffff;
	/* x is INF or NaN */
	if (ix >= 0x7ff00000) {
		return FADD(x, x);	/* return x+x; */
	}
	h = k_0p5;	/* h = 0.5; */
	if (jx < 0) {
		h = FNEG(h);	/* h = -h; */
	}
	/* |x| in [0,22], return sign(x)*0.5*(E+E/(E+1))) */
	if (ix < 0x40360000) {
		/* |x|<22 */
		if (ix < 0x3e300000) {	/* |x|<2**-28 */
			if (FGT(FADD(shuge, x), one)) {	/* shuge+x>one */
				return x;	/* sinh(tiny) = tiny with inexact */
			}
		}
		t = fd_expm1(fd_fabs(x));
		if (ix < 0x3ff00000) {
			return FMUL(h, FSUB(FMUL(k_2p0, t), FDIV(FMUL(t, t), FADD(t, one))));	/* return h*(2.0*t-t*t/(t+one)); */
		}
		return FMUL(h, FADD(t, FDIV(t, FADD(t, one))));	/* return h*(t+t/(t+one)); */
	}
	/* |x| in [22, log(maxdouble)] return 0.5*exp(|x|) */
	if (ix < 0x40862E42) {
		return FMUL(h, fd_exp(fd_fabs(x)));	/* return h*fd_exp(fd_fabs(x)); */
	}
	/* |x| in [log(maxdouble), overflowthresold] */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */
	if (ix < 0x408633CE || (ix == 0x408633ce) && (lx <= 0x8fb9f87d)) {	/* ix<0x408633CE || (ix==0x408633ce)&&(lx<=(unsigned)0x8fb9f87d) */
		w = fd_exp(FMUL(k_0p5, fd_fabs(x)));	/* w = fd_exp(0.5*fd_fabs(x)); */
		t = FMUL(h, w);	/* t = h*w; */
		return FMUL(t, w);	/* return t*w; */
	}
	/* |x| > overflowthresold, sinh(x) overflow */
	return FMUL(x, shuge);	/* return x*shuge; */
}
