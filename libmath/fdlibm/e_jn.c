/* @(#)e_jn.c 1.4 95/01/18 */
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
/*
 * __ieee754_jn(n, x), __ieee754_yn(n, x)
 * floating point Bessel's function of the 1st and 2nd kind
 * of order n
 *
 * Special cases:
 *	y0(0)=y1(0)=yn(n,0) = -inf with division by zero signal;
 *	y0(-ve)=y1(-ve)=yn(n,-ve) are NaN with invalid signal.
 * Note 2. About jn(n,x), yn(n,x)
 *	For n=0, j0(x) is called,
 *	for n=1, j1(x) is called,
 *	for n<x, forward recursion us used starting
 *	from values of j0(x) and j1(x).
 *	for n>x, a continued fraction approximation to
 *	j(n,x)/j(n-1,x) is evaluated and then backward
 *	recursion is used starting from a supposed value
 *	for j(n,x). The resulting value of j(0,x) is
 *	compared with the actual value to correct the
 *	supposed value of j(n,x).
 *
 *	yn(n,x) is similar in all respects, except
 *	that forward recursion is used for all
 *	values of n>1.
 *
 */
#include "fdlibm.h"
#include "e_jn.k.h"
F64K(invsqrtpi, 5.64189583547756279280e-01)
F64K(two, 2.00000000000000000000e+00)
F64K(one, 1.00000000000000000000e+00)
F64K(zero, 0.00000000000000000000e+00)
F64K(k_0p5, 0.5)
F64K(k_1p0e9, 1.0e9)
F64K(k_7p09782712893383973096e02, 7.09782712893383973096e+02)
F64K(k_1e100, 1e100)

