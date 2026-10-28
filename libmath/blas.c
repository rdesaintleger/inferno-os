/*
 * Vector operations of the Math module, on float64_t (SoftFloat).
 * The caller looks at the flags (FPflags) when they return.
 */
#include "lib9.h"
#include "mathi.h"
#include <inferno/fp.h>

static const float64_t zero = { 0 };

float64_t
dot(int n, float64_t *x, float64_t *y)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = f64_add(sum, f64_mul(*x, *y));
		x++;
		y++;
	}
	return sum;
}


int
iamax(int n, float64_t *x)
{
	int	i, m;
	float64_t xm, a;

	if (n <= 0)
		return 0;
	m = 0;
	xm = inferno_fp_abs(*x);
	for (i = 1; i < n; i++) {
		a = inferno_fp_abs(*++x);
		if (f64_lt(xm, a)) {
			m = i;
			xm = a;
		}
	}
	return m;
}


float64_t
norm1(int n, float64_t *x)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = f64_add(sum, inferno_fp_abs(*x));
		x++;
	}
	return sum;
}


float64_t
norm2(int n, float64_t *x)
{
	float64_t sum = zero;

	if (n <= 0)
		return zero;
	while (n--) {
		sum = f64_add(sum, f64_mul(*x, *x));
		x++;
	}
	return f64_sqrt(sum);
}
