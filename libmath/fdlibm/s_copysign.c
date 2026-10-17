/* @(#)s_copysign.c 1.3 95/01/18 */
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
 * copysign(double x, double y)
 * copysign(x,y) returns a value with the magnitude of x and
 * with the sign bit of y.
 */
#include "fdlibm.h"
#include "s_copysign.k.h"
float64_t fd_copysign(float64_t x, float64_t y)
{
	FSETHI(x, (FHI(x) & 0x7fffffff) | (FHI(y) & 0x80000000));	/* __HI(x) = (__HI(x)&0x7fffffff)|(__HI(y)&0x80000000); */
	return x;
}
