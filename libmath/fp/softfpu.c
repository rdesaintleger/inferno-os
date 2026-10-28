/*
 * The floating-point unit of the Inferno virtual machine.
 *
 * It is a register, not an instruction set: the control word (rounding mode
 * and exceptions that trap) and the status word (exceptions seen since they
 * were last cleared) of the running process.  The arithmetic is done by
 * SoftFloat, which only sets flags; FPflags() moves them to the status word
 * and says which of them trap.  The caller raises the run-time error.
 *
 * Nothing here depends on the host or on the compiler.  The state of the
 * running process is in the variables below; the scheduler switches it with
 * FPsave and FPrestore.
 */
#include "lib9.h"
#include "mathi.h"
#include <inferno/fp.h>

enum
{
	EXC	= INVAL|ZDIV|OVFL|UNFL|INEX,
	VALID	= EXC|RND_MASK
};

static ulong	fpctl;
static ulong	fpst;

static void
setmode(void)
{
	switch(fpctl & RND_MASK){
	case RND_NINF:
		softfloat_roundingMode = softfloat_round_min;
		break;
	case RND_PINF:
		softfloat_roundingMode = softfloat_round_max;
		break;
	case RND_Z:
		softfloat_roundingMode = softfloat_round_minMag;
		break;
	default:
		softfloat_roundingMode = softfloat_round_near_even;
		break;
	}
	softfloat_detectTininess = softfloat_tininess_afterRounding;
}

void
FPinit(void)
{
	fpctl = INVAL|ZDIV|OVFL|UNFL;	/* round to nearest; INEX is silent */
	fpst = 0;
	softfloat_exceptionFlags = 0;
	setmode();
}

void
FPsave(void *v)
{
	InfernoFPU *u;

	u = v;
	u->control = fpctl;
	u->status = fpst;
}

void
FPrestore(void *v)
{
	InfernoFPU *u;

	u = v;
	fpctl = u->control;
	fpst = u->status;
	softfloat_exceptionFlags = 0;
	setmode();
}

ulong
getFPcontrol(void)
{
	return fpctl;
}

ulong
FPcontrol(ulong fcr, ulong mask)
{
	ulong old;

	old = fpctl;
	mask &= VALID;
	fpctl = (old & ~mask) | (fcr & mask);
	setmode();
	return old & mask;
}

ulong
getFPstatus(void)
{
	return fpst;
}

ulong
FPstatus(ulong fsr, ulong mask)
{
	ulong old;

	old = fpst;
	mask &= EXC;
	fpst = (old & ~mask) | (fsr & mask);
	return old & mask;
}

/*
 * Move the flags raised by SoftFloat to the status word and clear them.
 * The result is the set of exceptions that trap: raised, and not masked.
 */
ulong
FPflags(void)
{
	uint_fast8_t fl;
	ulong st;

	fl = softfloat_exceptionFlags;
	softfloat_exceptionFlags = 0;
	st = 0;
	if(fl & softfloat_flag_invalid)
		st |= INVAL;
	if(fl & softfloat_flag_infinite)
		st |= ZDIV;
	if(fl & softfloat_flag_overflow)
		st |= OVFL;
	if(fl & softfloat_flag_underflow)
		st |= UNFL;
	if(fl & softfloat_flag_inexact)
		st |= INEX;
	fpst |= st;
	return st & fpctl & EXC;
}
