
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
extern double fd_acos __P((double));
extern double fd_acosh __P((double));
extern double fd_asin __P((double));
extern double fd_atan2 __P((double,double));
extern double fd_atanh __P((double));
extern double fd_cosh __P((double));
extern double fd_exp __P((double));
extern double fd_fmod __P((double,double));
extern double fd_hypot __P((double,double));
extern double fd_j0 __P((double));
extern double fd_j1 __P((double));
extern double fd_jn __P((int,double));
extern double fd_lgamma_r __P((double,int *));
extern double fd_log __P((double));
extern double fd_log10 __P((double));
extern double fd_pow __P((double,double));
extern double fd_remainder __P((double,double));
extern double fd_sinh __P((double));
extern double fd_sqrt __P((double));
extern double fd_y0 __P((double));
extern double fd_y1 __P((double));
extern double fd_yn __P((int,double));
extern int    fd_rem_pio2 __P((double,double*));

/* ANSI/POSIX and BSD functions */
extern double fd_asinh __P((double));
extern double fd_atan __P((double));
extern double fd_cbrt __P((double));
extern double fd_ceil __P((double));
extern double fd_cos __P((double));
extern double fd_erf __P((double));
extern double fd_erfc __P((double));
extern double fd_expm1 __P((double));
extern double fd_fabs __P((double));
extern double fd_floor __P((double));
extern double fd_log1p __P((double));
extern double fd_modf __P((double, double *));
extern double fd_sin __P((double));
extern double fd_tan __P((double));
extern double fd_tanh __P((double));

/* functions callable from C, intended to support IEEE arithmetic */
extern double fd_copysign __P((double, double));
extern int    fd_finite __P((double));
extern int    fd_ilogb __P((double));
extern double fd_nextafter __P((double, double));
extern double fd_rint __P((double));
extern double fd_scalbn __P((double, int));

/* fdlibm kernel functions */
extern double fd_kernel_sin __P((double,double,int));
extern double fd_kernel_cos __P((double,double));
extern double fd_kernel_tan __P((double,double,int));
extern int    fd_kernel_rem_pio2 __P((double*,double*,int,int,int,const int*));
