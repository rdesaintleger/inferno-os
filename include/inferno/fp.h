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
 * The state of the computation (rounding mode, exception flags) is the SoftFPU
 * of the running process, see below.  The arithmetic only raises flags; it
 * never traps.  Traps are the business of the caller, who looks at the flags
 * with inferno_fp_pending and inferno_fp_flags.
 *
 * This header is the whole interface of libmath: the VM, limbo and the
 * libraries use nothing else of floating point, neither SoftFloat directly nor
 * the float64_t representation.  An implementation of libmath that keeps these
 * names, with the same behaviour, can replace the one of this directory (see
 * libmath/README).
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
 * The SoftFPU: the floating-point unit of the Dis virtual machine is a
 * register, not an instruction set (libmath/fp/softfpu.c).  It holds the
 * control word (rounding mode, and the exceptions that trap) and the status
 * word (the exceptions seen since it was last cleared) of the running
 * process.  The VM switches it with inferno_fp_save and inferno_fp_restore.
 */
typedef struct SoftFPU SoftFPU;
struct SoftFPU
{
	uint32_t	control;
	uint32_t	status;
};

/* bits of the control word (trap enable) and of the status word (flags) */
enum
{
	INFERNO_FP_INVAL	= 0x0001,
	INFERNO_FP_ZDIV		= 0x0002,
	INFERNO_FP_OVFL		= 0x0004,
	INFERNO_FP_UNFL		= 0x0008,
	INFERNO_FP_INEX		= 0x0010,

	/* rounding mode, in the control word */
	INFERNO_FP_RND_NR	= 0x0000,	/* to nearest, ties to even */
	INFERNO_FP_RND_NINF	= 0x0100,	/* toward -infinity */
	INFERNO_FP_RND_PINF	= 0x0200,	/* toward +infinity */
	INFERNO_FP_RND_Z	= 0x0300,	/* toward zero */
	INFERNO_FP_RND_MASK	= 0x0300
};

extern void inferno_fp_init(void);		/* the state of a new process: INVAL, ZDIV, OVFL, UNFL trap, round to nearest */
extern void inferno_fp_save(SoftFPU*);		/* the state of the running process to a SoftFPU... */
extern void inferno_fp_restore(SoftFPU*);	/* ...and back */
extern uint32_t inferno_fp_get_control(void);
extern uint32_t inferno_fp_control(uint32_t, uint32_t);	/* (new bits, mask): returns the old bits under the mask */
extern uint32_t inferno_fp_get_status(void);
extern uint32_t inferno_fp_status(uint32_t, uint32_t);	/* same, for the status word */

/*
 * The arithmetic raises flags; they are collected here.  inferno_fp_pending
 * is true if an operation has raised one since the last inferno_fp_flags (or
 * inferno_fp_discard).  inferno_fp_flags adds them to the status word, clears
 * them and returns the exceptions that trap, those that are enabled in the
 * control word: if it is not zero, the caller raises the run-time error.
 */
#define inferno_fp_pending()		(softfloat_exceptionFlags != 0)
#define inferno_fp_discard()		(softfloat_exceptionFlags = 0)
extern uint32_t inferno_fp_flags(void);

/*
 * The elementary functions, the conversions to text, ... are computed in
 * round to nearest, whatever the mode of the process is:
 * inferno_fp_round_nearest sets it and returns the mode it replaces,
 * inferno_fp_round_restore gives it back.
 */
extern int inferno_fp_round_nearest(void);
extern void inferno_fp_round_restore(int);

/* a constant: the initializer of a float64_t of these bits */
#define INFERNO_FP_CONST(bits)		{ (bits) }

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
#define inferno_fp_lt_quiet(a, b)	f64_lt_quiet((a), (b))	/* no flag for a quiet NaN */
#define inferno_fp_le(a, b)		f64_le((a), (b))
#define inferno_fp_gt(a, b)		f64_lt((b), (a))
#define inferno_fp_ge(a, b)		f64_le((b), (a))

/* x is a NaN; exact, no flag */
#define inferno_fp_isnan(x)		((((x).v) & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) \
					&& (((x).v) & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)

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

/* the same for a float32_t */
#define inferno_fp_bits32(x)		((x).v)
extern float32_t inferno_fp_from_bits32(uint32_t);

/*
 * Conversion with the host double: its 64 bits are copied, nothing is computed.
 * For the places where the C varargs convention carries a double (the verbs
 * of printf), and for the host tools.
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
 * They only raise flags: they do not trap and do not choose the rounding
 * mode.  They are written for round to nearest: whoever calls them uses
 * inferno_fp_round_nearest before and inferno_fp_round_restore after, and
 * inferno_fp_flags after, to collect the flags.
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
 * They only raise flags: they do not trap and do not choose the rounding
 * mode.  They are written for round to nearest: whoever calls them uses
 * inferno_fp_round_nearest before and inferno_fp_round_restore after, and
 * inferno_fp_flags after, to collect the flags.
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


/* functions of Math that are not in fdlibm; the flags are those of the arithmetic */
extern float64_t fd_fdim(float64_t, float64_t);
extern float64_t fd_fmax(float64_t, float64_t);
extern float64_t fd_fmin(float64_t, float64_t);

/*
 * Vectors and matrices of float64_t (libmath/blas.c, gemm.c): the sum of
 * products, the index of the largest absolute value, the sum of absolute
 * values, the euclidean norm, and C = alpha*op(A)*op(B) + beta*C.
 */
extern float64_t inferno_fp_dot(int, float64_t*, float64_t*);
extern int inferno_fp_iamax(int, float64_t*);
extern float64_t inferno_fp_norm1(int, float64_t*);
extern float64_t inferno_fp_norm2(int, float64_t*);
extern void inferno_fp_gemm(int, int, int, int, int, float64_t,
	float64_t*, int, float64_t*, int, float64_t, float64_t*, int);

/*
 * Numbers and text.
 *
 * fd_strtod: the float64_t nearest to the decimal number at the start of the
 * string (ties to even), *end is set after it.
 * fd_dtoa: the digits of a number in buf, and the position of the decimal
 * point in *decpt; mode 0 is the shortest string that reads back as the same
 * number, mode 2 gives ndigits significant digits, mode 3 ndigits after the
 * point (both correctly rounded, trailing zeros suppressed).  *sign is set for
 * a negative number or -0; *decpt is 9999 for infinity and NaN.  The result is
 * the length of the string.
 * fd_g_fmt: the shortest %g form, for a limit buf of 32 bytes.
 * inferno_fp_fmtconv: the verb of fmtinstall for e, f, g, E and G.
 */
extern float64_t fd_strtod(const char*, char**);
extern int fd_dtoa(float64_t, int, int, int*, int*, char*, int);
extern char* fd_g_fmt(char*, float64_t, int);
struct Fmt;
extern int inferno_fp_fmtconv(struct Fmt*);

#endif
