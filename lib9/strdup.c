#include "lib9.h"

char*
HOSTED_API(strdup)(const char *s) 
{  
	char *os;

	os = HOSTED_API(malloc)(strlen(s) + 1);
	if(os == 0)
		return 0;
	return strcpy(os, s);
}
