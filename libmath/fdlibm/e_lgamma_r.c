/* @(#)e_lgamma_r.c 1.3 95/01/18 */
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
/* __ieee754_lgamma_r(x, signgamp)
 * Reentrant version of the logarithm of the Gamma function
 * with user provide pointer for the sign of Gamma(x).
 *
 * Method:
 *   1. Argument Reduction for 0 < x <= 8
 * 	Since gamma(1+s)=s*gamma(s), for x in [0,8], we may
 * 	reduce x to a number in [1.5,2.5] by
 * 		lgamma(1+s) = log(s) + lgamma(s)
 *	for example,
 *		lgamma(7.3) = log(6.3) + lgamma(6.3)
 *			    = log(6.3*5.3) + lgamma(5.3)
 *			    = log(6.3*5.3*4.3*3.3*2.3) + lgamma(2.3)
 *   2. Polynomial approximation of lgamma around its
 *	minimun ymin=1.461632144968362245 to maintain monotonicity.
 *	On [ymin-0.23, ymin+0.27] (i.e., [1.23164,1.73163]), use
 *		Let z = x-ymin;
 *		lgamma(x) = -1.214862905358496078218 + z^2*poly(z)
 *	where
 *		poly(z) is a 14 degree polynomial.
 *   2. Rational approximation in the primary interval [2,3]
 *	We use the following approximation:
 *		s = x-2.0;
 *		lgamma(x) = 0.5*s + s*P(s)/Q(s)
 *	with accuracy
 *		|P/Q - (lgamma(x)-0.5s)| < 2**-61.71
 *	Our algorithms are based on the following observation
 *
 *                             zeta(2)-1    2    zeta(3)-1    3
 * lgamma(2+s) = s*(1-Euler) + --------- * s  -  --------- * s  + ...
 *                                 2                 3
 *
 *	where Euler = 0.5771... is the Euler constant, which is very
 *	close to 0.5.
 *
 *   3. For x>=8, we have
 *	lgamma(x)~(x-0.5)log(x)-x+0.5*log(2pi)+1/(12x)-1/(360x**3)+....
 *	(better formula:
 *	   lgamma(x)~(x-0.5)*(log(x)-1)-.5*(log(2pi)-1) + ...)
 *	Let z = 1/x, then we approximation
 *		f(z) = lgamma(x) - (x-0.5)(log(x)-1)
 *	by
 *	  			    3       5             11
 *		w = w0 + w1*z + w2*z  + w3*z  + ... + w6*z
 *	where
 *		|w - f(z)| < 2**-58.74
 *
 *   4. For negative x, since (G is gamma function)
 *		-x*G(-x)*G(x) = pi/sin(pi*x),
 * 	we have
 * 		G(x) = pi/(sin(pi*x)*(-x)*G(-x))
 *	since G(-x) is positive, sign(G(x)) = sign(sin(pi*x)) for x<0
 *	Hence, for x<0, signgam = sign(sin(pi*x)) and
 *		lgamma(x) = log(|Gamma(x)|)
 *			  = log(pi/(|x*sin(pi*x)|)) - lgamma(-x);
 *	Note: one should avoid compute pi*(-x) directly in the
 *	      computation of sin(pi*(-x)).
 *
 *   5. Special Cases
 *		lgamma(2+s) ~ s*(1-Euler) for tiny s
 *		lgamma(1)=lgamma(2)=0
 *		lgamma(x) ~ -log(x) for tiny x
 *		lgamma(0) = lgamma(inf) = inf
 *	 	lgamma(-integer) = +-inf
 *
 */
