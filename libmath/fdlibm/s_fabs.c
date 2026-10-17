/* @(#)s_fabs.c 1.3 95/01/18 */
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
 * fabs(x) returns the absolute value of x.
 */
#include "fdlibm.h"
#include "s_fabs.k.h"
float64_t fd_fabs(float64_t x)
{
	FSETHI(x, FHI(x) & 0x7fffffff);	/* __HI(x) &= 0x7fffffff; */
	return x;
}
