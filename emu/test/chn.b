implement Chn;
include "sys.m";
	sys: Sys;
include "draw.m";

# Channel semantics that the other tests do not reach: buffered ring wrap-around,
# alt on buffered channels, buffered pointer elements abandoned with items left,
# reference cycles through a channel buffer (only the sweeper can free them),
# and blocked senders/receivers killed with the channel still referenced.
# usage: chn [n]   expected: "functional checks: 0 failure(s)", then "done n=<n>"

Chn: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

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

# front/size of the ring wrap many times; order and values must be preserved
ring()
{
	c := chan[3] of int;
	next := 0;
	want := 0;
	n := 0;
	for(round := 0; round < 200; round++){
		while(n < 3){
			c <-= next++;
			n++;
		}
		for(k := 0; k < 2; k++){
			v := <-c;
			check(v == want, sys->sprint("ring order: got %d want %d", v, want));
			want++;
			n--;
		}
	}
	while(n > 0){
		v := <-c;
		check(v == want, sys->sprint("ring drain: got %d want %d", v, want));
		want++;
		n--;
	}
}

# alt must only pick a send when there is room and a receive when there is data
# (Dis forbids a send and a receive on the same channel in one alt)
altbuf()
{
	empty := chan[2] of int;
	room := chan[2] of int;
	got := 0;
	alt {
	<-empty =>
		check(0, "alt: receive chosen on an empty buffered channel");
	room <-= 1 =>
		got = 1;
	}
	check(got == 1, "alt: send not chosen when there is room");
	check((<-room) == 1, "alt: item sent through alt");

	full := chan[2] of int;
	data := chan[2] of int;
	full <-= 7;
	full <-= 8;
	data <-= 5;
	v := -1;
	alt {
	full <-= 3 =>
		check(0, "alt: send chosen on a full buffered channel");
	v = <-data =>
		;
	}
	check(v == 5, sys->sprint("alt: received %d, want 5", v));
	check((<-full) == 7 && (<-full) == 8, "alt: full channel content");

	# two buffered channels, only one has data
	a := chan[1] of string;
	b := chan[1] of string;
	b <-= "b";
	s := "";
	alt {
	s = <-a =>
		check(0, "alt: received from the empty channel");
	s = <-b =>
		;
	}
	check(s == "b", "alt: two channels");
}

# pointer elements left in the buffer when the channel is dropped
leftovers(n: int)
{
	for(i := 0; i < n; i++){
		c := chan[4] of string;
		c <-= "one " + string i;
		c <-= "two";
		c <-= "three";
		d := chan[2] of array of byte;
		d <-= array[2048] of byte;
		d <-= array[2048] of byte;
		c = nil;
		d = nil;
	}
}

Box: adt {
	c:	chan of ref Box;
	v:	int;
};

# a box reachable only through its own channel buffer: only the sweeper can free it
cycles(n: int)
{
	for(i := 0; i < n; i++){
		b := ref Box(chan[2] of ref Box, i);
		b.c <-= b;
		b = nil;
	}
}

blocked(c: chan of int, pid: chan of int)
{
	pid <-= sys->pctl(0, nil);
	c <-= 1;			# blocks: nobody receives
}

blockedrecv(c: chan of int, pid: chan of int)
{
	pid <-= sys->pctl(0, nil);
	<-c;				# blocks: nobody sends
}

kill(pid: int)
{
	fd := sys->open("/prog/" + string pid + "/ctl", Sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}

# senders and receivers killed while queued on the channel
killed(n: int)
{
	for(i := 0; i < n; i++){
		c := chan of int;
		p := chan of int;
		spawn blocked(c, p);
		spawn blockedrecv(chan of int, p);
		kill(<-p);
		kill(<-p);
		c = nil;
	}
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	n := 1000;
	if(tl argv != nil)
		n = int hd tl argv;
	mem("start");
	ring();
	altbuf();
	leftovers(n);
	cycles(n);
	killed(n / 10);
	sys->sleep(4000);
	mem("end");
	sys->print("functional checks: %d failure(s)\n", bad);
	sys->print("done n=%d\n", n);
}
