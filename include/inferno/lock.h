#ifndef _INFERNO_LOCK_H_
#define _INFERNO_LOCK_H_

/*
 *  synchronization
 */
typedef
struct Lock {
	int	val;
	int	pid;
} Lock;

typedef struct QLock QLock;
struct QLock
{
	Lock	use;			/* to access Qlock structure */
	struct Proc	*head;			/* next process waiting for object */
	struct Proc	*tail;			/* last process waiting for object */
	int	locked;			/* flag */
};

typedef
struct RWLock
{
	Lock	l;			/* Lock modify lock */
	QLock	x;			/* Mutual exclusion lock */
	QLock	k;			/* Lock for waiting writers */
	int	readers;		/* Count of readers in lock */
} RWLock;

extern	void	lock(Lock*);
extern	void	unlock(Lock*);
extern	int	canlock(Lock*);

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);

extern	int	canrlock(RWLock*);
extern	int	canwlock(RWLock*);
extern	void	rlock(RWLock*);
extern	void	runlock(RWLock*);
extern	void	wlock(RWLock*);
extern	void	wunlock(RWLock*);

#endif /* _INFERNO_LOCK_H_ */
