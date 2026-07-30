#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "mathi.h"

enum
{
	TOKI0,
	TOKI1,
	TOKI2,
	TOKI3,
	TOKSB,
	TOKFP
};
#include "tab.h"

typedef struct Addr	Addr;
struct Addr
{
	uchar	mode;
	Adr	a;
};

#pragma	varargck	type	"a"	Addr*

char*	opnam[256];
int	iconv(Fmt*);
int	aconv(Fmt*);

int
aconv(Fmt *f)
{
	Addr *a;
	char buf[64];

	a = va_arg(f->args, Addr*);
	if(a == nil)
		return HOSTED_API(fmtstrcpy)(f, "AZ");
	switch(a->mode & AMASK) {
	case AFP:	HOSTED_API(sprint)(buf, "%d(fp)", a->a.ind);	break;
	case AMP:	HOSTED_API(sprint)(buf, "%d(mp)", a->a.ind);	break;
	case AIMM:	HOSTED_API(sprint)(buf, "$%d", a->a.imm);		break;
	case AIND|AFP:	HOSTED_API(sprint)(buf, "%d(%d(fp))", a->a.i.s, a->a.i.f); break;
	case AIND|AMP:	HOSTED_API(sprint)(buf, "%d(%d(mp))", a->a.i.s, a->a.i.f); break;
	}
	return HOSTED_API(fmtstrcpy)(f, buf);
}

int
Dconv(Fmt *f)
{
	int j;
	Inst *i;
	Addr s, d;
	char buf[128];
	static int init;

	if(init == 0) {
		for(j = 0; keywds[j].name != nil; j++)
			opnam[keywds[j].op] = keywds[j].name;

		HOSTED_API(fmtinstall)('a', aconv);
		init = 1;
	}

	i = va_arg(f->args, Inst*);
	if(i == nil)
		return HOSTED_API(fmtstrcpy)(f, "IZ");

	switch(keywds[i->op].terminal) {
	case TOKI0:
		HOSTED_API(sprint)(buf, "%s", opnam[i->op]);
		break;
	case TOKI1:
		d.a = i->d;
		d.mode = UDST(i->add);
		HOSTED_API(sprint)(buf, "%s\t%a", opnam[i->op], &d);
		break;
	case TOKI3:
		d.a = i->d;
		d.mode = UDST(i->add);
		s.a = i->s;
		s.mode = USRC(i->add);
		switch(i->add&ARM) {
		default:
			HOSTED_API(sprint)(buf, "%s\t%a, %a", opnam[i->op], &s, &d);
			break;
		case AXIMM:
			HOSTED_API(sprint)(buf, "%s\t%a, $%d, %a", opnam[i->op], &s, i->reg, &d);
			break;
		case AXINF:
			HOSTED_API(sprint)(buf, "%s\t%a, %d(fp), %a", opnam[i->op], &s, i->reg, &d);
			break;
		case AXINM:
			HOSTED_API(sprint)(buf, "%s\t%a, %d(mp), %a", opnam[i->op], &s, i->reg, &d);
			break;
		}
		break;
	case TOKI2:
		d.a = i->d;
		d.mode = UDST(i->add);
		s.a = i->s;
		s.mode = USRC(i->add);
		HOSTED_API(sprint)(buf, "%s\t%a, %a", opnam[i->op], &s, &d);
		break;
	}

	return HOSTED_API(fmtstrcpy)(f, buf);
}

