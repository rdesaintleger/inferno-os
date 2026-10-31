#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#undef _POSIX_C_SOURCE 

#include	<unistd.h>
#include	<signal.h>
#include 	<pthread.h>
#include	<limits.h>
#include	<errno.h>
#include	<semaphore.h>
#include	<sched.h>
#include	<fcntl.h>
#include	<sys/socket.h>
#include	<inferno/lock-types.h>

#ifdef __NetBSD__
#define PTHREAD_STACK_MIN ((size_t)sysconf(_SC_THREAD_STACK_MIN))
#endif


typedef struct Osdep Osdep;
struct Osdep {
	pthread_t	self;
	inferno_thread_t	th;	/* descriptor of the thread for the synchronization API */
};

static pthread_key_t  prdakey;

extern int dflag;

Proc*
getup(void)
{
	return pthread_getspecific(prdakey);
}

void
pexit(char *msg, int t)
{
	Osenv *e;
	Proc *p;
	Osdep *os;

	USED(t);

	lock(&procs.l);
	p = up;
	if(p->prev)
		p->prev->next = p->next;
	else
		procs.head = p->next;

	if(p->next)
		p->next->prev = p->prev;
	else
		procs.tail = p->prev;
	unlock(&procs.l);

	if(0)
		HOSTED_API(print)("pexit: %s: %s\n", p->text, msg);

	e = p->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
		closeegrp(e->egrp);
		closesigs(e->sigs);
		HOSTED_API(free)(e->user);
	}
	HOSTED_API(free)(p->prog);
	os = p->os;
	if(os != nil){
		inferno_thread_detach();
		inferno_thread_destroy(&os->th);
		HOSTED_API(free)(os);
	}
	HOSTED_API(free)(p);
	pthread_exit(0);
}

static void*
tramp(void *arg)
{
	Proc *p;
	Osdep *os;

	p = arg;
	os = p->os;
	os->self = pthread_self();
	if(pthread_setspecific(prdakey, arg))
		panic("set specific data failed in tramp\n");
	inferno_thread_attach(&os->th);
#ifdef _GNU_SOURCE
	if(0){
		pthread_attr_t attr;
		memset(&attr, 0, sizeof(attr));
		pthread_getattr_np(pthread_self(), &attr);
		size_t s;
		pthread_attr_getstacksize(&attr, &s);
		HOSTED_API(print)("stack size = %d\n", s);
	}
#endif
	p->func(p->arg);
	pexit("{Tramp}", 0);
	return nil;
}

void
kproc(char *name, void (*func)(void*), void *arg, int flags)
{
	pthread_t thread;
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;
	Egrp *eg;
	pthread_attr_t attr;
	Osdep *os;

	p = newproc();
	if(p == nil)
		panic("kproc: no memory");

	os = HOSTED_API(malloc)(sizeof(*os));
	if(os == nil)
		panic("kproc: no memory");
	os->self = 0;	/* set by tramp */
	if(inferno_thread_init(&os->th) != 0)
		panic("kproc: no memory");
	p->os = os;

	if(flags & KPDUPPG) {
		pg = up->env->pgrp;
		incref(&pg->r);
		p->env->pgrp = pg;
	}
	if(flags & KPDUPFDG) {
		fg = up->env->fgrp;
		incref(&fg->r);
		p->env->fgrp = fg;
	}
	if(flags & KPDUPENVG) {
		eg = up->env->egrp;
		incref(&eg->r);
		p->env->egrp = eg;
	}

	p->env->uid = up->env->uid;
	p->env->gid = up->env->gid;
	kstrdup(&p->env->user, up->env->user);

	strcpy(p->text, name);

	p->func = func;
	p->arg = arg;

	lock(&procs.l);
	if(procs.tail != nil) {
		p->prev = procs.tail;
		procs.tail->next = p;
	} else {
		procs.head = p;
		p->prev = nil;
	}
	procs.tail = p;
	unlock(&procs.l);

	memset(&attr, 0, sizeof(attr));
	if(pthread_attr_init(&attr) == -1)
		panic("pthread_attr_init failed");
	if(flags & KPX11)
		pthread_attr_setstacksize(&attr, 512*1024);	/* could be a parameter */
	else if(KSTACK > 0)
		pthread_attr_setstacksize(&attr, (KSTACK < PTHREAD_STACK_MIN? PTHREAD_STACK_MIN: KSTACK)+1024);
	pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread, &attr, tramp, p))
		panic("thr_create failed\n");
	pthread_attr_destroy(&attr);
}

/*
 *  Called by swiproc, under p->sysio, to pull p out of a host call: the wait that p
 *  is in (or is about to enter: the interrupt is level-triggered) returns.
 */
void
oshostintr(Proc *p)
{
	Osdep *os;

	os = p->os;
	if(os != nil)
		inferno_thread_interrupt(&os->th);
}

/* called by osleave under sysio: an interrupt that the call did not consume is forgotten */
void
osintrclear(void)
{
	inferno_thread_clear_interrupt();
}

/*
 *  Wait, in a host call announced by osenter, until fd is ready (OSREAD, OSWRITE):
 *  the call that follows will not block.  If swiproc interrupts the wait, the host
 *  call is left (osleave) and Eintr is raised, so this does not return.
 */
void
oswaitfd(int fd, int events)
{
	int ready, ev;

	ev = 0;
	if(events & OSREAD)
		ev |= INFERNO_FD_READ;
	if(events & OSWRITE)
		ev |= INFERNO_FD_WRITE;
	if(inferno_wait_fd(fd, ev, -1, &ready) != INFERNO_WAIT_INTR)
		return;
	osleave();
	error(Eintr);
}

/* connect, interruptible; between osenter and osleave like the other calls */
int
osconnect(int fd, void *sa, int len)
{
	int fl, r, e, err;
	socklen_t el;

	fl = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	r = connect(fd, sa, len);
	if(r < 0 && errno == EINPROGRESS){
		oswaitfd(fd, OSWRITE);
		err = 0;
		el = sizeof(err);
		if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) < 0)
			err = errno;
		if(err != 0){
			errno = err;
			r = -1;
		}else
			r = 0;
	}
	e = errno;
	fcntl(fd, F_SETFL, fl);
	errno = e;
	return r;
}

void
osblock(void)
{
	/* a pause point for the tokens; an interrupt is not for this wait (swiproc does not make one outside a host call) */
	while(inferno_wait() != INFERNO_WAIT_WOKEN)
		{}
}

void
osready(Proc *p)
{
	Osdep *os;

	os = p->os;
	inferno_wake(&os->th);
}

void
kprocinit(Proc *p)
{
	Osdep *os;

	if(pthread_key_create(&prdakey, NULL))
		panic("key_create failed");
	if(pthread_setspecific(prdakey, p))
		panic("set specific thread data failed");

	/* the initial thread has host data and a descriptor like the others */
	os = HOSTED_API(malloc)(sizeof(*os));
	if(os == nil)
		panic("kprocinit: no memory");
	os->self = pthread_self();
	if(inferno_thread_init(&os->th) != 0)
		panic("kprocinit: no memory");
	p->os = os;
	inferno_thread_attach(&os->th);
}

void
osyield(void)
{
	sched_yield();
}

void
ospause(void)
{
	/* main just wants this thread to go away */
	pthread_exit(0);
}

void
oslopri(void)
{
	struct sched_param param;
	int policy;
	pthread_t self;

	self = pthread_self();
	pthread_getschedparam(self, &policy, &param);
	param.sched_priority = sched_get_priority_min(policy);
	pthread_setschedparam(self,  policy, &param);
}
