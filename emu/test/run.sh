load std

# Regression suite for emu-t.  `mk test` (emu/Linux) runs it as
#	emu-t -r$ROOT -c1 -g1024x768 /dis/sh.dis /dis/test/run.sh
# and the exit status of emu is 0 when everything passed, 1 otherwise.
# A test prints lines; the lines that start with "start " or "end " (pool sizes)
# are dropped, the others must be exactly those of <name>.exp.

bind -a '#T' /dev			# snapshot device of the headless backend
failed=

fn compare {
	name=$1
	grep -v '^(start|end) ' /tmp/$name.out > /tmp/$name.got
	if {diff /tmp/$name.got /dis/test/$name.exp > /dev/null} {
		echo PASS $name
	} {
		echo FAIL $name
		diff /tmp/$name.got /dis/test/$name.exp
		failed=$failed $name
	}
}

# t name [args]: a Limbo test, /dis/test/name.dis
fn t {
	(name args) = $*
	/dis/test/$name.dis $args > /tmp/$name.out >[2=1]
	compare $name
}

# s name: a shell script test, /dis/test/name.sh
fn s {
	name=$1
	sh /dis/test/$name.sh > /tmp/$name.out >[2=1]
	compare $name
}

t chn 200
t lblv 50
t clash
s ui						# last: leaves the window manager running

if {~ $#failed 0} {
	echo all tests passed
	echo halt 0 > /dev/sysctl
} {
	echo failed: $failed
	echo halt 1 > /dev/sysctl
}
