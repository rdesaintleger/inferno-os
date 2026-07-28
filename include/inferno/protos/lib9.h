#ifndef _LIB9_PROTOS_H_
#define _LIB9_PROTOS_H_

#include "inferno/hosted.h"

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

#endif /* _LIB9_PROTOS_H_ */