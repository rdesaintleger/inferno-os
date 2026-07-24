# Directories common to all architectures.
# Build in order:
#	- critical libraries used by the limbo compiler
#	- the limbo compiler (used to build some subsequent libraries)
#	- the remaining libraries
#	- commands
#	- utilities

EMUDIRS=\
	lib9\
	libbio\
	libmp\
	libsec\
	libmath\
	utils/iyacc\
	limbo\
	libinterp\
	libkeyring\
	libdraw\
	libtk\
	libfreetype\
	libmemdraw\
	libmemlayer\
	utils/data2c\
	utils/ndate\
	emu\

<mkconfig

DIRS=\
	$EMUDIRS\
#	appl\

foo:QV:
	echo mk all, clean, install, installall or nuke

all:V:		all-$HOSTMODEL
clean:V:	clean-$HOSTMODEL
install:V:	install-$HOSTMODEL
installall:V:	installall-$HOSTMODEL
emu:V:	emu/all-$HOSTMODEL
emuinstall:V:	emu/install-$HOSTMODEL
emuclean:V:	emu/clean-$HOSTMODEL
emunuke:V:	emu/nuke-$HOSTMODEL
nuke:V:		nuke-$HOSTMODEL

cleandist:V: clean
	rm -f $ROOT/$OBJDIR/lib/lib*.a

nukedist:V: nuke
	rm -f $ROOT/$OBJDIR/bin/*.exe
	rm -f $ROOT/$OBJDIR/lib/lib*.a
	
&-Posix:QV:
	for j in $DIRS utils tools
	do
		echo "(cd $j; mk $MKFLAGS $stem)"
		(cd $j; mk $MKFLAGS $stem) || exit 1
	done

emu/&-Posix:QV:
	for j in $EMUDIRS
	do
		echo "(cd $j; mk $MKFLAGS $stem)"
		(cd $j; mk $MKFLAGS $stem) || exit 1
	done

# Convenience targets

Linux-% linux-%:V:
	mk 'SYSHOST=Linux' 'OBJTYPE=386' $stem

mkdirs:V:	mkdirs-$SHELLTYPE

mkdirs-rc:V:
	mkdir -p `{cat lib/emptydirs}
	chmod 555 mnt/* n/client/* n/*

mkdirs-sh:V:
	mkdir -p `cat lib/emptydirs`
	chmod 555 mnt/* n/client/* n/*
