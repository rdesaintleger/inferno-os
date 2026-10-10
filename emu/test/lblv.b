implement Lblv;
include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
	Display, Image, Rect, Point: import draw;
include "tk.m";
	tk: Tk;

# A widget's -variable and a channel variable share one namespace per toplevel
# (only checkbutton and radiobutton take -variable; a plain label rejects it).
# Covers what gcv/lot2t do not: destroying a widget whose -variable names a channel
# variable (libtk frees the variable, releasing the channel), the type clashes in
# both creation orders, and the same with the toplevel freed by the sweeper.
# usage: lblv [n]   expected: "functional checks: 0 failure(s)", then "done n=<n>"
# The "behaviour:" lines record current behaviour; they must be identical between builds.

Lblv: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

bad := 0;

check(ok: int, what: string)
{
	if(!ok){
		sys->print("FAIL %s\n", what);
		bad++;
	}
}

mem(tag: string)
{
	fd := sys->open("/dev/memory", Sys->OREAD);
	if(fd == nil)
		return;
	buf := array[1024] of byte;
	n := sys->read(fd, buf, len buf);
	if(n > 0){
		(nil, l) := sys->tokenize(string buf[0:n], "\n");
		for(; l != nil; l = tl l)
			sys->print("%s %s\n", tag, hd l);
	}
}

recv(c: chan of string, ms: int): string
{
	t := chan[1] of int;		# buffered: the timer process must not stay blocked
	spawn timer(t, ms);
	alt {
	s := <-c => return s;
	<-t => return "(timeout)";
	}
}

timer(t: chan of int, ms: int)
{
	sys->sleep(ms);
	t <-= 1;
}

# one pass; destroy: destroy the label explicitly, otherwise leave it to the collector
pass(d: ref Display, first: int, destroy: int)
{
	top := tk->toplevel(d, "");
	c := chan[4] of string;

	e := tk->cmd(top, "radiobutton .l -text hi -variable x -value a");
	check(e == "" || e == ".l", "radiobutton with a not yet existing variable: " + e);
	e = tk->namechan(top, c, "x");
	check(e == "", "namechan x: " + e);
	e = tk->cmd(top, "send x hello");
	check(e == "", "send x: " + e);
	s := recv(c, 2000);
	check(s == "hello", "message through x: " + s);

	if(destroy){
		tk->cmd(top, "destroy .l");
		e = tk->cmd(top, "send x again");
		if(first)
			sys->print("behaviour: send after destroying the radiobutton: %s\n", e);
		s = recv(c, 50);
		if(first)
			sys->print("behaviour: message after destroying the radiobutton: %s\n", s);
	}

	# type clash, other direction (the radiobutton-on-a-channel-variable direction is in clash.b)
	e = tk->cmd(top, "checkbutton .k -text k -variable y");
	check(e == "" || e == ".k", "checkbutton y: " + e);
	e = tk->namechan(top, chan[1] of string, "y");
	if(first)
		sys->print("behaviour: namechan on a string variable: %s\n", e);
	e = tk->cmd(top, "send y z");
	if(first)
		sys->print("behaviour: send to a string variable: %s\n", e);

	# replace the channel while a label names the variable
	c2 := chan[4] of string;
	e = tk->namechan(top, c2, "x");
	check(e == "", "namechan replace: " + e);
	tk->cmd(top, "send x second");
	top = nil;
	c = nil;
	c2 = nil;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	n := 200;
	if(tl argv != nil)
		n = int hd tl argv;
	d := Display.allocate(nil);
	mem("start");
	for(i := 0; i < n; i++)
		pass(d, i < 2, i % 2 == 0);	# even: widget destroyed by the script, odd: by the sweeper
	sys->sleep(4000);
	mem("end");
	sys->print("functional checks: %d failure(s)\n", bad);
	sys->print("done n=%d\n", n);
}
