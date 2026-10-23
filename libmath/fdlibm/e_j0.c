/* @(#)e_j0.c 1.3 95/01/18 */
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
/* __ieee754_j0(x), __ieee754_y0(x)
 * Bessel function of the first and second kinds of order zero.
 * Method -- j0(x):
 *	1. For tiny x, we use j0(x) = 1 - x^2/4 + x^4/64 - ...
 *	2. Reduce x to |x| since j0(x)=j0(-x),  and
 *	   for x in (0,2)
 *		j0(x) = 1-z/4+ z^2*R0/S0,  where z = x*x;
 *	   (precision:  |j0-1+z/4-z^2R0/S0 |<2**-63.67 )
 *	   for x in (2,inf)
 * 		j0(x) = sqrt(2/(pi*x))*(p0(x)*cos(x0)-q0(x)*sin(x0))
 * 	   where x0 = x-pi/4. It is better to compute sin(x0),cos(x0)
 *	   as follow:
 *		cos(x0) = cos(x)cos(pi/4)+sin(x)sin(pi/4)
 *			= 1/sqrt(2) * (cos(x) + sin(x))
 *		sin(x0) = sin(x)cos(pi/4)-cos(x)sin(pi/4)
 *			= 1/sqrt(2) * (sin(x) - cos(x))
 * 	   (To avoid cancellation, use
 *		sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
 * 	    to compute the worse one.)
 *
 *	3 Special cases
 *		j0(nan)= nan
 *		j0(0) = 1
 *		j0(inf) = 0
 *
 * Method -- y0(x):
 *	1. For x<2.
 *	   Since
 *		y0(x) = 2/pi*(j0(x)*(ln(x/2)+Euler) + x^2/4 - ...)
 *	   therefore y0(x)-2/pi*j0(x)*ln(x) is an even function.
 *	   We use the following function to approximate y0,
 *		y0(x) = U(z)/V(z) + (2/pi)*(j0(x)*ln(x)), z= x^2
 *	   where
 *		U(z) = u00 + u01*z + ... + u06*z^6
 *		V(z) = 1  + v01*z + ... + v04*z^4
 *	   with absolute approximation error bounded by 2**-72.
 *	   Note: For tiny x, U/V = u0 and j0(x)~1, hence
 *		y0(tiny) = u0 + (2/pi)*ln(tiny), (choose tiny<2**-27)
 *	2. For x>=2.
 * 		y0(x) = sqrt(2/(pi*x))*(p0(x)*cos(x0)+q0(x)*sin(x0))
 * 	   where x0 = x-pi/4. It is better to compute sin(x0),cos(x0)
 *	   by the method mentioned above.
 *	3. Special cases: y0(0)=-inf, y0(x<0)=NaN, y0(inf)=0.
 */
#include "fdlibm.h"
#include "e_j0.k.h"
static float64_t pzero(float64_t);
static float64_t qzero(float64_t);
F64K(huge, 1e300)
F64K(one, 1.0)
F64K(invsqrtpi, 5.64189583547756279280e-01)
F64K(tpi, 6.36619772367581382433e-01)
F64K(R02, 1.56249999999999947958e-02)
F64K(R03, -1.89979294238854721751e-04)
F64K(R04, 1.82954049532700665670e-06)
F64K(R05, -4.61832688532103189199e-09)
F64K(S01, 1.56191029464890010492e-02)
F64K(S02, 1.16926784663337450260e-04)
F64K(S03, 5.13546550207318111446e-07)
F64K(S04, 1.16614003333790000205e-09)
F64K(zero, 0.0)
F64K(k_0p25, 0.25)
F64K(k_m0p25, -0.25)
F64K(k_0p5, 0.5)
F64K(k_mp125, -.125)

