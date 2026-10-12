load std
# covers: dev:prog
# The process device: a process appears as /prog/<pid>, can be inspected and killed.

fn check { if {~ $2 $3} {echo ok $1} {echo FAIL $1: want $2, got $3} }

# sh reports a killed background job on stderr: not part of the result
{
	sleep 100 &
	pid=$apid
	sleep 1
	check listed 1 `{ls /prog | grep '^/prog/'$pid'$' | wc -l}
	check status-file /prog/$pid/status `{ls /prog/$pid/status}
	check has-ctl /prog/$pid/ctl `{ls /prog/$pid/ctl}
	echo kill > /prog/$pid/ctl
	sleep 1
	check killed 0 `{ls /prog | grep '^/prog/'$pid'$' | wc -l}
	echo done
} >[2] /dev/null
