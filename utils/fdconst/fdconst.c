/*
 * fdconst - build-time generator of float64 constant "sidecar" headers.
 *
 * A source file declares its floating-point constants with the marker macros
 *
 *	F64K(name, 6.93147180369123816490e-01)
 *	F64KA(halF, 2, 0.5, -0.5)
 *
 * which expand to nothing for the C compiler.  fdconst reads the source,
 * converts every decimal literal to its IEEE 754 binary64 bit pattern and
 * writes a header defining the constants as `static const float64_t', to be
 * included by the source itself:
 *
 *	#include "e_exp.k.h"
 *
 * The conversion is done with integer arithmetic only (dec2bits below): no
 * double, no strtod, no host floating point, so the generated bits do not
 * depend on the machine that builds them.  Results are correctly rounded
 * (round half to even).  Literals whose value is subnormal or overflows,
 * and literals with more than 120 significant digits, are refused.
 *
 * usage:	fdconst [-o out] file.c	write the sidecar (default stdout)
 *		fdconst -l file.c	list name, decimal text and bits
 *		fdconst -s		self-test filter: decimals on stdin,
 *					one 16-digit hex pattern (or ERR) per line
 *
 * Standard C99 only; it deliberately does not use lib9 so that it can be
 * built before every other library.
 */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- integer-only decimal -> binary64 ----------------------------------- */

#define NL 90				/* 90 x 32 = 2880 bits */
typedef struct Big Big;
struct Big {
	uint32_t w[NL];
	int n;
};

static void
bzero_(Big *a)
{
	memset(a, 0, sizeof *a);
}

static void
btrim(Big *a)
{
	while(a->n > 0 && a->w[a->n-1] == 0)
		a->n--;
}

static int
bmulsmall(Big *a, uint32_t m, uint32_t add)
{
	uint64_t c = add;
	int i;

	for(i = 0; i < a->n; i++){
		c += (uint64_t)a->w[i] * m;
		a->w[i] = (uint32_t)c;
		c >>= 32;
	}
	if(c){
		if(a->n >= NL)
			return -1;
		a->w[a->n++] = (uint32_t)c;
	}
	return 0;
}

static int
bbits(const Big *a)
{
	int b = 0;
	uint32_t t;

	if(a->n == 0)
		return 0;
	t = a->w[a->n-1];
	while(t){
		b++;
		t >>= 1;
	}
	return (a->n-1)*32 + b;
}

/* r = a << s */
static int
bshl(Big *r, const Big *a, int s)
{
	int ws = s/32, bs = s%32, i;

	bzero_(r);
	if(a->n == 0)
		return 0;
	if(a->n + ws + 1 > NL)
		return -1;
	for(i = 0; i < a->n; i++){
		uint64_t v = (uint64_t)a->w[i] << bs;
		r->w[i+ws] |= (uint32_t)v;
		r->w[i+ws+1] |= (uint32_t)(v >> 32);
	}
	r->n = a->n + ws + 1;
	btrim(r);
	return 0;
}

static int
bcmp_(const Big *a, const Big *b)
{
	int i;

	if(a->n != b->n)
		return a->n < b->n ? -1 : 1;
	for(i = a->n - 1; i >= 0; i--)
		if(a->w[i] != b->w[i])
			return a->w[i] < b->w[i] ? -1 : 1;
	return 0;
}

/* a -= b, requires a >= b */
static void
bsub(Big *a, const Big *b)
{
	int64_t br = 0;
	int i;

	for(i = 0; i < a->n; i++){
		int64_t v = (int64_t)a->w[i] - (i < b->n ? b->w[i] : 0) - br;
		br = v < 0;
		if(v < 0)
			v += (int64_t)1 << 32;
		a->w[i] = (uint32_t)v;
	}
	btrim(a);
}