float64_t fd_j0(float64_t x)
{
	float64_t z, s, c, ss, cc, r, u, v;
	int32_t hx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		return FDIV(one, FMUL(x, x));	/* return one/(x*x); */
	}
	x = fd_fabs(x);
	if (ix >= 0x40000000) {
		/* |x| >= 2.0 */
		s = fd_sin(x);
		c = fd_cos(x);
		ss = FSUB(s, c);	/* ss = s-c; */
		cc = FADD(s, c);	/* cc = s+c; */
		if (ix < 0x7fe00000) {
			/* make sure x+x not overflow */
			z = FNEG(fd_cos(FADD(x, x)));	/* z = -fd_cos(x+x); */
			if (FLT(FMUL(s, c), zero)) {	/* (s*c)<zero */
				cc = FDIV(z, ss);	/* cc = z/ss; */
			} else {
				ss = FDIV(z, cc);	/* ss = z/cc; */
			}
		}
		/*
		 * j0(x) = 1/sqrt(pi) * (P(0,x)*cc - Q(0,x)*ss) / sqrt(x)
		 * y0(x) = 1/sqrt(pi) * (P(0,x)*ss + Q(0,x)*cc) / sqrt(x)
		 */
		if (ix > 0x48000000) {
			z = FDIV(FMUL(invsqrtpi, cc), fd_sqrt(x));	/* z = (invsqrtpi*cc)/fd_sqrt(x); */
		} else {
			u = pzero(x);
			v = qzero(x);
			z = FDIV(FMUL(invsqrtpi, FSUB(FMUL(u, cc), FMUL(v, ss))), fd_sqrt(x));	/* z = invsqrtpi*(u*cc-v*ss)/fd_sqrt(x); */
		}
		return z;
	}
	if (ix < 0x3f200000) {
		/* |x| < 2**-13 */
		if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
			/* raise inexact if x != 0 */
			if (ix < 0x3e400000) {
				return one;	/* |x|<2**-27 */
			} else {
				return FSUB(one, FMUL(FMUL(k_0p25, x), x));	/* return one - 0.25*x*x; */
			}
		}
	}
	z = FMUL(x, x);	/* z = x*x; */
	r = FMUL(z, FADD(R02, FMUL(z, FADD(R03, FMUL(z, FADD(R04, FMUL(z, R05)))))));	/* r = z*(R02+z*(R03+z*(R04+z*R05))); */
	s = FADD(one, FMUL(z, FADD(S01, FMUL(z, FADD(S02, FMUL(z, FADD(S03, FMUL(z, S04))))))));	/* s = one+z*(S01+z*(S02+z*(S03+z*S04))); */
	if (ix < 0x3FF00000) {
		/* |x| < 1.00 */
		return FADD(one, FMUL(z, FADD(k_m0p25, FDIV(r, s))));	/* return one + z*(-0.25+(r/s)); */
	} else {
		u = FMUL(k_0p5, x);	/* u = 0.5*x; */
		return FADD(FMUL(FADD(one, u), FSUB(one, u)), FMUL(z, FDIV(r, s)));	/* return((one+u)*(one-u)+z*(r/s)); */
	}
}

