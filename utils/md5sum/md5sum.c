#include <lib9.h>
#include <libsec.h>

#pragma	varargck	type	"M"	uchar*

static int
digestfmt(Fmt *fmt)
{
	char buf[MD5dlen*2+1];
	uchar *p;
	int i;

	p = va_arg(fmt->args, uchar*);
	for(i=0; i<MD5dlen; i++)
		HOSTED_API(sprint)(buf+2*i, "%.2ux", p[i]);
	return HOSTED_API(fmtstrcpy)(fmt, buf);
}

static void
sum(int fd, char *name)
{
	int n;
	uchar buf[8192], digest[MD5dlen];
	DigestState *s;

	s = md5(nil, 0, nil, nil);
	while((n = read(fd, buf, sizeof buf)) > 0)
		md5(buf, n, nil, s);
	md5(nil, 0, digest, s);
	if(name == nil)
		HOSTED_API(print)("%M\n", digest);
	else
		HOSTED_API(print)("%M\t%s\n", digest, name);
}

void
main(int argc, char *argv[])
{
	int i, fd;

	ARGBEGIN{
	default:
		HOSTED_API(fprint)(2, "usage: md5sum [file...]\n");
		exits("usage");
	}ARGEND

	HOSTED_API(fmtinstall)('M', digestfmt);

	if(argc == 0)
		sum(0, nil);
	else for(i = 0; i < argc; i++){
		fd = open(argv[i], OREAD);
		if(fd < 0){
			HOSTED_API(fprint)(2, "md5sum: can't open %s: %r\n", argv[i]);
			continue;
		}
		sum(fd, argv[i]);
		close(fd);
	}
	exits(nil);
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
