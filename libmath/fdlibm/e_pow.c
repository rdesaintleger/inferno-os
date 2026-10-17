/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* __ieee754_pow(x,y) return x**y
 *
 *		      n
 * Method:  Let x =  2   * (1+f)
 *	1. Compute and return log2(x) in two pieces:
 *		log2(x) = w1 + w2,
 *	   where w1 has 53-24 = 29 bit trailing zeros.
 *	2. Perform y*log2(x) = n+y' by simulating muti-precision
 *	   arithmetic, where |y'|<=0.5.
 *	3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *	1.  (anything) ** 0  is 1
 *	2.  (anything) ** 1  is itself
 *	3.  (anything) ** NAN is NAN
 *	4.  NAN ** (anything except 0) is NAN
 *	5.  +-(|x| > 1) **  +INF is +INF
 *	6.  +-(|x| > 1) **  -INF is +0
 *	7.  +-(|x| < 1) **  +INF is +0
 *	8.  +-(|x| < 1) **  -INF is +INF
 *	9.  +-1         ** +-INF is NAN
 *	10. +0 ** (+anything except 0, NAN)               is +0
 *	11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *	12. +0 ** (-anything except 0, NAN)               is +INF
 *	13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *	14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *	15. +INF ** (+anything except 0,NAN) is +INF
 *	16. +INF ** (-anything except 0,NAN) is +0
 *	17. -INF ** (anything)  = -0 ** (-anything)
 *	18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *	19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular
 *			pow(integer,integer)
 *	always returns the correct integer provided it is
 *	representable.
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "e_pow.k.h"
F64KA(bp, 2, 1.0, 1.5)
F64KA(dp_h, 2, 0.0, 5.84962487220764160156e-01)
F64KA(dp_l, 2, 0.0, 1.35003920212974897128e-08)
F64K(zero, 0.0)
F64K(one, 1.0)
F64K(two, 2.0)
F64K(two53, 9007199254740992.0)
F64K(huge, 1.0e300)
F64K(tiny, 1.0e-300)
F64K(L1, 5.99999999999994648725e-01)
F64K(L2, 4.28571428578550184252e-01)
F64K(L3, 3.33333329818377432918e-01)
F64K(L4, 2.72728123808534006489e-01)
F64K(L5, 2.30660745775561754067e-01)
F64K(L6, 2.06975017800338417784e-01)
F64K(P1, 1.66666666666666019037e-01)
F64K(P2, -2.77777777770155933842e-03)
F64K(P3, 6.61375632143793436117e-05)
F64K(P4, -1.65339022054652515390e-06)
F64K(P5, 4.13813679705723846039e-08)
F64K(lg2, 6.93147180559945286227e-01)
F64K(lg2_h, 6.93147182464599609375e-01)
F64K(lg2_l, -1.90465429995776804525e-09)
F64K(ovt, 8.0085662595372944372e-0017)
F64K(cp, 9.61796693925975554329e-01)
F64K(cp_h, 9.61796700954437255859e-01)
F64K(cp_l, -7.02846165095275826516e-09)
F64K(ivln2, 1.44269504088896338700e+00)
F64K(ivln2_h, 1.44269502162933349609e+00)
F64K(ivln2_l, 1.92596299112661746887e-08)
F64K(k_0p5, 0.5)
F64K(k_0p3333333333333333333333, 0.3333333333333333333333)
F64K(k_0p25, 0.25)
F64K(k_3p0, 3.0)

