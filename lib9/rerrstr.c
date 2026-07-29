#include "lib9.h"

void
rerrstr(char *buf, uint nbuf)
{
	char tmp[ERRMAX];

	tmp[0] = 0;
	errstr(tmp, sizeof tmp);
	HOSTED_API(utfecpy)(buf, buf+nbuf, tmp);
	errstr(tmp, sizeof tmp);
}
