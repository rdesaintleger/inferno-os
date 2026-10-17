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

/* the bits of a number */
#define inferno_fp_bits(x)		((x).v)
extern float64_t inferno_fp_from_bits(uint64_t);

/*
 * The two 32-bit halves of a number: hi holds the sign, the exponent and the
 * 20 high bits of the fraction; lo the 32 low bits of the fraction.
 * with_hi and with_lo return a copy of x with one half replaced.
 */
#define inferno_fp_hi(x)		((int32_t)(uint32_t)((x).v >> 32))
#define inferno_fp_lo(x)		((int32_t)(uint32_t)((x).v & 0xFFFFFFFFu))
extern float64_t inferno_fp_with_hi(float64_t x, int32_t hi);
extern float64_t inferno_fp_with_lo(float64_t x, int32_t lo);

#endif
