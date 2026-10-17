/* @(#)s_erf.c 1.3 95/01/18 */
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
/* double erf(double x)
 * double erfc(double x)
 *			     x
 *		      2      |\
 *     erf(x)  =  ---------  | exp(-t*t)dt
 *	 	   sqrt(pi) \|
 *			     0
 *
 *     erfc(x) =  1-erf(x)
 *  Note that
 *		erf(-x) = -erf(x)
 *		erfc(-x) = 2 - erfc(x)
 *
 * Method:
 *	1. For |x| in [0, 0.84375]
 *	    erf(x)  = x + x*R(x^2)
 *          erfc(x) = 1 - erf(x)           if x in [-.84375,0.25]
 *                  = 0.5 + ((0.5-x)-x*R)  if x in [0.25,0.84375]
 *	   where R = P/Q where P is an odd poly of degree 8 and
 *	   Q is an odd poly of degree 10.
 *						 -57.90
 *			| R - (erf(x)-x)/x | <= 2
 *
 *
 *	   Remark. The formula is derived by noting
 *          erf(x) = (2/sqrt(pi))*(x - x^3/3 + x^5/10 - x^7/42 + ....)
 *	   and that
 *          2/sqrt(pi) = 1.128379167095512573896158903121545171688
 *	   is close to one. The interval is chosen because the fix
 *	   point of erf(x) is near 0.6174 (i.e., erf(x)=x when x is
 *	   near 0.6174), and by some experiment, 0.84375 is chosen to
 * 	   guarantee the error is less than one ulp for erf.
 *
 *      2. For |x| in [0.84375,1.25], let s = |x| - 1, and
 *         c = 0.84506291151 rounded to single (24 bits)
 *         	erf(x)  = sign(x) * (c  + P1(s)/Q1(s))
 *         	erfc(x) = (1-c)  - P1(s)/Q1(s) if x > 0
 *			  1+(c+P1(s)/Q1(s))    if x < 0
 *         	|P1/Q1 - (erf(|x|)-c)| <= 2**-59.06
 *	   Remark: here we use the taylor series expansion at x=1.
 *		erf(1+s) = erf(1) + s*Poly(s)
 *			 = 0.845.. + P1(s)/Q1(s)
 *	   That is, we use rational approximation to approximate
 *			erf(1+s) - (c = (single)0.84506291151)
 *	   Note that |P1/Q1|< 0.078 for x in [0.84375,1.25]
 *	   where
 *		P1(s) = degree 6 poly in s
 *		Q1(s) = degree 6 poly in s
 *
 *      3. For x in [1.25,1/0.35(~2.857143)],
 *         	erfc(x) = (1/x)*exp(-x*x-0.5625+R1/S1)
 *         	erf(x)  = 1 - erfc(x)
 *	   where
 *		R1(z) = degree 7 poly in z, (z=1/x^2)
 *		S1(z) = degree 8 poly in z
 *
 *      4. For x in [1/0.35,28]
 *         	erfc(x) = (1/x)*exp(-x*x-0.5625+R2/S2) if x > 0
 *			= 2.0 - (1/x)*exp(-x*x-0.5625+R2/S2) if -6<x<0
 *			= 2.0 - tiny		(if x <= -6)
 *         	erf(x)  = sign(x)*(1.0 - erfc(x)) if x < 6, else
 *         	erf(x)  = sign(x)*(1.0 - tiny)
 *	   where
 *		R2(z) = degree 6 poly in z, (z=1/x^2)
 *		S2(z) = degree 7 poly in z
 *
 *      Note1:
 *	   To compute exp(-x*x-0.5625+R/S), let s be a single
 *	   precision number and s := x; then
 *		-x*x = -s*s + (s-x)*(s+x)
 *	        exp(-x*x-0.5626+R/S) =
 *			exp(-s*s-0.5625)*exp((s-x)*(s+x)+R/S);
 *      Note2:
 *	   Here 4 and 5 make use of the asymptotic series
 *			  exp(-x*x)
 *		erfc(x) ~ ---------- * ( 1 + Poly(1/x^2) )
 *			  x*sqrt(pi)
 *	   We use rational approximation to approximate
 *      	g(s)=f(1/x^2) = log(erfc(x)*x) - x*x + 0.5625
 *	   Here is the error bound for R1/S1 and R2/S2
 *      	|R1/S1 - f(x)|  < 2**(-62.57)
 *      	|R2/S2 - f(x)|  < 2**(-61.52)
 *
 *      5. For inf > x >= 28
 *         	erf(x)  = sign(x) *(1 - tiny)  (raise inexact)
 *         	erfc(x) = tiny*tiny (raise underflow) if x > 0
 *			= 2 - tiny if x<0
 *
 *      7. Special case:
 *         	erf(0)  = 0, erf(inf)  = 1, erf(-inf) = -1,
 *         	erfc(0) = 1, erfc(inf) = 0, erfc(-inf) = 2,
 *	   	erfc/erf(NaN) is NaN
 */
