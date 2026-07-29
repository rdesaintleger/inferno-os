#include	"mk.h"

/* table-driven version in bootes dump of 12/31/96 */

long
mtime(char *name)
{
	return mkmtime(name);
}

long
timeof(char *name, int force)
{
	Symtab *sym;
	long t;

	if(HOSTED_API(utfrune)(name, '('))
		return atimeof(force, name);	/* archive */

	if(force)
		return mtime(name);


	sym = symlook(name, S_TIME, 0);
	if (sym)
		return (long) sym->value;		/* uggh */

	t = mtime(name);
	if(t == 0)
		return 0;

	symlook(name, S_TIME, (void*)t);		/* install time in cache */
	return t;
}

void
touch(char *name)
{
	Bprint(&bout, "touch(%s)\n", name);
	if(nflag)
		return;

	if(HOSTED_API(utfrune)(name, '('))
		atouch(name);		/* archive */
	else if(chgtime(name) < 0) {
		perror(name);
		Exit();
	}
}

void
delete(char *name)
{
	if(HOSTED_API(utfrune)(name, '(') == 0) {		/* file */
		if(remove(name) < 0)
			perror(name);
	} else
		HOSTED_API(fprint)(2, "hoon off; mk can'tdelete archive members\n");
}

void
timeinit(char *s)
{
	long t;
	char *cp;
	Rune r;
	int c, n;

	t = time(0);
	while (*s) {
		cp = s;
		do{
			n = HOSTED_API(chartorune)(&r, s);
			if (r == ' ' || r == ',' || r == '\n')
				break;
			s += n;
		} while(*s);
		c = *s;
		*s = 0;
		symlook(HOSTED_API(strdup)(cp), S_TIME, (void *)t)->value = (void *)t;
		if (c)
			*s++ = c;
		while(*s){
			n = HOSTED_API(chartorune)(&r, s);
			if(r != ' ' && r != ',' && r != '\n')
				break;
			s += n;
		}
	}
}
