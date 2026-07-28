#include "lib9.h"

int
HOSTED_API(utfnlen)(char *s, long m)
{
	int c;
	long n;
	Rune rune;
	char *es;

	es = s + m;
	for(n = 0; s < es; n++) {
		c = *(uchar*)s;
		if(c < Runeself){
			if(c == '\0')
				break;
			s++;
			continue;
		}
		if(!HOSTED_API(fullrune)(s, es-s))
			break;
		s += HOSTED_API(chartorune)(&rune, s);
	}
	return n;
}
