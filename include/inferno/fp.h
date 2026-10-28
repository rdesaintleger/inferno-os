/*
 * inferno/fp.h
 *
 * Floating-point API of Inferno.
 *
 * Arithmetic on IEEE 754 binary64 numbers (type float64_t, a structure that
 * holds the 64 bits of the number), implemented with SoftFloat.  Nothing here
 * depends on the floating-point unit, the endianness or the C library of the
 * host: the same operands give the same bits everywhere.
 *
 * The state of the computation (rounding mode, exception flags) is kept by
 * SoftFloat in global variables, see <inferno/softfloat.h>.  The functions
 * only raise flags; they never trap.  Traps are the business of the caller.
 *
 * Comparisons follow C99 annex F: inferno_fp_eq and inferno_fp_ne are quiet,
 * the ordered comparisons (lt, le, gt, ge) raise the invalid exception when an
 * operand is a NaN.
 */
#ifndef _INFERNO_FP_H_
#define _INFERNO_FP_H_

#include <stdint.h>
#include <inferno/softfloat.h>

/*
 * Floating-point state of a process (see libmath/fp/softfpu.c): the control
 * word (rounding mode and traps) and the accumulated status flags, in the
 * Inferno encoding of mathi.h (INVAL, ZDIV, OVFL, UNFL, INEX, RND_*).
 */
typedef struct InfernoFPU InfernoFPU;
struct InfernoFPU
{
	uint32_t	control;
	uint32_t	status;
};

/* arithmetic: the result is rounded as the current rounding mode says */
#define inferno_fp_add(a, b)		f64_add((a), (b))
#define inferno_fp_sub(a, b)		f64_sub((a), (b))
#define inferno_fp_mul(a, b)		f64_mul((a), (b))
#define inferno_fp_div(a, b)		f64_div((a), (b))
#define inferno_fp_sqrt(a)		f64_sqrt((a))
#define inferno_fp_rem(a, b)		f64_rem((a), (b))

/* sign: exact, no flag raised, a NaN keeps its payload */
extern float64_t inferno_fp_neg(float64_t);
extern float64_t inferno_fp_abs(float64_t);

/* comparisons: the result is 0 or 1 */
#define inferno_fp_eq(a, b)		f64_eq((a), (b))
#define inferno_fp_ne(a, b)		(!f64_eq((a), (b)))
#define inferno_fp_lt(a, b)		f64_lt((a), (b))
#define inferno_fp_le(a, b)		f64_le((a), (b))
#define inferno_fp_gt(a, b)		f64_lt((b), (a))
#define inferno_fp_ge(a, b)		f64_le((b), (a))

/* x is not zero (a NaN is not zero); same as C `if(x)'; exact, no flag */
#define inferno_fp_nz(x)		((((x).v) << 1) != 0)

/* conversions with integers; to_i32 truncates toward zero */
#define inferno_fp_from_i32(n)		i32_to_f64((n))
#define inferno_fp_from_u32(n)		ui32_to_f64((n))
#define inferno_fp_to_i32(x)		((int32_t)f64_to_i32_r_minMag((x), false))
#define inferno_fp_from_i64(n)		i64_to_f64((n))
#define inferno_fp_to_i64(x)		((int64_t)f64_to_i64_r_minMag((x), false))
#define inferno_fp_from_f32(x)		f32_to_f64((x))
#define inferno_fp_to_f32(x)		f64_to_f32((x))

/* the bits of a number */
#define inferno_fp_bits(x)		((x).v)
extern float64_t inferno_fp_from_bits(uint64_t);

/*
 * Transitional: conversion with the host double, only for the parsing and
 * the formatting of numbers (dtoa) that still run on the host.
 */
extern double inferno_fp_to_host(float64_t);
extern float64_t inferno_fp_from_host(double);

/*
 * The two 32-bit halves of a number: hi holds the sign, the exponent and the
 * 20 high bits of the fraction; lo the 32 low bits of the fraction.
 * with_hi and with_lo return a copy of x with one half replaced.
 */
#define inferno_fp_hi(x)		((int32_t)(uint32_t)((x).v >> 32))
#define inferno_fp_lo(x)		((int32_t)(uint32_t)((x).v & 0xFFFFFFFFu))
extern float64_t inferno_fp_with_hi(float64_t x, int32_t hi);
extern float64_t inferno_fp_with_lo(float64_t x, int32_t lo);

/*
 * Elementary functions of fdlibm (libmath/fdlibm), computed with SoftFloat.
 *
 * They only accumulate the exception flags of SoftFloat: they do not trap and
 * do not choose the rounding mode.  They are written for round to nearest:
 * whoever calls them sets softfloat_roundingMode to softfloat_round_near_even,
 * clears softfloat_exceptionFlags before the call and reads it afterwards.
 */
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

/*
 * Elementary functions of fdlibm (libmath/fdlibm), computed with SoftFloat.
 *
 * They only accumulate the exception flags of SoftFloat: they do not trap and
 * do not choose the rounding mode.  They are written for round to nearest:
 * whoever calls them sets softfloat_roundingMode to softfloat_round_near_even,
 * clears softfloat_exceptionFlags before the call and reads it afterwards.
 */
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

#endif