#include "fdlibm.h"
#include "s_erf.k.h"
F64K(tiny, 1e-300)
F64K(half, 5.00000000000000000000e-01)
F64K(one, 1.00000000000000000000e+00)
F64K(two, 2.00000000000000000000e+00)
F64K(erx, 8.45062911510467529297e-01)
F64K(efx, 1.28379167095512586316e-01)
F64K(efx8, 1.02703333676410069053e+00)
F64K(pp0, 1.28379167095512558561e-01)
F64K(pp1, -3.25042107247001499370e-01)
F64K(pp2, -2.84817495755985104766e-02)
F64K(pp3, -5.77027029648944159157e-03)
F64K(pp4, -2.37630166566501626084e-05)
F64K(qq1, 3.97917223959155352819e-01)
F64K(qq2, 6.50222499887672944485e-02)
F64K(qq3, 5.08130628187576562776e-03)
F64K(qq4, 1.32494738004321644526e-04)
F64K(qq5, -3.96022827877536812320e-06)
F64K(pa0, -2.36211856075265944077e-03)
F64K(pa1, 4.14856118683748331666e-01)
F64K(pa2, -3.72207876035701323847e-01)
F64K(pa3, 3.18346619901161753674e-01)
F64K(pa4, -1.10894694282396677476e-01)
F64K(pa5, 3.54783043256182359371e-02)
F64K(pa6, -2.16637559486879084300e-03)
F64K(qa1, 1.06420880400844228286e-01)
F64K(qa2, 5.40397917702171048937e-01)
F64K(qa3, 7.18286544141962662868e-02)
F64K(qa4, 1.26171219808761642112e-01)
F64K(qa5, 1.36370839120290507362e-02)
F64K(qa6, 1.19844998467991074170e-02)
F64K(ra0, -9.86494403484714822705e-03)
F64K(ra1, -6.93858572707181764372e-01)
F64K(ra2, -1.05586262253232909814e+01)
F64K(ra3, -6.23753324503260060396e+01)
F64K(ra4, -1.62396669462573470355e+02)
F64K(ra5, -1.84605092906711035994e+02)
F64K(ra6, -8.12874355063065934246e+01)
F64K(ra7, -9.81432934416914548592e+00)
F64K(sa1, 1.96512716674392571292e+01)
F64K(sa2, 1.37657754143519042600e+02)
F64K(sa3, 4.34565877475229228821e+02)
F64K(sa4, 6.45387271733267880336e+02)
F64K(sa5, 4.29008140027567833386e+02)
F64K(sa6, 1.08635005541779435134e+02)
F64K(sa7, 6.57024977031928170135e+00)
F64K(sa8, -6.04244152148580987438e-02)
F64K(rb0, -9.86494292470009928597e-03)
F64K(rb1, -7.99283237680523006574e-01)
F64K(rb2, -1.77579549177547519889e+01)
F64K(rb3, -1.60636384855821916062e+02)
F64K(rb4, -6.37566443368389627722e+02)
F64K(rb5, -1.02509513161107724954e+03)
F64K(rb6, -4.83519191608651397019e+02)
F64K(sb1, 3.03380607434824582924e+01)
F64K(sb2, 3.25792512996573918826e+02)
F64K(sb3, 1.53672958608443695994e+03)
F64K(sb4, 3.19985821950859553908e+03)
F64K(sb5, 2.55305040643316442583e+03)
F64K(sb6, 4.74528541206955367215e+02)
F64K(sb7, -2.24409524465858183362e+01)
F64K(k_0p125, 0.125)
F64K(k_8p0, 8.0)
F64K(k_0p5625, 0.5625)

