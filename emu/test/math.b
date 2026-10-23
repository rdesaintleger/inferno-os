implement Mathtest;
include "sys.m";
	sys: Sys;
include "draw.m";
include "math.m";
	math: Math;

# Math: every function, against values that are mathematically known (exact when the
# result is exactly representable, otherwise within a relative tolerance).
# covers: Math.acos Math.acosh Math.asin Math.asinh Math.atan Math.atan2 Math.atanh Math.cbrt
# covers: Math.ceil Math.copysign Math.cos Math.cosh Math.dot Math.erf Math.erfc Math.exp
# covers: Math.expm1 Math.fabs Math.fdim Math.fmin Math.fmax Math.finite Math.floor Math.fmod
# covers: Math.gemm Math.getFPcontrol Math.getFPstatus Math.FPcontrol Math.FPstatus Math.hypot
# covers: Math.iamax Math.ilogb Math.isnan Math.j0 Math.j1 Math.jn Math.lgamma Math.log
# covers: Math.log10 Math.log1p Math.modf Math.nextafter Math.norm1 Math.norm2 Math.pow
# covers: Math.pow10 Math.remainder Math.rint Math.scalbn Math.sin Math.sinh Math.sort Math.sqrt
# covers: Math.tan Math.tanh Math.y0 Math.y1 Math.yn Math.import_int Math.import_real32
# covers: Math.import_real Math.export_int Math.export_real32 Math.export_real
# covers: Math.bits32real Math.bits64real Math.realbits32 Math.realbits64

Mathtest: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

bad := 0;

check(ok: int, what: string)
{
	if(!ok){
		sys->print("FAIL %s\n", what);
		bad++;
	}
}

# a == b within a relative tolerance
near(a, b: real, what: string)
{
	d := a - b;
	if(d < 0.)
		d = -d;
	m := b;
	if(m < 0.)
		m = -m;
	if(m < 1.)
		m = 1.;
	if(!(d <= 1e-13 * m)){
		sys->print("FAIL %s: got %.17g want %.17g\n", what, a, b);
		bad++;
	}
}

exact(a, b: real, what: string)
{
	if(a != b){
		sys->print("FAIL %s: got %.17g want %.17g\n", what, a, b);
		bad++;
	}
}

elementary()
{
	pi := Math->Pi;
	exact(math->sqrt(16.), 4., "sqrt");
	exact(math->cbrt(27.), 3., "cbrt");
	exact(math->pow(2., 10.), 1024., "pow");
	exact(math->pow10(3), 1000., "pow10");
	exact(math->hypot(3., 4.), 5., "hypot");
	exact(math->exp(0.), 1., "exp 0");
	near(math->exp(1.), 2.718281828459045, "exp 1");
	exact(math->expm1(0.), 0., "expm1 0");
	near(math->expm1(1.), 1.718281828459045, "expm1 1");
	exact(math->log(1.), 0., "log 1");
	near(math->log(math->exp(1.)), 1., "log e");
	exact(math->log10(1000.), 3., "log10");
	exact(math->log1p(0.), 0., "log1p 0");
	near(math->log1p(1.), 0.6931471805599453, "log1p 1");
	exact(math->sin(0.), 0., "sin 0");
	exact(math->cos(0.), 1., "cos 0");
	exact(math->tan(0.), 0., "tan 0");
	near(math->sin(pi/2.), 1., "sin pi/2");
	near(math->cos(pi), -1., "cos pi");
	near(math->tan(pi/4.), 1., "tan pi/4");
	exact(math->asin(0.), 0., "asin 0");
	near(math->asin(1.), pi/2., "asin 1");
	exact(math->acos(1.), 0., "acos 1");
	near(math->acos(0.), pi/2., "acos 0");
	near(math->atan(1.), pi/4., "atan 1");
	near(math->atan2(1., 1.), pi/4., "atan2 1 1");
	near(math->atan2(0., -1.), pi, "atan2 0 -1");
	near(math->atan2(-1., 0.), -pi/2., "atan2 -1 0");
	exact(math->sinh(0.), 0., "sinh 0");
	exact(math->cosh(0.), 1., "cosh 0");
	exact(math->tanh(0.), 0., "tanh 0");
	near(math->sinh(1.), 1.1752011936438014, "sinh 1");
	near(math->cosh(1.), 1.5430806348152437, "cosh 1");
	near(math->tanh(1.), 0.7615941559557649, "tanh 1");
	exact(math->asinh(0.), 0., "asinh 0");
	exact(math->acosh(1.), 0., "acosh 1");
	exact(math->atanh(0.), 0., "atanh 0");
	near(math->asinh(1.), 0.881373587019543, "asinh 1");
	near(math->acosh(2.), 1.3169578969248166, "acosh 2");
	near(math->atanh(0.5), 0.5493061443340548, "atanh 0.5");
	exact(math->erf(0.), 0., "erf 0");
	near(math->erf(1.), 0.8427007929497149, "erf 1");
	exact(math->erfc(0.), 1., "erfc 0");
	near(math->erf(0.7) + math->erfc(0.7), 1., "erf + erfc");
	(sg, lg) := math->lgamma(5.);
	check(sg == 1, "lgamma sign");
	near(lg, 3.1780538303479458, "lgamma 5 = log 24");
	(sg, lg) = math->lgamma(0.5);
	near(lg, 0.5723649429247001, "lgamma 0.5 = log sqrt(pi)");
}

