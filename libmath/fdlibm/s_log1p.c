/* @(#)s_log1p.c 1.4 96/03/07 */
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
/* double log1p(double x)
 *
 * Method :
 *   1. Argument Reduction: find k and f such that
 *			1+x = 2^k * (1+f),
 *	   where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *      Note. If k=0, then f=x is exact. However, if k!=0, then f
 *	may not be representable exactly. In that case, a correction
 *	term is need. Let u=1+x rounded. Let c = (1+x)-u, then
 *	log(1+x) - log(u) ~ c/u. Thus, we proceed to compute log(u),
 *	and add back the correction term c/u.
 *	(Note: when x > 2**53, one can simply return log(x))
 *
 *   2. Approximation of log1p(f).
 *	Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *	     	 = 2s + s*R
 *      We use a special Remes algorithm on [0,0.1716] to generate
 * 	a polynomial of degree 14 to approximate R The maximum error
 *	of this polynomial approximation is bounded by 2**-58.45. In
 *	other words,
 *		        2      4      6      8      10      12      14
 *	    R(z) ~ Lp1*s +Lp2*s +Lp3*s +Lp4*s +Lp5*s  +Lp6*s  +Lp7*s
 *  	(the values of Lp1 to Lp7 are listed in the program)
 *	and
 *	    |      2          14          |     -58.45
 *	    | Lp1*s +...+Lp7*s    -  R(z) | <= 2
 *	    |                             |
 *	Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 *	In order to guarantee error in log below 1ulp, we compute log
 *	by
 *		log1p(f) = f - (hfsq - s*(hfsq+R)).
 *
 *	3. Finally, log1p(x) = k*ln2 + log1p(f).
 *		 	     = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
 *	   Here ln2 is split into two floating point number:
 *			ln2_hi + ln2_lo,
 *	   where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 *	log1p(x) is NaN with signal if x < -1 (including -INF) ;
 *	log1p(+INF) is +INF; log1p(-1) is -INF with signal;
 *	log1p(NaN) is that NaN with no signal.
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
 *
 * Note: Assuming log() return accurate answer, the following
 * 	 algorithm can be used to compute log1p(x) to within a few ULP:
 *
 *		u = 1+x;
 *		if(u==1.0) return x ; else
 *			   return log(u)*(x/(u-1.0));
 *
 *	 See HP-15C Advanced Functions Handbook, p.193.
 */
#include "fdlibm.h"
#include "s_log1p.k.h"
F64K(ln2_hi, 6.93147180369123816490e-01)
F64K(ln2_lo, 1.90821492927058770002e-10)
F64K(two54, 1.80143985094819840000e+16)
F64K(Lp1, 6.666666666666735130e-01)
F64K(Lp2, 3.999999999940941908e-01)
F64K(Lp3, 2.857142874366239149e-01)
F64K(Lp4, 2.222219843214978396e-01)
F64K(Lp5, 1.818357216161805012e-01)
F64K(Lp6, 1.531383769920937332e-01)
F64K(Lp7, 1.479819860511658591e-01)
F64K(zero, 0.0)
F64K(k_m1p0, -1.0)
F64K(k_0p5, 0.5)
F64K(k_1p0, 1.0)
F64K(k_0p66666666666666666, 0.66666666666666666)
F64K(k_2p0, 2.0)