float64_t fd_erf(float64_t x)
{
	int32_t hx, ix, i;
	float64_t R, S, P, Q, s, y, z, r;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		/* erf(nan)=nan */
		i = (int32_t)(((uint32_t)hx >> 31) << 1);	/* i = ((unsigned)hx>>31)<<1; */
		return FADD(FI((1 - i)), FDIV(one, x));	/* return (double)(1-i)+one/x; */	/* erf(+-inf)=+-1 */
	}
	if (ix < 0x3feb0000) {
		/* |x|<0.84375 */
		if (ix < 0x3e300000) {
			/* |x|<2**-28 */
			if (ix < 0x00800000) {
				return FMUL(k_0p125, FADD(FMUL(k_8p0, x), FMUL(efx8, x)));	/* return 0.125*(8.0*x+efx8*x); */	/*avoid underflow */
			}
			return FADD(x, FMUL(efx, x));	/* return x + efx*x; */
		}
		z = FMUL(x, x);	/* z = x*x; */
		r = FADD(pp0, FMUL(z, FADD(pp1, FMUL(z, FADD(pp2, FMUL(z, FADD(pp3, FMUL(z, pp4))))))));	/* r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4))); */
		s = FADD(one, FMUL(z, FADD(qq1, FMUL(z, FADD(qq2, FMUL(z, FADD(qq3, FMUL(z, FADD(qq4, FMUL(z, qq5))))))))));	/* s = one+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5)))); */
		y = FDIV(r, s);	/* y = r/s; */
		return FADD(x, FMUL(x, y));	/* return x + x*y; */
	}
	if (ix < 0x3ff40000) {
		/* 0.84375 <= |x| < 1.25 */
		s = FSUB(fd_fabs(x), one);	/* s = fd_fabs(x)-one; */
		P = FADD(pa0, FMUL(s, FADD(pa1, FMUL(s, FADD(pa2, FMUL(s, FADD(pa3, FMUL(s, FADD(pa4, FMUL(s, FADD(pa5, FMUL(s, pa6))))))))))));	/* P = pa0+s*(pa1+s*(pa2+s*(pa3+s*(pa4+s*(pa5+s*pa6))))); */
		Q = FADD(one, FMUL(s, FADD(qa1, FMUL(s, FADD(qa2, FMUL(s, FADD(qa3, FMUL(s, FADD(qa4, FMUL(s, FADD(qa5, FMUL(s, qa6))))))))))));	/* Q = one+s*(qa1+s*(qa2+s*(qa3+s*(qa4+s*(qa5+s*qa6))))); */
		if (hx >= 0) {
			return FADD(erx, FDIV(P, Q));	/* return erx + P/Q; */
		} else {
			return FSUB(FNEG(erx), FDIV(P, Q));	/* return -erx - P/Q; */
		}
	}
	if (ix >= 0x40180000) {
		/* inf>|x|>=6 */
		if (hx >= 0) {
			return FSUB(one, tiny);	/* return one-tiny; */
		} else {
			return FSUB(tiny, one);	/* return tiny-one; */
		}
	}
	x = fd_fabs(x);
	s = FDIV(one, FMUL(x, x));	/* s = one/(x*x); */
	if (ix < 0x4006DB6E) {
		/* |x| < 1/0.35 */
		R = FADD(ra0, FMUL(s, FADD(ra1, FMUL(s, FADD(ra2, FMUL(s, FADD(ra3, FMUL(s, FADD(ra4, FMUL(s, FADD(ra5, FMUL(s, FADD(ra6, FMUL(s, ra7))))))))))))));	/* R=ra0+s*(ra1+s*(ra2+s*(ra3+s*(ra4+s*( ra5+s*(ra6+s*ra7)))))); */
		S = FADD(one, FMUL(s, FADD(sa1, FMUL(s, FADD(sa2, FMUL(s, FADD(sa3, FMUL(s, FADD(sa4, FMUL(s, FADD(sa5, FMUL(s, FADD(sa6, FMUL(s, FADD(sa7, FMUL(s, sa8))))))))))))))));	/* S=one+s*(sa1+s*(sa2+s*(sa3+s*(sa4+s*( sa5+s*(sa6+s*(sa7+s*sa8))))))); */
	} else {
		/* |x| >= 1/0.35 */
		R = FADD(rb0, FMUL(s, FADD(rb1, FMUL(s, FADD(rb2, FMUL(s, FADD(rb3, FMUL(s, FADD(rb4, FMUL(s, FADD(rb5, FMUL(s, rb6))))))))))));	/* R=rb0+s*(rb1+s*(rb2+s*(rb3+s*(rb4+s*( rb5+s*rb6))))); */
		S = FADD(one, FMUL(s, FADD(sb1, FMUL(s, FADD(sb2, FMUL(s, FADD(sb3, FMUL(s, FADD(sb4, FMUL(s, FADD(sb5, FMUL(s, FADD(sb6, FMUL(s, sb7))))))))))))));	/* S=one+s*(sb1+s*(sb2+s*(sb3+s*(sb4+s*( sb5+s*(sb6+s*sb7)))))); */
	}
	z = x;
	FSETLO(z, 0);	/* __LO(z) = 0; */
	r = FMUL(fd_exp(FSUB(FMUL(FNEG(z), z), k_0p5625)), fd_exp(FADD(FMUL(FSUB(z, x), FADD(z, x)), FDIV(R, S))));	/* r = fd_exp(-z*z-0.5625)*fd_exp((z-x)*(z+x)+R/S); */
	if (hx >= 0) {
		return FSUB(one, FDIV(r, x));	/* return one-r/x; */
	} else {
		return FSUB(FDIV(r, x), one);	/* return r/x-one; */
	}
}