bessel()
{
	exact(math->j0(0.), 1., "j0 0");
	exact(math->j1(0.), 0., "j1 0");
	exact(math->jn(2, 0.), 0., "jn 2 0");
	near(math->j0(1.), 0.7651976865579666, "j0 1");
	near(math->j1(1.), 0.4400505857449335, "j1 1");
	near(math->jn(2, 1.), 0.1149034849319005, "jn 2 1");
	near(math->y0(1.), 0.08825696421567697, "y0 1");
	near(math->y1(1.), -0.7812128213002887, "y1 1");
	near(math->yn(2, 1.), -1.6506826068162546, "yn 2 1");
}

floating()
{
	exact(math->fabs(-2.5), 2.5, "fabs");
	exact(math->floor(-1.5), -2., "floor");
	exact(math->ceil(-1.5), -1., "ceil");
	exact(math->rint(2.5), 2., "rint rounds half to even (2.5)");
	exact(math->rint(3.5), 4., "rint rounds half to even (3.5)");
	exact(math->fmod(7., 3.), 1., "fmod");
	exact(math->fmod(-7., 3.), -1., "fmod sign");
	exact(math->remainder(7., 3.), 1., "remainder 7 3");
	exact(math->remainder(8., 3.), -1., "remainder 8 3");
	exact(math->fdim(5., 3.), 2., "fdim");
	exact(math->fdim(3., 5.), 0., "fdim negative");
	exact(math->fmin(2., 3.), 2., "fmin");
	exact(math->fmax(2., 3.), 3., "fmax");
	exact(math->copysign(3., -1.), -3., "copysign");
	exact(math->scalbn(1., 10), 1024., "scalbn");
	check(math->ilogb(8.) == 3, "ilogb");
	check(math->nextafter(1., 2.) > 1., "nextafter up");
	check(math->nextafter(1., 0.) < 1., "nextafter down");
	exact(math->nextafter(1., 2.) - 1., Math->MachEps, "nextafter step is machine epsilon");
	(ip, fp) := math->modf(3.75);
	check(ip == 3, sys->sprint("modf integer part %d", ip));
	exact(fp, 0.75, "modf fraction");
	(ip, fp) = math->modf(-3.75);
	check(ip == -3, sys->sprint("modf integer part of a negative: %d", ip));
	exact(fp, -0.75, "modf negative fraction");
	zero := 0.;
	inf := Math->Infinity;
	nan := Math->NaN;
	check(math->finite(1.) == 1, "finite 1");
	check(math->finite(inf) == 0, "finite infinity");
	check(math->finite(nan) == 0, "finite nan");
	check(math->isnan(nan) == 1, "isnan nan");
	check(math->isnan(1.) == 0, "isnan 1");
	check(math->isnan(inf) == 0, "isnan infinity");
	check(1. / (zero + 1e400) == 0., "infinity constant");
	# IEEE says nan != nan; the interpreter says otherwise (recorded, not asserted)
	sys->print("behaviour: nan != nan is %d, nan == nan is %d\n", nan != nan, nan == nan);
}

