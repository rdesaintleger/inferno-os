/* @(#)e_asin.c 1.4 96/03/07 */
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
/* __ieee754_asin(x)
 * Method :
 *	Since  asin(x) = x + x^3/6 + x^5*3/40 + x^7*15/336 + ...
 *	we approximate asin(x) on [0,0.5] by
 *		asin(x) = x + x*x^2*R(x^2)
 *	where
 *		R(x^2) is a rational approximation of (asin(x)-x)/x^3
 *	and its Remes error is bounded by
 *		|(asin(x)-x)/x^3 - R(x^2)| < 2^(-58.75)
 *
 *	For x in [0.5,1]
 *		asin(x) = pi/2-2*asin(sqrt((1-x)/2))
 *	Let y = (1-x), z = y/2, s := sqrt(z), and pio2_hi+pio2_lo=pi/2;
 *	then for x>0.98
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio2_hi - (2*(s+s*z*R(z)) - pio2_lo)
 *	For x<=0.98, let pio4_hi = pio2_hi/2, then
 *		f = hi part of s;
 *		c = sqrt(z) - f = (z-f*f)/(s+f) 	...f+c=sqrt(z)
 *	and
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio4_hi+(pio4-2s)-(2s*z*R(z)-pio2_lo)
 *			= pio4_hi+(pio4-2f)-(2s*z*R(z)-(pio2_lo+2c))
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 */
#include "fdlibm.h"
#include "e_asin.k.h"
F64K(one, 1.00000000000000000000e+00)
F64K(huge, 1.000e+300)
F64K(pio2_hi, 1.57079632679489655800e+00)
F64K(pio2_lo, 6.12323399573676603587e-17)
F64K(pio4_hi, 7.85398163397448278999e-01)
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
F64K(k_0p5, 0.5)
F64K(k_2p0, 2.0)

float64_t fd_asin(float64_t x)
{
	float64_t t, w, p, q, c, r, s;
	int32_t hx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) {
		/* |x|>= 1 */
		if (((ix - 0x3ff00000) | FLO(x)) == 0) {	/* ((ix-0x3ff00000)|__LO(x))==0 */
			/* asin(1)=+-pi/2 with inexact */
			return FADD(FMUL(x, pio2_hi), FMUL(x, pio2_lo));	/* return x*pio2_hi+x*pio2_lo; */
		}
		return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */	/* asin(|x|>1) is NaN */
	} else if (ix < 0x3fe00000) {
		/* |x|<0.5 */
		if (ix < 0x3e400000) {
			/* if |x| < 2**-27 */
			if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
				return x;	/* return x with inexact if x!=0*/
			}
		} else {
			t = FMUL(x, x);	/* t = x*x; */
		}
		p = FMUL(t, FADD(pS0, FMUL(t, FADD(pS1, FMUL(t, FADD(pS2, FMUL(t, FADD(pS3, FMUL(t, FADD(pS4, FMUL(t, pS5)))))))))));	/* p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5))))); */
		q = FADD(one, FMUL(t, FADD(qS1, FMUL(t, FADD(qS2, FMUL(t, FADD(qS3, FMUL(t, qS4))))))));	/* q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4))); */
		w = FDIV(p, q);	/* w = p/q; */
		return FADD(x, FMUL(x, w));	/* return x+x*w; */
	}
	/* 1> |x|>= 0.5 */
	w = FSUB(one, fd_fabs(x));	/* w = one-fd_fabs(x); */
	t = FMUL(w, k_0p5);	/* t = w*0.5; */
	p = FMUL(t, FADD(pS0, FMUL(t, FADD(pS1, FMUL(t, FADD(pS2, FMUL(t, FADD(pS3, FMUL(t, FADD(pS4, FMUL(t, pS5)))))))))));	/* p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5))))); */
	q = FADD(one, FMUL(t, FADD(qS1, FMUL(t, FADD(qS2, FMUL(t, FADD(qS3, FMUL(t, qS4))))))));	/* q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4))); */
	s = fd_sqrt(t);
	if (ix >= 0x3FEF3333) {
		/* if |x| > 0.975 */
		w = FDIV(p, q);	/* w = p/q; */
		t = FSUB(pio2_hi, FSUB(FMUL(k_2p0, FADD(s, FMUL(s, w))), pio2_lo));	/* t = pio2_hi-(2.0*(s+s*w)-pio2_lo); */
	} else {
		w = s;
		FSETLO(w, 0);	/* __LO(w) = 0; */
		c = FDIV(FSUB(t, FMUL(w, w)), FADD(s, w));	/* c = (t-w*w)/(s+w); */
		r = FDIV(p, q);	/* r = p/q; */
		p = FSUB(FMUL(FMUL(k_2p0, s), r), FSUB(pio2_lo, FMUL(k_2p0, c)));	/* p = 2.0*s*r-(pio2_lo-2.0*c); */
		q = FSUB(pio4_hi, FMUL(k_2p0, w));	/* q = pio4_hi-2.0*w; */
		t = FSUB(pio4_hi, FSUB(p, q));	/* t = pio4_hi-(p-q); */
	}
	if (hx > 0) {
		return t;
	} else {
		return FNEG(t);	/* return -t; */
	}
}