F64K(u00, -7.38042951086872317523e-02)
F64K(u01, 1.76666452509181115538e-01)
F64K(u02, -1.38185671945596898896e-02)
F64K(u03, 3.47453432093683650238e-04)
F64K(u04, -3.81407053724364161125e-06)
F64K(u05, 1.95590137035022920206e-08)
F64K(u06, -3.98205194132103398453e-11)
F64K(v01, 1.27304834834123699328e-02)
F64K(v02, 7.60068627350353253702e-05)
F64K(v03, 2.59150851840457805467e-07)
F64K(v04, 4.41110311332675467403e-10)
float64_t fd_y0(float64_t x)
{
	float64_t z, s, c, ss, cc, u, v;
	int32_t hx, ix, lx;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = 0x7fffffff & hx;
	lx = FLO(x);	/* lx = __LO(x); */
	/* Y0(NaN) is NaN, y0(-inf) is Nan, y0(inf) is 0  */
	if (ix >= 0x7ff00000) {
		return FDIV(one, FADD(x, FMUL(x, x)));	/* return one/(x+x*x); */
	}
	if ((ix | lx) == 0) {
		return FDIV(FNEG(one), zero);	/* return -one/zero; */
	}
	if (hx < 0) {
		return FDIV(zero, zero);	/* return zero/zero; */
	}
	if (ix >= 0x40000000) {
		/* |x| >= 2.0 */
		/* y0(x) = sqrt(2/(pi*x))*(p0(x)*sin(x0)+q0(x)*cos(x0))
		 * where x0 = x-pi/4
		 *      Better formula:
		 *              cos(x0) = cos(x)cos(pi/4)+sin(x)sin(pi/4)
		 *                      =  1/sqrt(2) * (sin(x) + cos(x))
		 *              sin(x0) = sin(x)cos(3pi/4)-cos(x)sin(3pi/4)
		 *                      =  1/sqrt(2) * (sin(x) - cos(x))
		 * To avoid cancellation, use
		 *              sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
		 * to compute the worse one.
		 */
		s = fd_sin(x);
		c = fd_cos(x);
		ss = FSUB(s, c);	/* ss = s-c; */
		cc = FADD(s, c);	/* cc = s+c; */
		/*
		 * j0(x) = 1/sqrt(pi) * (P(0,x)*cc - Q(0,x)*ss) / sqrt(x)
		 * y0(x) = 1/sqrt(pi) * (P(0,x)*ss + Q(0,x)*cc) / sqrt(x)
		 */
		if (ix < 0x7fe00000) {
			/* make sure x+x not overflow */
			z = FNEG(fd_cos(FADD(x, x)));	/* z = -fd_cos(x+x); */
			if (FLT(FMUL(s, c), zero)) {	/* (s*c)<zero */
				cc = FDIV(z, ss);	/* cc = z/ss; */
			} else {
				ss = FDIV(z, cc);	/* ss = z/cc; */
			}
		}
		if (ix > 0x48000000) {
			z = FDIV(FMUL(invsqrtpi, ss), fd_sqrt(x));	/* z = (invsqrtpi*ss)/fd_sqrt(x); */
		} else {
			u = pzero(x);
			v = qzero(x);
			z = FDIV(FMUL(invsqrtpi, FADD(FMUL(u, ss), FMUL(v, cc))), fd_sqrt(x));	/* z = invsqrtpi*(u*ss+v*cc)/fd_sqrt(x); */
		}
		return z;
	}
	if (ix <= 0x3e400000) {
		/* x < 2**-27 */
		return FADD(u00, FMUL(tpi, fd_log(x)));	/* return(u00 + tpi*fd_log(x)); */
	}
	z = FMUL(x, x);	/* z = x*x; */
	u = FADD(u00, FMUL(z, FADD(u01, FMUL(z, FADD(u02, FMUL(z, FADD(u03, FMUL(z, FADD(u04, FMUL(z, FADD(u05, FMUL(z, u06))))))))))));	/* u = u00+z*(u01+z*(u02+z*(u03+z*(u04+z*(u05+z*u06))))); */
	v = FADD(one, FMUL(z, FADD(v01, FMUL(z, FADD(v02, FMUL(z, FADD(v03, FMUL(z, v04))))))));	/* v = one+z*(v01+z*(v02+z*(v03+z*v04))); */
	return FADD(FDIV(u, v), FMUL(tpi, FMUL(fd_j0(x), fd_log(x))));	/* return(u/v + tpi*(fd_j0(x)*fd_log(x))); */
}

/* The asymptotic expansions of pzero is
 *	1 - 9/128 s^2 + 11025/98304 s^4 - ...,	where s = 1/x.
 * For x >= 2, We approximate pzero by
 * 	pzero(x) = 1 + (R/S)
 * where  R = pR0 + pR1*s^2 + pR2*s^4 + ... + pR5*s^10
 * 	  S = 1 + pS0*s^2 + ... + pS4*s^10
 * and
 *	| pzero(x)-1-R/S | <= 2  ** ( -60.26)
 */
