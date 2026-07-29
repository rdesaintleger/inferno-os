#include "lib9.h"
#include "draw.h"
#include "kernel.h"

static char*
skip(char *s)
{
	while(*s==' ' || *s=='\n' || *s=='\t')
		s++;
	return s;
}

Font*
buildfont(Display *d, char *buf, char *name)
{
	Font *fnt;
	Cachefont *c;
	char *s, *t;
	ulong min, max;
	int offset;
	char badform[] = "bad font format: number expected (char position %d)";

	s = buf;
	fnt = HOSTED_API(malloc)(sizeof(Font));
	if(fnt == 0)
		return 0;
	memset(fnt, 0, sizeof(Font));
	fnt->display = d;
	fnt->name = HOSTED_API(strdup)(name);
	fnt->ncache = NFCACHE+NFLOOK;
	fnt->nsubf = NFSUBF;
	fnt->cache = HOSTED_API(malloc)(fnt->ncache * sizeof(fnt->cache[0]));
	fnt->subf = HOSTED_API(malloc)(fnt->nsubf * sizeof(fnt->subf[0]));
	if(fnt->name==0 || fnt->cache==0 || fnt->subf==0){
    Err2:
		HOSTED_API(free)(fnt->name);
		HOSTED_API(free)(fnt->cache);
		HOSTED_API(free)(fnt->subf);
		HOSTED_API(free)(fnt->sub);
		HOSTED_API(free)(fnt);
		return 0;
	}
	fnt->height = strtol(s, &s, 0);
	s = skip(s);
	fnt->ascent = strtol(s, &s, 0);
	s = skip(s);
	if(fnt->height<=0 || fnt->ascent<=0){
		kwerrstr("bad height or ascent in font file");
		goto Err2;
	}
	fnt->width = 0;
	fnt->nsub = 0;
	fnt->sub = 0;

	memset(fnt->subf, 0, fnt->nsubf * sizeof(fnt->subf[0]));
	memset(fnt->cache, 0, fnt->ncache*sizeof(fnt->cache[0]));
	fnt->age = 1;
	do{
		/* must be looking at a number now */
		if(*s<'0' || '9'<*s){
			kwerrstr(badform, s-buf);
			goto Err3;
		}
		min = strtol(s, &s, 0);
		s = skip(s);
		/* must be looking at a number now */
		if(*s<'0' || '9'<*s){
			kwerrstr(badform, s-buf);
			goto Err3;
		}
		max = strtol(s, &s, 0);
		s = skip(s);
		if(*s==0 || min>=Runemax || max>=Runemax || min>max){
			kwerrstr("illegal subfont range");
    Err3:
			freefont(fnt);
			return 0;
		}
		t = s;
		offset = strtol(s, &t, 0);
		if(t>s && (*t==' ' || *t=='\t' || *t=='\n'))
			s = skip(t);
		else
			offset = 0;
		fnt->sub = HOSTED_API(realloc)(fnt->sub, (fnt->nsub+1)*sizeof(Cachefont*));
		if(fnt->sub == 0){
			/* realloc manual says fnt->sub may have been destroyed */
			fnt->nsub = 0;
			goto Err3;
		}
		c = HOSTED_API(malloc)(sizeof(Cachefont));
		if(c == 0)
			goto Err3;
		fnt->sub[fnt->nsub] = c;
		c->min = min;
		c->max = max;
		c->offset = offset;
		t = s;
		while(*s && *s!=' ' && *s!='\n' && *s!='\t')
			s++;
		*s++ = 0;
		c->subfontname = 0;
		c->name = HOSTED_API(strdup)(t);
		if(c->name == 0){
			HOSTED_API(free)(c);
			goto Err3;
		}
		s = skip(s);
		fnt->nsub++;
	}while(*s);
	return fnt;
}

void
freefont(Font *f)
{
	int i;
	Cachefont *c;
	Subfont *s;

	if(f == 0)
		return;

	for(i=0; i<f->nsub; i++){
		c = f->sub[i];
		HOSTED_API(free)(c->subfontname);
		HOSTED_API(free)(c->name);
		HOSTED_API(free)(c);
	}
	for(i=0; i<f->nsubf; i++){
		s = f->subf[i].f;
/*		if(s && s!=display->defaultsubfont)*/	/* Plan 9 uses this */
		if(s)
			freesubfont(s);
	}
	freeimage(f->cacheimage);
	HOSTED_API(free)(f->name);
	HOSTED_API(free)(f->cache);
	HOSTED_API(free)(f->subf);
	HOSTED_API(free)(f->sub);
	HOSTED_API(free)(f);
}
