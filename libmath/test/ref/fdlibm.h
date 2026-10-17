
/* @(#)fdlibm.h 1.5 04/04/22 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* Sometimes it's necessary to define __LITTLE_ENDIAN explicitly
   but these catch some common cases. */

#if defined(i386) || defined(i486) || \
	defined(intel) || defined(x86) || defined(i86pc) || \
	defined(__alpha) || defined(__osf__)
#define __LITTLE_ENDIAN
#endif

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#define __HIp(x) *(1+(int*)x)
#define __LOp(x) *(int*)x
#else
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1+(int*)x)
#endif

#ifdef __STDC__
#define	__P(p)	p
#else
#define	__P(p)	()
#endif

/*
 * Elementary functions used by Inferno.  All names carry the fd_ prefix so
 * that they cannot clash with the host's libm; the SVID/X-Open error
 * handling (wrappers, matherr, _LIB_VERSION) is not part of this copy:
 * errors are reported through IEEE exceptions only.
 */

/* ieee style elementary functions */
extern double ref_acos __P((double));
extern double ref_acosh __P((double));
extern double ref_asin __P((double));
extern double ref_atan2 __P((double,double));
extern double ref_atanh __P((double));
extern double ref_cosh __P((double));
extern double ref_exp __P((double));
extern double ref_fmod __P((double,double));
extern double ref_hypot __P((double,double));
extern double ref_j0 __P((double));
extern double ref_j1 __P((double));
extern double ref_jn __P((int,double));
extern double ref_lgamma_r __P((double,int *));
extern double ref_log __P((double));
extern double ref_log10 __P((double));
extern double ref_pow __P((double,double));
extern double ref_remainder __P((double,double));
extern double ref_sinh __P((double));
extern double ref_sqrt __P((double));
extern double ref_y0 __P((double));
extern double ref_y1 __P((double));
extern double ref_yn __P((int,double));
extern int    ref_rem_pio2 __P((double,double*));

/* ANSI/POSIX and BSD functions */
extern double ref_asinh __P((double));
extern double ref_atan __P((double));
extern double ref_cbrt __P((double));
extern double ref_ceil __P((double));
extern double ref_cos __P((double));
extern double ref_erf __P((double));
extern double ref_erfc __P((double));
extern double ref_expm1 __P((double));
extern double ref_fabs __P((double));
extern double ref_floor __P((double));
extern double ref_log1p __P((double));
extern double ref_modf __P((double, double *));
extern double ref_sin __P((double));
extern double ref_tan __P((double));
extern double ref_tanh __P((double));

/* functions callable from C, intended to support IEEE arithmetic */
extern double ref_copysign __P((double, double));
extern int    ref_finite __P((double));
extern int    ref_ilogb __P((double));
extern double ref_nextafter __P((double, double));
extern double ref_rint __P((double));
extern double ref_scalbn __P((double, int));

/* fdlibm kernel functions */
extern double ref_kernel_sin __P((double,double,int));
extern double ref_kernel_cos __P((double,double));
extern double ref_kernel_tan __P((double,double,int));
extern int    ref_kernel_rem_pio2 __P((double*,double*,int,int,int,const int*));