F64KA(pR8, 6, 0.00000000000000000000e+00, -7.03124999999900357484e-02, -8.08167041275349795626e+00, -2.57063105679704847262e+02, -2.48521641009428822144e+03, -5.25304380490729545272e+03)
F64KA(pS8, 5, 1.16534364619668181717e+02, 3.83374475364121826715e+03, 4.05978572648472545552e+04, 1.16752972564375915681e+05, 4.76277284146730962675e+04)
F64KA(pR5, 6, -1.14125464691894502584e-11, -7.03124940873599280078e-02, -4.15961064470587782438e+00, -6.76747652265167261021e+01, -3.31231299649172967747e+02, -3.46433388365604912451e+02)
F64KA(pS5, 5, 6.07539382692300335975e+01, 1.05125230595704579173e+03, 5.97897094333855784498e+03, 9.62544514357774460223e+03, 2.40605815922939109441e+03)
F64KA(pR3, 6, -2.54704601771951915620e-09, -7.03119616381481654654e-02, -2.40903221549529611423e+00, -2.19659774734883086467e+01, -5.80791704701737572236e+01, -3.14479470594888503854e+01)
F64KA(pS3, 5, 3.58560338055209726349e+01, 3.61513983050303863820e+02, 1.19360783792111533330e+03, 1.12799679856907414432e+03, 1.73580930813335754692e+02)
F64KA(pR2, 6, -8.87534333032526411254e-08, -7.03030995483624743247e-02, -1.45073846780952986357e+00, -7.63569613823527770791e+00, -1.11931668860356747786e+01, -3.23364579351335335033e+00)
F64KA(pS2, 5, 2.22202997532088808441e+01, 1.36206794218215208048e+02, 2.70470278658083486789e+02, 1.53875394208320329881e+02, 1.46576176948256193810e+01)
static float64_t pzero(float64_t x)
{
	/* for x in [inf, 8]=1/[0,0.125] */
	/* for x in [8,4.5454]=1/[0.125,0.22001] */
	/* for x in [4.547,2.8571]=1/[0.2199,0.35001] */
	/* for x in [2.8570,2]=1/[0.3499,0.5] */
	const float64_t *p = 0, *q = 0;	/* set below for every x >= 2 */
	float64_t z, r, s;
	int32_t ix;
	ix = 0x7fffffff & FHI(x);	/* ix = 0x7fffffff&__HI(x); */
	if (ix >= 0x40200000) {
		p = pR8;
		q = pS8;
	} else if (ix >= 0x40122E8B) {
		p = pR5;
		q = pS5;
	} else if (ix >= 0x4006DB6D) {
		p = pR3;
		q = pS3;
	} else if (ix >= 0x40000000) {
		p = pR2;
		q = pS2;
	}
	z = FDIV(one, FMUL(x, x));	/* z = one/(x*x); */
	r = FADD(p[0], FMUL(z, FADD(p[1], FMUL(z, FADD(p[2], FMUL(z, FADD(p[3], FMUL(z, FADD(p[4], FMUL(z, p[5]))))))))));	/* r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5])))); */
	s = FADD(one, FMUL(z, FADD(q[0], FMUL(z, FADD(q[1], FMUL(z, FADD(q[2], FMUL(z, FADD(q[3], FMUL(z, q[4]))))))))));	/* s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*q[4])))); */
	return FADD(one, FDIV(r, s));	/* return one+ r/s; */
}

/* For x >= 8, the asymptotic expansions of qzero is
 *	-1/8 s + 75/1024 s^3 - ..., where s = 1/x.
 * We approximate pzero by
 * 	qzero(x) = s*(-1.25 + (R/S))
 * where  R = qR0 + qR1*s^2 + qR2*s^4 + ... + qR5*s^10
 * 	  S = 1 + qS0*s^2 + ... + qS5*s^12
 * and
 *	| qzero(x)/s +1.25-R/S | <= 2  ** ( -61.22)
 */