#include "fdlibm.h"
#include "e_lgamma_r.k.h"
F64K(two52, 4.50359962737049600000e+15)
F64K(half, 5.00000000000000000000e-01)
F64K(one, 1.00000000000000000000e+00)
F64K(pi, 3.14159265358979311600e+00)
F64K(a0, 7.72156649015328655494e-02)
F64K(a1, 3.22467033424113591611e-01)
F64K(a2, 6.73523010531292681824e-02)
F64K(a3, 2.05808084325167332806e-02)
F64K(a4, 7.38555086081402883957e-03)
F64K(a5, 2.89051383673415629091e-03)
F64K(a6, 1.19270763183362067845e-03)
F64K(a7, 5.10069792153511336608e-04)
F64K(a8, 2.20862790713908385557e-04)
F64K(a9, 1.08011567247583939954e-04)
F64K(a10, 2.52144565451257326939e-05)
F64K(a11, 4.48640949618915160150e-05)
F64K(tc, 1.46163214496836224576e+00)
F64K(tf, -1.21486290535849611461e-01)
F64K(tt, -3.63867699703950536541e-18)
F64K(t0, 4.83836122723810047042e-01)
F64K(t1, -1.47587722994593911752e-01)
F64K(t2, 6.46249402391333854778e-02)
F64K(t3, -3.27885410759859649565e-02)
F64K(t4, 1.79706750811820387126e-02)
F64K(t5, -1.03142241298341437450e-02)
F64K(t6, 6.10053870246291332635e-03)
F64K(t7, -3.68452016781138256760e-03)
F64K(t8, 2.25964780900612472250e-03)
F64K(t9, -1.40346469989232843813e-03)
F64K(t10, 8.81081882437654011382e-04)
F64K(t11, -5.38595305356740546715e-04)
F64K(t12, 3.15632070903625950361e-04)
F64K(t13, -3.12754168375120860518e-04)
F64K(t14, 3.35529192635519073543e-04)
F64K(u0, -7.72156649015328655494e-02)
F64K(u1, 6.32827064025093366517e-01)
F64K(u2, 1.45492250137234768737e+00)
F64K(u3, 9.77717527963372745603e-01)
F64K(u4, 2.28963728064692451092e-01)
F64K(u5, 1.33810918536787660377e-02)
F64K(v1, 2.45597793713041134822e+00)
F64K(v2, 2.12848976379893395361e+00)
F64K(v3, 7.69285150456672783825e-01)
F64K(v4, 1.04222645593369134254e-01)
F64K(v5, 3.21709242282423911810e-03)
F64K(s0, -7.72156649015328655494e-02)
F64K(s1, 2.14982415960608852501e-01)
F64K(s2, 3.25778796408930981787e-01)
F64K(s3, 1.46350472652464452805e-01)
F64K(s4, 2.66422703033638609560e-02)
F64K(s5, 1.84028451407337715652e-03)
F64K(s6, 3.19475326584100867617e-05)
F64K(r1, 1.39200533467621045958e+00)
F64K(r2, 7.21935547567138069525e-01)
F64K(r3, 1.71933865632803078993e-01)
F64K(r4, 1.86459191715652901344e-02)
F64K(r5, 7.77942496381893596434e-04)
F64K(r6, 7.32668430744625636189e-06)
F64K(w0, 4.18938533204672725052e-01)
F64K(w1, 8.33333333333329678849e-02)
F64K(w2, -2.77777777728775536470e-03)
F64K(w3, 7.93650558643019558500e-04)
F64K(w4, -5.95187557450339963135e-04)
F64K(w5, 8.36339918996282139126e-04)
F64K(w6, -1.63092934096575273989e-03)
F64K(zero, 0.00000000000000000000e+00)
F64K(k_2p0, 2.0)
F64K(k_4p0, 4.0)
F64K(k_1p5, 1.5)
F64K(k_m0p5, -0.5)
F64K(k_6p0, 6.0)
F64K(k_5p0, 5.0)
F64K(k_3p0, 3.0)

static float64_t sin_pi(float64_t x)
{
	float64_t y, z;
	int32_t n, ix;
	ix = 0x7fffffff & FHI(x);	/* ix = 0x7fffffff&__HI(x); */
	if (ix < 0x3fd00000) {
		return fd_kernel_sin(FMUL(pi, x), zero, 0);	/* return fd_kernel_sin(pi*x,zero,0); */
	}
	y = FNEG(x);	/* y = -x; */	/* x is assume negative */
	/*
	 * argument reduction, make sure inexact flag not raised if input
	 * is an integer
	 */
	z = fd_floor(y);
	if (FNE(z, y)) {	/* z!=y */
		/* inexact anyway */
		y = FMUL(y, half);	/* y *= 0.5; */
		y = FMUL(k_2p0, FSUB(y, fd_floor(y)));	/* y = 2.0*(y - fd_floor(y)); */	/* y = |x| mod 2.0 */
		n = FTOI(FMUL(y, k_4p0));	/* n = (int) (y*4.0); */
	} else {
		if (ix >= 0x43400000) {
			y = zero;
			n = 0;	/* y must be even */
		} else {
			if (ix < 0x43300000) {
				z = FADD(y, two52);	/* z = y+two52; */	/* exact */
			}
			n = FLO(z) & 1;	/* n = __LO(z)&1; */	/* lower word of z */
			y = FI(n);	/* y = n; */
			n <<= 2;
		}
	}
	switch (n) {
	case 0:
		y = fd_kernel_sin(FMUL(pi, y), zero, 0);	/* y = fd_kernel_sin(pi*y,zero,0); */
		break;
	case 1:
	case 2:
		y = fd_kernel_cos(FMUL(pi, FSUB(half, y)), zero);	/* y = fd_kernel_cos(pi*(0.5-y),zero); */
		break;
	case 3:
	case 4:
		y = fd_kernel_sin(FMUL(pi, FSUB(one, y)), zero, 0);	/* y = fd_kernel_sin(pi*(one-y),zero,0); */
		break;
	case 5:
	case 6:
		y = FNEG(fd_kernel_cos(FMUL(pi, FSUB(y, k_1p5)), zero));	/* y = -fd_kernel_cos(pi*(y-1.5),zero); */
		break;
	default:
		y = fd_kernel_sin(FMUL(pi, FSUB(y, k_2p0)), zero, 0);	/* y = fd_kernel_sin(pi*(y-2.0),zero,0); */
		break;
	}
	return FNEG(y);	/* return -y; */
}

