/* @(#)e_j1.c 1.3 95/01/18 */
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
/* __ieee754_j1(x), __ieee754_y1(x)
 * Bessel function of the first and second kinds of order zero.
 * Method -- j1(x):
 *	1. For tiny x, we use j1(x) = x/2 - x^3/16 + x^5/384 - ...
 *	2. Reduce x to |x| since j1(x)=-j1(-x),  and
 *	   for x in (0,2)
 *		j1(x) = x/2 + x*z*R0/S0,  where z = x*x;
 *	   (precision:  |j1/x - 1/2 - R0/S0 |<2**-61.51 )
 *	   for x in (2,inf)
 * 		j1(x) = sqrt(2/(pi*x))*(p1(x)*cos(x1)-q1(x)*sin(x1))
 * 		y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x1)+q1(x)*cos(x1))
 * 	   where x1 = x-3*pi/4. It is better to compute sin(x1),cos(x1)
 *	   as follow:
 *		cos(x1) =  cos(x)cos(3pi/4)+sin(x)sin(3pi/4)
 *			=  1/sqrt(2) * (sin(x) - cos(x))
 *		sin(x1) =  sin(x)cos(3pi/4)-cos(x)sin(3pi/4)
 *			= -1/sqrt(2) * (sin(x) + cos(x))
 * 	   (To avoid cancellation, use
 *		sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
 * 	    to compute the worse one.)
 *
 *	3 Special cases
 *		j1(nan)= nan
 *		j1(0) = 0
 *		j1(inf) = 0
 *
 * Method -- y1(x):
 *	1. screen out x<=0 cases: y1(0)=-inf, y1(x<0)=NaN
 *	2. For x<2.
 *	   Since
 *		y1(x) = 2/pi*(j1(x)*(ln(x/2)+Euler)-1/x-x/2+5/64*x^3-...)
 *	   therefore y1(x)-2/pi*j1(x)*ln(x)-1/x is an odd function.
 *	   We use the following function to approximate y1,
 *		y1(x) = x*U(z)/V(z) + (2/pi)*(j1(x)*ln(x)-1/x), z= x^2
 *	   where for x in [0,2] (abs err less than 2**-65.89)
 *		U(z) = U0[0] + U0[1]*z + ... + U0[4]*z^4
 *		V(z) = 1  + v0[0]*z + ... + v0[4]*z^5
 *	   Note: For tiny x, 1/x dominate y1 and hence
 *		y1(tiny) = -2/pi/tiny, (choose tiny<2**-54)
 *	3. For x>=2.
 * 		y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x1)+q1(x)*cos(x1))
 * 	   where x1 = x-3*pi/4. It is better to compute sin(x1),cos(x1)
 *	   by method mentioned above.
 */
#include "fdlibm.h"
#include "e_j1.k.h"
static float64_t pone(float64_t);
static float64_t qone(float64_t);
F64K(huge, 1e300)
F64K(one, 1.0)
F64K(invsqrtpi, 5.64189583547756279280e-01)
F64K(tpi, 6.36619772367581382433e-01)
F64K(r00, -6.25000000000000000000e-02)
F64K(r01, 1.40705666955189706048e-03)
F64K(r02, -1.59955631084035597520e-05)
F64K(r03, 4.96727999609584448412e-08)
F64K(s01, 1.91537599538363460805e-02)
F64K(s02, 1.85946785588630915560e-04)
F64K(s03, 1.17718464042623683263e-06)
F64K(s04, 5.04636257076217042715e-09)
F64K(s05, 1.23542274426137913908e-11)
F64K(zero, 0.0)
F64K(k_0p5, 0.5)
F64K(k_p375, .375)

