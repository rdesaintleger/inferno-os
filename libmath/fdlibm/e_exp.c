/* @(#)e_exp.c 1.6 04/04/22 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* __ieee754_exp(x)
 * Returns the exponential of x.
 *
 * Method
 *   1. Argument reduction:
 *      Reduce x to an r so that |r| <= 0.5*ln2 ~ 0.34658.
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2.
 *
 *      Here r will be represented as r = hi-lo for better
 *	accuracy.
 *
 *   2. Approximation of exp(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Write
 *	    R(r**2) = r*(exp(r)+1)/(exp(r)-1) = 2 + r*r/6 - r**4/360 + ...
 *      We use a special Remes algorithm on [0,0.34658] to generate
 * 	a polynomial of degree 5 to approximate R. The maximum error
 *	of this polynomial approximation is bounded by 2**-59. In
 *	other words,
 *	    R(z) ~ 2.0 + P1*z + P2*z**2 + P3*z**3 + P4*z**4 + P5*z**5
 *  	(where z=r*r, and the values of P1 to P5 are listed below)
 *	and
 *	    |                  5          |     -59
 *	    | 2.0+P1*z+...+P5*z   -  R(z) | <= 2
 *	    |                             |
 *	The computation of exp(r) thus becomes
 *                             2*r
 *		exp(r) = 1 + -------
 *		              R - r
 *                                 r*R1(r)
 *		       = 1 + r + ----------- (for better accuracy)
 *		                  2 - R1(r)
 *	where
 *			         2       4             10
 *		R1(r) = r - (P1*r  + P2*r  + ... + P5*r   ).
 *
 *   3. Scale back to obtain exp(x):
 *	From step 1, we have
 *	   exp(x) = 2^k * exp(r)
 *
 * Special cases:
 *	exp(INF) is INF, exp(NaN) is NaN;
 *	exp(-INF) is 0, and
 *	for finite argument, only exp(0)=1 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double
 *	    if x >  7.09782712893383973096e+02 then exp(x) overflow
 *	    if x < -7.45133219101941108420e+02 then exp(x) underflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "e_exp.k.h"
F64K(one, 1.0)
F64KA(halF, 2, 0.5, -0.5)
F64K(huge, 1.0e+300)
F64K(twom1000, 9.33263618503218878990e-302)
F64K(o_threshold, 7.09782712893383973096e+02)
F64K(u_threshold, -7.45133219101941108420e+02)
F64KA(ln2HI, 2, 6.93147180369123816490e-01, -6.93147180369123816490e-01)
F64KA(ln2LO, 2, 1.90821492927058770002e-10, -1.90821492927058770002e-10)
F64K(invln2, 1.44269504088896338700e+00)
F64K(P1, 1.66666666666666019037e-01)
F64K(P2, -2.77777777770155933842e-03)
F64K(P3, 6.61375632143793436117e-05)
F64K(P4, -1.65339022054652515390e-06)
F64K(P5, 4.13813679705723846039e-08)
F64K(k_0p0, 0.0)
F64K(k_2p0, 2.0)

float64_t fd_exp(float64_t x)
{
	/* 2**-1000=0x01700000,0*/
	/* default IEEE double exp */
	float64_t y, hi, lo, c, t;
	int32_t k = 0, xsb;
	uint32_t hx;
	hx = (uint32_t)FHI(x);	/* hx = __HI(x); */	/* high word of x */
	xsb = (int32_t)((hx >> 31) & 1);	/* xsb = (hx>>31)&1; */	/* sign bit of x */
	hx &= 0x7fffffff;	/* high word of |x| */
	/* filter out non-finite argument */
	if (hx >= 0x40862E42) {
		/* if |x|>=709.78... */
		if (hx >= 0x7ff00000) {
			if (((hx & 0xfffff) | FLO(x)) != 0) {	/* ((hx&0xfffff)|__LO(x))!=0 */
				return FADD(x, x);	/* return x+x; */	/* NaN */
			} else {
				return (xsb == 0) ? x : k_0p0;	/* return (xsb==0)? x:0.0; */	/* exp(+-inf)={inf,0} */
			}
		}
		if (FGT(x, o_threshold)) {	/* x > o_threshold */
			return FMUL(huge, huge);	/* return huge*huge; */	/* overflow */
		}
		if (FLT(x, u_threshold)) {	/* x < u_threshold */
			return FMUL(twom1000, twom1000);	/* return twom1000*twom1000; */	/* underflow */
		}
	}
	/* argument reduction */
	if (hx > 0x3fd62e42) {
		/* if  |x| > 0.5 ln2 */
		if (hx < 0x3FF0A2B2) {
			/* and |x| < 1.5 ln2 */
			hi = FSUB(x, ln2HI[xsb]);	/* hi = x-ln2HI[xsb]; */
			lo = ln2LO[xsb];
			k = 1 - xsb - xsb;
		} else {
			k = FTOI(FADD(FMUL(invln2, x), halF[xsb]));	/* k = (int)(invln2*x+halF[xsb]); */
			t = FI(k);	/* t = k; */
			hi = FSUB(x, FMUL(t, ln2HI[0]));	/* hi = x - t*ln2HI[0]; */	/* t*ln2HI is exact here */
			lo = FMUL(t, ln2LO[0]);	/* lo = t*ln2LO[0]; */
		}
		x = FSUB(hi, lo);	/* x = hi - lo; */
	} else if (hx < 0x3e300000) {
		/* when |x|<2**-28 */
		if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
			return FADD(one, x);	/* return one+x; */	/* trigger inexact */
		}
	} else {
		k = 0;
	}
	/* x is now in primary range */
	t = FMUL(x, x);	/* t = x*x; */
	c = FSUB(x, FMUL(t, FADD(P1, FMUL(t, FADD(P2, FMUL(t, FADD(P3, FMUL(t, FADD(P4, FMUL(t, P5))))))))));	/* c = x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5)))); */
	if (k == 0) {
		return FSUB(one, FSUB(FDIV(FMUL(x, c), FSUB(c, k_2p0)), x));	/* return one-((x*c)/(c-2.0)-x); */
	} else {
		y = FSUB(one, FSUB(FSUB(lo, FDIV(FMUL(x, c), FSUB(k_2p0, c))), hi));	/* y = one-((lo-(x*c)/(2.0-c))-hi); */
	}
	if (k >= -1021) {
		FSETHI(y, FHI(y) + (k << 20));	/* __HI(y) += (k<<20); */	/* add k to y's exponent */
		return y;
	} else {
		FSETHI(y, FHI(y) + ((k + 1000) << 20));	/* __HI(y) += ((k+1000)<<20); */	/* add k to y's exponent */
		return FMUL(y, twom1000);	/* return y*twom1000; */
	}
}