/* returns 0 and the bit pattern in *out, or -1 on syntax error or out of range */
static int
dec2bits(const char *s, uint64_t *out)
{
	Big num, den, t, R;
	uint64_t q = 0, mant, rem, half;
	int neg = 0, nd = 0, anyd = 0, seen_pt = 0, frac = 0, e10 = 0;
	int sticky, L, d, sh, E, k, bit, s2;

	if(*s == '-'){
		neg = 1;
		s++;
	}else if(*s == '+')
		s++;
	bzero_(&num);
	bzero_(&den);
	for(; *s; s++){
		if(*s == '.'){
			if(seen_pt)
				return -1;
			seen_pt = 1;
			continue;
		}
		if(!isdigit((unsigned char)*s))
			break;
		anyd = 1;
		if(seen_pt)
			frac++;
		if(nd == 0 && *s == '0')
			continue;		/* leading zeros */
		if(++nd > 120)
			return -1;
		if(bmulsmall(&num, 10, *s - '0'))
			return -1;
	}
	if(!anyd)
		return -1;			/* no digit at all */
	if(*s == 'e' || *s == 'E'){
		int es = 1, ev = 0, any = 0;

		s++;
		if(*s == '-'){
			es = -1;
			s++;
		}else if(*s == '+')
			s++;
		for(; isdigit((unsigned char)*s); s++){
			ev = ev*10 + (*s - '0');
			any = 1;
			if(ev > 9999)
				return -1;
		}
		if(!any)
			return -1;
		e10 = es * ev;
	}
	if(*s)
		return -1;
	e10 -= frac;
	if(num.n == 0){
		*out = (uint64_t)neg << 63;
		return 0;
	}
	den.w[0] = 1;
	den.n = 1;
	if(e10 >= 0){
		for(k = 0; k < e10; k++)
			if(bmulsmall(&num, 10, 0))
				return -1;
	}else{
		for(k = 0; k < -e10; k++)
			if(bmulsmall(&den, 10, 0))
				return -1;
	}
	/* scale so that num/den lies in (2^55, 2^57), then long division */
	d = bbits(&num) - bbits(&den);
	s2 = 56 - d;
	if(s2 >= 0){
		if(bshl(&t, &num, s2))
			return -1;
		num = t;
	}else{
		if(bshl(&t, &den, -s2))
			return -1;
		den = t;
	}
	R = num;
	for(bit = 57; bit >= 0; bit--){
		if(bshl(&t, &den, bit))
			return -1;
		if(bcmp_(&R, &t) >= 0){
			bsub(&R, &t);
			q |= (uint64_t)1 << bit;
		}
	}
	sticky = R.n != 0;
	L = 0;
	{
		uint64_t x = q;

		while(x){
			L++;
			x >>= 1;
		}
	}
	E = (L - 1) - s2;
	if(E < -1022 || E > 1023)
		return -1;
	sh = L - 53;
	mant = q >> sh;
	rem = q & (((uint64_t)1 << sh) - 1);
	half = (uint64_t)1 << (sh - 1);
	if(rem > half || (rem == half && (sticky || (mant & 1))))
		mant++;
	if(mant == ((uint64_t)1 << 53)){
		mant >>= 1;
		E++;
		if(E > 1023)
			return -1;
	}
	*out = ((uint64_t)neg << 63) | ((uint64_t)(E + 1023) << 52) | (mant & (((uint64_t)1 << 52) - 1));
	return 0;
}

/* ---- source scanning ---------------------------------------------------- */

typedef struct Ent Ent;
struct Ent {
	char *name;
	int count;			/* 0: scalar (F64K); n >= 1: array (F64KA) */
	char **text;			/* decimal text of each value */
	uint64_t *bits;
	int line;
};

static Ent *ents;
static int nents, errors;
static const char *srcname = "-";

static void
err(int line, const char *fmt, const char *arg)
{
	fprintf(stderr, "fdconst: %s:%d: ", srcname, line);
	fprintf(stderr, fmt, arg);
	fputc('\n', stderr);
	errors++;
}

static char *
dupn(const char *s, size_t n)
{
	char *p = malloc(n + 1);

	if(p == NULL){
		fprintf(stderr, "fdconst: out of memory\n");
		exit(2);
	}
	memcpy(p, s, n);
	p[n] = 0;
	return p;
}

static int
isidc(int c)
{
	return isalnum((unsigned char)c) || c == '_';
}

/*
 * Parse the argument list of a marker starting just after its '('.
 * Splits at top-level commas, drops comments, trims blanks.
 * Returns the index just after the matching ')' or -1.
 */
static long
margs(const char *s, long n, long i, char ***argv, int *argc, int *line)
{
	int depth = 1, cap = 8;
	long start = i;
	char **a = malloc(cap * sizeof *a);
	char *buf = malloc(n - i + 2);
	size_t w = 0;

	*argc = 0;
	if(a == NULL || buf == NULL)
		return -1;
	(void)start;
	for(; i < n; i++){
		char c = s[i];

		if(c == '\n')
			(*line)++;
		if(c == '/' && s[i+1] == '*'){
			i += 2;
			while(i < n && !(s[i] == '*' && s[i+1] == '/')){
				if(s[i] == '\n')
					(*line)++;
				i++;
			}
			i++;
			continue;
		}
		if(c == '/' && s[i+1] == '/'){
			while(i < n && s[i] != '\n')
				i++;
			(*line)++;
			continue;
		}
		if(c == '('){
			depth++;
		}else if(c == ')'){
			if(--depth == 0)
				goto done;
		}
		if(c == ',' && depth == 1){
			buf[w] = 0;
			if(*argc == cap){
				cap *= 2;
				a = realloc(a, cap * sizeof *a);
			}
			a[(*argc)++] = dupn(buf, w);
			w = 0;
			continue;
		}
		buf[w++] = c;
	}
	return -1;
done:
	buf[w] = 0;
	if(*argc == cap){
		cap *= 2;
		a = realloc(a, cap * sizeof *a);
	}
	a[(*argc)++] = dupn(buf, w);
	free(buf);
	*argv = a;
	return i + 1;
}

