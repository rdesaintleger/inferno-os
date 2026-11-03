implement Prof;
include "sys.m";
	sys: Sys;
include "draw.m";

# The profilers of #P, under a process that runs: the sampler (start) and the memory
# profiler (startmp12) must each make records (the entries of /prof besides ctl).
# The sampler makes a record, for a module it has not seen, from its own thread, which
# has to wait for the VM that the running process holds: it must not do it while holding
# the lock of the profile, which the VM thread takes (startmp12) at each allocation.
# covers: dev:prof
# usage: prof   expected: two lines "prof: ... made records"

Prof: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

# the records: the entries of /prof, the file ctl excluded
records(): int
{
	fd := sys->open("/prof", Sys->OREAD);
	if(fd == nil)
		return -1;
	n := 0;
	for(;;){
		(k, d) := sys->dirread(fd);
		if(k <= 0)
			break;
		for(i := 0; i < k; i++)
			if(d[i].name != "ctl")
				n++;
	}
	return n;
}

ctl(fd: ref Sys->FD, s: string): int
{
	return sys->fprint(fd, "%s", s);
}

# computation and allocation, for as long as the profiler needs
work(rounds: int): int
{
	x := 0;
	for(r := 0; r < rounds; r++){
		l: list of string;
		for(i := 0; i < 200; i++){
			a := array[64] of byte;
			a[i % 64] = byte i;
			l = string i :: l;
			x += int a[i % 64];
		}
	}
	return x;
}

# starts the profiler cmd, keeps a process busy until it made records (at most about
# two seconds), stops it and drops the records
run(fd: ref Sys->FD, cmd, what: string)
{
	if(ctl(fd, "interval 5") < 0 || ctl(fd, cmd) < 0){
		sys->print("prof: %s: cannot start (%r)\n", what);
		return;
	}
	n := 0;
	for(t := 0; t < 100 && n <= 0; t++){
		work(20);
		sys->sleep(20);
		n = records();
	}
	ctl(fd, "stop");
	ctl(fd, "end");
	if(n > 0)
		sys->print("prof: %s made records\n", what);
	else
		sys->print("prof: FAIL %s made no records\n", what);
}

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys->bind("#P", "/prof", Sys->MREPL) < 0){
		sys->print("prof: no #P\n");
		return;
	}
	fd := sys->open("/prof/ctl", Sys->OWRITE);
	if(fd == nil){
		sys->print("prof: cannot open /prof/ctl\n");
		return;
	}
	run(fd, "start", "sampler");
	run(fd, "startmp12", "memory profiler");
}
