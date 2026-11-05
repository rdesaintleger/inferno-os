#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"interp.h"

/*
 *  The locks of the Rendez: Sleep, Wakeup and swiproc take the token of the stripe
 *  that the address of the Rendez selects.  Nobody holds the token of a stripe while
 *  taking that of another one, so sharing them is harmless, and the tokens are
 *  initialized once, at startup.
 */
enum { Nrendezl = 64 };
static inferno_token_t rendezl[Nrendezl];

static inferno_token_t*
rendezlock(Rendez *r)
{
	return &rendezl[((uintptr_t)r >> 4) % Nrendezl];
}

void
rendezinit(void)
{
	int i;

	for(i = 0; i < Nrendezl; i++)
		if(inferno_token_init(&rendezl[i]) != 0)
			panic("rendezinit: no memory");
}

Proc*
newproc(void)
{
	Proc *p;

	p = HOSTED_API(malloc)(sizeof(Proc));
	if(p == nil)
		return nil;

	if(inferno_token_init(&p->rlock) != 0){
		HOSTED_API(free)(p);
		return nil;
	}
	if(inferno_token_init(&p->sysio) != 0){
		inferno_token_destroy(&p->rlock);
		HOSTED_API(free)(p);
		return nil;
	}

	p->type = Unknown;
	p->killed = 0;
	p->swipend = 0;
	p->env = &p->defenv;
	kstrdup(&p->env->user, "*nouser");
	p->env->errstr = p->env->errbuf0;
	p->env->syserrstr = p->env->errbuf1;
	addprog(p);

	return p;
}

/* the tokens of a Proc go away with it (pexit) */
void
freeproc(Proc *p)
{
	inferno_token_destroy(&p->rlock);
	inferno_token_destroy(&p->sysio);
	HOSTED_API(free)(p);
}

void
Sleep(Rendez *r, int (*f)(void*), void *arg)
{
	inferno_token_t *rl, *set[2];
	int done;

	rl = rendezlock(r);
	set[0] = &up->rlock;
	set[1] = rl;
	inferno_token_get_all(2, set);

	/*
	 * if interrupted or condition happened, never mind;
	 * f looks at its state without any lock: it is called with both tokens held,
	 * and a pause there would let a Wakeup go by
	 */
	done = up->killed;
	if(!done){
		inferno_token_nogo_begin();
		done = f(arg);
		inferno_token_nogo_end();
	}
	if(done) {
		inferno_token_put(rl);
	}else{
		if(r->p != nil)
			panic("double sleep %s[%lud] %s[%lud] r=0x%lux\n", r->p->text, r->p->pid, up->text, up->pid, r);

		r->p = up;
		inferno_token_put(rl);
		up->swipend = 0;
		up->r = r;	/* for swiproc */
		inferno_token_put(&up->rlock);

		osblock();

		inferno_token_get(&up->rlock);
		up->r = nil;
	}

	if(up->killed || up->swipend) {
		up->killed = 0;
		up->swipend = 0;
		inferno_token_put(&up->rlock);
		error(Eintr);
	}
	inferno_token_put(&up->rlock);
}

int
Wakeup(Rendez *r)
{
	Proc *p;
	inferno_token_t *rl;

	rl = rendezlock(r);
	inferno_token_get(rl);
	p = r->p;
	if(p != nil) {
		r->p = nil;
		osready(p);
	}
	inferno_token_put(rl);
	return p != nil;
}

void
swiproc(Proc *p, int interp)
{
	Rendez *r;
	inferno_token_t *rl;

	if(p == nil)
		return;

	/*
	 * Pull out of emu Sleep
	 */
	inferno_token_get(&p->rlock);
	if(!interp)
		p->killed = 1;
	for(;;){
		r = p->r;
		if(r == nil)
			break;
		rl = rendezlock(r);
		/*
		 * taking the second token may pause, and the first one is lost with
		 * the pause: p may have left r in the meantime, so look again
		 */
		inferno_token_get(rl);
		if(p->r != r){
			inferno_token_put(rl);
			continue;
		}
		if(r->p == p){
			p->swipend = 1;
			r->p = nil;
			osready(p);
		}
		inferno_token_put(rl);
		inferno_token_put(&p->rlock);
		return;
	}
	inferno_token_put(&p->rlock);

	/*
	 * Maybe pull out of Host OS
	 */
	inferno_token_get(&p->sysio);
	if(p->syscall) {
		p->swipend = 1;
		oshostintr(p);	/* under sysio: osleave clears it under the same lock */
	}
	inferno_token_put(&p->sysio);
}