static char *
trim(char *s)
{
	char *e;

	while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;
	e = s + strlen(s);
	while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
		*--e = 0;
	return s;
}

static int
validname(const char *s)
{
	if(!(isalpha((unsigned char)*s) || *s == '_'))
		return 0;
	for(; *s; s++)
		if(!isidc(*s))
			return 0;
	return 1;
}

static void
addent(Ent *e)
{
	ents = realloc(ents, (nents + 1) * sizeof *ents);
	if(ents == NULL){
		fprintf(stderr, "fdconst: out of memory\n");
		exit(2);
	}
	ents[nents++] = *e;
}

static void
marker(char **a, int argc, int isarray, int line)
{
	Ent e;
	int i, want, first;
	char *nm;

	memset(&e, 0, sizeof e);
	e.line = line;
	if(argc < 2){
		err(line, "%s needs a name and a value", isarray ? "F64KA" : "F64K");
		return;
	}
	nm = trim(a[0]);
	if(!validname(nm)){
		err(line, "bad constant name `%s'", nm);
		return;
	}
	for(i = 0; i < nents; i++)
		if(strcmp(ents[i].name, nm) == 0){
			err(line, "constant `%s' defined twice", nm);
			return;
		}
	e.name = dupn(nm, strlen(nm));
	if(isarray){
		char *c = trim(a[1]), *end;
		long n = strtol(c, &end, 10);

		if(*c == 0 || *end != 0 || n < 1 || n > 65536){
			err(line, "bad element count `%s'", c);
			return;
		}
		want = (int)n;
		first = 2;
		if(argc - first != want && !(argc - first == want + 1 && *trim(a[argc-1]) == 0)){
			err(line, "F64KA count does not match number of values for `%s'", nm);
			return;
		}
		e.count = want;
	}else{
		if(argc != 2){
			err(line, "F64K takes exactly a name and a value (`%s')", nm);
			return;
		}
		want = 1;
		first = 1;
		e.count = 0;
	}
	e.text = malloc(want * sizeof *e.text);
	e.bits = malloc(want * sizeof *e.bits);
	for(i = 0; i < want; i++){
		char *v = trim(a[first + i]);

		if(dec2bits(v, &e.bits[i])){
			err(line, "cannot convert `%s' (syntax, subnormal, overflow or too long)", v);
			return;
		}
		e.text[i] = dupn(v, strlen(v));
	}
	addent(&e);
}

static void
scan(const char *s, long n)
{
	long i = 0;
	int line = 1, bol = 1;

	while(i < n){
		char c = s[i];

		if(c == '\n'){
			line++;
			bol = 1;
			i++;
			continue;
		}
		if(c == ' ' || c == '\t' || c == '\r'){
			i++;
			continue;
		}
		if(c == '/' && s[i+1] == '*'){
			i += 2;
			while(i < n && !(s[i] == '*' && s[i+1] == '/')){
				if(s[i] == '\n')
					line++;
				i++;
			}
			i += 2;
			bol = 0;
			continue;
		}
		if(c == '/' && s[i+1] == '/'){
			while(i < n && s[i] != '\n')
				i++;
			continue;
		}
		if(c == '#' && bol){		/* preprocessor line (with continuations) */
			while(i < n && s[i] != '\n'){
				if(s[i] == '\\' && s[i+1] == '\n'){
					line++;
					i++;
				}
				i++;
			}
			continue;
		}
		bol = 0;
		if(c == '"' || c == '\''){
			long j = i + 1;

			while(j < n && s[j] != c && s[j] != '\n'){
				if(s[j] == '\\')
					j++;
				j++;
			}
			i = j + 1;
			continue;
		}
		if(isalpha((unsigned char)c) || c == '_'){
			long j = i;
			int isa;

			while(j < n && isidc(s[j]))
				j++;
			isa = (j - i == 5 && memcmp(s + i, "F64KA", 5) == 0);
			if(isa || (j - i == 4 && memcmp(s + i, "F64K", 4) == 0)){
				long k = j;
				int ln = line;

				while(k < n && (s[k] == ' ' || s[k] == '\t'))
					k++;
				if(k < n && s[k] == '('){
					char **a = NULL;
					int ac = 0;
					long e = margs(s, n, k + 1, &a, &ac, &line);

					if(e < 0){
						err(ln, "unterminated %s", isa ? "F64KA" : "F64K");
						return;
					}
					marker(a, ac, isa, ln);
					i = e;
					continue;
				}
			}
			i = j;
			continue;
		}
		i++;
	}
}

