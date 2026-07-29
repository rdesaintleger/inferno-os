#include "os.h"
#include <mp.h>
#include <libsec.h>

DSApub*
dsapuballoc(void)
{
	DSApub *dsa;

	dsa = HOSTED_API(mallocz)(sizeof(*dsa), 1);
	if(dsa == nil)
		HOSTED_API(sysfatal)("dsapuballoc");
	return dsa;
}

void
dsapubfree(DSApub *dsa)
{
	if(dsa == nil)
		return;
	mpfree(dsa->p);
	mpfree(dsa->q);
	mpfree(dsa->alpha);
	mpfree(dsa->key);
	HOSTED_API(free)(dsa);
}


DSApriv*
dsaprivalloc(void)
{
	DSApriv *dsa;

	dsa = HOSTED_API(mallocz)(sizeof(*dsa), 1);
	if(dsa == nil)
		HOSTED_API(sysfatal)("dsaprivalloc");
	return dsa;
}

void
dsaprivfree(DSApriv *dsa)
{
	if(dsa == nil)
		return;
	mpfree(dsa->pub.p);
	mpfree(dsa->pub.q);
	mpfree(dsa->pub.alpha);
	mpfree(dsa->pub.key);
	mpfree(dsa->secret);
	HOSTED_API(free)(dsa);
}

DSAsig*
dsasigalloc(void)
{
	DSAsig *dsa;

	dsa = HOSTED_API(mallocz)(sizeof(*dsa), 1);
	if(dsa == nil)
		HOSTED_API(sysfatal)("dsasigalloc");
	return dsa;
}

void
dsasigfree(DSAsig *dsa)
{
	if(dsa == nil)
		return;
	mpfree(dsa->r);
	mpfree(dsa->s);
	HOSTED_API(free)(dsa);
}