float64_t fd_pow(float64_t x, float64_t y)
{
	/* -(1024-log2(ovfl+.5ulp)) */
	/* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
	/* 0x3FEEC709, 0xE0000000 =(float)cp */
	/* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
	/* 0x3FF71547, 0x652B82FE =1/ln2 */
	/* 0x3FF71547, 0x60000000 =24b 1/ln2*/
	/* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/
	float64_t z, ax, z_h, z_l, p_h, p_l;
	float64_t y1, t1, t2, r, s, t, u, v, w;
	int32_t i, j, k, yisint, n;
	int32_t hx, hy, ix, iy;
	uint32_t lx, ly;
	hx = FHI(x);	/* hx = __HI(x); */
	lx = (uint32_t)FLO(x);	/* lx = __LO(x); */
	hy = FHI(y);	/* hy = __HI(y); */
	ly = (uint32_t)FLO(y);	/* ly = __LO(y); */
	ix = hx & 0x7fffffff;
	iy = hy & 0x7fffffff;
	/* y==zero: x**0 = 1 */
	if ((iy | ly) == 0) {
		return one;
	}
	/* +-NaN return x+y */
	if (ix > 0x7ff00000 || ((ix == 0x7ff00000) && (lx != 0)) || iy > 0x7ff00000 || ((iy == 0x7ff00000) && (ly != 0))) {
		return FADD(x, y);	/* return x+y; */
	}
	/* determine if y is an odd int when x < 0
	 * yisint = 0	... y is not an integer
	 * yisint = 1	... y is an odd int
	 * yisint = 2	... y is an even int
	 */
	yisint = 0;
	if (hx < 0) {
		if (iy >= 0x43400000) {
			yisint = 2;	/* even integer y */
		} else if (iy >= 0x3ff00000) {
			k = (iy >> 20) - 0x3ff;	/* exponent */
			if (k > 20) {
				j = (int32_t)(ly >> (52 - k));	/* j = ly>>(52-k); */
				if ((j << (52 - k)) == ly) {
					yisint = 2 - (j & 1);
				}
			} else if (ly == 0) {
				j = iy >> (20 - k);
				if ((j << (20 - k)) == iy) {
					yisint = 2 - (j & 1);
				}
			}
		}
	}
	/* special value of y */
	if (ly == 0) {
		if (iy == 0x7ff00000) {
			/* y is +-inf */
			if (((ix - 0x3ff00000) | lx) == 0) {
				return FSUB(y, y);	/* return y - y; */	/* inf**+-1 is NaN */
			} else if (ix >= 0x3ff00000) {	/* (|x|>1)**+-inf = inf,0 */
				return (hy >= 0) ? y : zero;
			} else {
				return (hy < 0) ? FNEG(y) : zero;	/* return (hy<0)?-y: zero; */	/* (|x|<1)**-,+inf = inf,0 */
			}
		}
		if (iy == 0x3ff00000) {
			/* y is  +-1 */
			if (hy < 0) {
				return FDIV(one, x);	/* return one/x; */
			} else {
				return x;
			}
		}
		if (hy == 0x40000000) {
			return FMUL(x, x);	/* return x*x; */	/* y is 2 */
		}
		if (hy == 0x3fe00000) {
			/* y is  0.5 */
			if (hx >= 0) {	/* x >= +0 */
				return fd_sqrt(x);
			}
		}
	}
	ax = fd_fabs(x);
	/* special value of x */
	if (lx == 0) {
		if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
			z = ax;	/*x is +-0,+-inf,+-1*/
			if (hy < 0) {
				z = FDIV(one, z);	/* z = one/z; */	/* z = (1/|x|) */
			}
			if (hx < 0) {
				if (((ix - 0x3ff00000) | yisint) == 0) {
					z = FDIV(FSUB(z, z), FSUB(z, z));	/* z = (z-z)/(z-z); */	/* (-1)**non-int is NaN */
				} else if (yisint == 1) {
					z = FNEG(z);	/* z = -z; */	/* (x<0)**odd = -(|x|**odd) */
				}
			}
			return z;
		}
	}
	n = (hx >> 31) + 1;
	/* (x<0)**(non-int) is NaN */
	if ((n | yisint) == 0) {
		return FDIV(FSUB(x, x), FSUB(x, x));	/* return (x-x)/(x-x); */
	}
	s = one;	/* s (sign of result -ve**odd) = -1 else = 1 */
	if ((n | (yisint - 1)) == 0) {
		s = FNEG(one);	/* s = -one; */	/* (-ve)**(odd int) */
	}
	/* |y| is huge */
	if (iy > 0x41e00000) {
		/* if |y| > 2**31 */
		if (iy > 0x43f00000) {
			/* if |y| > 2**64, must o/uflow */
			if (ix <= 0x3fefffff) {
				return (hy < 0) ? FMUL(huge, huge) : FMUL(tiny, tiny);	/* return (hy<0)? huge*huge:tiny*tiny; */
			}
			if (ix >= 0x3ff00000) {
				return (hy > 0) ? FMUL(huge, huge) : FMUL(tiny, tiny);	/* return (hy>0)? huge*huge:tiny*tiny; */
			}
		}
		/* over/underflow if x is not close to one */
		if (ix < 0x3fefffff) {
			return (hy < 0) ? FMUL(FMUL(s, huge), huge) : FMUL(FMUL(s, tiny), tiny);	/* return (hy<0)? s*huge*huge:s*tiny*tiny; */
		}
		if (ix > 0x3ff00000) {
			return (hy > 0) ? FMUL(FMUL(s, huge), huge) : FMUL(FMUL(s, tiny), tiny);	/* return (hy>0)? s*huge*huge:s*tiny*tiny; */
		}
		/* now |1-x| is tiny <= 2**-20, suffice to compute
		log(x) by x-x^2/2+x^3/3-x^4/4 */
		t = FSUB(ax, one);	/* t = ax-one; */	/* t has 20 trailing zeros */
		w = FMUL(FMUL(t, t), FSUB(k_0p5, FMUL(t, FSUB(k_0p3333333333333333333333, FMUL(t, k_0p25)))));	/* w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25)); */
		u = FMUL(ivln2_h, t);	/* u = ivln2_h*t; */	/* ivln2_h has 21 sig. bits */
		v = FSUB(FMUL(t, ivln2_l), FMUL(w, ivln2));	/* v = t*ivln2_l-w*ivln2; */
		t1 = FADD(u, v);	/* t1 = u+v; */
		FSETLO(t1, 0);	/* __LO(t1) = 0; */
		t2 = FSUB(v, FSUB(t1, u));	/* t2 = v-(t1-u); */
	} else {
		float64_t ss, s2, s_h, s_l, t_h, t_l;
		n = 0;
		/* take care subnormal number */
		if (ix < 0x00100000) {
			ax = FMUL(ax, two53);	/* ax *= two53; */
			n -= 53;
			ix = FHI(ax);	/* ix = __HI(ax); */
		}
		n += (ix >> 20) - 0x3ff;	/* n += ((ix)>>20)-0x3ff; */
		j = ix & 0x000fffff;
		/* determine interval */
		ix = j | 0x3ff00000;	/* normalize ix */
		if (j <= 0x3988E) {
			k = 0;	/* |x|<sqrt(3/2) */
		} else if (j < 0xBB67A) {
			k = 1;	/* |x|<sqrt(3) */
		} else {
			k = 0;
			n += 1;
			ix -= 0x00100000;
		}
		FSETHI(ax, ix);	/* __HI(ax) = ix; */
		/* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
		u = FSUB(ax, bp[k]);	/* u = ax-bp[k]; */	/* bp[0]=1.0, bp[1]=1.5 */
		v = FDIV(one, FADD(ax, bp[k]));	/* v = one/(ax+bp[k]); */
		ss = FMUL(u, v);	/* ss = u*v; */
		s_h = ss;
		FSETLO(s_h, 0);	/* __LO(s_h) = 0; */
		/* t_h=ax+bp[k] High */
		t_h = zero;
		FSETHI(t_h, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));	/* __HI(t_h)=((ix>>1)|0x20000000)+0x00080000+(k<<18); */
		t_l = FSUB(ax, FSUB(t_h, bp[k]));	/* t_l = ax - (t_h-bp[k]); */
		s_l = FMUL(v, FSUB(FSUB(u, FMUL(s_h, t_h)), FMUL(s_h, t_l)));	/* s_l = v*((u-s_h*t_h)-s_h*t_l); */
		/* compute log(ax) */
		s2 = FMUL(ss, ss);	/* s2 = ss*ss; */
		r = FMUL(FMUL(s2, s2), FADD(L1, FMUL(s2, FADD(L2, FMUL(s2, FADD(L3, FMUL(s2, FADD(L4, FMUL(s2, FADD(L5, FMUL(s2, L6)))))))))));	/* r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6))))); */
		r = FADD(r, FMUL(s_l, FADD(s_h, ss)));	/* r += s_l*(s_h+ss); */
		s2 = FMUL(s_h, s_h);	/* s2 = s_h*s_h; */
		t_h = FADD(FADD(k_3p0, s2), r);	/* t_h = 3.0+s2+r; */
		FSETLO(t_h, 0);	/* __LO(t_h) = 0; */
		t_l = FSUB(r, FSUB(FSUB(t_h, k_3p0), s2));	/* t_l = r-((t_h-3.0)-s2); */
		/* u+v = ss*(1+...) */
		u = FMUL(s_h, t_h);	/* u = s_h*t_h; */
		v = FADD(FMUL(s_l, t_h), FMUL(t_l, ss));	/* v = s_l*t_h+t_l*ss; */
		/* 2/(3log2)*(ss+...) */
		p_h = FADD(u, v);	/* p_h = u+v; */
		FSETLO(p_h, 0);	/* __LO(p_h) = 0; */
		p_l = FSUB(v, FSUB(p_h, u));	/* p_l = v-(p_h-u); */
		z_h = FMUL(cp_h, p_h);	/* z_h = cp_h*p_h; */	/* cp_h+cp_l = 2/(3*log2) */
		z_l = FADD(FADD(FMUL(cp_l, p_h), FMUL(p_l, cp)), dp_l[k]);	/* z_l = cp_l*p_h+p_l*cp+dp_l[k]; */
		/* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
		t = FI(n);	/* t = (double)n; */
		t1 = FADD(FADD(FADD(z_h, z_l), dp_h[k]), t);	/* t1 = (((z_h+z_l)+dp_h[k])+t); */
		FSETLO(t1, 0);	/* __LO(t1) = 0; */
		t2 = FSUB(z_l, FSUB(FSUB(FSUB(t1, t), dp_h[k]), z_h));	/* t2 = z_l-(((t1-t)-dp_h[k])-z_h); */
	}
	/* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
	y1 = y;
	FSETLO(y1, 0);	/* __LO(y1) = 0; */
	p_l = FADD(FMUL(FSUB(y, y1), t1), FMUL(y, t2));	/* p_l = (y-y1)*t1+y*t2; */
	p_h = FMUL(y1, t1);	/* p_h = y1*t1; */
	z = FADD(p_l, p_h);	/* z = p_l+p_h; */
	j = FHI(z);	/* j = __HI(z); */
	i = FLO(z);	/* i = __LO(z); */
	if (j >= 0x40900000) {
		/* z >= 1024 */
		if (((j - 0x40900000) | i) != 0) {	/* if z > 1024 */
			return FMUL(FMUL(s, huge), huge);	/* return s*huge*huge; */	/* overflow */
		} else {
			if (FGT(FADD(p_l, ovt), FSUB(z, p_h))) {	/* p_l+ovt>z-p_h */
				return FMUL(FMUL(s, huge), huge);	/* return s*huge*huge; */	/* overflow */
			}
		}
	} else if ((j & 0x7fffffff) >= 0x4090cc00) {
		/* z <= -1075 */
		if (((j - 0xc090cc00) | i) != 0) {	/* z < -1075 */
			return FMUL(FMUL(s, tiny), tiny);	/* return s*tiny*tiny; */	/* underflow */
		} else {
			if (FLE(p_l, FSUB(z, p_h))) {	/* p_l<=z-p_h */
				return FMUL(FMUL(s, tiny), tiny);	/* return s*tiny*tiny; */	/* underflow */
			}
		}
	}
	/*
	 * compute 2**(p_h+p_l)
	 */
	i = j & 0x7fffffff;
	k = (i >> 20) - 0x3ff;
	n = 0;
	if (i > 0x3fe00000) {
		/* if |z| > 0.5, set n = [z+0.5] */
		n = j + (0x00100000 >> (k + 1));
		k = ((n & 0x7fffffff) >> 20) - 0x3ff;	/* new k for n */
		t = zero;
		FSETHI(t, (n & ~(0x000fffff >> k)));	/* __HI(t) = (n&~(0x000fffff>>k)); */
		n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
		if (j < 0) {
			n = -n;
		}
		p_h = FSUB(p_h, t);	/* p_h -= t; */
	}
	t = FADD(p_l, p_h);	/* t = p_l+p_h; */
	FSETLO(t, 0);	/* __LO(t) = 0; */
	u = FMUL(t, lg2_h);	/* u = t*lg2_h; */
	v = FADD(FMUL(FSUB(p_l, FSUB(t, p_h)), lg2), FMUL(t, lg2_l));	/* v = (p_l-(t-p_h))*lg2+t*lg2_l; */
	z = FADD(u, v);	/* z = u+v; */
	w = FSUB(v, FSUB(z, u));	/* w = v-(z-u); */
	t = FMUL(z, z);	/* t = z*z; */
	t1 = FSUB(z, FMUL(t, FADD(P1, FMUL(t, FADD(P2, FMUL(t, FADD(P3, FMUL(t, FADD(P4, FMUL(t, P5))))))))));	/* t1 = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5)))); */
	r = FSUB(FDIV(FMUL(z, t1), FSUB(t1, two)), FADD(w, FMUL(z, w)));	/* r = (z*t1)/(t1-two)-(w+z*w); */
	z = FSUB(one, FSUB(r, z));	/* z = one-(r-z); */
	j = FHI(z);	/* j = __HI(z); */
	j += (n << 20);
	if ((j >> 20) <= 0) {
		z = fd_scalbn(z, n);	/* subnormal output */
	} else {
		FSETHI(z, FHI(z) + (n << 20));	/* __HI(z) += (n<<20); */
	}
	return FMUL(s, z);	/* return s*z; */
}
