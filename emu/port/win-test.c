/*
 * Graphics backend for tests: a software screen and no display server.
 *
 * The screen is a plain XRGB32 buffer that devdraw draws into (softscreen), so
 * nothing has to be flushed.  Input comes through the files the system already
 * has: keys written to /dev/keyboard, pointer state written to /dev/pointer
 * ("m x y b", see virtualpointer in devpointer.c).  The screen can be read back
 * from Limbo with Image.readpixels.  Select it with `draw win-test` in the
 * configuration file (emu/Linux/emu-t).
 */
#include "dat.h"
#include "fns.h"
#include "error.h"
#include <draw.h>
#include "cursor.h"

char*	gkscanid;		/* no raw scan codes: /dev/scancode stays unavailable */

static uchar*	screendata;
static char*	clipboard;

uchar*
attachscreen(Rectangle *r, ulong *chan, int *d, int *width, int *softscreen)
{
	Xsize &= ~0x3;		/* multiple of 4, as for the X11 backend */

	r->min.x = 0;
	r->min.y = 0;
	r->max.x = Xsize;
	r->max.y = Ysize;

	*chan = XRGB32;
	*d = 32;
	*width = Xsize;		/* in words */
	*softscreen = 1;
	virtualpointer = 1;

	if(screendata == nil){
		screendata = HOSTED_API(malloc)(Xsize*Ysize*4);
		if(screendata == nil){
			HOSTED_API(fprint)(2, "emu: cannot allocate screen buffer (%dx%d)\n", Xsize, Ysize);
			return nil;
		}
		memset(screendata, 0, Xsize*Ysize*4);
	}
	return screendata;
}

/* the pixels and the size of the screen, for devsnap.c; nil before the screen is attached */
uchar*
testscreen(int *w, int *h)
{
	*w = Xsize & ~3;
	*h = Ysize;
	return screendata;
}

void
flushmemscreen(Rectangle r)
{
	USED(&r);		/* the screen is the buffer devdraw draws into */
}

void
setpointer(int x, int y)
{
	USED(x);
	USED(y);		/* see virtualpointer */
}

void
drawcursor(Drawcursor *c)
{
	USED(c);		/* the cursor is not part of the screen contents */
}

char*
clipread(void)
{
	char *p;
	int n;

	if(clipboard == nil)
		return nil;
	n = strlen(clipboard)+1;
	p = HOSTED_API(malloc)(n);
	if(p != nil)
		memmove(p, clipboard, n);
	return p;
}

int
clipwrite(char *buf)
{
	char *p;
	int n;

	n = strlen(buf)+1;
	p = HOSTED_API(malloc)(n);
	if(p == nil)
		return 0;
	memmove(p, buf, n);
	HOSTED_API(free)(clipboard);
	clipboard = p;
	return 0;
}
