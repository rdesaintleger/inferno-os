/* @(#)e_acosh.c 1.3 95/01/18 */
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
/* __ieee754_acosh(x)
 * Method :
 *	Based on
 *		acosh(x) = log [ x + sqrt(x*x-1) ]
 *	we have
 *		acosh(x) := log(x)+ln2,	if x is large; else
 *		acosh(x) := log(2x-1/(sqrt(x*x-1)+x)) if x>2; else
 *		acosh(x) := log1p(t+sqrt(2.0*t+t*t)); where t=x-1.
 *
 * Special cases:
 *	acosh(x) is NaN with signal if x<1.
 *	acosh(NaN) is NaN without signal.
 */
#include "fdlibm.h"
#include "e_acosh.k.h"
F64K(one, 1.0)
F64K(ln2, 6.93147180559945286227e-01)
F64K(k_0p0, 0.0)
F64K(k_2p0, 2.0)

float64_t fd_acosh(float64_t x)
{
	float64_t t;
	int32_t hx;
	hx = FHI(x);	/* hx = __HI(x); */
	if (hx < 0x3ff00000) {
		/* x < 1 */
		return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */
	} else if (hx >= 0x41b00000) {
		/* x > 2**28 */
		if (hx >= 0x7ff00000) {
			/* x is inf of NaN */
			return FADD(x, x);	/* return x+x; */
		} else {
			return FADD(fd_log(x), ln2);	/* return fd_log(x)+ln2; */	/* acosh(huge)=log(2x) */
		}
	} else if (((hx - 0x3ff00000) | FLO(x)) == 0) {	/* ((hx-0x3ff00000)|__LO(x))==0 */
		return k_0p0;	/* return 0.0; */	/* acosh(1) = 0 */
	} else if (hx > 0x40000000) {
		/* 2**28 > x > 2 */
		t = FMUL(x, x);	/* t=x*x; */
		return fd_log(FSUB(FMUL(k_2p0, x), FDIV(one, FADD(x, fd_sqrt(FSUB(t, one))))));	/* return fd_log(2.0*x-one/(x+fd_sqrt(t-one))); */
	} else {
		/* 1<x<2 */
		t = FSUB(x, one);	/* t = x-one; */
		return fd_log1p(FADD(t, fd_sqrt(FADD(FMUL(k_2p0, t), FMUL(t, t)))));	/* return fd_log1p(t+fd_sqrt(2.0*t+t*t)); */
	}
}