# the control and status words
fpenv()
{
	old := math->getFPcontrol();
	trap := Math->INVAL | Math->ZDIV | Math->OVFL | Math->UNFL;
	check((old & trap) == trap && (old & Math->INEX) == 0, sys->sprint("default control word %x: invalid, divide by zero, overflow, underflow trap; inexact is quiet", old));
	check((old & Math->RND_MASK) == Math->RND_NR, "default rounding is to nearest");

	# a trapping exception is a run-time error
	zero := 0.;
	trapped := 0;
	{
		x := 1. / zero;
		check(x == x, "not reached");
	} exception {
	"sys: fp:*" =>
		trapped = 1;
	}
	check(trapped == 1, "division by zero traps by default");

	math->FPcontrol(Math->RND_Z, Math->RND_MASK);
	check((math->getFPcontrol() & Math->RND_MASK) == Math->RND_Z, "rounding mode toward zero");
	math->FPcontrol(Math->RND_PINF, Math->RND_MASK);
	check((math->getFPcontrol() & Math->RND_MASK) == Math->RND_PINF, "rounding mode toward +infinity");
	math->FPcontrol(Math->RND_NINF, Math->RND_MASK);
	check((math->getFPcontrol() & Math->RND_MASK) == Math->RND_NINF, "rounding mode toward -infinity");
	math->FPcontrol(old, Math->RND_MASK);
	check(math->getFPcontrol() == old, "control word restored");

	# without the trap the same division gives infinity and sets the status flag
	math->FPcontrol(0, Math->ZDIV);
	math->FPstatus(0, ~0);
	check(math->getFPstatus() == 0, "status cleared");
	x := 1. / zero;
	check(x == Math->Infinity, "1/0 is infinity once the trap is off");
	check((math->getFPstatus() & Math->ZDIV) != 0, sys->sprint("division by zero flag (status %x)", math->getFPstatus()));
	math->FPstatus(0, ~0);
	check(math->getFPstatus() == 0, "status cleared again");
	math->FPcontrol(old, Math->ZDIV);
	check(math->getFPcontrol() == old, "trap restored");
}

# The elementary functions are computed in software, by SoftFloat: they only
# raise flags.  The traps of the process are applied after the call and give
# the same run-time error as the hardware did, the flags join the status, and
# the rounding mode of the process does not change the result.
fpfunc()
{
	old := math->getFPcontrol();
	traps := Math->INVAL | Math->ZDIV | Math->OVFL | Math->UNFL;
	zero := 0.;
	ones := 1000.;
	trapped := 0;

	# default control word: the exceptions trap
	{
		x := math->log(zero);
		check(x == x, "not reached");
	} exception {
	"sys: fp:*" =>
		trapped = 1;
	}
	check(trapped == 1, "log(0) traps (divide by zero)");

	trapped = 0;
	{
		x := math->sqrt(-1.);
		check(x == x, "not reached");
	} exception {
	"sys: fp:*" =>
		trapped = 1;
	}
	check(trapped == 1, "sqrt(-1) traps (invalid)");

	trapped = 0;
	{
		x := math->exp(ones);
		check(x == x, "not reached");
	} exception {
	"sys: fp:*" =>
		trapped = 1;
	}
	check(trapped == 1, "exp(1000) traps (overflow)");

	# the traps off: the result of IEEE 754 and the flag
	math->FPcontrol(0, traps);
	math->FPstatus(0, ~0);
	x := math->log(zero);
	check(x == -Math->Infinity, "log(0) is -infinity");
	check((math->getFPstatus() & Math->ZDIV) != 0, sys->sprint("log(0) sets the divide by zero flag (status %x)", math->getFPstatus()));

	math->FPstatus(0, ~0);
	x = math->sqrt(-1.);
	check(math->isnan(x), "sqrt(-1) is a NaN");
	check((math->getFPstatus() & Math->INVAL) != 0, sys->sprint("sqrt(-1) sets the invalid flag (status %x)", math->getFPstatus()));

	math->FPstatus(0, ~0);
	x = math->exp(ones);
	check(x == Math->Infinity, "exp(1000) is infinity");
	check((math->getFPstatus() & Math->OVFL) != 0, sys->sprint("exp(1000) sets the overflow flag (status %x)", math->getFPstatus()));

	math->FPstatus(0, ~0);
	x = math->exp(-ones * 1000.);
	check(x == 0., "exp(-1000000) is zero");
	check((math->getFPstatus() & Math->UNFL) != 0, sys->sprint("exp(-1000000) sets the underflow flag (status %x)", math->getFPstatus()));

	# inexact is quiet: the flag is set, nothing traps
	math->FPstatus(0, ~0);
	x = math->exp(1.);
	check((math->getFPstatus() & Math->INEX) != 0, sys->sprint("exp(1) sets the inexact flag (status %x)", math->getFPstatus()));
	check((math->getFPstatus() & traps) == 0, "exp(1) raises nothing else");
	math->FPcontrol(old, traps);
	check(math->getFPcontrol() == old, "traps restored");

	# a result does not depend on the rounding mode of the process
	r := math->exp(1.);
	s := math->sin(1.);
	modes := array[] of {Math->RND_Z, Math->RND_PINF, Math->RND_NINF};
	for(i := 0; i < len modes; i++){
		math->FPcontrol(modes[i], Math->RND_MASK);
		exact(math->exp(1.), r, "exp(1) in another rounding mode");
		exact(math->sin(1.), s, "sin(1) in another rounding mode");
	}
	math->FPcontrol(old, Math->RND_MASK);
	math->FPstatus(0, ~0);
	check(math->getFPcontrol() == old, "control word restored");
}

