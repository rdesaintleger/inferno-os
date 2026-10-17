/* @(#)s_expm1.c 1.5 04/04/22 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* expm1(x)
 * Returns exp(x)-1, the exponential of x minus 1.
 *
 * Method
 *   1. Argument reduction:
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2 ~ 0.34658
 *
 *      Here a correction term c will be computed to compensate
 *	the error in r when rounded to a floating-point number.
 *
 *   2. Approximating expm1(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Since
 *	    r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 - r^4/360 + ...
 *	we define R1(r*r) by
 *	    r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 * R1(r*r)
 *	That is,
 *	    R1(r**2) = 6/r *((exp(r)+1)/(exp(r)-1) - 2/r)
 *		     = 6/r * ( 1 + 2.0*(1/(exp(r)-1) - 1/r))
 *		     = 1 - r^2/60 + r^4/2520 - r^6/100800 + ...
 *      We use a special Remes algorithm on [0,0.347] to generate
 * 	a polynomial of degree 5 in r*r to approximate R1. The
 *	maximum error of this polynomial approximation is bounded
 *	by 2**-61. In other words,
 *	    R1(z) ~ 1.0 + Q1*z + Q2*z**2 + Q3*z**3 + Q4*z**4 + Q5*z**5
 *	where 	Q1  =  -1.6666666666666567384E-2,
 * 		Q2  =   3.9682539681370365873E-4,
 * 		Q3  =  -9.9206344733435987357E-6,
 * 		Q4  =   2.5051361420808517002E-7,
 * 		Q5  =  -6.2843505682382617102E-9;
 *  	(where z=r*r, and the values of Q1 to Q5 are listed below)
 *	with error bounded by
 *	    |                  5           |     -61
 *	    | 1.0+Q1*z+...+Q5*z   -  R1(z) | <= 2
 *	    |                              |
 *
 *	expm1(r) = exp(r)-1 is then computed by the following
 * 	specific way which minimize the accumulation rounding error:
 *			       2     3
 *			      r     r    [ 3 - (R1 + R1*r/2)  ]
 *	      expm1(r) = r + --- + --- * [--------------------]
 *		              2     2    [ 6 - r*(3 - R1*r/2) ]
 *
 *	To compensate the error in the argument reduction, we use
 *		expm1(r+c) = expm1(r) + c + expm1(r)*c
 *			   ~ expm1(r) + c + r*c
 *	Thus c+r*c will be added in as the correction terms for
 *	expm1(r+c). Now rearrange the term to avoid optimization
 * 	screw up:
 *		        (      2                                    2 )
 *		        ({  ( r    [ R1 -  (3 - R1*r/2) ]  )  }    r  )
 *	 expm1(r+c)~r - ({r*(--- * [--------------------]-c)-c} - --- )
 *	                ({  ( 2    [ 6 - r*(3 - R1*r/2) ]  )  }    2  )
 *                      (                                             )
 *
 *		   = r - E
 *   3. Scale back to obtain expm1(x):
 *	From step 1, we have
 *	   expm1(x) = either 2^k*[expm1(r)+1] - 1
 *		    = or     2^k*[expm1(r) + (1-2^-k)]
 *   4. Implementation notes:
 *	(A). To save one multiplication, we scale the coefficient Qi
 *	     to Qi*2^i, and replace z by (x^2)/2.
 *	(B). To achieve maximum accuracy, we compute expm1(x) by
 *	  (i)   if x < -56*ln2, return -1.0, (raise inexact if x!=inf)
 *	  (ii)  if k=0, return r-E
 *	  (iii) if k=-1, return 0.5*(r-E)-0.5
 *        (iv)	if k=1 if r < -0.25, return 2*((r+0.5)- E)
 *	       	       else	     return  1.0+2.0*(r-E);
 *	  (v)   if (k<-2||k>56) return 2^k(1-(E-r)) - 1 (or exp(x)-1)
 *	  (vi)  if k <= 20, return 2^k((1-2^-k)-(E-r)), else
 *	  (vii) return 2^k(1-((E+2^-k)-r))
 *
 * Special cases:
 *	expm1(INF) is INF, expm1(NaN) is NaN;
 *	expm1(-INF) is -1, and
 *	for finite argument, only expm1(0)=0 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double
 *	    if x >  7.09782712893383973096e+02 then expm1(x) overflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "s_expm1.k.h"
F64K(one, 1.0)
F64K(huge, 1.0e+300)
F64K(tiny, 1.0e-300)
F64K(o_threshold, 7.09782712893383973096e+02)
F64K(ln2_hi, 6.93147180369123816490e-01)
F64K(ln2_lo, 1.90821492927058770002e-10)
F64K(invln2, 1.44269504088896338700e+00)
F64K(Q1, -3.33333333333331316428e-02)
F64K(Q2, 1.58730158725481460165e-03)
F64K(Q3, -7.93650757867487942473e-05)
F64K(Q4, 4.00821782732936239552e-06)
F64K(Q5, -2.01099218183624371326e-07)
F64K(k_m1p0, -1.0)
F64K(k_0p0, 0.0)
F64K(k_0p5, 0.5)
F64K(k_m0p5, -0.5)
F64K(k_3p0, 3.0)
F64K(k_6p0, 6.0)
F64K(k_m0p25, -0.25)
F64K(k_m2p0, -2.0)
F64K(k_2p0, 2.0)

float64_t fd_expm1(float64_t x)
{
	float64_t y, hi, lo, c, t, e, hxs, hfx, r1;
	int32_t k, xsb;
	uint32_t hx;
	hx = (uint32_t)FHI(x);	/* hx = __HI(x); */	/* high word of x */
	xsb = (int32_t)(hx & 0x80000000);	/* xsb = hx&0x80000000; */	/* sign bit of x */
	if (xsb == 0) {
		y = x;
	} else {
		y = FNEG(x);	/* y= -x; */	/* y = |x| */
	}
	hx &= 0x7fffffff;	/* high word of |x| */
	/* filter out huge and non-finite argument */
	if (hx >= 0x4043687A) {
		/* if |x|>=56*ln2 */
		if (hx >= 0x40862E42) {
			/* if |x|>=709.78... */
			if (hx >= 0x7ff00000) {
				if (((hx & 0xfffff) | FLO(x)) != 0) {	/* ((hx&0xfffff)|__LO(x))!=0 */
					return FADD(x, x);	/* return x+x; */	/* NaN */
				} else {
					return (xsb == 0) ? x : k_m1p0;	/* return (xsb==0)? x:-1.0; */	/* exp(+-inf)={inf,-1} */
				}
			}
			if (FGT(x, o_threshold)) {	/* x > o_threshold */
				return FMUL(huge, huge);	/* return huge*huge; */	/* overflow */
			}
		}
		if (xsb != 0) {
			/* x < -56*ln2, return -1.0 with inexact */
			if (FLT(FADD(x, tiny), k_0p0)) {	/* x+tiny<0.0 */	/* raise inexact */
				return FSUB(tiny, one);	/* return tiny-one; */	/* return -1 */
			}
		}
	}
	/* argument reduction */
	if (hx > 0x3fd62e42) {
		/* if  |x| > 0.5 ln2 */
		if (hx < 0x3FF0A2B2) {
			/* and |x| < 1.5 ln2 */
			if (xsb == 0) {
				hi = FSUB(x, ln2_hi);	/* hi = x - ln2_hi; */
				lo = ln2_lo;
				k = 1;
			} else {
				hi = FADD(x, ln2_hi);	/* hi = x + ln2_hi; */
				lo = FNEG(ln2_lo);	/* lo = -ln2_lo; */
				k = -1;
			}
		} else {
			k = FTOI(FADD(FMUL(invln2, x), ((xsb == 0) ? k_0p5 : k_m0p5)));	/* k = invln2*x+((xsb==0)?0.5:-0.5); */
			t = FI(k);	/* t = k; */
			hi = FSUB(x, FMUL(t, ln2_hi));	/* hi = x - t*ln2_hi; */	/* t*ln2_hi is exact here */
			lo = FMUL(t, ln2_lo);	/* lo = t*ln2_lo; */
		}
		x = FSUB(hi, lo);	/* x = hi - lo; */
		c = FSUB(FSUB(hi, x), lo);	/* c = (hi-x)-lo; */
	} else if (hx < 0x3c900000) {
		/* when |x|<2**-54, return x */
		t = FADD(huge, x);	/* t = huge+x; */	/* return x with inexact flags when x!=0 */
		return FSUB(x, FSUB(t, FADD(huge, x)));	/* return x - (t-(huge+x)); */
	} else {
		k = 0;
	}
	/* x is now in primary range */
	hfx = FMUL(k_0p5, x);	/* hfx = 0.5*x; */
	hxs = FMUL(x, hfx);	/* hxs = x*hfx; */
	r1 = FADD(one, FMUL(hxs, FADD(Q1, FMUL(hxs, FADD(Q2, FMUL(hxs, FADD(Q3, FMUL(hxs, FADD(Q4, FMUL(hxs, Q5))))))))));	/* r1 = one+hxs*(Q1+hxs*(Q2+hxs*(Q3+hxs*(Q4+hxs*Q5)))); */
	t = FSUB(k_3p0, FMUL(r1, hfx));	/* t = 3.0-r1*hfx; */
	e = FMUL(hxs, FDIV(FSUB(r1, t), FSUB(k_6p0, FMUL(x, t))));	/* e = hxs*((r1-t)/(6.0 - x*t)); */
	if (k == 0) {
		return FSUB(x, FSUB(FMUL(x, e), hxs));	/* return x - (x*e-hxs); */	/* c is 0 */
	} else {
		e = FSUB(FMUL(x, FSUB(e, c)), c);	/* e = (x*(e-c)-c); */
		e = FSUB(e, hxs);	/* e -= hxs; */
		if (k == -1) {
			return FSUB(FMUL(k_0p5, FSUB(x, e)), k_0p5);	/* return 0.5*(x-e)-0.5; */
		}
		if (k == 1) {
			if (FLT(x, k_m0p25)) {	/* x < -0.25 */
				return FMUL(k_m2p0, FSUB(e, FADD(x, k_0p5)));	/* return -2.0*(e-(x+0.5)); */
			} else {
				return FADD(one, FMUL(k_2p0, FSUB(x, e)));	/* return one+2.0*(x-e); */
			}
		}
		if (k <= -2 || k > 56) {
			/* suffice to return exp(x)-1 */
			y = FSUB(one, FSUB(e, x));	/* y = one-(e-x); */
			FSETHI(y, FHI(y) + (k << 20));	/* __HI(y) += (k<<20); */	/* add k to y's exponent */
			return FSUB(y, one);	/* return y-one; */
		}
		t = one;
		if (k < 20) {
			FSETHI(t, 0x3ff00000 - (0x200000 >> k));	/* __HI(t) = 0x3ff00000 - (0x200000>>k); */	/* t=1-2^-k */
			y = FSUB(t, FSUB(e, x));	/* y = t-(e-x); */
			FSETHI(y, FHI(y) + (k << 20));	/* __HI(y) += (k<<20); */	/* add k to y's exponent */
		} else {
			FSETHI(t, ((0x3ff - k) << 20));	/* __HI(t) = ((0x3ff-k)<<20); */	/* 2^-k */
			y = FSUB(x, FADD(e, t));	/* y = x-(e+t); */
			y = FADD(y, one);	/* y += one; */
			FSETHI(y, FHI(y) + (k << 20));	/* __HI(y) += (k<<20); */	/* add k to y's exponent */
		}
	}
	return y;
}
