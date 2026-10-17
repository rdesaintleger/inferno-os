/* @(#)s_asinh.c 1.3 95/01/18 */
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
/* asinh(x)
 * Method :
 *	Based on
 *		asinh(x) = sign(x) * log [ |x| + sqrt(x*x+1) ]
 *	we have
 *	asinh(x) := x  if  1+x*x=1,
 *		 := sign(x)*(log(x)+ln2)) for large |x|, else
 *		 := sign(x)*log(2|x|+1/(|x|+sqrt(x*x+1))) if|x|>2, else
 *		 := sign(x)*log1p(|x| + x^2/(1 + sqrt(1+x^2)))
 */
#include "fdlibm.h"
#include "s_asinh.k.h"
F64K(one, 1.00000000000000000000e+00)
F64K(ln2, 6.93147180559945286227e-01)
F64K(huge, 1.00000000000000000000e+300)
F64K(k_2p0, 2.0)

float64_t fd_asinh(float64_t x)
{
	float64_t t, w;
	int32_t hx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		return FADD(x, x);	/* return x+x; */	/* x is inf or NaN */
	}
	if (ix < 0x3e300000) {
		/* |x|<2**-28 */
		if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
			return x;	/* return x inexact except 0 */
		}
	}
	if (ix > 0x41b00000) {
		/* |x| > 2**28 */
		w = FADD(fd_log(fd_fabs(x)), ln2);	/* w = fd_log(fd_fabs(x))+ln2; */
	} else if (ix > 0x40000000) {
		/* 2**28 > |x| > 2.0 */
		t = fd_fabs(x);
		w = fd_log(FADD(FMUL(k_2p0, t), FDIV(one, FADD(fd_sqrt(FADD(FMUL(x, x), one)), t))));	/* w = fd_log(2.0*t+one/(fd_sqrt(x*x+one)+t)); */
	} else {
		/* 2.0 > |x| > 2**-28 */
		t = FMUL(x, x);	/* t = x*x; */
		w = fd_log1p(FADD(fd_fabs(x), FDIV(t, FADD(one, fd_sqrt(FADD(one, t))))));	/* w =fd_log1p(fd_fabs(x)+t/(one+fd_sqrt(one+t))); */
	}
	if (hx > 0) {
		return w;
	} else {
		return FNEG(w);	/* return -w; */
	}
}