float64_t fd_j1(float64_t x)
{
	float64_t z, s, c, ss, cc, r, u, v, y;
	int32_t hx, ix;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = hx & 0x7fffffff;
	if (ix >= 0x7ff00000) {
		return FDIV(one, x);	/* return one/x; */
	}
	y = fd_fabs(x);
	if (ix >= 0x40000000) {
		/* |x| >= 2.0 */
		s = fd_sin(y);
		c = fd_cos(y);
		ss = FSUB(FNEG(s), c);	/* ss = -s-c; */
		cc = FSUB(s, c);	/* cc = s-c; */
		if (ix < 0x7fe00000) {
			/* make sure y+y not overflow */
			z = fd_cos(FADD(y, y));	/* z = fd_cos(y+y); */
			if (FGT(FMUL(s, c), zero)) {	/* (s*c)>zero */
				cc = FDIV(z, ss);	/* cc = z/ss; */
			} else {
				ss = FDIV(z, cc);	/* ss = z/cc; */
			}
		}
		/*
		 * j1(x) = 1/sqrt(pi) * (P(1,x)*cc - Q(1,x)*ss) / sqrt(x)
		 * y1(x) = 1/sqrt(pi) * (P(1,x)*ss + Q(1,x)*cc) / sqrt(x)
		 */
		if (ix > 0x48000000) {
			z = FDIV(FMUL(invsqrtpi, cc), fd_sqrt(y));	/* z = (invsqrtpi*cc)/fd_sqrt(y); */
		} else {
			u = pone(y);
			v = qone(y);
			z = FDIV(FMUL(invsqrtpi, FSUB(FMUL(u, cc), FMUL(v, ss))), fd_sqrt(y));	/* z = invsqrtpi*(u*cc-v*ss)/fd_sqrt(y); */
		}
		if (hx < 0) {
			return FNEG(z);	/* return -z; */
		} else {
			return z;
		}
	}
	if (ix < 0x3e400000) {
		/* |x|<2**-27 */
		if (FGT(FADD(huge, x), one)) {	/* huge+x>one */
			return FMUL(k_0p5, x);	/* return 0.5*x; */	/* inexact if x!=0 necessary */
		}
	}
	z = FMUL(x, x);	/* z = x*x; */
	r = FMUL(z, FADD(r00, FMUL(z, FADD(r01, FMUL(z, FADD(r02, FMUL(z, r03)))))));	/* r = z*(r00+z*(r01+z*(r02+z*r03))); */
	s = FADD(one, FMUL(z, FADD(s01, FMUL(z, FADD(s02, FMUL(z, FADD(s03, FMUL(z, FADD(s04, FMUL(z, s05))))))))));	/* s = one+z*(s01+z*(s02+z*(s03+z*(s04+z*s05)))); */
	r = FMUL(r, x);	/* r *= x; */
	return FADD(FMUL(x, k_0p5), FDIV(r, s));	/* return(x*0.5+r/s); */
}

F64KA(U0, 5, -1.96057090646238940668e-01, 5.04438716639811282616e-02, -1.91256895875763547298e-03, 2.35252600561610495928e-05, -9.19099158039878874504e-08)
F64KA(V0, 5, 1.99167318236649903973e-02, 2.02552581025135171496e-04, 1.35608801097516229404e-06, 6.22741452364621501295e-09, 1.66559246207992079114e-11)
float64_t fd_y1(float64_t x)
{
	float64_t z, s, c, ss, cc, u, v;
	int32_t hx, ix, lx;
	hx = FHI(x);	/* hx = __HI(x); */
	ix = 0x7fffffff & hx;
	lx = FLO(x);	/* lx = __LO(x); */
	/* if Y1(NaN) is NaN, Y1(-inf) is NaN, Y1(inf) is 0 */
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
		s = fd_sin(x);
		c = fd_cos(x);
		ss = FSUB(FNEG(s), c);	/* ss = -s-c; */
		cc = FSUB(s, c);	/* cc = s-c; */
		if (ix < 0x7fe00000) {
			/* make sure x+x not overflow */
			z = fd_cos(FADD(x, x));	/* z = fd_cos(x+x); */
			if (FGT(FMUL(s, c), zero)) {	/* (s*c)>zero */
				cc = FDIV(z, ss);	/* cc = z/ss; */
			} else {
				ss = FDIV(z, cc);	/* ss = z/cc; */
			}
		}
		/* y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x0)+q1(x)*cos(x0))
		 * where x0 = x-3pi/4
		 *      Better formula:
		 *              cos(x0) = cos(x)cos(3pi/4)+sin(x)sin(3pi/4)
		 *                      =  1/sqrt(2) * (sin(x) - cos(x))
		 *              sin(x0) = sin(x)cos(3pi/4)-cos(x)sin(3pi/4)
		 *                      = -1/sqrt(2) * (cos(x) + sin(x))
		 * To avoid cancellation, use
		 *              sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
		 * to compute the worse one.
		 */
		if (ix > 0x48000000) {
			z = FDIV(FMUL(invsqrtpi, ss), fd_sqrt(x));	/* z = (invsqrtpi*ss)/fd_sqrt(x); */
		} else {
			u = pone(x);
			v = qone(x);
			z = FDIV(FMUL(invsqrtpi, FADD(FMUL(u, ss), FMUL(v, cc))), fd_sqrt(x));	/* z = invsqrtpi*(u*ss+v*cc)/fd_sqrt(x); */
		}
		return z;
	}
	if (ix <= 0x3c900000) {
		/* x < 2**-54 */
		return FDIV(FNEG(tpi), x);	/* return(-tpi/x); */
	}
	z = FMUL(x, x);	/* z = x*x; */
	u = FADD(U0[0], FMUL(z, FADD(U0[1], FMUL(z, FADD(U0[2], FMUL(z, FADD(U0[3], FMUL(z, U0[4]))))))));	/* u = U0[0]+z*(U0[1]+z*(U0[2]+z*(U0[3]+z*U0[4]))); */
	v = FADD(one, FMUL(z, FADD(V0[0], FMUL(z, FADD(V0[1], FMUL(z, FADD(V0[2], FMUL(z, FADD(V0[3], FMUL(z, V0[4]))))))))));	/* v = one+z*(V0[0]+z*(V0[1]+z*(V0[2]+z*(V0[3]+z*V0[4])))); */
	return FADD(FMUL(x, FDIV(u, v)), FMUL(tpi, FSUB(FMUL(fd_j1(x), fd_log(x)), FDIV(one, x))));	/* return(x*(u/v) + tpi*(fd_j1(x)*fd_log(x)-one/x)); */
}

