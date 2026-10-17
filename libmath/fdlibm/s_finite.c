/* @(#)s_finite.c 1.3 95/01/18 */
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
 * finite(x) returns 1 is x is finite, else 0;
 * no branching!
 */
#include "fdlibm.h"
#include "s_finite.k.h"
int32_t fd_finite(float64_t x)
{
	int32_t hx;
	hx = FHI(x);	/* hx = __HI(x); */
	return (int32_t)((uint32_t)((hx & 0x7fffffff) - 0x7ff00000) >> 31);	/* return (unsigned)((hx&0x7fffffff)-0x7ff00000)>>31; */
}
