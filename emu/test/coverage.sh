#!/bin/sh
# Which functions and devices have a test.  A test declares what it covers in a
# comment, anywhere in the file:   covers: Sys.open Sys.read dev:env
# usage: coverage.sh [-u]      -u lists what is not covered yet
cd `dirname $0`
all=/tmp/cov.all.$$
got=/tmp/cov.got.$$
sh inventory.sh functions > $all
sh inventory.sh devices >> $all
sort -u $all -o $all
grep -h '^[ \t]*#.*covers:' `ls *.b *.sh | grep -v '^\(coverage\|inventory\)\.sh$'` 2>/dev/null | sed 's/.*covers://' | tr ' \t' '\n\n' | grep . | sort -u > $got
n=`wc -l < $all`
c=`comm -12 $all $got | wc -l`
echo "covered: $c of $n"
unknown=`comm -13 $all $got`
[ -n "$unknown" ] && echo "declared but not in the inventory: $unknown"
[ "$1" = -u ] && comm -23 $all $got
rm -f $all $got
exit 0
