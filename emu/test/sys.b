implement Systest;
include "sys.m";
	sys: Sys;
include "draw.m";

# Sys, the part that needs no network: files, directories, name space, pipes,
# formatting, strings and UTF, time, errors.
# covers: Sys.create Sys.open Sys.write Sys.read Sys.readn Sys.seek Sys.pread Sys.pwrite
# covers: Sys.stat Sys.fstat Sys.wstat Sys.fwstat Sys.fd2path Sys.dup Sys.fildes Sys.iounit
# covers: Sys.dirread Sys.remove Sys.chdir Sys.bind Sys.unmount Sys.pipe Sys.stream
# covers: Sys.sprint Sys.aprint Sys.print Sys.fprint Sys.tokenize
# covers: Sys.byte2char Sys.char2byte Sys.utfbytes Sys.millisec Sys.sleep Sys.werrstr Sys.pctl

Systest: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

bad := 0;
dir := "/tmp/systest";

check(ok: int, what: string)
{
	if(!ok){
		sys->print("FAIL %s\n", what);
		bad++;
	}
}

errstr(fd: ref Sys->FD): string
{
	if(fd != nil)
		return "ok";
	return sys->sprint("%r");
}

readall(fd: ref Sys->FD): string
{
	s := "";
	buf := array[64] of byte;
	while((n := sys->read(fd, buf, len buf)) > 0)
		s += string buf[0:n];
	return s;
}

names(fd: ref Sys->FD): string
{
	l: list of string;
	for(;;){
		(n, d) := sys->dirread(fd);
		if(n <= 0)
			break;
		for(i := 0; i < n; i++)
			l = d[i].name :: l;
	}
	# sorted
	s := "";
	for(;;){
		min := "";
		for(t := l; t != nil; t = tl t)
			if(min == "" || hd t < min)
				min = hd t;
		if(min == "")
			break;
		s += min + " ";
		r: list of string;
		for(t = l; t != nil; t = tl t)
			if(hd t != min)
				r = hd t :: r;
		l = r;
	}
	return s;
}

fmt()
{
	check(sys->sprint("%d-%s-%c-%x", 12, "ab", 'z', 255) == "12-ab-z-ff", "sprint ints, strings, chars");
	check(sys->sprint("%5.2f|%-4d|%04d", 3.14159, 7, 42) == " 3.14|7   |0042", "sprint widths");
	check(sys->sprint("%bd", big 9) == "9", "sprint big");
	a := sys->aprint("%s%d", "x", 7);
	check(len a == 2 && int a[0] == 'x' && int a[1] == '7', "aprint");
	check(sys->fprint(sys->fildes(1), "") == 0, "fprint nothing");
	check(sys->print("") == 0, "print nothing");
}

strings()
{
	(n, l) := sys->tokenize("a b  c", " ");
	check(n == 3 && hd l == "a" && hd tl l == "b" && hd tl tl l == "c", "tokenize spaces");
	(n, l) = sys->tokenize("a:b,c", ":,");
	check(n == 3, "tokenize two delimiters");
	(n, l) = sys->tokenize("", " ");
	check(n == 0 && l == nil, "tokenize empty");

	buf := array[8] of byte;
	check(sys->char2byte('a', buf, 0) == 1 && buf[0] == byte 'a', "char2byte ascii");
	check(sys->char2byte(16r263A, buf, 0) == 3, "char2byte 3 bytes");
	check(buf[0] == byte 16rE2 && buf[1] == byte 16r98 && buf[2] == byte 16rBA, "char2byte bytes");
	(c, nb, st) := sys->byte2char(buf, 0);
	check(c == 16r263A && nb == 3 && st != 0, sys->sprint("byte2char: %x %d %d", c, nb, st));
	check(sys->utfbytes(buf, 3) == 3, "utfbytes complete");
	check(sys->utfbytes(buf, 2) == 0, "utfbytes incomplete");
}

errors()
{
	sys->werrstr("custom error");
	check(sys->sprint("%r") == "custom error", "werrstr / %r");
	check(sys->open("/no/such/file", Sys->OREAD) == nil, "open of a missing file fails");
	check(sys->sprint("%r") != "custom error", "the error message changed");
}

timing()
{
	t0 := sys->millisec();
	sys->sleep(60);
	t1 := sys->millisec();
	check(t1 - t0 >= 50, sys->sprint("sleep 60 took %d ms", t1 - t0));
	check(sys->pctl(0, nil) > 0, "pctl returns the pid");
}

