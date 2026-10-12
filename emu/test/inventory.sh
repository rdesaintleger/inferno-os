#!/bin/sh
# What the regression tests are supposed to cover, derived from the sources.
# usage: inventory.sh functions|devices        (functions needs libinterp/runt.h: build first)
#   functions: every function of the modules built into the emulator (Sys.open, Image.draw, ...)
#   devices:   every device of the emu-t configuration, as dev:<name>
ROOT=${ROOT:-../..}
case "$1" in
functions)
	grep -ho '^typedef struct F_[A-Za-z0-9]*_[A-Za-z0-9_]* ' $ROOT/libinterp/runt.h |
		sed 's/typedef struct F_//; s/ $//; s/_/./' | sort -u
	;;
devices)
	awk '/^[^\t]/ {sec=$1; next} sec=="dev" && NF>0 {print "dev:" $1}' $ROOT/emu/Linux/emu-t | sort -u
	;;
*)
	echo 'usage: inventory.sh functions|devices' >&2
	exit 1
	;;
esac