float64_t fd_erfc(float64_t x)
{
	int32_t hx, ix;
	float64_t R, S, P, Q, s, y, z, r;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		/* erfc(nan)=nan */
		/* erfc(+-inf)=0,2 */
		return FADD(FU((((uint32_t)hx >> 31) << 1)), FDIV(one, x));	/* return (double)(((unsigned)hx>>31)<<1)+one/x; */
	}
	if (ix < 0x3feb0000) {
		/* |x|<0.84375 */
		if (ix < 0x3c700000) {	/* |x|<2**-56 */
			return FSUB(one, x);	/* return one-x; */
		}
		z = FMUL(x, x);	/* z = x*x; */
		r = FADD(pp0, FMUL(z, FADD(pp1, FMUL(z, FADD(pp2, FMUL(z, FADD(pp3, FMUL(z, pp4))))))));	/* r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4))); */
		s = FADD(one, FMUL(z, FADD(qq1, FMUL(z, FADD(qq2, FMUL(z, FADD(qq3, FMUL(z, FADD(qq4, FMUL(z, qq5))))))))));	/* s = one+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5)))); */
		y = FDIV(r, s);	/* y = r/s; */
		if (hx < 0x3fd00000) {
			/* x<1/4 */
			return FSUB(one, FADD(x, FMUL(x, y)));	/* return one-(x+x*y); */
		} else {
			r = FMUL(x, y);	/* r = x*y; */
			r = FADD(r, FSUB(x, half));	/* r += (x-half); */
			return FSUB(half, r);	/* return half - r ; */
		}
	}
	if (ix < 0x3ff40000) {
		/* 0.84375 <= |x| < 1.25 */
		s = FSUB(fd_fabs(x), one);	/* s = fd_fabs(x)-one; */
		P = FADD(pa0, FMUL(s, FADD(pa1, FMUL(s, FADD(pa2, FMUL(s, FADD(pa3, FMUL(s, FADD(pa4, FMUL(s, FADD(pa5, FMUL(s, pa6))))))))))));	/* P = pa0+s*(pa1+s*(pa2+s*(pa3+s*(pa4+s*(pa5+s*pa6))))); */
		Q = FADD(one, FMUL(s, FADD(qa1, FMUL(s, FADD(qa2, FMUL(s, FADD(qa3, FMUL(s, FADD(qa4, FMUL(s, FADD(qa5, FMUL(s, qa6))))))))))));	/* Q = one+s*(qa1+s*(qa2+s*(qa3+s*(qa4+s*(qa5+s*qa6))))); */
		if (hx >= 0) {
			z = FSUB(one, erx);	/* z = one-erx; */
			return FSUB(z, FDIV(P, Q));	/* return z - P/Q; */
		} else {
			z = FADD(erx, FDIV(P, Q));	/* z = erx+P/Q; */
			return FADD(one, z);	/* return one+z; */
		}
	}
	if (ix < 0x403c0000) {
		/* |x|<28 */
		x = fd_fabs(x);
		s = FDIV(one, FMUL(x, x));	/* s = one/(x*x); */
		if (ix < 0x4006DB6D) {
			/* |x| < 1/.35 ~ 2.857143*/
			R = FADD(ra0, FMUL(s, FADD(ra1, FMUL(s, FADD(ra2, FMUL(s, FADD(ra3, FMUL(s, FADD(ra4, FMUL(s, FADD(ra5, FMUL(s, FADD(ra6, FMUL(s, ra7))))))))))))));	/* R=ra0+s*(ra1+s*(ra2+s*(ra3+s*(ra4+s*( ra5+s*(ra6+s*ra7)))))); */
			S = FADD(one, FMUL(s, FADD(sa1, FMUL(s, FADD(sa2, FMUL(s, FADD(sa3, FMUL(s, FADD(sa4, FMUL(s, FADD(sa5, FMUL(s, FADD(sa6, FMUL(s, FADD(sa7, FMUL(s, sa8))))))))))))))));	/* S=one+s*(sa1+s*(sa2+s*(sa3+s*(sa4+s*( sa5+s*(sa6+s*(sa7+s*sa8))))))); */
		} else {
			/* |x| >= 1/.35 ~ 2.857143 */
			if (hx < 0 && ix >= 0x40180000) {
				return FSUB(two, tiny);	/* return two-tiny; */	/* x < -6 */
			}
			R = FADD(rb0, FMUL(s, FADD(rb1, FMUL(s, FADD(rb2, FMUL(s, FADD(rb3, FMUL(s, FADD(rb4, FMUL(s, FADD(rb5, FMUL(s, rb6))))))))))));	/* R=rb0+s*(rb1+s*(rb2+s*(rb3+s*(rb4+s*( rb5+s*rb6))))); */
			S = FADD(one, FMUL(s, FADD(sb1, FMUL(s, FADD(sb2, FMUL(s, FADD(sb3, FMUL(s, FADD(sb4, FMUL(s, FADD(sb5, FMUL(s, FADD(sb6, FMUL(s, sb7))))))))))))));	/* S=one+s*(sb1+s*(sb2+s*(sb3+s*(sb4+s*( sb5+s*(sb6+s*sb7)))))); */
		}
		z = x;
		FSETLO(z, 0);	/* __LO(z) = 0; */
		r = FMUL(fd_exp(FSUB(FMUL(FNEG(z), z), k_0p5625)), fd_exp(FADD(FMUL(FSUB(z, x), FADD(z, x)), FDIV(R, S))));	/* r = fd_exp(-z*z-0.5625)* fd_exp((z-x)*(z+x)+R/S); */
		if (hx > 0) {
			return FDIV(r, x);	/* return r/x; */
		} else {
			return FSUB(two, FDIV(r, x));	/* return two-r/x; */
		}
	} else {
		if (hx > 0) {
			return FMUL(tiny, tiny);	/* return tiny*tiny; */
		} else {
			return FSUB(two, tiny);	/* return two-tiny; */
		}
	}
}
