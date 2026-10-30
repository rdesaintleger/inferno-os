#!/bin/sh
# Builds and runs the tests of the synchronization API on every backend.
# Called by `mk test`; OBJTYPE=386 builds 32 bits.
cd `dirname $0` || exit 1
CC=${SYNCCC:-cc}
case "$OBJTYPE" in
386)	CC="$CC -m32";;
esac
FLAGS="-std=c99 -Wall -Wextra -pedantic -g -O1 -D_POSIX_C_SOURCE=200809L -I../../include"
out=${TMPDIR:-/tmp}/synctest.$$
trap 'rm -f $out.*' 0
rc=0
echo "== backend pthread"
$CC $FLAGS -DBACKEND_PTHREAD -o $out.pthread test.c lock-pthread.c -lpthread || exit 1
$out.pthread || rc=1
exit $rc
