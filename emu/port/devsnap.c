/*
 * Screen snapshot, for tests with the win-test backend (emu/Linux/emu-t).
 *
 * #T/snapshot.ppm is the current contents of the software screen as a binary
 * PPM (P6), so that a shell script can do
 *	bind -a '#T' /dev
 *	cp /dev/snapshot.ppm /tmp/shot.ppm
 *	md5sum /dev/snapshot.ppm
 * The file is read-only.  Reading it before anything has used /dev/draw
 * (no screen yet) fails.
 */
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

extern	uchar*	testscreen(int*, int*);	/* win-test.c: pixels (XRGB32, little endian) and size */

enum{
	Qdir,
	Qppm,
};

static Dirtab snaptab[]={
	".",		{Qdir, 0, QTDIR},	0,	0555,
	"snapshot.ppm",	{Qppm},			0,	0444,
};

static int
snapheader(char *buf, int n, int w, int h)
{
	return HOSTED_API(snprint)(buf, n, "P6\n%d %d\n255\n", w, h);
}

static void
snapinit(void)
{
	char hdr[64];
	int w, h;

	w = Xsize & ~3;
	h = Ysize;
	snaptab[Qppm].length = snapheader(hdr, sizeof(hdr), w, h) + 3*w*h;
}

static Chan*
snapattach(char *spec)
{
	return devattach('T', spec);
}

static Walkqid*
snapwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, snaptab, nelem(snaptab), devgen);
}

static int
snapstat(Chan *c, uchar *db, int n)
{
	return devstat(c, db, n, snaptab, nelem(snaptab), devgen);
}

static Chan*
snapopen(Chan *c, int omode)
{
	return devopen(c, omode, snaptab, nelem(snaptab), devgen);
}

static void
snapclose(Chan *c)
{
	USED(c);
}

static long
snapread(Chan *c, void *a, long n, vlong off)
{
	uchar *screen, *p;
	char hdr[64];
	int w, h, hl;
	vlong total, pos, d;
	long i;

	if(c->qid.type & QTDIR)
		return devdirread(c, a, n, snaptab, nelem(snaptab), devgen);

	screen = testscreen(&w, &h);
	if(screen == nil)
		error("no screen");
	hl = snapheader(hdr, sizeof(hdr), w, h);
	total = hl + 3*(vlong)w*h;
	if(off >= total)
		return 0;
	if(off+n > total)
		n = total-off;
	p = a;
	for(i = 0; i < n; i++){
		pos = off+i;
		if(pos < hl)
			p[i] = hdr[pos];
		else{
			d = pos-hl;
			p[i] = screen[(d/3)*4 + 2 - d%3];	/* XRGB32 in memory is B G R X */
		}
	}
	return n;
}

static long
snapwrite(Chan *c, void *a, long n, vlong off)
{
	USED(c);
	USED(a);
	USED(n);
	USED(off);
	error(Eperm);
	return 0;
}

Dev snapdevtab = {
	'T',
	"snap",

	snapinit,
	snapattach,
	snapwalk,
	snapstat,
	snapopen,
	devcreate,
	snapclose,
	snapread,
	devbread,
	snapwrite,
	devbwrite,
	devremove,
	devwstat,
};
