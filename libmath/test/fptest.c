/*
 * fptest - compares SoftFloat (through <inferno/fp.h>) with the floating-point
 * hardware of the host, bit for bit: results and exception flags, in the four
 * rounding modes.
 *
 * Operations: f64 add, sub, mul, div, sqrt, comparisons, conversions with
 * integers, roundToInt; f32 add, sub, mul, div, sqrt and conversions with f64.
 * Operands are pseudo-random (the sequence is always the same), with many
 * zeros, infinities, NaNs, subnormals and values near the limits.
 *
 * Any NaN equals any other NaN: the sign of a NaN produced by an operation on
 * a NaN is a choice of the implementation.  Conversions of values that do not
 * fit the integer are not compared: the integer returned is a choice too.
 *
 * usage: fptest [count]	operands per operation and mode, default 100000
 *
 * It is a test program: it needs a hosted C library and a host that computes
 * with strict IEEE 754 arithmetic (see mkfile): not the x87, no fused
 * multiply-add, rounding modes and flags available through <fenv.h>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <inferno/fp.h>

static uint64_t rs = UINT64_C(0x2545F4914F6CDD1D);
static uint64_t
rnd(void)
{
	rs ^= rs << 13;
	rs ^= rs >> 7;
	rs ^= rs << 17;
	return rs;
}

static uint64_t
pick64(void)
{
	uint64_t r = rnd();

	switch(rnd() % 8){
	case 0:
		return r;
	case 1:
		return r & UINT64_C(0x800FFFFFFFFFFFFF);			/* zero, subnormal */
	case 2:
		return (r & UINT64_C(0x800000000000000F)) | UINT64_C(0x7FF0000000000000) | ((r >> 8) & 1) << 51;	/* inf, nan */
	case 3:
		return (r & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)(1020 + rnd() % 8) << 52);	/* near 1 */
	case 4:
		return (r & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)(rnd() % 6) << 52);		/* tiny */
	case 5:
		return (r & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)(2040 + rnd() % 7) << 52);	/* huge */
	default:
		return (r & UINT64_C(0x800FFFFFFFFFFFFF)) | ((uint64_t)(rnd() % 2046 + 1) << 52);
	}
}

static uint32_t
pick32(void)
{
	uint32_t r = (uint32_t)rnd();

	switch(rnd() % 6){
	case 0:
		return r;
	case 1:
		return r & 0x807FFFFF;
	case 2:
		return (r & 0x8000000F) | 0x7F800000 | ((r >> 8) & 1) << 22;
	case 3:
		return (r & 0x807FFFFF) | ((uint32_t)(120 + rnd() % 15) << 23);
	case 4:
		return (r & 0x807FFFFF) | ((uint32_t)(250 + rnd() % 5) << 23);
	default:
		return (r & 0x807FFFFF) | ((uint32_t)(rnd() % 254 + 1) << 23);
	}
}

static double
tod(uint64_t b)
{
	double d;

	memcpy(&d, &b, 8);
	return d;
}

/*
 * Operands and results go through memory, never through a function return
 * value: on a 32-bit x86 the ABI returns a double in an x87 register, which
 * turns a signaling NaN into a quiet one and loses the invalid flag.
 */
#define SETD(v, b) do { uint64_t t_ = (b); memcpy((void *)&(v), &t_, 8); } while(0)
#define GETD(b, v) memcpy(&(b), (const void *)&(v), 8)
#define SETF(v, b) do { uint32_t t_ = (b); memcpy((void *)&(v), &t_, 4); } while(0)
#define GETF(b, v) memcpy(&(b), (const void *)&(v), 4)

static float64_t
F(uint64_t b)
{
	float64_t f;

	f.v = b;
	return f;
}

static float32_t
F32(uint32_t b)
{
	float32_t f;

	f.v = b;
	return f;
}

static long count = 100000, failures, total, nmis;
static const char *cur;
static const char *modename;

