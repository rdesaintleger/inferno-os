/* @(#)e_atan2.c 1.3 95/01/18 */
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
/* __ieee754_atan2(y,x)
 * Method :
 *	1. Reduce y to positive by atan2(y,x)=-atan2(-y,x).
 *	2. Reduce x to positive by (if x and y are unexceptional):
 *		ARG (x+iy) = arctan(y/x)   	   ... if x > 0,
 *		ARG (x+iy) = pi - arctan[y/(-x)]   ... if x < 0,
 *
 * Special cases:
 *
 *	ATAN2((anything), NaN ) is NaN;
 *	ATAN2(NAN , (anything) ) is NaN;
 *	ATAN2(+-0, +(anything but NaN)) is +-0  ;
 *	ATAN2(+-0, -(anything but NaN)) is +-pi ;
 *	ATAN2(+-(anything but 0 and NaN), 0) is +-pi/2;
 *	ATAN2(+-(anything but INF and NaN), +INF) is +-0 ;
 *	ATAN2(+-(anything but INF and NaN), -INF) is +-pi;
 *	ATAN2(+-INF,+INF ) is +-pi/4 ;
 *	ATAN2(+-INF,-INF ) is +-3pi/4;
 *	ATAN2(+-INF, (anything but,0,NaN, and INF)) is +-pi/2;
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "e_atan2.k.h"
F64K(tiny, 1.0e-300)
F64K(zero, 0.0)
F64K(pi_o_4, 7.8539816339744827900E-01)
F64K(pi_o_2, 1.5707963267948965580E+00)
F64K(pi, 3.1415926535897931160E+00)
F64K(pi_lo, 1.2246467991473531772E-16)
F64K(k_3p0, 3.0)
F64K(k_m3p0, -3.0)
F64K(k_0p5, 0.5)

float64_t fd_atan2(float64_t y, float64_t x)
{
	float64_t z;
	int32_t k, m, hx, hy, ix, iy;
	uint32_t lx, ly;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */
	hy = FHI(y);	/* hy = __HI(y); */
	iy = hy & 0x7fffffff;
	ly = (uint32_t)FLO(y);	/* ly = __LO(y); */
	if (((ix | ((lx | -lx) >> 31)) > 0x7ff00000) || ((iy | ((ly | -ly) >> 31)) > 0x7ff00000)) {	/* x or y is NaN */
		return FADD(x, y);	/* return x+y; */
	}
	if ((hx - 0x3ff00000 | lx) == 0) {
		return fd_atan(y);	/* x=1.0 */
	}
	m = ((hy >> 31) & 1) | ((hx >> 30) & 2);	/* 2*sign(x)+sign(y) */
	/* when y = 0 */
	if ((iy | ly) == 0) {
		switch (m) {
		case 0:
		case 1:
			return y;	/* atan(+-0,+anything)=+-0 */
		case 2:
			return FADD(pi, tiny);	/* return pi+tiny; */	/* atan(+0,-anything) = pi */
		case 3:
			return FSUB(FNEG(pi), tiny);	/* return -pi-tiny; */	/* atan(-0,-anything) =-pi */
		}
	}
	/* when x = 0 */
	if ((ix | lx) == 0) {
		return (hy < 0) ? FSUB(FNEG(pi_o_2), tiny) : FADD(pi_o_2, tiny);	/* return (hy<0)? -pi_o_2-tiny: pi_o_2+tiny; */
	}
	/* when x is INF */
	if (ix == 0x7ff00000) {
		if (iy == 0x7ff00000) {
			switch (m) {
			case 0:
				return FADD(pi_o_4, tiny);	/* return pi_o_4+tiny; */	/* atan(+INF,+INF) */
			case 1:
				return FSUB(FNEG(pi_o_4), tiny);	/* return -pi_o_4-tiny; */	/* atan(-INF,+INF) */
			case 2:
				return FADD(FMUL(k_3p0, pi_o_4), tiny);	/* return 3.0*pi_o_4+tiny; */	/*atan(+INF,-INF)*/
			case 3:
				return FSUB(FMUL(k_m3p0, pi_o_4), tiny);	/* return -3.0*pi_o_4-tiny; */	/*atan(-INF,-INF)*/
			}
		} else {
			switch (m) {
			case 0:
				return zero;	/* atan(+...,+INF) */
			case 1:
				return FNEG(zero);	/* return -zero ; */	/* atan(-...,+INF) */
			case 2:
				return FADD(pi, tiny);	/* return pi+tiny ; */	/* atan(+...,-INF) */
			case 3:
				return FSUB(FNEG(pi), tiny);	/* return -pi-tiny ; */	/* atan(-...,-INF) */
			}
		}
	}
	/* when y is INF */
	if (iy == 0x7ff00000) {
		return (hy < 0) ? FSUB(FNEG(pi_o_2), tiny) : FADD(pi_o_2, tiny);	/* return (hy<0)? -pi_o_2-tiny: pi_o_2+tiny; */
	}
	/* compute y/x */
	k = (iy - ix) >> 20;
	if (k > 60) {
		z = FADD(pi_o_2, FMUL(k_0p5, pi_lo));	/* z=pi_o_2+0.5*pi_lo; */	/* |y/x| > 2**60 */
	} else if (hx < 0 && k < -60) {
		z = zero;	/* z=0.0; */	/* |y|/x < -2**60 */
	} else {
		z = fd_atan(fd_fabs(FDIV(y, x)));	/* z=fd_atan(fd_fabs(y/x)); */	/* safe to do y/x */
	}
	switch (m) {
	case 0:
		return z;	/* atan(+,+) */
	case 1:
		FSETHI(z, FHI(z) ^ 0x80000000);	/* __HI(z) ^= 0x80000000; */
		return z;	/* atan(-,+) */
	case 2:
		return FSUB(pi, FSUB(z, pi_lo));	/* return pi-(z-pi_lo); */	/* atan(+,-) */
	default:
		return FSUB(FSUB(z, pi_lo), pi);	/* return (z-pi_lo)-pi; */	/* case 3 */ /* atan(-,-) */
	}
}
