/* Inferno tree allocator */

#include "inferno/hosted.h"
#include "inferno/pool.h"

extern	void	(*poolfault)(void *, char *);
extern	void	poolinit(void);
extern	size_t	poolmax(Pool*);
extern	void*	dopoolalloc(Pool*, size_t);
extern	void*	poolalloc(Pool*, size_t);
extern	void	poolfree(Pool*, void*);
extern	Bhdr*	poolchain(Pool*);
extern	int	poolcompact(Pool*);
extern	size_t	poolmsize(Pool*, void*);
extern	char*	poolname(Pool*);
extern	int	poolread(char*, int, size_t);
extern	void*	poolrealloc(Pool*, void*, size_t);
extern	int	poolsetsize(char*, int);
extern	void	poolsetcompact(Pool*, void (*)(void*, void*));
extern	char*	poolaudit(char*(*)(int, Bhdr *));
