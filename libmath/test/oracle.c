/*
 * oracle - compares libmath/fdlibm, ported to SoftFloat, with the reference
 * copy in ref/, bit for bit.
 *
 * Both compute with the same algorithm; they must return exactly the same
 * bits for every operand.  Operands are pseudo-random (the sequence is always
 * the same) and chosen to reach every branch: random bit patterns, values
 * built from the thresholds found in the sources (hiwords.h) and their
 * neighbours, values of moderate magnitude, and special values (zeros,
 * infinities, NaNs, subnormals, extremes).
 *
 * Any NaN equals any other NaN: the reference computes the sign of a NaN with
 * the hardware of the host, SoftFloat with its own rules.
 *
 * usage: oracle [count]	operands per function, default 100000
 *
 * It is a test program: it needs a hosted C library and a host that computes
 * with strict IEEE 754 binary64 arithmetic (see mkfile).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inferno/fp.h>
#include "fdlibm.h"

/* reference, from ref/ (the sources are those of fdlibm before the port) */
extern double ref_acos(double), ref_acosh(double), ref_asin(double), ref_atanh(double),
	ref_cosh(double), ref_exp(double), ref_j0(double), ref_j1(double), ref_log(double),
	ref_log10(double), ref_sinh(double), ref_sqrt(double), ref_y0(double), ref_y1(double),
	ref_asinh(double), ref_atan(double), ref_cbrt(double), ref_ceil(double), ref_cos(double),
	ref_erf(double), ref_erfc(double), ref_expm1(double), ref_fabs(double), ref_floor(double),
	ref_log1p(double), ref_sin(double), ref_tan(double), ref_tanh(double), ref_rint(double);
extern double ref_atan2(double, double), ref_fmod(double, double), ref_hypot(double, double),
	ref_pow(double, double), ref_remainder(double, double), ref_copysign(double, double),
	ref_nextafter(double, double);
extern double ref_jn(int, double), ref_yn(int, double), ref_scalbn(double, int);
extern int ref_finite(double), ref_ilogb(double);
extern double ref_lgamma_r(double, int *), ref_modf(double, double *);
extern int ref_rem_pio2(double, double *);
extern double ref_kernel_sin(double, double, int), ref_kernel_cos(double, double),
	ref_kernel_tan(double, double, int);

#include "hiwords.h"
#define NHI (sizeof hiwords / sizeof hiwords[0])

static uint64_t rs = UINT64_C(0x9E3779B97F4A7C15);
static uint64_t
rnd(void)
{
	rs ^= rs << 13;
	rs ^= rs >> 7;
	rs ^= rs << 17;
	return rs;
}

static double
tod(uint64_t b)
{
	double d;

	memcpy(&d, &b, 8);
	return d;
}

static uint64_t
tob(double d)
{
	uint64_t b;

	memcpy(&b, &d, 8);
	return b;
}

static float64_t
F(uint64_t b)
{
	float64_t f;

	f.v = b;
	return f;
}

static const uint64_t special[] = {
	0x0000000000000000, 0x8000000000000000, 0x7FF0000000000000, 0xFFF0000000000000,
	0x7FF8000000000000, 0xFFF8000000000000, 0x7FF0000000000001, 0x3FF0000000000000,
	0xBFF0000000000000, 0x3FE0000000000000, 0x4000000000000000, 0x7FEFFFFFFFFFFFFF,
	0xFFEFFFFFFFFFFFFF, 0x0010000000000000, 0x000FFFFFFFFFFFFF, 0x0000000000000001,
	0x8000000000000001, 0x3FE921FB54442D18, 0x3FF921FB54442D18, 0x400921FB54442D18,
	0x7E37E43C8800759C, 0x01A56E1FC2F8F359, 0x4086232BDD7ABCD2, 0xC0874910D52D3051,
};

static uint64_t
pickbits(void)
{
	uint64_t r = rnd(), b;
	uint32_t hi, lo;
	static const uint32_t los[] = { 0, 1, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu };

	switch(rnd() % 10){
	case 0: case 1: case 2:			/* any bit pattern */
		return r;
	case 3: case 4: case 5:			/* thresholds of the algorithms, +-1 */
		hi = hiwords[rnd() % NHI] + (uint32_t)(rnd() % 3) - 1;
		lo = (rnd() & 1) ? (uint32_t)rnd() : los[rnd() % 5];
		b = ((uint64_t)hi << 32) | lo;
		if(rnd() & 1)
			b ^= UINT64_C(0x8000000000000000);
		return b;
	case 6: case 7: case 8:			/* moderate magnitude, random fraction */
		b = r & UINT64_C(0x800FFFFFFFFFFFFF);
		b |= (uint64_t)(1023 + (int)(rnd() % 121) - 60) << 52;
		return b;
	default:
		return special[rnd() % (sizeof special / sizeof special[0])];
	}
}

