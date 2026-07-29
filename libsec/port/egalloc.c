#include "os.h"
#include <mp.h>
#include <libsec.h>

EGpub*
egpuballoc(void)
{
	EGpub *eg;

	eg = HOSTED_API(mallocz)(sizeof(*eg), 1);
	if(eg == nil)
		HOSTED_API(sysfatal)("egpuballoc");
	return eg;
}

void
egpubfree(EGpub *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->p);
	mpfree(eg->alpha);
	mpfree(eg->key);
	HOSTED_API(free)(eg);
}


EGpriv*
egprivalloc(void)
{
	EGpriv *eg;

	eg = HOSTED_API(mallocz)(sizeof(*eg), 1);
	if(eg == nil)
		HOSTED_API(sysfatal)("egprivalloc");
	return eg;
}

void
egprivfree(EGpriv *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->pub.p);
	mpfree(eg->pub.alpha);
	mpfree(eg->pub.key);
	mpfree(eg->secret);
	HOSTED_API(free)(eg);
}

EGsig*
egsigalloc(void)
{
	EGsig *eg;

	eg = HOSTED_API(mallocz)(sizeof(*eg), 1);
	if(eg == nil)
		HOSTED_API(sysfatal)("egsigalloc");
	return eg;
}

void
egsigfree(EGsig *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->r);
	mpfree(eg->s);
	HOSTED_API(free)(eg);
}
