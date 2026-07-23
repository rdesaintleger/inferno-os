#include <lib9.h>
static char t16e[] = "0123456789ABCDEF";

int
enc16(char *out, int lim, uchar *in, int n)
{
	uint c;
	char *eout = out + lim;
	char *start = out;

	while(n-- > 0){
		c = *in++;
		if(out + 2 >= eout)
			goto exhausted;
		*out++ = t16e[c>>4];
		*out++ = t16e[c&0xf];
	}
exhausted:
	*out = 0;
	return out - start;
}