static long count = 100000, failures, total;
static const char *cur;
static long nmis;

static int
same(double r, float64_t g)
{
	uint64_t a = tob(r), b = g.v;

	if(a == b)
		return 1;
	if((a & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (a & UINT64_C(0x000FFFFFFFFFFFFF)) != 0 &&
	   (b & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (b & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)
		return 1;
	return 0;
}

static void
bad(const char *what, uint64_t x, uint64_t y, uint64_t want, uint64_t got)
{
	failures++;
	if(nmis++ < 5)
		printf("  MISMATCH %s %s: x=%016llx y=%016llx expected=%016llx got=%016llx\n", cur, what,
			(unsigned long long)x, (unsigned long long)y, (unsigned long long)want, (unsigned long long)got);
}

static void
begin(const char *name)
{
	cur = name;
	nmis = 0;
	softfloat_roundingMode = softfloat_round_near_even;
	softfloat_detectTininess = softfloat_tininess_afterRounding;
	softfloat_exceptionFlags = 0;
}

static void
end(void)
{
	total += count;
	printf("%-12s %8ld operands  %s\n", cur, count, nmis ? "FAIL" : "ok");
}

#define D1(name) \
	static void t_##name(void) { long i; begin(#name); \
		for(i = 0; i < count; i++){ uint64_t b = pickbits(); double r = ref_##name(tod(b)); float64_t g = fd_##name(F(b)); \
			if(!same(r, g)) bad("", b, 0, tob(r), g.v); } end(); }
#define D2(name) \
	static void t_##name(void) { long i; begin(#name); \
		for(i = 0; i < count; i++){ uint64_t a = pickbits(), b = pickbits(); double r = ref_##name(tod(a), tod(b)); float64_t g = fd_##name(F(a), F(b)); \
			if(!same(r, g)) bad("", a, b, tob(r), g.v); } end(); }

D1(acos)
D1(acosh)
D1(asin)
D1(atanh)
D1(cosh)
D1(exp)
D1(j0)
D1(y0)
D1(j1)
D1(y1)
D1(log)
D1(log10)
D1(sinh)
D1(sqrt)
D1(asinh)
D1(atan)
D1(cbrt)
D1(ceil)
D1(cos)
D1(erf)
D1(erfc)
D1(expm1)
D1(fabs)
D1(floor)
D1(log1p)
D1(sin)
D1(tan)
D1(tanh)
D1(rint)
D2(atan2)
D2(fmod)
D2(hypot)
D2(pow)
D2(remainder)
D2(copysign)
D2(nextafter)

static void
t_jn(void)
{
	long i;

	begin("jn");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		int n = (int)(rnd() % 201) - 50;
		double r = ref_jn(n, tod(b));
		float64_t g = fd_jn(n, F(b));

		if(!same(r, g))
			bad("jn", b, (uint64_t)(int64_t)n, tob(r), g.v);
	}
	end();
}
static void
t_yn(void)
{
	long i;

	begin("yn");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		int n = (int)(rnd() % 201) - 50;
		double r = ref_yn(n, tod(b));
		float64_t g = fd_yn(n, F(b));

		if(!same(r, g))
			bad("yn", b, (uint64_t)(int64_t)n, tob(r), g.v);
	}
	end();
}
static void
t_scalbn(void)
{
	long i;

	begin("scalbn");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		int n = (rnd() & 7) ? (int)(rnd() % 4401) - 2200 : ((rnd() & 1) ? 50000 : -50000);
		double r = ref_scalbn(tod(b), n);
		float64_t g = fd_scalbn(F(b), n);

		if(!same(r, g))
			bad("scalbn", b, (uint64_t)(int64_t)n, tob(r), g.v);
	}
	end();
}
static void
t_finite(void)
{
	long i;

	begin("finite");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();

		if(ref_finite(tod(b)) != fd_finite(F(b)))
			bad("finite", b, 0, 0, 0);
	}
	end();
}
static void
t_ilogb(void)
{
	long i;

	begin("ilogb");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		int r = ref_ilogb(tod(b)), g = fd_ilogb(F(b));

		if(r != g)
			bad("ilogb", b, 0, (uint64_t)(int64_t)r, (uint64_t)(int64_t)g);
	}
	end();
}
static void
t_lgamma_r(void)
{
	long i;

	begin("lgamma_r");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		int32_t rs_, gs_;
		double r = ref_lgamma_r(tod(b), &rs_);
		float64_t g = fd_lgamma_r(F(b), &gs_);

		if(!same(r, g) || rs_ != gs_)
			bad("lgamma_r", b, (uint64_t)(int64_t)rs_, tob(r), g.v);
	}
	end();
}
static void
t_modf(void)
{
	long i;

	begin("modf");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		double rp;
		float64_t gp;
		double r = ref_modf(tod(b), &rp);
		float64_t g = fd_modf(F(b), &gp);

		if(!same(r, g) || !same(rp, gp))
			bad("modf", b, tob(rp), tob(r), g.v);
	}
	end();
}
static void
t_rem_pio2(void)
{
	long i;

	begin("rem_pio2");
	for(i = 0; i < count; i++){
		uint64_t b = pickbits();
		double ry[2];
		float64_t gy[2];
		int rn = ref_rem_pio2(tod(b), ry); int32_t gn = fd_rem_pio2(F(b), gy);

		if(rn != gn || !same(ry[0], gy[0]) || !same(ry[1], gy[1]))
			bad("rem_pio2", b, (uint64_t)(int64_t)rn, tob(ry[0]), gy[0].v);
	}
	end();
}

