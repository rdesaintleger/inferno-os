/*
 * Vector operations of the Math module, on float64_t (SoftFloat).
 * The caller looks at the flags (inferno_fp_flags) when they return.
 */
#include <inferno/fp.h>

static const float64_t zero = INFERNO_FP_CONST(0);

float64_t
inferno_fp_dot(int n, float64_t *x, float64_t *y)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = inferno_fp_add(sum, inferno_fp_mul(*x, *y));
		x++;
		y++;
	}
	return sum;
}


int
inferno_fp_iamax(int n, float64_t *x)
{
	int	i, m;
	float64_t xm, a;

	if (n <= 0)
		return 0;
	m = 0;
	xm = inferno_fp_abs(*x);
	for (i = 1; i < n; i++) {
		a = inferno_fp_abs(*++x);
		if (inferno_fp_lt(xm, a)) {
			m = i;
			xm = a;
		}
	}
	return m;
}


float64_t
inferno_fp_norm1(int n, float64_t *x)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = inferno_fp_add(sum, inferno_fp_abs(*x));
		x++;
	}
	return sum;
}


float64_t
inferno_fp_norm2(int n, float64_t *x)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = inferno_fp_add(sum, inferno_fp_mul(*x, *x));
		x++;
	}
	return inferno_fp_sqrt(sum);
}
