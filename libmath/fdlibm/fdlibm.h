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

/* fdlibm kernel functions */
extern float64_t fd_kernel_sin(float64_t, float64_t, int32_t);
extern float64_t fd_kernel_cos(float64_t, float64_t);
extern float64_t fd_kernel_tan(float64_t, float64_t, int32_t);
extern int32_t fd_kernel_rem_pio2(float64_t *, float64_t *, int32_t, int32_t, int32_t, const int32_t *);

#endif
