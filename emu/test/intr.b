implement Intr;
include "sys.m";
	sys: Sys;
include "draw.m";

# A process blocked in a host call (a read of the output of a host command, here)
# must stop when it is killed, even when the kill arrives right after the call was
# announced and before the host call is entered: the signal that interrupts the
# call used to be lost then, and the process stayed in the call.  The hooks of #T
# open that window (/dev/osenterdelay) and tell how many processes are in a host
# call (/dev/hostcalls); the Prog itself disappears at once, whatever its thread does.
# usage: intr [n]   expected: "intr: 0 of n killed processes stayed blocked"

Intr: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

reader(fd: ref Sys->FD, pidc: chan of int, go: chan of int)
{
	pidc <-= sys->pctl(0, nil);
	<-go;
	buf := array[64] of byte;
	sys->read(fd, buf, len buf);
}

# a host command that outputs nothing for a long while: /cmd/<n>/data.  The
# command is killed when its ctl file is closed, so the caller keeps ctl open.
hostcmd(): (ref Sys->FD, ref Sys->FD)
{
	ctl := sys->open("/cmd/clone", Sys->ORDWR);
	if(ctl == nil)
		return (nil, nil);
	buf := array[32] of byte;
	k := sys->read(ctl, buf, len buf);
	if(k <= 0)
		return (nil, nil);
	dir := "/cmd/" + string buf[0:k];
	if(sys->fprint(ctl, "exec sleep 20") < 0)
		return (nil, nil);
	return (ctl, sys->open(dir + "/data", Sys->OREAD));
}

hostcalls(): int
{
	fd := sys->open("/dev/hostcalls", Sys->OREAD);
	if(fd == nil)
		return -1;
	buf := array[32] of byte;
	k := sys->read(fd, buf, len buf);
	if(k <= 0)
		return -1;
	return int string buf[0:k];
}

# the osenters of the Prog pid wait ms
setdelay(pid, ms: int)
{
	fd := sys->open("/dev/osenterdelay", Sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "%d %d", pid, ms);
}

# wait until the number of processes in a host call is at most n (at most about s seconds)
settle(n: int, s: int): int
{
	for(i := 0; i < s*50; i++){
		if(hostcalls() <= n)
			return 1;
		sys->sleep(20);
	}
	return 0;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	sys->bind("#C", "/", Sys->MAFTER);	# the host command device: /cmd
	sys->bind("#T", "/dev", Sys->MAFTER);	# the test hooks
	if(hostcalls() < 0){
		sys->print("no /dev/hostcalls\n");
		return;
	}
	n := 10;
	if(tl argv != nil)
		n = int hd tl argv;
	stuck := 0;
	for(i := 0; i < n; i++){
		(ctl, data) := hostcmd();
		if(data == nil){
			sys->print("hostcmd: %r\n");
			return;
		}
		sys->sleep(50);		# the process that waits for the command is in its host call
		h0 := hostcalls();
		pidc := chan of int;
		go := chan of int;
		spawn reader(data, pidc, go);
		pid := <-pidc;
		setdelay(pid, 20);
		go <-= 1;
		sys->sleep(5);		# inside the delay of the reader's osenter
		pfd := sys->open("/prog/" + string pid + "/ctl", Sys->OWRITE);
		if(pfd == nil){
			sys->print("open prog ctl: %r\n");
			return;
		}
		sys->fprint(pfd, "kill");
		if(!settle(h0, 1))
			stuck++;
		ctl = nil;		# kills the command, which frees a reader that was stuck
		settle(0, 3);
	}
	setdelay(0, 0);
	sys->print("intr: %d of %d killed processes stayed blocked\n", stuck, n);
}