/* For x >= 8, the asymptotic expansions of pone is
 *	1 + 15/128 s^2 - 4725/2^15 s^4 - ...,	where s = 1/x.
 * We approximate pone by
 * 	pone(x) = 1 + (R/S)
 * where  R = pr0 + pr1*s^2 + pr2*s^4 + ... + pr5*s^10
 * 	  S = 1 + ps0*s^2 + ... + ps4*s^10
 * and
 *	| pone(x)-1-R/S | <= 2  ** ( -60.06)
 */
F64KA(pr8, 6, 0.00000000000000000000e+00, 1.17187499999988647970e-01, 1.32394806593073575129e+01, 4.12051854307378562225e+02, 3.87474538913960532227e+03, 7.91447954031891731574e+03)
F64KA(ps8, 5, 1.14207370375678408436e+02, 3.65093083420853463394e+03, 3.69562060269033463555e+04, 9.76027935934950801311e+04, 3.08042720627888811578e+04)
F64KA(pr5, 6, 1.31990519556243522749e-11, 1.17187493190614097638e-01, 6.80275127868432871736e+00, 1.08308182990189109773e+02, 5.17636139533199752805e+02, 5.28715201363337541807e+02)
F64KA(ps5, 5, 5.92805987221131331921e+01, 9.91401418733614377743e+02, 5.35326695291487976647e+03, 7.84469031749551231769e+03, 1.50404688810361062679e+03)
F64KA(pr3, 6, 3.02503916137373618024e-09, 1.17186865567253592491e-01, 3.93297750033315640650e+00, 3.51194035591636932736e+01, 9.10550110750781271918e+01, 4.85590685197364919645e+01)
F64KA(ps3, 5, 3.47913095001251519989e+01, 3.36762458747825746741e+02, 1.04687139975775130551e+03, 8.90811346398256432622e+02, 1.03787932439639277504e+02)
F64KA(pr2, 6, 1.07710830106873743082e-07, 1.17176219462683348094e-01, 2.36851496667608785174e+00, 1.22426109148261232917e+01, 1.76939711271687727390e+01, 5.07352312588818499250e+00)
F64KA(ps2, 5, 2.14364859363821409488e+01, 1.25290227168402751090e+02, 2.32276469057162813669e+02, 1.17679373287147100768e+02, 8.36463893371618283368e+00)
static float64_t pone(float64_t x)
{
	/* for x in [inf, 8]=1/[0,0.125] */
	/* for x in [8,4.5454]=1/[0.125,0.22001] */
	/* for x in [2.8570,2]=1/[0.3499,0.5] */
	const float64_t *p = 0, *q = 0;	/* set below for every x >= 2 */
	float64_t z, r, s;
	int32_t ix;
	ix = 0x7fffffff & FHI(x);	/* ix = 0x7fffffff&__HI(x); */
	if (ix >= 0x40200000) {
		p = pr8;
		q = ps8;
	} else if (ix >= 0x40122E8B) {
		p = pr5;
		q = ps5;
	} else if (ix >= 0x4006DB6D) {
		p = pr3;
		q = ps3;
	} else if (ix >= 0x40000000) {
		p = pr2;
		q = ps2;
	}
	z = FDIV(one, FMUL(x, x));	/* z = one/(x*x); */
	r = FADD(p[0], FMUL(z, FADD(p[1], FMUL(z, FADD(p[2], FMUL(z, FADD(p[3], FMUL(z, FADD(p[4], FMUL(z, p[5]))))))))));	/* r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5])))); */
	s = FADD(one, FMUL(z, FADD(q[0], FMUL(z, FADD(q[1], FMUL(z, FADD(q[2], FMUL(z, FADD(q[3], FMUL(z, q[4]))))))))));	/* s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*q[4])))); */
	return FADD(one, FDIV(r, s));	/* return one+ r/s; */
}

