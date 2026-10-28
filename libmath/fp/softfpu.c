/*
 * The floating-point unit of the Inferno virtual machine.
 *
 * It is a register, not an instruction set: the control word (rounding mode
 * and exceptions that trap) and the status word (exceptions seen since they
 * were last cleared) of the running process.  The arithmetic is done by
 * SoftFloat, which only sets flags; inferno_fp_flags() moves them to the status word
 * and says which of them trap.  The caller raises the run-time error.
 *
 * Nothing here depends on the host or on the compiler.  The state of the
 * running process is in the variables below; the scheduler switches it with
 * inferno_fp_save and inferno_fp_restore.
 */
#include <inferno/fp.h>

enum
{
	EXC	= INFERNO_FP_INVAL|INFERNO_FP_ZDIV|INFERNO_FP_OVFL|INFERNO_FP_UNFL|INFERNO_FP_INEX,
	VALID	= EXC|INFERNO_FP_RND_MASK
};

static uint32_t	fpctl;
static uint32_t	fpst;

static void
setmode(void)
{
	switch(fpctl & INFERNO_FP_RND_MASK){
	case INFERNO_FP_RND_NINF:
		softfloat_roundingMode = softfloat_round_min;
		break;
	case INFERNO_FP_RND_PINF:
		softfloat_roundingMode = softfloat_round_max;
		break;
	case INFERNO_FP_RND_Z:
		softfloat_roundingMode = softfloat_round_minMag;
		break;
	default:
		softfloat_roundingMode = softfloat_round_near_even;
		break;
	}
	softfloat_detectTininess = softfloat_tininess_afterRounding;
}

void
inferno_fp_init(void)
{
	fpctl = INFERNO_FP_INVAL|INFERNO_FP_ZDIV|INFERNO_FP_OVFL|INFERNO_FP_UNFL;	/* round to nearest; INFERNO_FP_INEX is silent */
	fpst = 0;
	softfloat_exceptionFlags = 0;
	setmode();
}

void
inferno_fp_save(SoftFPU *u)
{
	u->control = fpctl;
	u->status = fpst;
}

void
inferno_fp_restore(SoftFPU *u)
{
	fpctl = u->control;
	fpst = u->status;
	softfloat_exceptionFlags = 0;
	setmode();
}

uint32_t
inferno_fp_get_control(void)
{
	return fpctl;
}

uint32_t
inferno_fp_control(uint32_t fcr, uint32_t mask)
{
	uint32_t old;

	old = fpctl;
	mask &= VALID;
	fpctl = (old & ~mask) | (fcr & mask);
	setmode();
	return old & mask;
}

uint32_t
inferno_fp_get_status(void)
{
	return fpst;
}

uint32_t
inferno_fp_status(uint32_t fsr, uint32_t mask)
{
	uint32_t old;

	old = fpst;
	mask &= EXC;
	fpst = (old & ~mask) | (fsr & mask);
	return old & mask;
}

/*
 * Move the flags raised by SoftFloat to the status word and clear them.
 * The result is the set of exceptions that trap: raised, and not masked.
 */
uint32_t
inferno_fp_flags(void)
{
	uint_fast8_t fl;
	uint32_t st;

	fl = softfloat_exceptionFlags;
	softfloat_exceptionFlags = 0;
	st = 0;
	if(fl & softfloat_flag_invalid)
		st |= INFERNO_FP_INVAL;
	if(fl & softfloat_flag_infinite)
		st |= INFERNO_FP_ZDIV;
	if(fl & softfloat_flag_overflow)
		st |= INFERNO_FP_OVFL;
	if(fl & softfloat_flag_underflow)
		st |= INFERNO_FP_UNFL;
	if(fl & softfloat_flag_inexact)
		st |= INFERNO_FP_INEX;
	fpst |= st;
	return st & fpctl & EXC;
}

int
inferno_fp_round_nearest(void)
{
	int old;

	old = softfloat_roundingMode;
	softfloat_roundingMode = softfloat_round_near_even;
	return old;
}

void
inferno_fp_round_restore(int mode)
{
	softfloat_roundingMode = mode;
}
