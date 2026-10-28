/*
 * Conversion between decimal strings and float64_t.
 *
 *	fd_strtod	decimal string to the nearest float64_t
 *	fd_dtoa		float64_t to decimal digits
 *
 * They use integer arithmetic only (a small multiple-precision integer type
 * below), and build the result from its 64 bits: nothing depends on the
 * floating-point unit of the host, on its rounding mode or on its byte order.
 * They replace the dtoa.c of David M. Gay; the algorithms of fd_dtoa are those
 * of Steele and White, "How to print floating-point numbers accurately", and
 * of Burger and Dybvig, "Printing floating-point numbers quickly and
 * accurately".  The results are the same as before: the closest float64_t
 * (ties to even) for fd_strtod, and for fd_dtoa the shortest string that reads
 * back as the same number (mode 0) or a correctly rounded (ties to even) string
 * of a given number of digits (modes 2 and 3).
 */
#include "lib9.h"
#include <inferno/fp.h>

/*
 * Natural numbers of up to NW*32 bits.  w[0] is the least significant limb,
 * and n counts the limbs in use (n == 0 is zero).
 */
enum { NW = 200 };

typedef struct Big Big;
struct Big
{
	int	n;
	uint32_t w[NW];
};

static void
bzero_(Big *b)
{
	b->n = 0;
}

static void
bset(Big *b, uint64_t v)
{
	b->n = 0;
	while(v != 0){
		b->w[b->n++] = (uint32_t)v;
		v >>= 32;
	}
}

static int
bbits(const Big *b)
{
	uint32_t t;
	int k;

	if(b->n == 0)
		return 0;
	t = b->w[b->n-1];
	k = 0;
	while(t != 0){
		k++;
		t >>= 1;
	}
	return 32*(b->n-1) + k;
}

/* b = b*m + a */
static void
bmuladd(Big *b, uint32_t m, uint32_t a)
{
	uint64_t c;
	int i;

	c = a;
	for(i = 0; i < b->n; i++){
		c += (uint64_t)b->w[i] * m;
		b->w[i] = (uint32_t)c;
		c >>= 32;
	}
	if(c != 0 && b->n < NW)
		b->w[b->n++] = (uint32_t)c;
}

/* b = b * 10**k */
static void
bpow10(Big *b, int k)
{
	static const uint32_t p10[] = {
		1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
		1000000000
	};

	if(b->n == 0)
		return;
	while(k >= 9){
		bmuladd(b, p10[9], 0);
		k -= 9;
	}
	if(k > 0)
		bmuladd(b, p10[k], 0);
}

static void
bshl(Big *b, int k)
{
	int i, ws, bs;

	if(b->n == 0 || k <= 0)
		return;
	ws = k / 32;
	bs = k % 32;
	if(b->n + ws + 1 > NW)
		return;	/* cannot happen: see NW */
	if(bs != 0){
		b->w[b->n] = 0;
		for(i = b->n; i > 0; i--)
			b->w[i] = (b->w[i] << bs) | (b->w[i-1] >> (32-bs));
		b->w[0] <<= bs;
		b->n++;
	}
	if(ws != 0){
		for(i = b->n-1; i >= 0; i--)
			b->w[i+ws] = b->w[i];
		for(i = 0; i < ws; i++)
			b->w[i] = 0;
		b->n += ws;
	}
	while(b->n > 0 && b->w[b->n-1] == 0)
		b->n--;
}

static void
bshr1(Big *b)
{
	int i;

	for(i = 0; i < b->n; i++){
		b->w[i] >>= 1;
		if(i+1 < b->n)
			b->w[i] |= b->w[i+1] << 31;
	}
	while(b->n > 0 && b->w[b->n-1] == 0)
		b->n--;
}

static int
bcmp_(const Big *a, const Big *b)
{
	int i;

	if(a->n != b->n)
		return a->n < b->n ? -1 : 1;
	for(i = a->n-1; i >= 0; i--)
		if(a->w[i] != b->w[i])
			return a->w[i] < b->w[i] ? -1 : 1;
	return 0;
}