F64KA(qR8, 6, 0.00000000000000000000e+00, 7.32421874999935051953e-02, 1.17682064682252693899e+01, 5.57673380256401856059e+02, 8.85919720756468632317e+03, 3.70146267776887834771e+04)
F64KA(qS8, 6, 1.63776026895689824414e+02, 8.09834494656449805916e+03, 1.42538291419120476348e+05, 8.03309257119514397345e+05, 8.40501579819060512818e+05, -3.43899293537866615225e+05)
F64KA(qR5, 6, 1.84085963594515531381e-11, 7.32421766612684765896e-02, 5.83563508962056953777e+00, 1.35111577286449829671e+02, 1.02724376596164097464e+03, 1.98997785864605384631e+03)
F64KA(qS5, 6, 8.27766102236537761883e+01, 2.07781416421392987104e+03, 1.88472887785718085070e+04, 5.67511122894947329769e+04, 3.59767538425114471465e+04, -5.35434275601944773371e+03)
F64KA(qR3, 6, 4.37741014089738620906e-09, 7.32411180042911447163e-02, 3.34423137516170720929e+00, 4.26218440745412650017e+01, 1.70808091340565596283e+02, 1.66733948696651168575e+02)
F64KA(qS3, 6, 4.87588729724587182091e+01, 7.09689221056606015736e+02, 3.70414822620111362994e+03, 6.46042516752568917582e+03, 2.51633368920368957333e+03, -1.49247451836156386662e+02)
F64KA(qR2, 6, 1.50444444886983272379e-07, 7.32234265963079278272e-02, 1.99819174093815998816e+00, 1.44956029347885735348e+01, 3.16662317504781540833e+01, 1.62527075710929267416e+01)
F64KA(qS2, 6, 3.03655848355219184498e+01, 2.69348118608049844624e+02, 8.44783757595320139444e+02, 8.82935845112488550512e+02, 2.12666388511798828631e+02, -5.31095493882666946917e+00)
static float64_t qzero(float64_t x)
{
	/* for x in [inf, 8]=1/[0,0.125] */
	/* for x in [8,4.5454]=1/[0.125,0.22001] */
	/* for x in [4.547,2.8571]=1/[0.2199,0.35001] */
	/* for x in [2.8570,2]=1/[0.3499,0.5] */
	const float64_t *p = 0, *q = 0;	/* set below for every x >= 2 */
	float64_t s, r, z;
	int32_t ix;
	ix = 0x7fffffff & FHI(x);	/* ix = 0x7fffffff&__HI(x); */
	if (ix >= 0x40200000) {
		p = qR8;
		q = qS8;
	} else if (ix >= 0x40122E8B) {
		p = qR5;
		q = qS5;
	} else if (ix >= 0x4006DB6D) {
		p = qR3;
		q = qS3;
	} else if (ix >= 0x40000000) {
		p = qR2;
		q = qS2;
	}
	z = FDIV(one, FMUL(x, x));	/* z = one/(x*x); */
	r = FADD(p[0], FMUL(z, FADD(p[1], FMUL(z, FADD(p[2], FMUL(z, FADD(p[3], FMUL(z, FADD(p[4], FMUL(z, p[5]))))))))));	/* r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5])))); */
	s = FADD(one, FMUL(z, FADD(q[0], FMUL(z, FADD(q[1], FMUL(z, FADD(q[2], FMUL(z, FADD(q[3], FMUL(z, FADD(q[4], FMUL(z, q[5]))))))))))));	/* s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*(q[4]+z*q[5]))))); */
	return FDIV(FADD(k_mp125, FDIV(r, s)), x);	/* return (-.125 + r/s)/x; */
}