files()
{
	sys->remove(dir + "/b");
	sys->remove(dir + "/a");
	fd := sys->create(dir + "/a", Sys->ORDWR, 8r644);
	check(fd != nil, "create a: " + sys->sprint("%r"));
	if(fd == nil)
		return;
	check(sys->write(fd, array of byte "hello world", 11) == 11, "write");
	check(sys->seek(fd, big 0, Sys->SEEKSTART) == big 0, "seek start");
	buf := array[32] of byte;
	n := sys->read(fd, buf, len buf);
	check(n == 11 && string buf[0:n] == "hello world", "read back");
	check(sys->seek(fd, big 6, Sys->SEEKSTART) == big 6, "seek 6");
	n = sys->read(fd, buf, 5);
	check(n == 5 && string buf[0:n] == "world", "read after seek");
	check(sys->seek(fd, big -5, Sys->SEEKEND) == big 6, "seek from end");
	check(sys->seek(fd, big 2, Sys->SEEKRELA) == big 8, "seek relative");
	n = sys->pread(fd, buf, 5, big 0);
	check(n == 5 && string buf[0:n] == "hello", "pread");
	check(sys->pwrite(fd, array of byte "HELLO", 5, big 0) == 5, "pwrite");
	check(sys->seek(fd, big 0, Sys->SEEKSTART) == big 0, "seek 0");
	n = sys->readn(fd, buf, 100);
	check(n == 11 && string buf[0:n] == "HELLO world", "readn returns what the file has");
	check(sys->iounit(fd) >= 0, "iounit");
	check(sys->fd2path(fd) == dir + "/a", "fd2path: " + sys->fd2path(fd));

	(ok, d) := sys->stat(dir + "/a");
	check(ok == 0 && d.name == "a" && d.length == big 11, "stat name and length");
	check((d.mode & 8r777) == 8r644, sys->sprint("stat mode %o", d.mode & 8r777));
	check((d.mode & Sys->DMDIR) == 0, "stat: not a directory");
	(ok, d) = sys->fstat(fd);
	check(ok == 0 && d.length == big 11, "fstat");

	nd := sys->nulldir;
	nd.mode = 8r600;
	check(sys->fwstat(fd, nd) == 0, "fwstat");
	(ok, d) = sys->stat(dir + "/a");
	check((d.mode & 8r777) == 8r600, "mode changed by fwstat");
	nd = sys->nulldir;
	nd.name = "b";
	check(sys->wstat(dir + "/a", nd) == 0, "wstat rename");
	(ok, d) = sys->stat(dir + "/b");
	check(ok == 0 && d.name == "b", "renamed file exists");
	(ok, d) = sys->stat(dir + "/a");
	check(ok < 0, "old name is gone");

	# dup and fildes: a second descriptor on the same file
	fd2 := sys->open(dir + "/b", Sys->OREAD);
	check(fd2 != nil, "open b");
	n2 := sys->dup(fd2.fd, -1);
	check(n2 >= 0, "dup");
	fd3 := sys->fildes(n2);
	check(fd3 != nil && readall(fd3) == "HELLO world", "dup'ed descriptor reads the file");
	fd = nil;
	fd2 = nil;
	fd3 = nil;
}

dirs()
{
	sys->remove(dir + "/d/f");
	sys->remove(dir + "/d");
	d := sys->create(dir + "/d", Sys->OREAD, Sys->DMDIR | 8r755);
	check(d != nil, "mkdir d");
	(ok, st) := sys->stat(dir + "/d");
	check(ok == 0 && (st.mode & Sys->DMDIR) != 0, "d is a directory");
	f := sys->create(dir + "/d/f", Sys->OWRITE, 8r644);
	check(f != nil, "create d/f");
	f = nil;
	dd := sys->open(dir, Sys->OREAD);
	check(names(dd) == "b d ", "dirread: " + names(sys->open(dir, Sys->OREAD)));
	dd = nil;

	# chdir and relative names
	check(sys->chdir(dir + "/d") == 0, "chdir");
	check(sys->open("f", Sys->OREAD) != nil, "relative open after chdir");
	check(sys->fd2path(sys->open(".", Sys->OREAD)) == dir + "/d", "fd2path of .");
	sys->chdir("/");

	# bind: a directory seen at another place, then unmounted
	sys->create(dir + "/u", Sys->OREAD, Sys->DMDIR | 8r755);
	check(sys->bind(dir + "/d", dir + "/u", Sys->MBEFORE) >= 0, "bind before");
	check(sys->open(dir + "/u/f", Sys->OREAD) != nil, "file visible through the bind");
	check(sys->unmount(dir + "/d", dir + "/u") == 0, "unmount");
	check(sys->open(dir + "/u/f", Sys->OREAD) == nil, "file no longer visible");
	# Plan 9 would let us create in u again; here the union leaves a mark (also at d3495cc)
	f = sys->create(dir + "/u/g", Sys->OWRITE, 8r644);
	sys->print("behaviour: create in a directory after bind+unmount: %s\n", errstr(f));
	sys->remove(dir + "/u");

	check(sys->remove(dir + "/d/f") == 0, "remove file");
	check(sys->remove(dir + "/d") == 0, "remove directory");
	(ok, st) = sys->stat(dir + "/d");
	check(ok < 0, "directory is gone");
}

pipes()
{
	p := array[2] of ref Sys->FD;
	check(sys->pipe(p) == 0, "pipe");
	check(sys->write(p[0], array of byte "ping", 4) == 4, "write to a pipe");
	buf := array[16] of byte;
	n := sys->read(p[1], buf, len buf);
	check(n == 4 && string buf[0:n] == "ping", "read from the other end");
	check(sys->write(p[1], array of byte "pong", 4) == 4, "write the other way");
	n = sys->read(p[0], buf, len buf);
	check(n == 4 && string buf[0:n] == "pong", "read the other way");

	# stream copies until end of file
	src := sys->open(dir + "/b", Sys->OREAD);
	dst := sys->create(dir + "/c", Sys->OWRITE, 8r644);
	check(src != nil && dst != nil, sys->sprint("open for stream: %r"));
	check(sys->stream(src, dst, 4) == 11, "stream copies 11 bytes");
	dst = nil;
	check(readall(sys->open(dir + "/c", Sys->OREAD)) == "HELLO world", "streamed content");
	sys->remove(dir + "/c");
}

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	d := sys->create(dir, Sys->OREAD, Sys->DMDIR | 8r755);
	if(d == nil && sys->stat(dir).t0 < 0){
		sys->print("FAIL cannot create %s: %r\n", dir);
		return;
	}
	d = nil;
	fmt();
	strings();
	errors();
	timing();
	files();
	dirs();
	pipes();
	sys->remove(dir + "/b");
	sys->remove(dir);
	sys->print("functional checks: %d failure(s)\n", bad);
	sys->print("done\n");
}