static int
isnan64(uint64_t b)
{
	return (b & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (b & UINT64_C(0x000FFFFFFFFFFFFF)) != 0;
}

static int
isnan32(uint32_t b)
{
	return (b & 0x7F800000) == 0x7F800000 && (b & 0x007FFFFF) != 0;
}

static int
same64(uint64_t a, uint64_t b)
{
	return a == b || (isnan64(a) && isnan64(b));
}

static int
same32(uint32_t a, uint32_t b)
{
	return a == b || (isnan32(a) && isnan32(b));
}

static int
hostflags(void)
{
	int f = fetestexcept(FE_ALL_EXCEPT), r = 0;

	if(f & FE_INVALID) r |= softfloat_flag_invalid;
	if(f & FE_DIVBYZERO) r |= softfloat_flag_infinite;
	if(f & FE_OVERFLOW) r |= softfloat_flag_overflow;
	if(f & FE_UNDERFLOW) r |= softfloat_flag_underflow;
	if(f & FE_INEXACT) r |= softfloat_flag_inexact;
	return r;
}

static void
bad(uint64_t x, uint64_t y, uint64_t want, uint64_t got, int wf, int gf)
{
	failures++;
	if(nmis++ < 4)
		printf("  MISMATCH %s [%s]: x=%016llx y=%016llx expected=%016llx/%02x got=%016llx/%02x\n", cur, modename,
			(unsigned long long)x, (unsigned long long)y, (unsigned long long)want, wf,
			(unsigned long long)got, gf);
}

static const int hostmodes[4] = { FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD };
static const uint_fast8_t softmodes[4] = { softfloat_round_near_even, softfloat_round_minMag, softfloat_round_min, softfloat_round_max };
static const char *modenames[4] = { "near", "zero", "down", "up" };

static volatile double va, vb, vr;
static volatile float fa, fb, fr;

#define SETUP(m) \
	fesetround(hostmodes[m]); softfloat_roundingMode = softmodes[m]; \
	softfloat_detectTininess = softfloat_tininess_afterRounding; modename = modenames[m]

#define START feclearexcept(FE_ALL_EXCEPT); softfloat_exceptionFlags = 0

static void
done(void)
{
	total += 4 * count;
	printf("%-14s %8ld operands x 4 modes  %s\n", cur, count, nmis ? "FAIL" : "ok");
	nmis = 0;
}

#define OP64(name, hostexpr, sfexpr) \
	static void t_##name(void) { int m; long i; cur = #name; \
		for(m = 0; m < 4; m++){ SETUP(m); \
			for(i = 0; i < count; i++){ uint64_t a = pick64(), b = pick64(); uint64_t want, got; int wf, gf; \
				SETD(va, a); SETD(vb, b); \
				START; vr = (hostexpr); wf = hostflags(); GETD(want, vr); \
				START; got = (sfexpr).v; gf = softfloat_exceptionFlags; \
				if(!same64(want, got) || wf != gf) bad(a, b, want, got, wf, gf); } } \
		fesetround(FE_TONEAREST); done(); }

OP64(f64_add, va + vb, f64_add(F(a), F(b)))
OP64(f64_sub, va - vb, f64_sub(F(a), F(b)))
OP64(f64_mul, va * vb, f64_mul(F(a), F(b)))
OP64(f64_div, va / vb, f64_div(F(a), F(b)))
OP64(f64_sqrt, sqrt(va), f64_sqrt(F(a)))

#define OP32(name, hostexpr, sfexpr) \
	static void t_##name(void) { int m; long i; cur = #name; \
		for(m = 0; m < 4; m++){ SETUP(m); \
			for(i = 0; i < count; i++){ uint32_t a = pick32(), b = pick32(); uint32_t want, got; int wf, gf; \
				SETF(fa, a); SETF(fb, b); \
				START; fr = (hostexpr); wf = hostflags(); GETF(want, fr); \
				START; got = (sfexpr).v; gf = softfloat_exceptionFlags; \
				if(!same32(want, got) || wf != gf) bad(a, b, want, got, wf, gf); } } \
		fesetround(FE_TONEAREST); done(); }

OP32(f32_add, fa + fb, f32_add(F32(a), F32(b)))
OP32(f32_sub, fa - fb, f32_sub(F32(a), F32(b)))
OP32(f32_mul, fa * fb, f32_mul(F32(a), F32(b)))
OP32(f32_div, fa / fb, f32_div(F32(a), F32(b)))
OP32(f32_sqrt, sqrtf(fa), f32_sqrt(F32(a)))