float64_t fd_lgamma_r(float64_t x, int32_t *signgamp)
{
	float64_t t, y, z, nadj, p, p1, p2, p3, q, r, w;
	int32_t i, hx, lx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	lx = FLO(x);	/* lx = __LO(x); */
	/* purge off +-inf, NaN, +-0, and negative arguments */
	*signgamp = 1;
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		return FMUL(x, x);	/* return x*x; */
	}
	if ((ix | lx) == 0) {
		return FDIV(one, zero);	/* return one/zero; */
	}
	if (ix < 0x3b900000) {
		/* |x|<2**-70, return -log(|x|) */
		if (hx < 0) {
			*signgamp = -1;
			return FNEG(fd_log(FNEG(x)));	/* return -fd_log(-x); */
		} else {
			return FNEG(fd_log(x));	/* return -fd_log(x); */
		}
	}
	if (hx < 0) {
		if (ix >= 0x43300000) {	/* |x|>=2**52, must be -integer */
			return FDIV(one, zero);	/* return one/zero; */
		}
		t = sin_pi(x);
		if (FEQ(t, zero)) {	/* t==zero */
			return FDIV(one, zero);	/* return one/zero; */	/* -integer */
		}
		nadj = fd_log(FDIV(pi, fd_fabs(FMUL(t, x))));	/* nadj = fd_log(pi/fd_fabs(t*x)); */
		if (FLT(t, zero)) {	/* t<zero */
			*signgamp = -1;
		}
		x = FNEG(x);	/* x = -x; */
	}
	/* purge off 1 and 2 */
	if ((((ix - 0x3ff00000) | lx) == 0) || (((ix - 0x40000000) | lx) == 0)) {
		r = FI(0);	/* r = 0; */
		/* for x < 2.0 */
	} else if (ix < 0x40000000) {
		if (ix <= 0x3feccccc) {
			/* lgamma(x) = lgamma(x+1)-log(x) */
			r = FNEG(fd_log(x));	/* r = -fd_log(x); */
			if (ix >= 0x3FE76944) {
				y = FSUB(one, x);	/* y = one-x; */
				i = 0;
			} else if (ix >= 0x3FCDA661) {
				y = FSUB(x, FSUB(tc, one));	/* y= x-(tc-one); */
				i = 1;
			} else {
				y = x;
				i = 2;
			}
		} else {
			r = zero;
			if (ix >= 0x3FFBB4C3) {
				y = FSUB(k_2p0, x);	/* y=2.0-x; */
				i = 0;
			} else if (ix >= 0x3FF3B4C4) {	/* [1.7316,2] */
				y = FSUB(x, tc);	/* y=x-tc; */
				i = 1;
			} else {
				/* [1.23,1.73] */
				y = FSUB(x, one);	/* y=x-one; */
				i = 2;
			}
		}
		switch (i) {
		case 0:
			z = FMUL(y, y);	/* z = y*y; */
			p1 = FADD(a0, FMUL(z, FADD(a2, FMUL(z, FADD(a4, FMUL(z, FADD(a6, FMUL(z, FADD(a8, FMUL(z, a10))))))))));	/* p1 = a0+z*(a2+z*(a4+z*(a6+z*(a8+z*a10)))); */
			p2 = FMUL(z, FADD(a1, FMUL(z, FADD(a3, FMUL(z, FADD(a5, FMUL(z, FADD(a7, FMUL(z, FADD(a9, FMUL(z, a11)))))))))));	/* p2 = z*(a1+z*(a3+z*(a5+z*(a7+z*(a9+z*a11))))); */
			p = FADD(FMUL(y, p1), p2);	/* p = y*p1+p2; */
			r = FADD(r, FSUB(p, FMUL(half, y)));	/* r += (p-0.5*y); */
			break;
		case 1:
			z = FMUL(y, y);	/* z = y*y; */
			w = FMUL(z, y);	/* w = z*y; */
			p1 = FADD(t0, FMUL(w, FADD(t3, FMUL(w, FADD(t6, FMUL(w, FADD(t9, FMUL(w, t12))))))));	/* p1 = t0+w*(t3+w*(t6+w*(t9 +w*t12))); */	/* parallel comp */
			p2 = FADD(t1, FMUL(w, FADD(t4, FMUL(w, FADD(t7, FMUL(w, FADD(t10, FMUL(w, t13))))))));	/* p2 = t1+w*(t4+w*(t7+w*(t10+w*t13))); */
			p3 = FADD(t2, FMUL(w, FADD(t5, FMUL(w, FADD(t8, FMUL(w, FADD(t11, FMUL(w, t14))))))));	/* p3 = t2+w*(t5+w*(t8+w*(t11+w*t14))); */
			p = FSUB(FMUL(z, p1), FSUB(tt, FMUL(w, FADD(p2, FMUL(y, p3)))));	/* p = z*p1-(tt-w*(p2+y*p3)); */
			r = FADD(r, FADD(tf, p));	/* r += (tf + p); */
			break;
		case 2:
			p1 = FMUL(y, FADD(u0, FMUL(y, FADD(u1, FMUL(y, FADD(u2, FMUL(y, FADD(u3, FMUL(y, FADD(u4, FMUL(y, u5)))))))))));	/* p1 = y*(u0+y*(u1+y*(u2+y*(u3+y*(u4+y*u5))))); */
			p2 = FADD(one, FMUL(y, FADD(v1, FMUL(y, FADD(v2, FMUL(y, FADD(v3, FMUL(y, FADD(v4, FMUL(y, v5))))))))));	/* p2 = one+y*(v1+y*(v2+y*(v3+y*(v4+y*v5)))); */
			r = FADD(r, FADD(FMUL(k_m0p5, y), FDIV(p1, p2)));	/* r += (-0.5*y + p1/p2); */
		}
	} else if (ix < 0x40200000) {
		/* x < 8.0 */
		i = FTOI(x);	/* i = (int)x; */
		t = zero;
		y = FSUB(x, FI(i));	/* y = x-(double)i; */
		p = FMUL(y, FADD(s0, FMUL(y, FADD(s1, FMUL(y, FADD(s2, FMUL(y, FADD(s3, FMUL(y, FADD(s4, FMUL(y, FADD(s5, FMUL(y, s6)))))))))))));	/* p = y*(s0+y*(s1+y*(s2+y*(s3+y*(s4+y*(s5+y*s6)))))); */
		q = FADD(one, FMUL(y, FADD(r1, FMUL(y, FADD(r2, FMUL(y, FADD(r3, FMUL(y, FADD(r4, FMUL(y, FADD(r5, FMUL(y, r6))))))))))));	/* q = one+y*(r1+y*(r2+y*(r3+y*(r4+y*(r5+y*r6))))); */
		r = FADD(FMUL(half, y), FDIV(p, q));	/* r = half*y+p/q; */
		z = one;	/* lgamma(1+s) = log(s) + lgamma(s) */
		switch (i) {
		case 7:
			z = FMUL(z, FADD(y, k_6p0));	/* z *= (y+6.0); */	/* FALLTHRU */
		case 6:
			z = FMUL(z, FADD(y, k_5p0));	/* z *= (y+5.0); */	/* FALLTHRU */
		case 5:
			z = FMUL(z, FADD(y, k_4p0));	/* z *= (y+4.0); */	/* FALLTHRU */
		case 4:
			z = FMUL(z, FADD(y, k_3p0));	/* z *= (y+3.0); */	/* FALLTHRU */
		case 3:
			z = FMUL(z, FADD(y, k_2p0));	/* z *= (y+2.0); */	/* FALLTHRU */
			r = FADD(r, fd_log(z));	/* r += fd_log(z); */
			break;
		}
		/* 8.0 <= x < 2**58 */
	} else if (ix < 0x43900000) {
		t = fd_log(x);
		z = FDIV(one, x);	/* z = one/x; */
		y = FMUL(z, z);	/* y = z*z; */
		w = FADD(w0, FMUL(z, FADD(w1, FMUL(y, FADD(w2, FMUL(y, FADD(w3, FMUL(y, FADD(w4, FMUL(y, FADD(w5, FMUL(y, w6))))))))))));	/* w = w0+z*(w1+y*(w2+y*(w3+y*(w4+y*(w5+y*w6))))); */
		r = FADD(FMUL(FSUB(x, half), FSUB(t, one)), w);	/* r = (x-half)*(t-one)+w; */
	} else {
		/* 2**58 <= x <= inf */
		r = FMUL(x, FSUB(fd_log(x), one));	/* r = x*(fd_log(x)-one); */
	}
	if (hx < 0) {
		r = FSUB(nadj, r);	/* r = nadj - r; */
	}
	return r;
}