void
notkilled(void)
{
	inferno_token_get(&up->rlock);
	up->killed = 0;
	inferno_token_put(&up->rlock);
}

/*
 *  Test hooks, driven from #T (devsnap.c, headless emu-t only): osenterdelayms
 *  makes every osenter done for the Prog osenterdelaypid wait that long (yielding)
 *  after it has announced the host call, which is the window in which swiproc's
 *  signal used to be lost; oshostcalls counts the processes that are between
 *  osenter and osleave.  emu/test/intr.b uses them.
 */
int osenterdelayms;
int osenterdelaypid;

int
oshostcalls(void)
{
	Proc *p;
	int n;

	n = 0;
	inferno_token_get(&procs.l);
	for(p = procs.head; p != nil; p = p->next)
		if(p->syscall)
			n++;
	inferno_token_put(&procs.l);
	return n;
}

static void
osenterdelay(void)
{
	vlong t;

	if(osenterdelayms > 0 && up->iprog != nil && ((Prog*)up->iprog)->pid == osenterdelaypid){
		t = osusectime() + (vlong)osenterdelayms*1000;
		while(osusectime() < t)
			osyield();
	}
}

void
osenter(void)
{
	up->syscall = 1;
	osenterdelay();
}

/*
 *  The host call is over (or was interrupted).  An interrupt (oshostintr) is only
 *  made while syscall is set, under sysio, and the flag it leaves is cleared here
 *  under the same lock, so one that comes too late is never left for a later wait.
 */
void
osleave(void)
{
	int r;

	inferno_token_get(&up->sysio);
	up->syscall = 0;
	osintrclear();
	inferno_token_put(&up->sysio);

	inferno_token_get(&up->rlock);
	r = up->swipend;
	up->swipend = 0;
	inferno_token_put(&up->rlock);

	if(r != 0)
		error(Eintr);
}

void
rptwakeup(void *o, void *ar)
{
	Rept *r;

	r = ar;
	if(r == nil)
		return;
	lock(&r->l);
	r->o = o;
	unlock(&r->l);
	Wakeup(&r->r);
}

static int
rptactive(void *a)
{
	Rept *r = a;
	int i;
	lock(&r->l);
	i = r->active(r->o);
	unlock(&r->l);
	return i;
}

static void
rproc(void *a)
{
	long now, then;
	ulong t;
	int i;
	void *o;
	Rept *r;

	up->env->fgrp = newfgrp(nil);
	r = a;
	t = (ulong)r->t;

Wait:
	Sleep(&r->r, rptactive, r);
	lock(&r->l);
	o = r->o;
	unlock(&r->l);
	then = osmillisec();
	for(;;){
		osmillisleep(t);
		now = osmillisec();
		if(waserror())
			break;
		i = r->ck(o, now-then);
		poperror();
		if(i == -1)
			goto Wait;
		if(i == 0)
			continue;
		then = now;
		acquire();
		if(waserror()) {
			release();
			break;
		}
		r->f(o);
		poperror();
		release();
	}
	pexit("", 0);
}

void*
rptproc(char *s, int t, void *o, int (*active)(void*), int (*ck)(void*, int), void (*f)(void*))
{
	Rept *r;

	r = HOSTED_API(mallocz)(sizeof(Rept), 1);
	if(r == nil)
		return nil;
	r->t = t;
	r->active = active;
	r->ck = ck;
	r->f = f;
	r->o = o;
	kproc(s, rproc, r, KPDUPPG);
	return r;
}