/* For x >= 8, the asymptotic expansions of qone is
 *	3/8 s - 105/1024 s^3 - ..., where s = 1/x.
 * We approximate pone by
 * 	qone(x) = s*(0.375 + (R/S))
 * where  R = qr1*s^2 + qr2*s^4 + ... + qr5*s^10
 * 	  S = 1 + qs1*s^2 + ... + qs6*s^12
 * and
 *	| qone(x)/s -0.375-R/S | <= 2  ** ( -61.13)
 */
F64KA(qr8, 6, 0.00000000000000000000e+00, -1.02539062499992714161e-01, -1.62717534544589987888e+01, -7.59601722513950107896e+02, -1.18498066702429587167e+04, -4.84385124285750353010e+04)
F64KA(qs8, 6, 1.61395369700722909556e+02, 7.82538599923348465381e+03, 1.33875336287249578163e+05, 7.19657723683240939863e+05, 6.66601232617776375264e+05, -2.94490264303834643215e+05)
F64KA(qr5, 6, -2.08979931141764104297e-11, -1.02539050241375426231e-01, -8.05644828123936029840e+00, -1.83669607474888380239e+02, -1.37319376065508163265e+03, -2.61244440453215656817e+03)
F64KA(qs5, 6, 8.12765501384335777857e+01, 1.99179873460485964642e+03, 1.74684851924908907677e+04, 4.98514270910352279316e+04, 2.79480751638918118260e+04, -4.71918354795128470869e+03)
F64KA(qr3, 6, -5.07831226461766561369e-09, -1.02537829820837089745e-01, -4.61011581139473403113e+00, -5.78472216562783643212e+01, -2.28244540737631695038e+02, -2.19210128478909325622e+02)
F64KA(qs3, 6, 4.76651550323729509273e+01, 6.73865112676699709482e+02, 3.38015286679526343505e+03, 5.54772909720722782367e+03, 1.90311919338810798763e+03, -1.35201191444307340817e+02)
F64KA(qr2, 6, -1.78381727510958865572e-07, -1.02517042607985553460e-01, -2.75220568278187460720e+00, -1.96636162643703720221e+01, -4.23253133372830490089e+01, -2.13719211703704061733e+01)
F64KA(qs2, 6, 2.95333629060523854548e+01, 2.52981549982190529136e+02, 7.57502834868645436472e+02, 7.39393205320467245656e+02, 1.55949003336666123687e+02, -4.95949898822628210127e+00)
static float64_t qone(float64_t x)
{
	/* for x in [inf, 8]=1/[0,0.125] */
	/* for x in [8,4.5454]=1/[0.125,0.22001] */
	/* for x in [2.8570,2]=1/[0.3499,0.5] */
	const float64_t *p = 0, *q = 0;	/* set below for every x >= 2 */
	float64_t s, r, z;
	int32_t ix;
	ix = 0x7fffffff & FHI(x);	/* ix = 0x7fffffff&__HI(x); */
	if (ix >= 0x40200000) {
		p = qr8;
		q = qs8;
	} else if (ix >= 0x40122E8B) {
		p = qr5;
		q = qs5;
	} else if (ix >= 0x4006DB6D) {
		p = qr3;
		q = qs3;
	} else if (ix >= 0x40000000) {
		p = qr2;
		q = qs2;
	}
	z = FDIV(one, FMUL(x, x));	/* z = one/(x*x); */
	r = FADD(p[0], FMUL(z, FADD(p[1], FMUL(z, FADD(p[2], FMUL(z, FADD(p[3], FMUL(z, FADD(p[4], FMUL(z, p[5]))))))))));	/* r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5])))); */
	s = FADD(one, FMUL(z, FADD(q[0], FMUL(z, FADD(q[1], FMUL(z, FADD(q[2], FMUL(z, FADD(q[3], FMUL(z, FADD(q[4], FMUL(z, q[5]))))))))))));	/* s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*(q[4]+z*q[5]))))); */
	return FDIV(FADD(k_p375, FDIV(r, s)), x);	/* return (.375 + r/s)/x; */
}
