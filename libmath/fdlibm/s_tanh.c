/* @(#)s_tanh.c 1.3 95/01/18 */
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
/* Tanh(x)
 * Return the Hyperbolic Tangent of x
 *
 * Method :
 *				       x    -x
 *				      e  - e
 *	0. tanh(x) is defined to be -----------
 *				       x    -x
 *				      e  + e
 *	1. reduce x to non-negative by tanh(-x) = -tanh(x).
 *	2.  0      <= x <= 2**-55 : tanh(x) := x*(one+x)
 *					        -t
 *	    2**-55 <  x <=  1     : tanh(x) := -----; t = expm1(-2x)
 *					       t + 2
 *						     2
 *	    1      <= x <=  22.0  : tanh(x) := 1-  ----- ; t=expm1(2x)
 *						   t + 2
 *	    22.0   <  x <= INF    : tanh(x) := 1.
 *
 * Special cases:
 *	tanh(NaN) is NaN;
 *	only tanh(0)=0 is exact for finite argument.
 */
#include "fdlibm.h"
#include "s_tanh.k.h"
F64K(one, 1.0)
F64K(two, 2.0)
F64K(tiny, 1.0e-300)
float64_t fd_tanh(float64_t x)
{
	float64_t t, z;
	int32_t jx, ix;
	/* High word of |x|. */
	jx = FHI(x);	/* jx = __HI(x); */
	ix = jx & 0x7fffffff;
	/* x is INF or NaN */
	if (ix >= 0x7ff00000) {
		if (jx >= 0) {
			return FADD(FDIV(one, x), one);	/* return one/x+one; */	/* tanh(+-inf)=+-1 */
		} else {
			return FSUB(FDIV(one, x), one);	/* return one/x-one; */	/* tanh(NaN) = NaN */
		}
	}
	/* |x| < 22 */
	if (ix < 0x40360000) {
		/* |x|<22 */
		if (ix < 0x3c800000) {	/* |x|<2**-55 */
			return FMUL(x, FADD(one, x));	/* return x*(one+x); */	/* tanh(small) = small */
		}
		if (ix >= 0x3ff00000) {
			/* |x|>=1  */
			t = fd_expm1(FMUL(two, fd_fabs(x)));	/* t = fd_expm1(two*fd_fabs(x)); */
			z = FSUB(one, FDIV(two, FADD(t, two)));	/* z = one - two/(t+two); */
		} else {
			t = fd_expm1(FMUL(FNEG(two), fd_fabs(x)));	/* t = fd_expm1(-two*fd_fabs(x)); */
			z = FDIV(FNEG(t), FADD(t, two));	/* z= -t/(t+two); */
		}
		/* |x| > 22, return +-1 */
	} else {
		z = FSUB(one, tiny);	/* z = one - tiny; */	/* raised inexact flag */
	}
	return (jx >= 0) ? z : FNEG(z);	/* return (jx>=0)? z: -z; */
}
