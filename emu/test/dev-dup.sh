load std
# covers: dev:dup
# The dup device: one file per open descriptor of the process and its ctl file.

fn check { if {~ $2 $3} {echo ok $1} {echo FAIL $1: want $2, got $3} }

bind '#d' /fd
check fd0 /fd/0 `{ls /fd/0}
check fd1 /fd/1 `{ls /fd/1}
check fd2 /fd/2 `{ls /fd/2}
check ctl0 /fd/0ctl `{ls /fd/0ctl}
check missing 0 `{ls /fd | grep '^/fd/99$' | wc -l}
# writing to /fd/1 writes to standard output, which here is captured by the test runner
echo -n 'through-dup' > /fd/1
echo
echo done
