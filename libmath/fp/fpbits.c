/*
 * Operations of <inferno/fp.h> that work on the bits of a number.
 * They are exact, do not use SoftFloat and do not touch any flag.
 */
#include <stdint.h>
#include <inferno/fp.h>

float64_t
inferno_fp_from_bits(uint64_t v)
{
	float64_t r;

	r.v = v;
	return r;
}

float64_t
inferno_fp_neg(float64_t x)
{
	x.v ^= UINT64_C(0x8000000000000000);
	return x;
}

float64_t
inferno_fp_abs(float64_t x)
{
	x.v &= ~UINT64_C(0x8000000000000000);
	return x;
}

float64_t
inferno_fp_with_hi(float64_t x, int32_t hi)
{
	x.v = (x.v & UINT64_C(0xFFFFFFFF)) | ((uint64_t)(uint32_t)hi << 32);
	return x;
}

float64_t
inferno_fp_with_lo(float64_t x, int32_t lo)
{
	x.v = (x.v & ~UINT64_C(0xFFFFFFFF)) | (uint64_t)(uint32_t)lo;
	return x;
}
