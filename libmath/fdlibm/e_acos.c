/* @(#)e_acos.c 1.3 95/01/18 */
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
/* __ieee754_acos(x)
 * Method :
 *	acos(x)  = pi/2 - asin(x)
 *	acos(-x) = pi/2 + asin(x)
 * For |x|<=0.5
 *	acos(x) = pi/2 - (x + x*x^2*R(x^2))	(see asin.c)
 * For x>0.5
 * 	acos(x) = pi/2 - (pi/2 - 2asin(sqrt((1-x)/2)))
 *		= 2asin(sqrt((1-x)/2))
 *		= 2s + 2s*z*R(z) 	...z=(1-x)/2, s=sqrt(z)
 *		= 2f + (2c + 2s*z*R(z))
 *     where f=hi part of s, and c = (z-f*f)/(s+f) is the correction term
 *     for f so that f+c ~ sqrt(z).
 * For x<-0.5
 *	acos(x) = pi - 2asin(sqrt((1-|x|)/2))
 *		= pi - 0.5*(s+s*z*R(z)), where z=(1-|x|)/2,s=sqrt(z)
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 * Function needed: sqrt
 */
#include "fdlibm.h"
#include "e_acos.k.h"
F64K(one, 1.00000000000000000000e+00)
F64K(pi, 3.14159265358979311600e+00)
F64K(pio2_hi, 1.57079632679489655800e+00)
F64K(pio2_lo, 6.12323399573676603587e-17)
F64K(pS0, 1.66666666666666657415e-01)
F64K(pS1, -3.25565818622400915405e-01)
F64K(pS2, 2.01212532134862925881e-01)
F64K(pS3, -4.00555345006794114027e-02)
F64K(pS4, 7.91534994289814532176e-04)
F64K(pS5, 3.47933107596021167570e-05)
F64K(qS1, -2.40339491173441421878e+00)
F64K(qS2, 2.02094576023350569471e+00)
F64K(qS3, -6.88283971605453293030e-01)
F64K(qS4, 7.70381505559019352791e-02)
F64K(k_0p0, 0.0)
F64K(k_2p0, 2.0)
F64K(k_0p5, 0.5)

float64_t fd_acos(float64_t x)
{
	float64_t z, p, q, r, w, s, c, df;
	int32_t hx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) {
		/* |x| >= 1 */
		if (((ix - 0x3ff00000) | FLO(x)) == 0) {	/* ((ix-0x3ff00000)|__LO(x))==0 */
			/* |x|==1 */
			if (hx > 0) {
				return k_0p0;	/* return 0.0; */	/* acos(1) = 0 */
			} else {
				return FADD(pi, FMUL(k_2p0, pio2_lo));	/* return pi+2.0*pio2_lo; */	/* acos(-1)= pi */
			}
		}
		return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */	/* acos(|x|>1) is NaN */
	}
	if (ix < 0x3fe00000) {
		/* |x| < 0.5 */
		if (ix <= 0x3c600000) {
			return FADD(pio2_hi, pio2_lo);	/* return pio2_hi+pio2_lo; */	/*if|x|<2**-57*/
		}
		z = FMUL(x, x);	/* z = x*x; */
		p = FMUL(z, FADD(pS0, FMUL(z, FADD(pS1, FMUL(z, FADD(pS2, FMUL(z, FADD(pS3, FMUL(z, FADD(pS4, FMUL(z, pS5)))))))))));	/* p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5))))); */
		q = FADD(one, FMUL(z, FADD(qS1, FMUL(z, FADD(qS2, FMUL(z, FADD(qS3, FMUL(z, qS4))))))));	/* q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4))); */
		r = FDIV(p, q);	/* r = p/q; */
		return FSUB(pio2_hi, FSUB(x, FSUB(pio2_lo, FMUL(x, r))));	/* return pio2_hi - (x - (pio2_lo-x*r)); */
	} else if (hx < 0) {
		/* x < -0.5 */
		z = FMUL(FADD(one, x), k_0p5);	/* z = (one+x)*0.5; */
		p = FMUL(z, FADD(pS0, FMUL(z, FADD(pS1, FMUL(z, FADD(pS2, FMUL(z, FADD(pS3, FMUL(z, FADD(pS4, FMUL(z, pS5)))))))))));	/* p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5))))); */
		q = FADD(one, FMUL(z, FADD(qS1, FMUL(z, FADD(qS2, FMUL(z, FADD(qS3, FMUL(z, qS4))))))));	/* q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4))); */
		s = fd_sqrt(z);
		r = FDIV(p, q);	/* r = p/q; */
		w = FSUB(FMUL(r, s), pio2_lo);	/* w = r*s-pio2_lo; */
		return FSUB(pi, FMUL(k_2p0, FADD(s, w)));	/* return pi - 2.0*(s+w); */
	} else {
		/* x > 0.5 */
		z = FMUL(FSUB(one, x), k_0p5);	/* z = (one-x)*0.5; */
		s = fd_sqrt(z);
		df = s;
		FSETLO(df, 0);	/* __LO(df) = 0; */
		c = FDIV(FSUB(z, FMUL(df, df)), FADD(s, df));	/* c = (z-df*df)/(s+df); */
		p = FMUL(z, FADD(pS0, FMUL(z, FADD(pS1, FMUL(z, FADD(pS2, FMUL(z, FADD(pS3, FMUL(z, FADD(pS4, FMUL(z, pS5)))))))))));	/* p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5))))); */
		q = FADD(one, FMUL(z, FADD(qS1, FMUL(z, FADD(qS2, FMUL(z, FADD(qS3, FMUL(z, qS4))))))));	/* q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4))); */
		r = FDIV(p, q);	/* r = p/q; */
		w = FADD(FMUL(r, s), c);	/* w = r*s+c; */
		return FMUL(k_2p0, FADD(df, w));	/* return 2.0*(df+w); */
	}
}
