/* @(#)s_cbrt.c 1.3 95/01/18 */
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
#include "fdlibm.h"
#include "s_cbrt.k.h"
/* cbrt(x)
 * Return cube root of x
 */
static const uint32_t B1 = 715094163, B2 = 696219795;
F64K(C, 5.42857142857142815906e-01)
F64K(D, -7.05306122448979611050e-01)
F64K(E, 1.41428571428571436819e+00)
F64K(F, 1.60714285714285720630e+00)
F64K(G, 3.57142857142857150787e-01)
F64K(k_0p0, 0.0)

float64_t fd_cbrt(float64_t x)
{
	/* B1 = (682-0.03306235651)*2**20 */
	/* B2 = (664-0.03306235651)*2**20 */
	/* 19/35     = 0x3FE15F15, 0xF15F15F1 */
	/* -864/1225 = 0xBFE691DE, 0x2532C834 */
	/* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
	/* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
	/* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */
	int32_t hx;
	float64_t r;
	float64_t s;
	float64_t t = k_0p0;
	float64_t w;
	uint32_t sign;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	sign = hx & 0x80000000;	/* sign= sign(x) */
	hx ^= sign;
	if (hx >= 0x7ff00000) {
		return FADD(x, x);	/* return(x+x); */	/* cbrt(NaN,INF) is itself */
	}
	if ((hx | FLO(x)) == 0) {	/* (hx|__LO(x))==0 */
		return x;	/* return(x); */	/* cbrt(0) is itself */
	}
	FSETHI(x, hx);	/* __HI(x) = hx; */	/* x <- |x| */
	/* rough cbrt to 5 bits */
	if (hx < 0x00100000) {	/* subnormal number */
		FSETHI(t, 0x43500000);	/* __HI(t)=0x43500000; */	/* set t= 2**54 */
		t = FMUL(t, x);	/* t*=x; */
		FSETHI(t, FHI(t) / 3 + B2);	/* __HI(t)=__HI(t)/3+B2; */
	} else {
		FSETHI(t, hx / 3 + B1);	/* __HI(t)=hx/3+B1; */
	}
	/* new cbrt to 23 bits, may be implemented in single precision */
	r = FDIV(FMUL(t, t), x);	/* r=t*t/x; */
	s = FADD(C, FMUL(r, t));	/* s=C+r*t; */
	t = FMUL(t, FADD(G, FDIV(F, FADD(FADD(s, E), FDIV(D, s)))));	/* t*=G+F/(s+E+D/s); */
	/* chopped to 20 bits and make it larger than cbrt(x) */
	FSETLO(t, 0);	/* __LO(t)=0; */
	FSETHI(t, FHI(t) + 0x00000001);	/* __HI(t)+=0x00000001; */
	/* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s = FMUL(t, t);	/* s=t*t; */	/* t*t is exact */
	r = FDIV(x, s);	/* r=x/s; */
	w = FADD(t, t);	/* w=t+t; */
	r = FDIV(FSUB(r, t), FADD(w, r));	/* r=(r-t)/(w+r); */	/* r-s is exact */
	t = FADD(t, FMUL(t, r));	/* t=t+t*r; */
	/* retore the sign bit */
	FSETHI(t, FHI(t) | sign);	/* __HI(t) |= sign; */
	return t;	/* return(t); */
}