float64_t fd_jn(int32_t n, float64_t x)
{
	int32_t i, hx, ix, lx, sgn;
	float64_t a, b, temp, di;
	float64_t z, w;
	/* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
	 * Thus, J(-n,x) = J(n,-x)
	 */
	hx = FHI(x);	/* hx = __HI(x); */
	ix = 0x7fffffff & hx;
	lx = FLO(x);	/* lx = __LO(x); */
	/* if J(n,NaN) is NaN */
	if ((ix | ((uint32_t)(lx | -lx)) >> 31) > 0x7ff00000) {	/* (ix|((unsigned)(lx|-lx))>>31)>0x7ff00000 */
		return FADD(x, x);	/* return x+x; */
	}
	if (n < 0) {
		n = -n;
		x = FNEG(x);	/* x = -x; */
		hx ^= 0x80000000;
	}
	if (n == 0) {
		return fd_j0(x);	/* return(fd_j0(x)); */
	}
	if (n == 1) {
		return fd_j1(x);	/* return(fd_j1(x)); */
	}
	sgn = (n & 1) & (hx >> 31);	/* even n -- 0, odd n -- sign(x) */
	x = fd_fabs(x);
	if ((ix | lx) == 0 || ix >= 0x7ff00000) {	/* if x is 0 or inf */
		b = zero;
	} else if (FLE(FI(n), x)) {	/* (double)n<=x */
		/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
		if (ix >= 0x52D00000) {
			/* x > 2**302 */
			/* (x >> n**2)
			 *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
			 *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
			 *	    Let s=sin(x), c=cos(x),
			 *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
			 *
			 *		   n	sin(xn)*sqt2	cos(xn)*sqt2
			 *		----------------------------------
			 *		   0	 s-c		 c+s
			 *		   1	-s-c 		-c+s
			 *		   2	-s+c		-c-s
			 *		   3	 s+c		 c-s
			 */
			switch (n & 3) {
			case 0:
				temp = FADD(fd_cos(x), fd_sin(x));	/* temp = fd_cos(x)+fd_sin(x); */
				break;
			case 1:
				temp = FADD(FNEG(fd_cos(x)), fd_sin(x));	/* temp = -fd_cos(x)+fd_sin(x); */
				break;
			case 2:
				temp = FSUB(FNEG(fd_cos(x)), fd_sin(x));	/* temp = -fd_cos(x)-fd_sin(x); */
				break;
			case 3:
				temp = FSUB(fd_cos(x), fd_sin(x));	/* temp = fd_cos(x)-fd_sin(x); */
				break;
			}
			b = FDIV(FMUL(invsqrtpi, temp), fd_sqrt(x));	/* b = invsqrtpi*temp/fd_sqrt(x); */
		} else {
			a = fd_j0(x);
			b = fd_j1(x);
			for (i = 1; i < n; i++) {
				temp = b;
				b = FSUB(FMUL(b, FDIV(FI((i + i)), x)), a);	/* b = b*((double)(i+i)/x) - a; */	/* avoid underflow */
				a = temp;
			}
		}
	} else {
		if (ix < 0x3e100000) {
			/* x < 2**-29 */
			/* x is tiny, return the first Taylor expansion of J(n,x)
			 * J(n,x) = 1/n!*(x/2)^n  - ...
			 */
			if (n > 33) {	/* underflow */
				b = zero;
			} else {
				temp = FMUL(x, k_0p5);	/* temp = x*0.5; */
				b = temp;
				for (a = one, i = 2; i <= n; i++) {
					a = FMUL(a, FI(i));	/* a *= (double)i; */	/* a = n! */
					b = FMUL(b, temp);	/* b *= temp; */	/* b = (x/2)^n */
				}
				b = FDIV(b, a);	/* b = b/a; */
			}
		} else {
			/* use backward recurrence */
			/* 			x      x^2      x^2
			 *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
			 *			2n  - 2(n+1) - 2(n+2)
			 *
			 * 			1      1        1
			 *  (for large x)   =  ----  ------   ------   .....
			 *			2n   2(n+1)   2(n+2)
			 *			-- - ------ - ------ -
			 *			 x     x         x
			 *
			 * Let w = 2n/x and h=2/x, then the above quotient
			 * is equal to the continued fraction:
			 *		    1
			 *	= -----------------------
			 *		       1
			 *	   w - -----------------
			 *			  1
			 * 	        w+h - ---------
			 *		       w+2h - ...
			 *
			 * To determine how many terms needed, let
			 * Q(0) = w, Q(1) = w(w+h) - 1,
			 * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
			 * When Q(k) > 1e4	good for single
			 * When Q(k) > 1e9	good for double
			 * When Q(k) > 1e17	good for quadruple
			 */
			/* determine k */
			float64_t t, v;
			float64_t q0, q1, h, tmp;
			int32_t k, m;
			w = FDIV(FI((n + n)), x);	/* w = (n+n)/(double)x; */
			h = FDIV(two, x);	/* h = 2.0/(double)x; */
			q0 = w;
			z = FADD(w, h);	/* z = w+h; */
			q1 = FSUB(FMUL(w, z), one);	/* q1 = w*z - 1.0; */
			k = 1;
			while (FLT(q1, k_1p0e9)) {	/* q1<1.0e9 */
				k += 1;
				z = FADD(z, h);	/* z += h; */
				tmp = FSUB(FMUL(z, q1), q0);	/* tmp = z*q1 - q0; */
				q0 = q1;
				q1 = tmp;
			}
			m = n + n;
			for (t = zero, i = 2 * (n + k); i >= m; i -= 2) {
				t = FDIV(one, FSUB(FDIV(FI(i), x), t));	/* t = one/(i/x-t); */
			}
			a = t;
			b = one;
			/*  estimate log((2/x)^n*n!) = n*log(2/x)+n*ln(n)
			 *  Hence, if n*(log(2n/x)) > ...
			 *  single 8.8722839355e+01
			 *  double 7.09782712893383973096e+02
			 *  long double 1.1356523406294143949491931077970765006170e+04
			 *  then recurrent value may overflow and the result is
			 *  likely underflow to zero
			 */
			tmp = FI(n);	/* tmp = n; */
			v = FDIV(two, x);	/* v = two/x; */
			tmp = FMUL(tmp, fd_log(fd_fabs(FMUL(v, tmp))));	/* tmp = tmp*fd_log(fd_fabs(v*tmp)); */
			if (FLT(tmp, k_7p09782712893383973096e02)) {	/* tmp<7.09782712893383973096e+02 */
				for (i = n - 1, di = FI((i + i)); i > 0; i--) {	/* i=n-1,di=(double)(i+i);i>0;i-- */
					temp = b;
					b = FMUL(b, di);	/* b *= di; */
					b = FSUB(FDIV(b, x), a);	/* b = b/x - a; */
					a = temp;
					di = FSUB(di, two);	/* di -= two; */
				}
			} else {
				for (i = n - 1, di = FI((i + i)); i > 0; i--) {	/* i=n-1,di=(double)(i+i);i>0;i-- */
					temp = b;
					b = FMUL(b, di);	/* b *= di; */
					b = FSUB(FDIV(b, x), a);	/* b = b/x - a; */
					a = temp;
					di = FSUB(di, two);	/* di -= two; */
					/* scale b to avoid spurious overflow */
					if (FGT(b, k_1e100)) {	/* b>1e100 */
						a = FDIV(a, b);	/* a /= b; */
						t = FDIV(t, b);	/* t /= b; */
						b = one;
					}
				}
			}
			b = FDIV(FMUL(t, fd_j0(x)), b);	/* b = (t*fd_j0(x)/b); */
		}
	}
	if (sgn == 1) {
		return FNEG(b);	/* return -b; */
	} else {
		return b;
	}
}

