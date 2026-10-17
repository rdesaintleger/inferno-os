#pragma ident "@(#)k_tan.c 1.5 04/04/22 SMI"
/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* INDENT OFF */
/* __kernel_tan( x, y, k )
 * kernel tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input k indicates whether tan (if k = 1) or -1/tan (if k = -1) is returned.
 *
 * Algorithm
 *	1. Since tan(-x) = -tan(x), we need only to consider positive x.
 *	2. if x < 2^-28 (hx<0x3e300000 0), return x with inexact if x!=0.
 *	3. tan(x) is approximated by a odd polynomial of degree 27 on
 *	   [0,0.67434]
 *		  	         3             27
 *	   	tan(x) ~ x + T1*x + ... + T13*x
 *	   where
 *
 * 	        |tan(x)         2     4            26   |     -59.2
 * 	        |----- - (1+T1*x +T2*x +.... +T13*x    )| <= 2
 * 	        |  x 					|
 *
 *	   Note: tan(x+y) = tan(x) + tan'(x)*y
 *		          ~ tan(x) + (1+x*x)*y
 *	   Therefore, for better accuracy in computing tan(x+y), let
 *		     3      2      2       2       2
 *		r = x *(T2+x *(T3+x *(...+x *(T12+x *T13))))
 *	   then
 *		 		    3    2
 *		tan(x+y) = x + (T1*x + (x *(r+y)+y))
 *
 *      4. For x in [0.67434,pi/4],  let y = pi/4 - x, then
 *		tan(x) = tan(pi/4-y) = (1-tan(y))/(1+tan(y))
 *		       = 1 - 2*(tan(y) - (tan(y)^2)/(1+tan(y)))
 */
#include "fdlibm.h"
#include "k_tan.k.h"
F64KA(xxx, 16, 3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02, 8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04, 2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05, 2.59073051863633712884e-05, 1.00000000000000000000e+00, 7.85398163397448278999e-01, 3.06161699786838301793e-17)
#define	one	xxx[13]
#define	pio4	xxx[14]
#define	pio4lo	xxx[15]
#define	T	xxx
F64K(k_0p0, 0.0)
F64K(k_2p0, 2.0)
F64K(k_m1p0, -1.0)
F64K(k_1p0, 1.0)

/* INDENT ON */
float64_t fd_kernel_tan(float64_t x, float64_t y, int32_t iy)
{
	float64_t z, r, v, w, s;
	int32_t ix, hx;
	hx = FHI(x);	/* hx = __HI(x); */	/* high word of x */
	ix = hx & 0x7fffffff;	/* high word of |x| */
	if (ix < 0x3e300000) {
		/* x < 2**-28 */
		if (FTOI(x) == 0) {	/* (int) x == 0 */
			/* generate inexact */
			if (((ix | FLO(x)) | (iy + 1)) == 0) {	/* ((ix | __LO(x)) | (iy + 1)) == 0 */
				return FDIV(one, fd_fabs(x));	/* return one / fd_fabs(x); */
			} else {
				if (iy == 1) {
					return x;
				} else {
					/* compute -1 / (x+y) carefully */
					float64_t a, t;
					z = w = FADD(x, y);	/* z = w = x + y; */
					FSETLO(z, 0);	/* __LO(z) = 0; */
					v = FSUB(y, FSUB(z, x));	/* v = y - (z - x); */
					t = a = FDIV(FNEG(one), w);	/* t = a = -one / w; */
					FSETLO(t, 0);	/* __LO(t) = 0; */
					s = FADD(one, FMUL(t, z));	/* s = one + t * z; */
					return FADD(t, FMUL(a, FADD(s, FMUL(t, v))));	/* return t + a * (s + t * v); */
				}
			}
		}
	}
	if (ix >= 0x3FE59428) {
		/* |x| >= 0.6744 */
		if (hx < 0) {
			x = FNEG(x);	/* x = -x; */
			y = FNEG(y);	/* y = -y; */
		}
		z = FSUB(pio4, x);	/* z = pio4 - x; */
		w = FSUB(pio4lo, y);	/* w = pio4lo - y; */
		x = FADD(z, w);	/* x = z + w; */
		y = k_0p0;	/* y = 0.0; */
	}
	z = FMUL(x, x);	/* z = x * x; */
	w = FMUL(z, z);	/* w = z * z; */
	/*
	 * Break x^5*(T[1]+x^2*T[2]+...) into
	 * x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
	 * x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
	 */
	r = FADD(T[1], FMUL(w, FADD(T[3], FMUL(w, FADD(T[5], FMUL(w, FADD(T[7], FMUL(w, FADD(T[9], FMUL(w, T[11]))))))))));	/* r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11])))); */
	v = FMUL(z, FADD(T[2], FMUL(w, FADD(T[4], FMUL(w, FADD(T[6], FMUL(w, FADD(T[8], FMUL(w, FADD(T[10], FMUL(w, T[12])))))))))));	/* v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12]))))); */
	s = FMUL(z, x);	/* s = z * x; */
	r = FADD(y, FMUL(z, FADD(FMUL(s, FADD(r, v)), y)));	/* r = y + z * (s * (r + v) + y); */
	r = FADD(r, FMUL(T[0], s));	/* r += T[0] * s; */
	w = FADD(x, r);	/* w = x + r; */
	if (ix >= 0x3FE59428) {
		v = FI(iy);	/* v = (double) iy; */
		return FMUL(FI((1 - ((hx >> 30) & 2))), FSUB(v, FMUL(k_2p0, FSUB(x, FSUB(FDIV(FMUL(w, w), FADD(w, v)), r)))));	/* return (double) (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r))); */
	}
	if (iy == 1) {
		return w;
	} else {
		/*
		 * if allow error up to 2 ulp, simply return
		 * -1.0 / (x+r) here
		 */
		/* compute -1.0 / (x+r) accurately */
		float64_t a, t;
		z = w;
		FSETLO(z, 0);	/* __LO(z) = 0; */
		v = FSUB(r, FSUB(z, x));	/* v = r - (z - x); */	/* z+v = r+x */
		t = a = FDIV(k_m1p0, w);	/* t = a = -1.0 / w; */	/* a = -1.0/w */
		FSETLO(t, 0);	/* __LO(t) = 0; */
		s = FADD(k_1p0, FMUL(t, z));	/* s = 1.0 + t * z; */
		return FADD(t, FMUL(a, FADD(s, FMUL(t, v))));	/* return t + a * (s + t * v); */
	}
}
