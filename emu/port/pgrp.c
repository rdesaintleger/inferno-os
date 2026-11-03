#include	"dat.h"
#include	"fns.h"
#include	"error.h"

static Ref	pgrpid;
static Ref	mountid;
static inferno_mutex_t mountidl;	/* a copy takes its ids in one block; see pgrpinit */

void
pgrpinit(void)
{
	if(inferno_mutex_init(&mountidl, INFERNO_MUTEX_SHORT) != 0)
		panic("pgrpinit: no memory");
}

Pgrp*
newpgrp(void)
{
	Pgrp *p;

	p = HOSTED_API(malloc)(sizeof(Pgrp));
	if(p == nil)
		error(Enomem);
	refinit(&p->r, 1);
	p->pgrpid = incref(&pgrpid);
	p->progmode = 0644;
	return p;
}

void
closepgrp(Pgrp *p)
{
	Mhead **h, **e, *f, *next, *dead;
	Mount *mount;
	Chan *from;

	if(p == nil || decref(&p->r) != 0)
		return;

	/*
	 *  The heads are detached under the lock of the name space (nobody else holds
	 *  p any more, but the walks that still hold a head take its own lock) and
	 *  closed after it: cclose goes down to the close of the driver.
	 */
	dead = nil;
	wlock(&p->ns);
	p->pgrpid = -1;
	e = &p->mnthash[MNTHASH];
	for(h = p->mnthash; h < e; h++) {
		for(f = *h; f; f = next) {
			next = f->hash;
			f->hash = dead;
			dead = f;
		}
		*h = nil;
	}
	wunlock(&p->ns);
	for(f = dead; f; f = next) {
		wlock(&f->lock);
		from = f->from;
		mount = f->mount;
		f->mount = nil;
		next = f->hash;
		wunlock(&f->lock);
		cclose(from);
		mountfree(mount);
		putmhead(f);
	}
	cclose(p->dot);
	cclose(p->slash);
	HOSTED_API(free)(p);
}

void
pgrpinsert(Mount **order, Mount *m)
{
	Mount *f;

	m->order = 0;
	if(*order == 0) {
		*order = m;
		return;
	}
	for(f = *order; f; f = f->order) {
		if(m->mountid < f->mountid) {
			m->order = f;
			*order = m;
			return;
		}
		order = &f->order;
	}
	*order = m;
}

/*
 * pgrpcpy MUST preserve the mountid allocation order of the parent group
 */
void
pgrpcpy(Pgrp *to, Pgrp *from)
{
	int i;
	Mount *n, *m, **link, *order;
	Mhead *f, **tom, **l, *mh;

	wlock(&from->ns);
	if(waserror()){
		wunlock(&from->ns);
		nexterror();
	}
	order = 0;
	tom = to->mnthash;
	for(i = 0; i < MNTHASH; i++) {
		l = tom++;
		for(f = from->mnthash[i]; f; f = f->hash) {
			rlock(&f->lock);
			if(waserror()){
				runlock(&f->lock);
				nexterror();
			}
			mh = HOSTED_API(malloc)(sizeof(Mhead));
			if(mh == nil)
				error(Enomem);
			mh->from = f->from;
			refinit(&mh->r, 1);
			incref(&mh->from->r);
			*l = mh;
			l = &mh->hash;
			link = &mh->mount;
			for(m = f->mount; m; m = m->next) {
				n = newmount(mh, m->to, m->mflag, m->spec);
				m->copy = n;
				pgrpinsert(&order, m);
				*link = n;
				link = &n->next;	
			}
			poperror();
			runlock(&f->lock);
		}
	}
	/*
	 * Allocate mount ids in the same sequence as the parent group
	 */
	inferno_mutex_lock(&mountidl);
	for(m = order; m; m = m->order)
		m->copy->mountid = incref(&mountid) - 1;
	inferno_mutex_unlock(&mountidl);

	to->progmode = from->progmode;
	to->slash = cclone(from->slash);
	to->dot = cclone(from->dot);
	to->nodevs = from->nodevs;
	poperror();
	wunlock(&from->ns);
}

Fgrp*
newfgrp(Fgrp *old)
{
	Fgrp *new;
	int n;

	new = HOSTED_API(malloc)(sizeof(Fgrp));
	if(new == nil)
		error(Enomem);
	refinit(&new->r, 1);
	n = DELTAFD;
	if(old != nil){
		lock(&old->l);
		if(old->maxfd >= n)
			n = (old->maxfd+1 + DELTAFD-1)/DELTAFD * DELTAFD;
		new->maxfd = old->maxfd;
		unlock(&old->l);
	}
	new->nfd = n;
	new->fd = HOSTED_API(malloc)(n*sizeof(Chan*));
	if(new->fd == nil){
		HOSTED_API(free)(new);
		error(Enomem);
	}
	return new;
}

Fgrp*
dupfgrp(Fgrp *f)
{
	int i;
	Chan *c;
	Fgrp *new;
	int n;

	new = HOSTED_API(malloc)(sizeof(Fgrp));
	if(new == nil)
		error(Enomem);
	refinit(&new->r, 1);
	lock(&f->l);
	n = DELTAFD;
	if(f->maxfd >= n)
		n = (f->maxfd+1 + DELTAFD-1)/DELTAFD * DELTAFD;
	new->nfd = n;
	new->fd = HOSTED_API(malloc)(n*sizeof(Chan*));
	if(new->fd == nil){
		unlock(&f->l);
		HOSTED_API(free)(new);
		error(Enomem);
	}
	new->maxfd = f->maxfd;
	new->minfd = f->minfd;
	for(i = 0; i <= f->maxfd; i++) {
		if(c = f->fd[i]){
			incref(&c->r);
			new->fd[i] = c;
		}
	}
	unlock(&f->l);

	return new;
}

void
closefgrp(Fgrp *f)
{
	int i;
	Chan *c;

	if(f != nil && decref(&f->r) == 0) {
		for(i = 0; i <= f->maxfd; i++)
			if(c = f->fd[i])
				cclose(c);
		HOSTED_API(free)(f->fd);
		HOSTED_API(free)(f);
	}
}

Mount*
newmount(Mhead *mh, Chan *to, int flag, char *spec)
{
	Mount *m;

	m = HOSTED_API(malloc)(sizeof(Mount));
	if(m == nil)
		error(Enomem);
	m->to = to;
	m->head = mh;
	incref(&to->r);
	inferno_mutex_lock(&mountidl);
	m->mountid = incref(&mountid);
	inferno_mutex_unlock(&mountidl);
	m->mflag = flag;
	if(spec != 0)
		kstrdup(&m->spec, spec);

	return m;
}

void
mountfree(Mount *m)
{
	Mount *f;

	while(m) {
		f = m->next;
		cclose(m->to);
		m->mountid = 0;
		HOSTED_API(free)(m->spec);
		HOSTED_API(free)(m);
		m = f;
	}
}

void
closesigs(Skeyset *s)
{
	int i;

	if(s == nil || decref(&s->r) != 0)
		return;
	for(i=0; i<s->nkey; i++)
		freeskey(s->keys[i]);
	HOSTED_API(free)(s);
}

void
freeskey(Signerkey *key)
{
	if(key == nil || decref(&key->r) != 0)
		return;
	HOSTED_API(free)(key->owner);
	(*key->pkfree)(key->pk);
	HOSTED_API(free)(key);
}
