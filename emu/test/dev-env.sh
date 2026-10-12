load std
# covers: dev:env
# The environment device: variables are files; reading, creating, appending,
# truncating, listing and removing them.

fn check { if {~ $2 $3} {echo ok $1} {echo FAIL $1: want $2, got $3} }

bind -c '#e' /env
echo -n hello > /env/TESTVAR
check create-read hello `{cat /env/TESTVAR}
echo -n world >> /env/TESTVAR
check append helloworld `{cat /env/TESTVAR}
echo -n x > /env/TESTVAR
check truncate x `{cat /env/TESTVAR}
check listed /env/TESTVAR `{ls /env/TESTVAR}
check one-entry 1 `{ls /env | grep TESTVAR | wc -l}
rm /env/TESTVAR
check removed 0 `{ls /env | grep TESTVAR | wc -l}
echo -n 'a-b' > /env/TESTVAR2
check dash a-b `{cat /env/TESTVAR2}
rm /env/TESTVAR2
echo done