/* ---- output ------------------------------------------------------------- */

static void
guardname(const char *path, char *out, size_t n)
{
	const char *b = strrchr(path, '/');
	size_t i = 0;

	b = b ? b + 1 : path;
	for(; *b && i + 1 < n; b++)
		out[i++] = isalnum((unsigned char)*b) ? (char)toupper((unsigned char)*b) : '_';
	out[i] = 0;
}

static void
emit(FILE *f, const char *path, int list)
{
	int i, j;
	char g[128];

	if(list){
		for(i = 0; i < nents; i++)
			for(j = 0; j < (ents[i].count ? ents[i].count : 1); j++)
				fprintf(f, "%s%s\t%s\t0x%016llX\n", ents[i].name,
					ents[i].count ? "[]" : "", ents[i].text[j],
					(unsigned long long)ents[i].bits[j]);
		return;
	}
	guardname(path, g, sizeof g);
	fprintf(f, "/* generated by fdconst from %s - do not edit */\n", path);
	fprintf(f, "#ifndef FDCONST_%s_K_H\n#define FDCONST_%s_K_H\n\n", g, g);
	fprintf(f, "/* the markers define nothing: the constants are the definitions below */\n");
	fprintf(f, "#ifndef F64K\n#define F64K(name, val)\n#define F64KA(...)\n#endif\n\n");
	for(i = 0; i < nents; i++){
		Ent *e = &ents[i];

		if(e->count == 0){
			fprintf(f, "static const float64_t %s = { UINT64_C(0x%016llX) };\t/* %s */\n",
				e->name, (unsigned long long)e->bits[0], e->text[0]);
		}else{
			fprintf(f, "static const float64_t %s[%d] = {\n", e->name, e->count);
			for(j = 0; j < e->count; j++)
				fprintf(f, "\t{ UINT64_C(0x%016llX) },\t/* %s */\n",
					(unsigned long long)e->bits[j], e->text[j]);
			fprintf(f, "};\n");
		}
	}
	fprintf(f, "\n#endif\n");
}

static int
selftest(void)
{
	char line[1024];
	uint64_t b;

	while(fgets(line, sizeof line, stdin)){
		line[strcspn(line, "\r\n")] = 0;
		if(dec2bits(line, &b))
			printf("ERR\n");
		else
			printf("%016llx\n", (unsigned long long)b);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int list = 0, i;
	const char *outname = NULL, *in = NULL;
	FILE *f;
	char *s;
	long n;

	for(i = 1; i < argc; i++){
		if(strcmp(argv[i], "-s") == 0)
			return selftest();
		else if(strcmp(argv[i], "-l") == 0)
			list = 1;
		else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			outname = argv[++i];
		else if(argv[i][0] == '-' && argv[i][1] != 0)
			goto usage;
		else if(in == NULL)
			in = argv[i];
		else
			goto usage;
	}
	if(in == NULL)
		goto usage;
	f = fopen(in, "rb");
	if(f == NULL){
		fprintf(stderr, "fdconst: cannot open %s\n", in);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	rewind(f);
	s = malloc(n + 1);
	if(s == NULL || fread(s, 1, n, f) != (size_t)n){
		fprintf(stderr, "fdconst: cannot read %s\n", in);
		return 2;
	}
	fclose(f);
	s[n] = 0;
	srcname = in;
	scan(s, n);
	if(errors)
		return 1;
	if(outname != NULL){
		f = fopen(outname, "wb");
		if(f == NULL){
			fprintf(stderr, "fdconst: cannot create %s\n", outname);
			return 2;
		}
	}else
		f = stdout;
	emit(f, in, list);
	if(outname != NULL && fclose(f) != 0){
		remove(outname);
		return 2;
	}
	return 0;
usage:
	fprintf(stderr, "usage: fdconst [-o out] file.c | fdconst -l file.c | fdconst -s\n");
	return 2;
}
