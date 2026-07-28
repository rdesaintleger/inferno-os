#include "lib9.h"
#include <unistd.h>

char *
HOSTED_API(getwd)(char *buf, int size)
{
	return getcwd(buf, size);
}
