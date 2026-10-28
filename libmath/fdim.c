#include "lib9.h"
#include "mathi.h"
#include <inferno/fp.h>

/*
 * The comparisons are quiet: a NaN gives the same results as before and
 * does not raise INVAL.
 */
float64_t
fd_fdim(float64_t x, float64_t y)
{
	if(f64_lt_quiet(y, x))
		return f64_sub(x, y);
	return inferno_fp_from_i32(0);
}

float64_t
fd_fmax(float64_t x, float64_t y)
{
	if(f64_lt_quiet(y, x))
		return x;
	return y;
}

float64_t
fd_fmin(float64_t x, float64_t y)
{
	if(f64_lt_quiet(x, y))
		return x;
	return y;
}