float64_t fd_log1p(float64_t x)
{
	float64_t hfsq, f, c, s, z, R, u;
	int32_t k, hx, hu, ax;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	ax = hx & 0x7fffffff;
	k = 1;
	if (hx < 0x3FDA827A) {
		/* x < 0.41422  */
		if (ax >= 0x3ff00000) {
			/* x <= -1.0 */
			if (FEQ(x, k_m1p0)) {	/* x==-1.0 */
				return FDIV(FNEG(two54), zero);	/* return -two54/zero; */	/* log1p(-1)=+inf */
			} else {
				return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */	/* log1p(x<-1)=NaN */
			}
		}
		if (ax < 0x3e200000) {
			/* |x| < 2**-29 */
			if (FGT(FADD(two54, x), zero) && ax < 0x3c900000) {	/* two54+x>zero &&ax<0x3c900000 */	/* raise inexact */ /* |x| < 2**-54 */
				return x;
			} else {
				return FSUB(x, FMUL(FMUL(x, x), k_0p5));	/* return x - x*x*0.5; */
			}
		}
		if (hx > 0 || hx <= ((int32_t)0xbfd2bec3)) {	/* hx>0||hx<=((int)0xbfd2bec3) */
			k = 0;
			f = x;
			hu = 1;
		}
	}
	if (hx >= 0x7ff00000) {	/* -0.2929<x<0.41422 */
		return FADD(x, x);	/* return x+x; */
	}
	if (k != 0) {
		if (hx < 0x43400000) {
			u = FADD(k_1p0, x);	/* u = 1.0+x; */
			hu = FHI(u);	/* hu = __HI(u); */	/* high word of u */
			k = (hu >> 20) - 1023;
			c = (k > 0) ? FSUB(k_1p0, FSUB(u, x)) : FSUB(x, FSUB(u, k_1p0));	/* c = (k>0)? 1.0-(u-x):x-(u-1.0); */	/* correction term */
			c = FDIV(c, u);	/* c /= u; */
		} else {
			u = x;
			hu = FHI(u);	/* hu = __HI(u); */	/* high word of u */
			k = (hu >> 20) - 1023;
			c = FI(0);	/* c = 0; */
		}
		hu &= 0x000fffff;
		if (hu < 0x6a09e) {
			FSETHI(u, hu | 0x3ff00000);	/* __HI(u) = hu|0x3ff00000; */	/* normalize u */
		} else {
			k += 1;
			FSETHI(u, hu | 0x3fe00000);	/* __HI(u) = hu|0x3fe00000; */	/* normalize u/2 */
			hu = (0x00100000 - hu) >> 2;
		}
		f = FSUB(u, k_1p0);	/* f = u-1.0; */
	}
	hfsq = FMUL(FMUL(k_0p5, f), f);	/* hfsq=0.5*f*f; */
	if (hu == 0) {
		/* |f| < 2**-20 */
		if (FEQ(f, zero)) {	/* f==zero */
			if (k == 0) {
				return zero;
			} else {
				c = FADD(c, FMUL(FI(k), ln2_lo));	/* c += k*ln2_lo; */
				return FADD(FMUL(FI(k), ln2_hi), c);	/* return k*ln2_hi+c; */
			}
		}
		R = FMUL(hfsq, FSUB(k_1p0, FMUL(k_0p66666666666666666, f)));	/* R = hfsq*(1.0-0.66666666666666666*f); */
		if (k == 0) {
			return FSUB(f, R);	/* return f-R; */
		} else {
			return FSUB(FMUL(FI(k), ln2_hi), FSUB(FSUB(R, FADD(FMUL(FI(k), ln2_lo), c)), f));	/* return k*ln2_hi-((R-(k*ln2_lo+c))-f); */
		}
	}
	s = FDIV(f, FADD(k_2p0, f));	/* s = f/(2.0+f); */
	z = FMUL(s, s);	/* z = s*s; */
	R = FMUL(z, FADD(Lp1, FMUL(z, FADD(Lp2, FMUL(z, FADD(Lp3, FMUL(z, FADD(Lp4, FMUL(z, FADD(Lp5, FMUL(z, FADD(Lp6, FMUL(z, Lp7)))))))))))));	/* R = z*(Lp1+z*(Lp2+z*(Lp3+z*(Lp4+z*(Lp5+z*(Lp6+z*Lp7)))))); */
	if (k == 0) {
		return FSUB(f, FSUB(hfsq, FMUL(s, FADD(hfsq, R))));	/* return f-(hfsq-s*(hfsq+R)); */
	} else {
		return FSUB(FMUL(FI(k), ln2_hi), FSUB(FSUB(hfsq, FADD(FMUL(s, FADD(hfsq, R)), FADD(FMUL(FI(k), ln2_lo), c))), f));	/* return k*ln2_hi-((hfsq-(s*(hfsq+R)+(k*ln2_lo+c)))-f); */
	}
}
