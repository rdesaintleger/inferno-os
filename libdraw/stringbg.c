#include "lib9.h"
#include "draw.h"

Point
stringnbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst->clipr, bg, bgp, op);
}

Point
runestringnbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst->clipr, bg, bgp, op);
}
