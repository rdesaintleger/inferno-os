#include	<lib9.h>

void
main(void)
{
	ulong t;

	t = time(0);
	print("%lud\n", t);
	exits(0);
}

void *HOSTED_API(malloc)(size_t size) {
    return malloc(size);
}

void HOSTED_API(free)(void *ptr) {
    free(ptr);
}

void *HOSTED_API(calloc)(size_t n, size_t szelem) {
    return calloc(n, szelem);
}
