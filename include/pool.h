/* Inferno tree allocator */

#include "inferno/hosted.h"
#include "inferno/pool.h"

extern	void	(*poolfault)(void *, char *);
extern	void	poolinit(void);
extern	ulong	poolmax(Pool*);
extern	void*	dopoolalloc(Pool*, ulong);
extern	void*	poolalloc(Pool*, ulong);
extern	void	poolfree(Pool*, void*);
extern	Bhdr*	poolchain(Pool*);
extern	int	poolcompact(Pool*);
extern	ulong	poolmsize(Pool*, void*);
extern	char*	poolname(Pool*);
extern	int	poolread(char*, int, ulong);
extern	void*	poolrealloc(Pool*, void*, ulong);
extern	int	poolsetsize(char*, int);
extern	void	poolsetcompact(Pool*, void (*)(void*, void*));
extern	char*	poolaudit(char*(*)(int, Bhdr *));
