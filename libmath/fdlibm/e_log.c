/* @(#)e_log.c 1.4 96/03/07 */
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
/* __ieee754_log(x)
 * Return the logrithm of x
 *
 * Method :
 *   1. Argument Reduction: find k and f such that
 *			x = 2^k * (1+f),
 *	   where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *   2. Approximation of log(1+f).
 *	Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *	     	 = 2s + s*R
 *      We use a special Remes algorithm on [0,0.1716] to generate
 * 	a polynomial of degree 14 to approximate R The maximum error
 *	of this polynomial approximation is bounded by 2**-58.45. In
 *	other words,
 *		        2      4      6      8      10      12      14
 *	    R(z) ~ Lg1*s +Lg2*s +Lg3*s +Lg4*s +Lg5*s  +Lg6*s  +Lg7*s
 *  	(the values of Lg1 to Lg7 are listed in the program)
 *	and
 *	    |      2          14          |     -58.45
 *	    | Lg1*s +...+Lg7*s    -  R(z) | <= 2
 *	    |                             |
 *	Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 *	In order to guarantee error in log below 1ulp, we compute log
 *	by
 *		log(1+f) = f - s*(f - R)	(if f is not too large)
 *		log(1+f) = f - (hfsq - s*(hfsq+R)).	(better accuracy)
 *
 *	3. Finally,  log(x) = k*ln2 + log(1+f).
 *			    = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
 *	   Here ln2 is split into two floating point number:
 *			ln2_hi + ln2_lo,
 *	   where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 *	log(x) is NaN with signal if x < 0 (including -INF) ;
 *	log(+INF) is +INF; log(0) is -INF with signal;
 *	log(NaN) is that NaN with no signal.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "e_log.k.h"
F64K(ln2_hi, 6.93147180369123816490e-01)
F64K(ln2_lo, 1.90821492927058770002e-10)
F64K(two54, 1.80143985094819840000e+16)
F64K(Lg1, 6.666666666666735130e-01)
F64K(Lg2, 3.999999999940941908e-01)
F64K(Lg3, 2.857142874366239149e-01)
F64K(Lg4, 2.222219843214978396e-01)
F64K(Lg5, 1.818357216161805012e-01)
F64K(Lg6, 1.531383769920937332e-01)
F64K(Lg7, 1.479819860511658591e-01)
F64K(zero, 0.0)
F64K(k_1p0, 1.0)
F64K(k_0p5, 0.5)
F64K(k_0p33333333333333333, 0.33333333333333333)
F64K(k_2p0, 2.0)

float64_t fd_log(float64_t x)
{
	float64_t hfsq, f, s, z, R, w, t1, t2, dk;
	int32_t k, hx, i, j;
	uint32_t lx;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */	/* low word of x */
	k = 0;
	if (hx < 0x00100000) {
		/* x < 2**-1022  */
		if (((hx & 0x7fffffff) | lx) == 0) {
			return FDIV(FNEG(two54), zero);	/* return -two54/zero; */	/* log(+-0)=-inf */
		}
		if (hx < 0) {
			return FDIV(FSUB(x, x), zero);	/* return (x-x)/zero; */	/* log(-#) = NaN */
		}
		k -= 54;
		x = FMUL(x, two54);	/* x *= two54; */	/* subnormal number, scale up x */
		hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	}
	if (hx >= 0x7ff00000) {
		return FADD(x, x);	/* return x+x; */
	}
	k += (hx >> 20) - 1023;
	hx &= 0x000fffff;
	i = (hx + 0x95f64) & 0x100000;
	FSETHI(x, hx | (i ^ 0x3ff00000));	/* __HI(x) = hx|(i^0x3ff00000); */	/* normalize x or x/2 */
	k += (i >> 20);
	f = FSUB(x, k_1p0);	/* f = x-1.0; */
	if ((0x000fffff & (2 + hx)) < 3) {
		/* |f| < 2**-20 */
		if (FEQ(f, zero)) {	/* f==zero */
			if (k == 0) {
				return zero;
			} else {
				dk = FI(k);	/* dk=(double)k; */
				return FADD(FMUL(dk, ln2_hi), FMUL(dk, ln2_lo));	/* return dk*ln2_hi+dk*ln2_lo; */
			}
		}
		R = FMUL(FMUL(f, f), FSUB(k_0p5, FMUL(k_0p33333333333333333, f)));	/* R = f*f*(0.5-0.33333333333333333*f); */
		if (k == 0) {
			return FSUB(f, R);	/* return f-R; */
		} else {
			dk = FI(k);	/* dk=(double)k; */
			return FSUB(FMUL(dk, ln2_hi), FSUB(FSUB(R, FMUL(dk, ln2_lo)), f));	/* return dk*ln2_hi-((R-dk*ln2_lo)-f); */
		}
	}
	s = FDIV(f, FADD(k_2p0, f));	/* s = f/(2.0+f); */
	dk = FI(k);	/* dk = (double)k; */
	z = FMUL(s, s);	/* z = s*s; */
	i = hx - 0x6147a;
	w = FMUL(z, z);	/* w = z*z; */
	j = 0x6b851 - hx;
	t1 = FMUL(w, FADD(Lg2, FMUL(w, FADD(Lg4, FMUL(w, Lg6)))));	/* t1= w*(Lg2+w*(Lg4+w*Lg6)); */
	t2 = FMUL(z, FADD(Lg1, FMUL(w, FADD(Lg3, FMUL(w, FADD(Lg5, FMUL(w, Lg7)))))));	/* t2= z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7))); */
	i |= j;
	R = FADD(t2, t1);	/* R = t2+t1; */
	if (i > 0) {
		hfsq = FMUL(FMUL(k_0p5, f), f);	/* hfsq=0.5*f*f; */
		if (k == 0) {
			return FSUB(f, FSUB(hfsq, FMUL(s, FADD(hfsq, R))));	/* return f-(hfsq-s*(hfsq+R)); */
		} else {
			return FSUB(FMUL(dk, ln2_hi), FSUB(FSUB(hfsq, FADD(FMUL(s, FADD(hfsq, R)), FMUL(dk, ln2_lo))), f));	/* return dk*ln2_hi-((hfsq-(s*(hfsq+R)+dk*ln2_lo))-f); */
		}
	} else {
		if (k == 0) {
			return FSUB(f, FMUL(s, FSUB(f, R)));	/* return f-s*(f-R); */
		} else {
			return FSUB(FMUL(dk, ln2_hi), FSUB(FSUB(FMUL(s, FSUB(f, R)), FMUL(dk, ln2_lo)), f));	/* return dk*ln2_hi-((s*(f-R)-dk*ln2_lo)-f); */
		}
	}
}
