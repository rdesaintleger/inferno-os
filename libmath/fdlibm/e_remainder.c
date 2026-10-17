/* @(#)e_remainder.c 1.3 95/01/18 */
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
/* __ieee754_remainder(x,p)
 * Return :
 * 	returns  x REM p  =  x - [x/p]*p as if in infinite
 * 	precise arithmetic, where [x/p] is the (infinite bit)
 *	integer nearest x/p (in half way case choose the even one).
 * Method :
 *	Based on fmod() return x-[x/p]chopped*p exactlp.
 */
#include "fdlibm.h"
#include "e_remainder.k.h"
F64K(zero, 0.0)
F64K(k_0p5, 0.5)

float64_t fd_remainder(float64_t x, float64_t p)
{
	int32_t hx, hp;
	uint32_t sx, lx, lp;
	float64_t p_half;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */	/* low word of x */
	hp = FHI(p);	/* hp = __HI(p); */	/* high word of p */
	lp = (uint32_t)FLO(p);	/* lp = __LO(p); */	/* low word of p */
	sx = hx & 0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;
	/* purge off exception values */
	if ((hp | lp) == 0) {
		return FDIV(FMUL(x, p), FMUL(x, p));	/* return (x*p)/(x*p); */	/* p = 0 */
	}
	if ((hx >= 0x7ff00000) || ((hp >= 0x7ff00000) && (((hp - 0x7ff00000) | lp) != 0))) {	/* x not finite */ /* p is NaN */
		return FDIV(FMUL(x, p), FMUL(x, p));	/* return (x*p)/(x*p); */
	}
	if (hp <= 0x7fdfffff) {
		x = fd_fmod(x, FADD(p, p));	/* x = fd_fmod(x,p+p); */	/* now x < 2p */
	}
	if (((hx - hp) | (lx - lp)) == 0) {
		return FMUL(zero, x);	/* return zero*x; */
	}
	x = fd_fabs(x);
	p = fd_fabs(p);
	if (hp < 0x00200000) {
		if (FGT(FADD(x, x), p)) {	/* x+x>p */
			x = FSUB(x, p);	/* x-=p; */
			if (FGE(FADD(x, x), p)) {	/* x+x>=p */
				x = FSUB(x, p);	/* x -= p; */
			}
		}
	} else {
		p_half = FMUL(k_0p5, p);	/* p_half = 0.5*p; */
		if (FGT(x, p_half)) {	/* x>p_half */
			x = FSUB(x, p);	/* x-=p; */
			if (FGE(x, p_half)) {	/* x>=p_half */
				x = FSUB(x, p);	/* x -= p; */
			}
		}
	}
	FSETHI(x, FHI(x) ^ sx);	/* __HI(x) ^= sx; */
	return x;
}
