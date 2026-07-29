#ifndef _LIB9_PROTOS_H_
#define _LIB9_PROTOS_H_

#include "inferno/lib9.h"

extern char* HOSTED_API(strecpy)(char*, char*, char*);
extern char* HOSTED_API(strdup)(const char*);
extern int HOSTED_API(tokenize)(char*, char**, int);

/*
 * rune routines
 */
extern	int	HOSTED_API(runetochar)(char*, Rune*);
extern	int	HOSTED_API(chartorune)(Rune*, char*);
extern	int	HOSTED_API(runelen)(long);
extern	int	HOSTED_API(runenlen)(Rune*, int);
extern	int	HOSTED_API(fullrune)(char*, int);
extern	int	HOSTED_API(utflen)(char*);
extern	int	HOSTED_API(utfnlen)(char*, long);
extern	char*HOSTED_API(utfrune)(char*, long);
extern	char*HOSTED_API(utfrrune)(char*, long);
extern	char*	HOSTED_API(utfutf)(char*, char*);
extern	char*	HOSTED_API(utfecpy)(char*, char*, char*);

/*
 * malloc
 */
extern	void*	HOSTED_API(malloc)(size_t);
extern	void*	HOSTED_API(mallocz)(ulong, int);
extern	void	HOSTED_API(free)(void*);
extern	ulong	HOSTED_API(msize)(void*);
extern	void*	HOSTED_API(calloc)(size_t, size_t);
extern	void*	HOSTED_API(realloc)(void*, size_t);

/*
 * print routines
 */
extern	int	HOSTED_API(print)(char*, ...);
extern	char*	HOSTED_API(seprint)(char*, char*, char*, ...);
extern	char*	HOSTED_API(vseprint)(char*, char*, char*, va_list);
extern	int	HOSTED_API(snprint)(char*, int, char*, ...);
extern	int	HOSTED_API(vsnprint)(char*, int, char*, va_list);
extern	char*	HOSTED_API(smprint)(char*, ...);
extern	char*	HOSTED_API(vsmprint)(char*, va_list);
extern	int	HOSTED_API(sprint)(char*, char*, ...);
extern	int	HOSTED_API(fprint)(int, char*, ...);
extern	int	HOSTED_API(vfprint)(int, char*, va_list);

#endif /* _LIB9_PROTOS_H_ */