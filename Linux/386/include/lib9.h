#ifndef _LIB9_LINUX_H_
#define _LIB9_LINUX_H_

/* define _BSD_SOURCE to use ISO C, POSIX, and 4.3BSD things. */
#define	USE_PTHREADS
#ifndef _DEFAULT_SOURCE
#define	_DEFAULT_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#define _XOPEN_SOURCE  500
#define _LARGEFILE_SOURCE	1
#define _LARGEFILE64_SOURCE	1
#define _FILE_OFFSET_BITS 64
#ifdef USE_PTHREADS
#define	_REENTRANT	1
#endif
#include <features.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#define sync __os_sync
#include <unistd.h>
#undef sync
#include <errno.h>
#define __NO_STRING_INLINES
#include <string.h>
#include "math.h"
#include <fcntl.h>
#include <setjmp.h>
#include <float.h>
#include <time.h>
#include <endian.h>

#include "inferno/protos/lib9.h"

extern	int	(*doquote)(int);

/*
 * random number
 */
extern int nrand(int);
extern	ulong	truerand(void);
extern	ulong	ntruerand(ulong);

/*
 * math
 */
extern	int	isNaN(double);
extern	double	NaN(void);
extern	int	isInf(double, int);

/*
 * Time-of-day
 */
extern	vlong	osnsec(void);
#define	nsec	osnsec
	
/*
 * one-of-a-kind
 */
extern	void	_assert(char*);
extern	char*	HOSTED_API(cleanname)(char*);
extern	int	getfields(char*, char**, int, int, char*);
extern	char*	HOSTED_API(getwd)(char*, int);
extern	double	ipow10(int);
extern	vlong	HOSTED_API(strtoll)(const char*, char**, int);
extern	void	HOSTED_API(qsort)(void*, long, long, int (*)(void*, void*));
extern	uvlong	HOSTED_API(strtoull)(const char*, char**, int);
extern	void	HOSTED_API(sysfatal)(char*, ...);
extern	int	dec64(uchar*, int, char*, int);
extern	int	enc64(char*, int, uchar*, int);
extern	int	dec32(uchar*, int, char*, int);
extern	int	enc32(char*, int, uchar*, int);
extern	int	dec16(uchar*, int, char*, int);
extern	int	enc16(char*, int, uchar*, int);
extern	int	encodefmt(Fmt*);

/*
 *  synchronization
 */
extern int	_tas(int*);

extern	void	lock(Lock*);
extern	void	unlock(Lock*);
extern	int	canlock(Lock*);

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);
extern	void	_qlockinit(ulong (*)(ulong, ulong));	/* called only by the thread library */

extern	int	canrlock(RWLock*);
extern	int	canwlock(RWLock*);
extern	void	rlock(RWLock*);
extern	void	runlock(RWLock*);
extern	void	wlock(RWLock*);
extern	void	wunlock(RWLock*);

extern	Dir*	dirstat(char*);
extern	Dir*	dirfstat(int);
extern	int	dirwstat(char*, Dir*);
extern	long	dirread(int, Dir**);
extern	void	nulldir(Dir*);
extern	long	dirreadall(int, Dir**);

extern	void	_exits(char*);

extern	void	exits(char*);
extern	int	create(char*, int, int);
extern	int	errstr(char*, uint);

extern	void	perror(const char*);
extern	long	readn(int, void*, long);
extern	int	remove(const char*);
extern	void	rerrstr(char*, uint);
extern	vlong	seek(int, vlong, int);
extern	int	segflush(void*, ulong);
extern	void	werrstr(char*, ...);

extern char *argv0;

/*
 *	Extensions for Inferno to basic libc.h
 */

extern	void	setfcr(ulong);
extern	void	setfsr(ulong);
extern	ulong	getfcr(void);
extern	ulong	getfsr(void);

#endif /* _LIB9_LINUX_H_ */