/* kernels: |x| <= pi/4, y a small correction of x */
static void
kernelargs(uint64_t *x, uint64_t *y)
{
	uint64_t b = rnd();
	int e = (int)(rnd() % 40);			/* |x| in [2^-40, 1) */

	*x = (b & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)(1022 - e) << 52);
	if(tod(*x) > 0.785 || tod(*x) < -0.785)
		*x = (*x & ~UINT64_C(0x7FF0000000000000)) | ((uint64_t)(1021 - e) << 52);
	b = rnd();
	*y = (b & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)((int)((*x >> 52) & 0x7FF) - 54 - (int)(rnd() % 4)) << 52);
	if(rnd() % 8 == 0)
		*y = 0;
}

static void
t_kernel_sin(void)
{
	long i;

	begin("kernel_sin");
	for(i = 0; i < count; i++){
		uint64_t x, y;
		int iy = (int)(rnd() & 1);
		double r;
		float64_t g;

		kernelargs(&x, &y);
		r = ref_kernel_sin(tod(x), tod(y), iy);
		g = fd_kernel_sin(F(x), F(y), iy);
		if(!same(r, g))
			bad("kernel_sin", x, y, tob(r), g.v);
	}
	end();
}
static void
t_kernel_cos(void)
{
	long i;

	begin("kernel_cos");
	for(i = 0; i < count; i++){
		uint64_t x, y;
		double r;
		float64_t g;

		kernelargs(&x, &y);
		r = ref_kernel_cos(tod(x), tod(y));
		g = fd_kernel_cos(F(x), F(y));
		if(!same(r, g))
			bad("kernel_cos", x, y, tob(r), g.v);
	}
	end();
}
static void
t_kernel_tan(void)
{
	long i;

	begin("kernel_tan");
	for(i = 0; i < count; i++){
		uint64_t x, y;
		int iy = (rnd() & 1) ? 1 : -1;
		double r;
		float64_t g;

		kernelargs(&x, &y);
		r = ref_kernel_tan(tod(x), tod(y), iy);
		g = fd_kernel_tan(F(x), F(y), iy);
		if(!same(r, g))
			bad("kernel_tan", x, y, tob(r), g.v);
	}
	end();
}

int
main(int argc, char **argv)
{
	if(argc > 1)
		count = atol(argv[1]);
#define RUN(n) t_##n();
	RUN(acos)
	RUN(acosh)
	RUN(asin)
	RUN(atanh)
	RUN(cosh)
	RUN(exp)
	RUN(j0)
	RUN(y0)
	RUN(j1)
	RUN(y1)
	RUN(log)
	RUN(log10)
	RUN(sinh)
	RUN(sqrt)
	RUN(asinh)
	RUN(atan)
	RUN(cbrt)
	RUN(ceil)
	RUN(cos)
	RUN(erf)
	RUN(erfc)
	RUN(expm1)
	RUN(fabs)
	RUN(floor)
	RUN(log1p)
	RUN(sin)
	RUN(tan)
	RUN(tanh)
	RUN(rint)
	RUN(atan2)
	RUN(fmod)
	RUN(hypot)
	RUN(pow)
	RUN(remainder)
	RUN(copysign)
	RUN(nextafter)
	RUN(jn)
	RUN(yn)
	RUN(scalbn)
	RUN(finite)
	RUN(ilogb)
	RUN(lgamma_r)
	RUN(modf)
	RUN(rem_pio2)
	RUN(kernel_sin)
	RUN(kernel_cos)
	RUN(kernel_tan)
	printf("%ld operands tested, %ld mismatches\n", total, failures);
	return failures != 0;
}