linalg()
{
	x := array[] of {1., 2., 3.};
	y := array[] of {4., 5., 6.};
	exact(math->dot(x, y), 32., "dot");
	exact(math->norm1(array[] of {1., -2., 3.}), 6., "norm1");
	exact(math->norm2(array[] of {3., 4.}), 5., "norm2");
	check(math->iamax(array[] of {1., -5., 3.}) == 1, "iamax");

	v := array[] of {3., 1., 2.};
	p := array[] of {0, 1, 2};
	math->sort(v, p);
	check(v[0] == 3. && v[1] == 1. && v[2] == 2., "sort leaves its first argument alone");
	check(v[p[0]] <= v[p[1]] && v[p[1]] <= v[p[2]], "sort orders the permutation");
	check(p[0] == 1 && p[1] == 2 && p[2] == 0, sys->sprint("sort permutation %d %d %d", p[0], p[1], p[2]));

	# matrices are stored by columns: A = [1 2; 3 4], B = [5 6; 7 8]
	a := array[] of {1., 3., 2., 4.};
	b := array[] of {5., 7., 6., 8.};
	c := array[] of {0., 0., 0., 0.};
	math->gemm('N', 'N', 2, 2, 2, 1., a, 2, b, 2, 0., c, 2);
	check(c[0] == 19. && c[1] == 43. && c[2] == 22. && c[3] == 50., "gemm N N");
	math->gemm('T', 'N', 2, 2, 2, 1., a, 2, b, 2, 0., c, 2);
	check(c[0] == 26. && c[1] == 38. && c[2] == 30. && c[3] == 44., "gemm T N");
	math->gemm('N', 'T', 2, 2, 2, 1., a, 2, b, 2, 0., c, 2);
	check(c[0] == 17. && c[1] == 39. && c[2] == 23. && c[3] == 53., "gemm N T");
	c = array[] of {1., 1., 1., 1.};
	math->gemm('N', 'N', 2, 2, 2, 2., a, 2, b, 2, 1., c, 2);
	check(c[0] == 39. && c[1] == 87. && c[2] == 45. && c[3] == 101., "gemm alpha and beta");
}

export()
{
	r := array[] of {1.5, -2.25, 1e300};
	b := array[8*len r] of byte;
	math->export_real(b, r);
	check(b[0] == byte 16r3F && b[1] == byte 16rF8 && b[2] == byte 0, "export_real is big-endian IEEE");
	r2 := array[len r] of real;
	math->import_real(b, r2);
	check(r2[0] == 1.5 && r2[1] == -2.25 && r2[2] == 1e300, "import_real");

	b32 := array[4*len r] of byte;
	r3 := array[] of {1.5, -2.25, 0.};
	math->export_real32(b32, r3);
	check(b32[0] == byte 16r3F && b32[1] == byte 16rC0 && b32[2] == byte 0 && b32[3] == byte 0, "export_real32");
	r4 := array[len r3] of real;
	math->import_real32(b32, r4);
	check(r4[0] == 1.5 && r4[1] == -2.25 && r4[2] == 0., "import_real32");

	iv := array[] of {1, -1, 16r12345678};
	bi := array[4*len iv] of byte;
	math->export_int(bi, iv);
	check(bi[0] == byte 0 && bi[3] == byte 1 && bi[4] == byte 16rFF && bi[8] == byte 16r12 && bi[11] == byte 16r78, "export_int is big-endian");
	iv2 := array[len iv] of int;
	math->import_int(bi, iv2);
	check(iv2[0] == 1 && iv2[1] == -1 && iv2[2] == 16r12345678, "import_int");

	check(math->realbits32(1.) == 16r3F800000, "realbits32 1.0");
	check(math->realbits64(1.) == big 16r3FF0000000000000, "realbits64 1.0");
	exact(math->bits32real(16r3F800000), 1., "bits32real");
	exact(math->bits64real(big 16r4000000000000000), 2., "bits64real");
	exact(math->bits32real(math->realbits32(-0.15625)), -0.15625, "32-bit round trip");
	exact(math->bits64real(math->realbits64(1e-200)), 1e-200, "64-bit round trip");
}

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	math = load Math Math->PATH;
	if(math == nil){
		sys->print("FAIL load Math\n");
		return;
	}
	elementary();
	bessel();
	floating();
	fpenv();
	fpfunc();
	linalg();
	export();
	sys->print("functional checks: %d failure(s)\n", bad);
	sys->print("done\n");
}
