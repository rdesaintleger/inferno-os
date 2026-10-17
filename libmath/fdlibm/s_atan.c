/* @(#)s_atan.c 1.3 95/01/18 */
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
/* atan(x)
 * Method
 *   1. Reduce x to positive by atan(x) = -atan(-x).
 *   2. According to the integer k=4t+0.25 chopped, t=x, the argument
 *      is further reduced to one of the following intervals and the
 *      arctangent of t is evaluated by the corresponding formula:
 *
 *      [0,7/16]      atan(x) = t-t^3*(a1+t^2*(a2+...(a10+t^2*a11)...)
 *      [7/16,11/16]  atan(x) = atan(1/2) + atan( (t-0.5)/(1+t/2) )
 *      [11/16.19/16] atan(x) = atan( 1 ) + atan( (t-1)/(1+t) )
 *      [19/16,39/16] atan(x) = atan(3/2) + atan( (t-1.5)/(1+1.5t) )
 *      [39/16,INF]   atan(x) = atan(INF) + atan( -1/t )
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
#include "fdlibm.h"
#include "s_atan.k.h"
F64KA(atanhi, 4, 4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01, 1.57079632679489655800e+00)
F64KA(atanlo, 4, 2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17, 6.12323399573676603587e-17)
F64KA(aT, 11, 3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02)
F64K(one, 1.0)
F64K(huge, 1.0e300)
F64K(k_2p0, 2.0)
F64K(k_1p5, 1.5)
F64K(k_m1p0, -1.0)

float64_t fd_atan(float64_t x)
{
	/* atan(0.5)hi 0x3FDDAC67, 0x0561BB4F */
	/* atan(1.0)hi 0x3FE921FB, 0x54442D18 */
	/* atan(1.5)hi 0x3FEF730B, 0xD281F69B */
	/* atan(inf)hi 0x3FF921FB, 0x54442D18 */
	/* atan(0.5)lo 0x3C7A2B7F, 0x222F65E2 */
	/* atan(1.0)lo 0x3C81A626, 0x33145C07 */
	/* atan(1.5)lo 0x3C700788, 0x7AF0CBBD */
	/* atan(inf)lo 0x3C91A626, 0x33145C07 */
	float64_t w, s1, s2, z;
	int32_t ix, hx, id;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x44100000) {
		/* if |x| >= 2^66 */
		if (ix > 0x7ff00000 || (ix == 0x7ff00000 && (FLO(x) != 0))) {	/* ix>0x7ff00000|| (ix==0x7ff00000&&(__LO(x)!=0)) */
			return FADD(x, x);	/* return x+x; */	/* NaN */
		}
		if (hx > 0) {
			return FADD(atanhi[3], atanlo[3]);	/* return atanhi[3]+atanlo[3]; */
		} else {
			return FSUB(FNEG(atanhi[3]), atanlo[3]);	/* return -atanhi[3]-atanlo[3]; */
		}
	}
	if (ix < 0x3fdc0000) {
		/* |x| < 0.4375 */
		if (ix < 0x3e200000) {
			/* |x| < 2^-29 */
			if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
				return x;	/* raise inexact */
			}
		}
		id = -1;
	} else {
		x = fd_fabs(x);
		if (ix < 0x3ff30000) {
			/* |x| < 1.1875 */
			if (ix < 0x3fe60000) {
				/* 7/16 <=|x|<11/16 */
				id = 0;
				x = FDIV(FSUB(FMUL(k_2p0, x), one), FADD(k_2p0, x));	/* x = (2.0*x-one)/(2.0+x); */
			} else {
				/* 11/16<=|x|< 19/16 */
				id = 1;
				x = FDIV(FSUB(x, one), FADD(x, one));	/* x = (x-one)/(x+one); */
			}
		} else {
			if (ix < 0x40038000) {
				/* |x| < 2.4375 */
				id = 2;
				x = FDIV(FSUB(x, k_1p5), FADD(one, FMUL(k_1p5, x)));	/* x = (x-1.5)/(one+1.5*x); */
			} else {
				/* 2.4375 <= |x| < 2^66 */
				id = 3;
				x = FDIV(k_m1p0, x);	/* x = -1.0/x; */
			}
		}
	}
	/* end of argument reduction */
	z = FMUL(x, x);	/* z = x*x; */
	w = FMUL(z, z);	/* w = z*z; */
	/* break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly */
	s1 = FMUL(z, FADD(aT[0], FMUL(w, FADD(aT[2], FMUL(w, FADD(aT[4], FMUL(w, FADD(aT[6], FMUL(w, FADD(aT[8], FMUL(w, aT[10])))))))))));	/* s1 = z*(aT[0]+w*(aT[2]+w*(aT[4]+w*(aT[6]+w*(aT[8]+w*aT[10]))))); */
	s2 = FMUL(w, FADD(aT[1], FMUL(w, FADD(aT[3], FMUL(w, FADD(aT[5], FMUL(w, FADD(aT[7], FMUL(w, aT[9])))))))));	/* s2 = w*(aT[1]+w*(aT[3]+w*(aT[5]+w*(aT[7]+w*aT[9])))); */
	if (id < 0) {
		return FSUB(x, FMUL(x, FADD(s1, s2)));	/* return x - x*(s1+s2); */
	} else {
		z = FSUB(atanhi[id], FSUB(FSUB(FMUL(x, FADD(s1, s2)), atanlo[id]), x));	/* z = atanhi[id] - ((x*(s1+s2) - atanlo[id]) - x); */
		return (hx < 0) ? FNEG(z) : z;	/* return (hx<0)? -z:z; */
	}
}