static void
t_f32_to_f64(void)
{
	int m;
	long i;

	cur = "f32_to_f64";
	for(m = 0; m < 4; m++){
		SETUP(m);
		for(i = 0; i < count; i++){
			uint32_t a = pick32();
			uint64_t want, got;
			int wf, gf;

			SETF(fa, a);
			START; vr = fa; wf = hostflags(); GETD(want, vr);
			START; got = f32_to_f64(F32(a)).v; gf = softfloat_exceptionFlags;
			if(!same64(want, got) || wf != gf) bad(a, 0, want, got, wf, gf);
		}
	}
	fesetround(FE_TONEAREST);
	done();
}

static void
t_f64_to_f32(void)
{
	int m;
	long i;

	cur = "f64_to_f32";
	for(m = 0; m < 4; m++){
		SETUP(m);
		for(i = 0; i < count; i++){
			uint64_t a = pick64();
			uint32_t want, got;
			int wf, gf;

			SETD(va, a);
			START; fr = (float)va; wf = hostflags(); GETF(want, fr);
			START; got = f64_to_f32(F(a)).v; gf = softfloat_exceptionFlags;
			if(!same32(want, got) || wf != gf) bad(a, 0, want, got, wf, gf);
		}
	}
	fesetround(FE_TONEAREST);
	done();
}

static void
t_compare(void)
{
	long i;
	int m;

	cur = "f64_compare";
	for(m = 0; m < 1; m++){
		SETUP(m);
		for(i = 0; i < count * 4; i++){
			uint64_t a = pick64(), b = pick64();
			int w[3], g[3], wf, gf;

			SETD(va, a); SETD(vb, b);
			START; w[0] = va == vb; wf = hostflags();
			START; g[0] = inferno_fp_eq(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[0] != g[0] || wf != gf) bad(a, b, w[0], g[0], wf, gf);
			START; w[1] = va < vb; wf = hostflags();
			START; g[1] = inferno_fp_lt(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[1] != g[1] || wf != gf) bad(a, b, w[1], g[1], wf, gf);
			START; w[2] = va <= vb; wf = hostflags();
			START; g[2] = inferno_fp_le(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[2] != g[2] || wf != gf) bad(a, b, w[2], g[2], wf, gf);
			START; w[0] = va != vb; wf = hostflags();
			START; g[0] = inferno_fp_ne(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[0] != g[0] || wf != gf) bad(a, b, w[0], g[0], wf, gf);
			START; w[1] = va > vb; wf = hostflags();
			START; g[1] = inferno_fp_gt(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[1] != g[1] || wf != gf) bad(a, b, w[1], g[1], wf, gf);
			START; w[2] = va >= vb; wf = hostflags();
			START; g[2] = inferno_fp_ge(F(a), F(b)); gf = softfloat_exceptionFlags;
			if(w[2] != g[2] || wf != gf) bad(a, b, w[2], g[2], wf, gf);
		}
	}
	total += 6 * 4 * count;
	printf("%-14s %8ld operands x 6      %s\n", cur, count * 4, nmis ? "FAIL" : "ok");
	nmis = 0;
}

static volatile int32_t vi32;
static volatile int64_t vi64;

static void
t_int_conversions(void)
{
	int m;
	long i;

	cur = "int_to_f64";
	for(m = 0; m < 4; m++){
		SETUP(m);
		for(i = 0; i < count; i++){
			int64_t n = (int64_t)rnd() >> (rnd() % 64);
			int32_t k = (int32_t)(rnd() >> (rnd() % 32));
			uint64_t want, got;
			int wf, gf;

			vi64 = n;
			START; vr = (double)vi64; wf = hostflags(); GETD(want, vr);
			START; got = i64_to_f64(n).v; gf = softfloat_exceptionFlags;
			if(!same64(want, got) || wf != gf) bad((uint64_t)n, 0, want, got, wf, gf);
			vi32 = k;
			START; vr = (double)vi32; wf = hostflags(); GETD(want, vr);
			START; got = inferno_fp_from_i32(k).v; gf = softfloat_exceptionFlags;
			if(!same64(want, got) || wf != gf) bad((uint64_t)(int64_t)k, 0, want, got, wf, gf);
		}
	}
	fesetround(FE_TONEAREST);
	done();
	cur = "f64_to_int";
	SETUP(0);
	for(i = 0; i < count * 4; i++){
		uint64_t a = pick64();
		double d = tod(a);
		int32_t w32, g32;
		int64_t w64, g64;

		if(!(d > -2147483648.0 && d < 2147483648.0))
			goto big;
		SETD(va, a);
		w32 = (int32_t)va;
		g32 = inferno_fp_to_i32(F(a));
		if(w32 != g32) bad(a, 0, (uint64_t)(int64_t)w32, (uint64_t)(int64_t)g32, 0, 0);
	big:
		if(!(d > -9223372036854775808.0 && d < 9223372036854775808.0))
			continue;
		SETD(va, a);
		w64 = (int64_t)va;
		g64 = (int64_t)f64_to_i64_r_minMag(F(a), false);
		if(w64 != g64) bad(a, 0, (uint64_t)w64, (uint64_t)g64, 0, 0);
	}
	total += 4 * count;
	printf("%-14s %8ld operands          %s\n", cur, count * 4, nmis ? "FAIL" : "ok");
	nmis = 0;
}

