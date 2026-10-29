#ifndef _INFERNO_LOCK_H_
#define _INFERNO_LOCK_H_

/*
 *  synchronization
 */
typedef
struct Lock {
	int	val;
} Lock;

typedef struct QLock QLock;
struct QLock
{
	Lock	use;			/* to access Qlock structure */
	struct Proc	*head;			/* next process waiting for object */
	struct Proc	*tail;			/* last process waiting for object */
	int	locked;			/* flag */
};

typedef struct RWlock RWlock;
struct RWlock
{
	Lock	l;			/* Lock modify lock */
	QLock	x;			/* Mutual exclusion lock */
	QLock	k;			/* Lock for waiting writers */
	int	readers;		/* Count of readers in lock */
};

extern	void	lock(Lock*);
extern	void	unlock(Lock*);

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);

extern	int	canrlock(RWlock*);
extern	void	rlock(RWlock*);
extern	void	runlock(RWlock*);
extern	void	wlock(RWlock*);
extern	void	wunlock(RWlock*);

#endif /* _INFERNO_LOCK_H_ */