float64_t fd_yn(int32_t n, float64_t x)
{
	int32_t i, hx, ix, lx;
	int32_t sign;
	float64_t a, b, temp;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = 0x7fffffff & hx;
	lx = FLO(x);	/* lx = __LO(x); */
	/* if Y(n,NaN) is NaN */
	if ((ix | ((uint32_t)(lx | -lx)) >> 31) > 0x7ff00000) {	/* (ix|((unsigned)(lx|-lx))>>31)>0x7ff00000 */
		return FADD(x, x);	/* return x+x; */
	}
	if ((ix | lx) == 0) {
		return FDIV(FNEG(one), zero);	/* return -one/zero; */
	}
	if (hx < 0) {
		return FDIV(zero, zero);	/* return zero/zero; */
	}
	sign = 1;
	if (n < 0) {
		n = -n;
		sign = 1 - ((n & 1) << 1);
	}
	if (n == 0) {
		return fd_y0(x);	/* return(fd_y0(x)); */
	}
	if (n == 1) {
		return FMUL(FI(sign), fd_y1(x));	/* return(sign*fd_y1(x)); */
	}
	if (ix == 0x7ff00000) {
		return zero;
	}
	if (ix >= 0x52D00000) {
		/* x > 2**302 */
		/* (x >> n**2)
		 *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
		 *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
		 *	    Let s=sin(x), c=cos(x),
		 *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
		 *
		 *		   n	sin(xn)*sqt2	cos(xn)*sqt2
		 *		----------------------------------
		 *		   0	 s-c		 c+s
		 *		   1	-s-c 		-c+s
		 *		   2	-s+c		-c-s
		 *		   3	 s+c		 c-s
		 */
		switch (n & 3) {
		case 0:
			temp = FSUB(fd_sin(x), fd_cos(x));	/* temp = fd_sin(x)-fd_cos(x); */
			break;
		case 1:
			temp = FSUB(FNEG(fd_sin(x)), fd_cos(x));	/* temp = -fd_sin(x)-fd_cos(x); */
			break;
		case 2:
			temp = FADD(FNEG(fd_sin(x)), fd_cos(x));	/* temp = -fd_sin(x)+fd_cos(x); */
			break;
		case 3:
			temp = FADD(fd_sin(x), fd_cos(x));	/* temp = fd_sin(x)+fd_cos(x); */
			break;
		}
		b = FDIV(FMUL(invsqrtpi, temp), fd_sqrt(x));	/* b = invsqrtpi*temp/fd_sqrt(x); */
	} else {
		a = fd_y0(x);
		b = fd_y1(x);
		/* quit if b is -inf */
		for (i = 1; i < n && (FHI(b) != 0xfff00000); i++) {	/* i=1;i<n&&(__HI(b) != 0xfff00000);i++ */
			temp = b;
			b = FSUB(FMUL(FDIV(FI((i + i)), x), b), a);	/* b = ((double)(i+i)/x)*b - a; */
			a = temp;
		}
	}
	if (sign > 0) {
		return b;
	} else {
		return FNEG(b);	/* return -b; */
	}
}
