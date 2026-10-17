/*
 * fdlibm.h - header of fdlibm, ported to SoftFloat.
 *
 * fdlibm computes with the type float64_t of <inferno/fp.h> instead of the
 * `double' of the host.  The code is the algorithm of fdlibm 5.3, with the
 * operators of C replaced by the operations of SoftFloat, which round exactly
 * as the operators did on a machine with strict IEEE 754 binary64 arithmetic.
 *
 * The names below are short forms of the operations of <inferno/fp.h> and are
 * only used by fdlibm.  The original expression is kept in a comment next to
 * each statement that was rewritten.
 *
 * Constants are declared in the source with the markers F64K and F64KA and
 * defined in a header generated at build time by utils/fdconst.
 *
 */
#ifndef _FDLIBM_H_
#define _FDLIBM_H_

#include <stdint.h>
#include <inferno/fp.h>

#define FADD(a, b)	inferno_fp_add((a), (b))
#define FSUB(a, b)	inferno_fp_sub((a), (b))
#define FMUL(a, b)	inferno_fp_mul((a), (b))
#define FDIV(a, b)	inferno_fp_div((a), (b))
#define FNEG(a)		inferno_fp_neg((a))

#define FEQ(a, b)	inferno_fp_eq((a), (b))
#define FNE(a, b)	inferno_fp_ne((a), (b))
#define FLT(a, b)	inferno_fp_lt((a), (b))
#define FLE(a, b)	inferno_fp_le((a), (b))
#define FGT(a, b)	inferno_fp_gt((a), (b))
#define FGE(a, b)	inferno_fp_ge((a), (b))
#define FNZ(a)		inferno_fp_nz((a))	/* as C `if(a)' */

#define FI(n)		inferno_fp_from_i32((n))	/* (double)n */
#define FU(n)		inferno_fp_from_u32((n))	/* (double)n, n unsigned */
#define FTOI(a)		inferno_fp_to_i32((a))		/* (int)a */

#define FHI(a)		inferno_fp_hi((a))		/* high word, int */
#define FLO(a)		inferno_fp_lo((a))		/* low word, int */
#define FSETHI(a, v)	((a) = inferno_fp_with_hi((a), (v)))
#define FSETLO(a, v)	((a) = inferno_fp_with_lo((a), (v)))

/* ieee style elementary functions */
extern float64_t fd_acos(float64_t);
extern float64_t fd_acosh(float64_t);
extern float64_t fd_asin(float64_t);
extern float64_t fd_atan2(float64_t, float64_t);
extern float64_t fd_atanh(float64_t);
extern float64_t fd_cosh(float64_t);
extern float64_t fd_exp(float64_t);
extern float64_t fd_fmod(float64_t, float64_t);
extern float64_t fd_hypot(float64_t, float64_t);
extern float64_t fd_j0(float64_t);
extern float64_t fd_j1(float64_t);
extern float64_t fd_jn(int32_t, float64_t);
extern float64_t fd_lgamma_r(float64_t, int32_t *);
extern float64_t fd_log(float64_t);
extern float64_t fd_log10(float64_t);
extern float64_t fd_pow(float64_t, float64_t);
extern float64_t fd_remainder(float64_t, float64_t);
extern float64_t fd_sinh(float64_t);
extern float64_t fd_sqrt(float64_t);
extern float64_t fd_y0(float64_t);
extern float64_t fd_y1(float64_t);
extern float64_t fd_yn(int32_t, float64_t);
extern int32_t fd_rem_pio2(float64_t, float64_t *);

/* ANSI/POSIX and BSD functions */
extern float64_t fd_asinh(float64_t);
extern float64_t fd_atan(float64_t);
extern float64_t fd_cbrt(float64_t);
extern float64_t fd_ceil(float64_t);
extern float64_t fd_cos(float64_t);
extern float64_t fd_erf(float64_t);
extern float64_t fd_erfc(float64_t);
extern float64_t fd_expm1(float64_t);
extern float64_t fd_fabs(float64_t);
extern float64_t fd_floor(float64_t);
extern float64_t fd_log1p(float64_t);
extern float64_t fd_modf(float64_t, float64_t *);
extern float64_t fd_sin(float64_t);
extern float64_t fd_tan(float64_t);
extern float64_t fd_tanh(float64_t);

/* functions callable from C, intended to support IEEE arithmetic */
extern float64_t fd_copysign(float64_t, float64_t);
extern int32_t fd_finite(float64_t);
extern int32_t fd_ilogb(float64_t);
extern float64_t fd_nextafter(float64_t, float64_t);
extern float64_t fd_rint(float64_t);
extern float64_t fd_scalbn(float64_t, int32_t);

/* fdlibm kernel functions */
extern float64_t fd_kernel_sin(float64_t, float64_t, int32_t);
extern float64_t fd_kernel_cos(float64_t, float64_t);
extern float64_t fd_kernel_tan(float64_t, float64_t, int32_t);
extern int32_t fd_kernel_rem_pio2(float64_t *, float64_t *, int32_t, int32_t, int32_t, const int32_t *);

#endif