/* a = a + b */
static void
badd(Big *a, const Big *b)
{
	uint64_t c;
	int i, n;

	n = a->n > b->n ? a->n : b->n;
	c = 0;
	for(i = 0; i < n; i++){
		c += (i < a->n ? a->w[i] : 0);
		c += (i < b->n ? b->w[i] : 0);
		a->w[i] = (uint32_t)c;
		c >>= 32;
	}
	a->n = n;
	if(c != 0 && a->n < NW)
		a->w[a->n++] = (uint32_t)c;
}

/* a = a - b, with a >= b */
static void
bsub(Big *a, const Big *b)
{
	int64_t c;
	int i;

	c = 0;
	for(i = 0; i < a->n; i++){
		c += a->w[i];
		c -= (i < b->n ? b->w[i] : 0);
		a->w[i] = (uint32_t)c;
		c >>= 32;
	}
	while(a->n > 0 && a->w[a->n-1] == 0)
		a->n--;
}

/*
 * fd_strtod
 */
enum
{
	MAXD	= 800	/* significant digits kept; the others only make a sticky digit */
};

static float64_t
mkfp(int sign, uint64_t bits)
{
	if(sign)
		bits |= UINT64_C(0x8000000000000000);
	return inferno_fp_from_bits(bits);
}

static int
match(const char **sp, const char *t)
{
	int c, d;
	const char *s = *sp;

	while((d = *t++) != 0){
		if((c = *++s) >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		if(c != d)
			return 0;
	}
	*sp = s + 1;
	return 1;
}

float64_t
fd_strtod(const char *s00, char **se)
{
	const char *s, *s1;
	char dig[MAXD+1];
	int sign, nd, seen, e10, esign, ex, c, sticky, infrac, i, k, t, ue, shift, bl;
	Big num, den, dd;
	uint64_t q, m, rem, half;
	float64_t r;

	sign = 0;
	nd = 0;
	seen = 0;
	e10 = 0;
	sticky = 0;
	infrac = 0;
	for(s = s00; ; s++){
		c = *s;
		if(c == ' ' || (c >= '\t' && c <= '\r'))
			continue;
		break;
	}
	if(c == '-' || c == '+'){
		sign = c == '-';
		s++;
	}
	for(;; s++){
		c = *s;
		if(c == '.' && !infrac){
			infrac = 1;
			continue;
		}
		if(c < '0' || c > '9')
			break;
		seen = 1;
		if(nd == 0 && c == '0'){
			if(infrac)
				e10--;
			continue;
		}
		if(nd < MAXD){
			dig[nd++] = c;
			if(infrac)
				e10--;
		}else{
			if(c != '0')
				sticky = 1;
			if(!infrac)
				e10++;
		}
	}
	if(!seen){
		/* infinity and NaN, as the strings "infinity" and "nan" */
		s1 = s00;
		for(s = s00; *s == ' ' || (*s >= '\t' && *s <= '\r'); s++)
			;
		if(*s == '-' || *s == '+')
			s++;
		c = *s;
		if(c == 'i' || c == 'I'){
			if(match(&s, "nfinity")){
				r = mkfp(sign, UINT64_C(0x7FF0000000000000));
				goto ret;
			}
		}else if(c == 'n' || c == 'N'){
			if(match(&s, "an")){
				r = mkfp(sign, UINT64_C(0x7FF8000000000000));
				goto ret;
			}
		}
		s = s1;
		r = mkfp(0, 0);
		goto ret;
	}
	/* the exponent */
	c = *s;
	if(c == 'e' || c == 'E'){
		s1 = s;
		esign = 0;
		c = *++s;
		if(c == '-' || c == '+'){
			esign = c == '-';
			c = *++s;
		}
		if(c >= '0' && c <= '9'){
			ex = 0;
			while(c >= '0' && c <= '9'){
				if(ex < 100000)
					ex = ex*10 + (c - '0');
				c = *++s;
			}
			e10 += esign ? -ex : ex;
		}else
			s = s1;
	}
	if(nd == 0){
		r = mkfp(sign, 0);
		goto ret;
	}
	/* value = digits * 10**e10, digits having nd digits (and a sticky one) */
	if(sticky){
		/* the digits that were not kept, as one nonzero digit */
		dig[nd++] = '1';
		e10--;
	}
	if(nd + e10 > 310){
		r = mkfp(sign, UINT64_C(0x7FF0000000000000));
		goto ret;
	}
	if(nd + e10 < -325){
		r = mkfp(sign, 0);
		goto ret;
	}
	bzero_(&num);
	for(i = 0; i < nd; ){
		t = 0;
		k = 0;
		while(i < nd && k < 9){
			t = t*10 + (dig[i] - '0');
			i++;
			k++;
		}
		{
			uint32_t p = 1;
			int j;
			for(j = 0; j < k; j++)
				p *= 10;
			bmuladd(&num, p, (uint32_t)t);
		}
	}
	bset(&den, 1);
	if(e10 >= 0)
		bpow10(&num, e10);
	else
		bpow10(&den, -e10);

	/* q = floor(num * 2**t / den), in [2**55, 2**57) */
	t = 56 - (bbits(&num) - bbits(&den));
	if(t >= 0)
		bshl(&num, t);
	else
		bshl(&den, -t);
	dd = den;
	bshl(&dd, 57);
	q = 0;
	for(i = 57; i >= 0; i--){
		q <<= 1;
		if(bcmp_(&num, &dd) >= 0){
			bsub(&num, &dd);
			q |= 1;
		}
		bshr1(&dd);
	}
	sticky = num.n != 0;

	bl = 0;
	for(m = q; m != 0; m >>= 1)
		bl++;
	/* the lowest bit kept: 53 bits, or the weight 2**-1074 */
	shift = bl - 53;
	if(shift < t - 1074)
		shift = t - 1074;
	if(shift >= 58){
		r = mkfp(sign, 0);
		goto ret;
	}
	m = q >> shift;
	rem = q & ((UINT64_C(1) << shift) - 1);
	half = UINT64_C(1) << (shift - 1);
	if(rem > half || (rem == half && (sticky || (m & 1))))
		m++;
	ue = shift - t;
	if(m >= (UINT64_C(1) << 53)){
		m >>= 1;
		ue++;
	}
	if(m < (UINT64_C(1) << 52))
		r = mkfp(sign, m);
	else{
		k = ue + 1075;
		if(k >= 2047)
			r = mkfp(sign, UINT64_C(0x7FF0000000000000));
		else
			r = mkfp(sign, ((uint64_t)k << 52) | (m & ((UINT64_C(1) << 52) - 1)));
	}
ret:
	if(se != nil)
		*se = (char*)s;
	return r;
}

/*
 * fd_dtoa
 */
static void
carry(char *buf, int *len, int *k)
{
	int i;

	for(i = *len-1; i >= 0; i--){
		if(buf[i] != '9'){
			buf[i]++;
			*len = i+1;
			return;
		}
	}
	buf[0] = '1';
	*len = 1;
	(*k)++;
}

int
fd_dtoa(float64_t d, int mode, int ndigits, int *decpt, int *sign, char *buf, int nbuf)
{
	uint64_t bits, m;
	int ef, e2, k, len, i, dg, low_ok, high_ok, ilim, bl, fixed, lo, hi;
	Big R, S, Mp, Mm, T;

	bits = inferno_fp_bits(d);
	*sign = (bits >> 63) != 0;
	bits &= ~UINT64_C(0x8000000000000000);
	ef = (int)(bits >> 52);
	m = bits & ((UINT64_C(1) << 52) - 1);
	if(ef == 2047){
		*decpt = 9999;
		strcpy(buf, m == 0 ? "Infinity" : "NaN");
		return strlen(buf);
	}
	if(ef == 0 && m == 0){
		*decpt = 1;
		strcpy(buf, "0");
		return 1;
	}
	if(ef == 0)
		e2 = -1074;
	else{
		e2 = ef - 1075;
		m |= UINT64_C(1) << 52;
	}
	fixed = mode == 2 || mode == 3;

	/* d = m * 2**e2 = R/S; Mp and Mm are the half-gaps above and below */
	bset(&R, m);
	bset(&S, 1);
	bzero_(&Mp);
	bzero_(&Mm);
	low_ok = high_ok = 1;
	if(e2 >= 0)
		bshl(&R, e2);
	else
		bshl(&S, -e2);
	if(fixed){
		/* no gaps; the upper test is strict, see below */
	}else{
		low_ok = high_ok = (m & 1) == 0;
		bshl(&R, 1);
		bshl(&S, 1);
		bset(&Mp, 1);
		if(e2 > 0)
			bshl(&Mp, e2);
		Mm = Mp;
		if(m == (UINT64_C(1) << 52) && ef > 1){
			/* the gap below is half the gap above */
			bshl(&R, 1);
			bshl(&S, 1);
			bshl(&Mp, 1);
		}
	}

	/*
	 * k: d/10**k in [0.1, 1[ (so that the first digit is the next one).
	 * The estimate, from the binary logarithm, is corrected below.
	 */
	bl = bbits(&R) - bbits(&S);
	if(bl >= 0)
		k = (int)((bl * 78913L) >> 18);
	else
		k = -(int)(((-bl) * 78913L + 0x3ffff) >> 18);
	if(k >= 0)
		bpow10(&S, k);
	else{
		bpow10(&R, -k);
		bpow10(&Mp, -k);
		bpow10(&Mm, -k);
	}
	for(;;){
		T = R;
		badd(&T, &Mp);
		i = bcmp_(&T, &S);
		if(high_ok ? i >= 0 : i > 0){
			bmuladd(&S, 10, 0);
			k++;
			continue;
		}
		bmuladd(&T, 10, 0);
		i = bcmp_(&T, &S);
		if(high_ok ? i < 0 : i <= 0){
			bmuladd(&R, 10, 0);
			bmuladd(&Mp, 10, 0);
			bmuladd(&Mm, 10, 0);
			k--;
			continue;
		}
		break;
	}

	len = 0;
	if(fixed){
		if(mode == 2)
			ilim = ndigits <= 0 ? 1 : ndigits;
		else
			ilim = k + ndigits;
		if(ilim > nbuf - 2)
			ilim = nbuf - 2;
		if(ilim < 0){
			*decpt = -ndigits;
			buf[0] = 0;
			return 0;
		}
		if(ilim == 0){
			/* the value is rounded to 0 or to 10**k */
			bmuladd(&R, 2, 0);
			i = bcmp_(&R, &S);
			if(i > 0){
				*decpt = k + 1;
				strcpy(buf, "1");
				return 1;
			}
			*decpt = k;
			buf[0] = 0;
			return 0;
		}
		for(len = 0; len < ilim; len++){
			bmuladd(&R, 10, 0);
			for(dg = 0; bcmp_(&R, &S) >= 0; dg++)
				bsub(&R, &S);
			buf[len] = '0' + dg;
		}
		bmuladd(&R, 2, 0);
		i = bcmp_(&R, &S);
		if(i > 0 || (i == 0 && ((buf[len-1] - '0') & 1)))
			carry(buf, &len, &k);
	}else{
		for(;;){
			bmuladd(&R, 10, 0);
			bmuladd(&Mp, 10, 0);
			bmuladd(&Mm, 10, 0);
			for(dg = 0; bcmp_(&R, &S) >= 0; dg++)
				bsub(&R, &S);
			i = bcmp_(&R, &Mm);
			lo = low_ok ? i <= 0 : i < 0;
			T = R;
			badd(&T, &Mp);
			i = bcmp_(&T, &S);
			hi = high_ok ? i >= 0 : i > 0;
			if(!lo && !hi){
				buf[len++] = '0' + dg;
				if(len >= nbuf - 2)
					break;
				continue;
			}
			if(lo && !hi){
				buf[len++] = '0' + dg;
			}else if(!lo && hi){
				buf[len++] = '0' + dg;
				carry(buf, &len, &k);
			}else{
				T = R;
				bmuladd(&T, 2, 0);
				i = bcmp_(&T, &S);
				buf[len++] = '0' + dg;
				if(i > 0 || (i == 0 && (dg & 1)))
					carry(buf, &len, &k);
			}
			break;
		}
	}
	while(len > 1 && buf[len-1] == '0')
		len--;
	buf[len] = 0;
	*decpt = k;
	return len;
}