static void
t_round_to_int(void)
{
	long i;

	cur = "f64_roundToInt";
	SETUP(0);
	for(i = 0; i < count * 4; i++){
		uint64_t a = pick64();
		uint64_t want, got;

		va = tod(a);
		vr = floor(va); GETD(want, vr);
		got = f64_roundToInt(F(a), softfloat_round_min, false).v;
		if(!same64(want, got)) bad(a, 0, want, got, 0, 0);
		vr = ceil(va); GETD(want, vr);
		got = f64_roundToInt(F(a), softfloat_round_max, false).v;
		if(!same64(want, got)) bad(a, 1, want, got, 0, 0);
		vr = trunc(va); GETD(want, vr);
		got = f64_roundToInt(F(a), softfloat_round_minMag, false).v;
		if(!same64(want, got)) bad(a, 2, want, got, 0, 0);
		vr = nearbyint(va); GETD(want, vr);
		got = f64_roundToInt(F(a), softfloat_round_near_even, false).v;
		if(!same64(want, got)) bad(a, 3, want, got, 0, 0);
	}
	total += 4 * count;
	printf("%-14s %8ld operands x 4      %s\n", cur, count * 4, nmis ? "FAIL" : "ok");
	nmis = 0;
}

static void
t_bits(void)
{
	long i;

	cur = "bits";
	for(i = 0; i < count; i++){
		uint64_t a = pick64();
		int32_t h = (int32_t)rnd(), l = (int32_t)rnd();
		float64_t x = F(a);

		if((uint32_t)inferno_fp_hi(x) != (uint32_t)(a >> 32) || (uint32_t)inferno_fp_lo(x) != (uint32_t)a)
			bad(a, 0, 0, 0, 0, 0);
		if(inferno_fp_with_hi(x, h).v != (((uint64_t)(uint32_t)h << 32) | (a & 0xFFFFFFFF)))
			bad(a, 1, 0, 0, 0, 0);
		if(inferno_fp_with_lo(x, l).v != ((a & ~UINT64_C(0xFFFFFFFF)) | (uint32_t)l))
			bad(a, 2, 0, 0, 0, 0);
		if(inferno_fp_neg(x).v != (a ^ UINT64_C(0x8000000000000000)) || inferno_fp_abs(x).v != (a & ~UINT64_C(0x8000000000000000)))
			bad(a, 3, 0, 0, 0, 0);
		if(inferno_fp_nz(x) != (tod(a) != 0.0 || isnan64(a)))
			bad(a, 4, 0, 0, 0, 0);
	}
	total += count;
	printf("%-14s %8ld operands          %s\n", cur, count, nmis ? "FAIL" : "ok");
	nmis = 0;
}

int
main(int argc, char **argv)
{
	if(argc > 1)
		count = atol(argv[1]);
	t_f64_add();
	t_f64_sub();
	t_f64_mul();
	t_f64_div();
	t_f64_sqrt();
	t_compare();
	t_int_conversions();
	t_round_to_int();
	t_f32_add();
	t_f32_sub();
	t_f32_mul();
	t_f32_div();
	t_f32_sqrt();
	t_f32_to_f64();
	t_f64_to_f32();
	t_bits();
	printf("%ld operands tested, %ld mismatches\n", total, failures);
	return failures != 0;
}
