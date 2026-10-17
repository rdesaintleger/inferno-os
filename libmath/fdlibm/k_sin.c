/* @(#)k_sin.c 1.3 95/01/18 */
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
/* __kernel_sin( x, y, iy)
 * kernel sin function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input iy indicates whether y is 0. (if iy=0, y assume to be 0).
 *
 * Algorithm
 *	1. Since sin(-x) = -sin(x), we need only to consider positive x.
 *	2. if x < 2^-27 (hx<0x3e400000 0), return x with inexact if x!=0.
 *	3. sin(x) is approximated by a polynomial of degree 13 on
 *	   [0,pi/4]
 *		  	         3            13
 *	   	sin(x) ~ x + S1*x + ... + S6*x
 *	   where
 *
 * 	|sin(x)         2     4     6     8     10     12  |     -58
 * 	|----- - (1+S1*x +S2*x +S3*x +S4*x +S5*x  +S6*x   )| <= 2
 * 	|  x 					           |
 *
 *	4. sin(x+y) = sin(x) + sin'(x')*y
 *		    ~ sin(x) + (1-x*x/2)*y
 *	   For better accuracy, let
 *		     3      2      2      2      2
 *		r = x *(S2+x *(S3+x *(S4+x *(S5+x *S6))))
 *	   then                   3    2
 *		sin(x) = x + (S1*x + (x *(r-y/2)+y))
 */
#include "fdlibm.h"
#include "k_sin.k.h"
F64K(half, 5.00000000000000000000e-01)
F64K(S1, -1.66666666666666324348e-01)
F64K(S2, 8.33333333332248946124e-03)
F64K(S3, -1.98412698298579493134e-04)
F64K(S4, 2.75573137070700676789e-06)
F64K(S5, -2.50507602534068634195e-08)
F64K(S6, 1.58969099521155010221e-10)
float64_t fd_kernel_sin(float64_t x, float64_t y, int32_t iy)
{
	float64_t z, r, v;
	int32_t ix;
	ix = FHI(x) & 0x7fffffff;	/* ix = __HI(x)&0x7fffffff; */	/* high word of x */
	if (ix < 0x3e400000) {	/* |x| < 2**-27 */
		if (FTOI(x) == 0) {	/* (int)x==0 */
			return x;
		}
	}
	z = FMUL(x, x);	/* z = x*x; */	/* generate inexact */
	v = FMUL(z, x);	/* v = z*x; */
	r = FADD(S2, FMUL(z, FADD(S3, FMUL(z, FADD(S4, FMUL(z, FADD(S5, FMUL(z, S6))))))));	/* r = S2+z*(S3+z*(S4+z*(S5+z*S6))); */
	if (iy == 0) {
		return FADD(x, FMUL(v, FADD(S1, FMUL(z, r))));	/* return x+v*(S1+z*r); */
	} else {
		return FSUB(x, FSUB(FSUB(FMUL(z, FSUB(FMUL(half, y), FMUL(v, r))), y), FMUL(v, S1)));	/* return x-((z*(half*y-v*r)-y)-v*S1); */
	}
}
