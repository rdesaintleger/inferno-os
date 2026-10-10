#include <lib9.h>
#include <kernel.h>
#include "draw.h"
#include "tk.h"
#include "label.h"

char*
tksetvar(TkTop *top, char *c, char *newval)
{
	TkVar *v;
	TkWin *tkw;
	Tk *f, *m;
	void (*vc)(Tk*, char*, char*);

	if (c == nil || c[0] == '\0')
		return nil;

	v = tkmkvar(top, c, &tkstringvar);
	if(v == nil)
		return TkNomem;
	if(v->ops != &tkstringvar)
		return TkNotvt;

	if(newval == nil)
		newval = "";

	if(v->value != nil) {
		if (strcmp(v->value, newval) == 0)
			return nil;
		HOSTED_API(free)(v->value);
	}

	v->value = HOSTED_API(strdup)(newval);
	if(v->value == nil)
		return TkNomem;

	for(f = top->root; f; f = f->siblings) {
		if(f->type == TKmenu) {
			tkw = TKobj(TkWin, f);
			for(m = tkw->slave; m; m = m->next)
				if ((vc = tkmethod[m->type]->varchanged) != nil)
					(*vc)(m, c, newval);
		} else
			if ((vc = tkmethod[f->type]->varchanged) != nil)
				(*vc)(f, c, newval);
	}

	return nil;
}

char*
tkvariable(TkTop *t, char *arg, char **ret)
{
	TkVar *v;
	char *fmt, *e, *buf, *ebuf, *val;
	int l;

	l = strlen(arg) + 2;
	buf = HOSTED_API(malloc)(l);
	if(buf == nil)
		return TkNomem;
	ebuf = buf+l;

	arg = tkword(t, arg, buf, ebuf, nil);
	arg = tkskip(arg, " \t");
	if (*arg == '\0') {
		if(strcmp(buf, "lasterror") == 0) {
			HOSTED_API(free)(buf);
			if(t->err == nil)
				return nil;
			fmt = "%s: %s";
			if(strlen(t->errcmd) == sizeof(t->errcmd)-1)
				fmt = "%s...: %s";
			e = tkvalue(ret, fmt, t->errcmd, t->err);
			t->err = nil;
			return e;
		}
		v = tkfindvar(t, buf);
		HOSTED_API(free)(buf);
		if(v == nil || v->value == nil)
			return nil;
		if(v->ops != &tkstringvar)
			return TkNotvt;
		return tkvalue(ret, "%s", v->value);
	}
	val = buf+strlen(buf)+1;
	tkword(t, arg, val, ebuf, nil);
	e = tksetvar(t, buf, val);
	HOSTED_API(free)(buf);
	return e;
}

/*
 * Variables.  A variable belongs to the toplevel's list; its ops say how
 * to release the value and whether it can receive messages.  libtk only
 * knows string variables (tkstringvar); a host may create others by passing
 * its own ops to tkmkvar.
 */
static void
tkstringfree(TkTop *t, TkVar *v)
{
	USED(t);
	HOSTED_API(free)(v->value);
}

const TkVarOps tkstringvar = {
	tkstringfree,
	nil,
};

/* the variable called name, or nil */
TkVar*
tkfindvar(TkTop *t, char *name)
{
	TkVar *v;

	for(v = t->vars; v; v = v->link)
		if(strcmp(v->name, name) == 0)
			return v;
	return nil;
}

/* the variable called name, created with ops if it does not exist (its kind is then ops) */
TkVar*
tkmkvar(TkTop *t, char *name, const TkVarOps *ops)
{
	TkVar *v;

	v = tkfindvar(t, name);
	if(v != nil)
		return v;

	v = HOSTED_API(malloc)(sizeof(TkVar)+strlen(name)+1);
	if(v == nil)
		return nil;
	strcpy(v->name, name);
	v->link = t->vars;
	t->vars = v;
	v->ops = ops;
	v->value = nil;
	return v;
}

static void
tkdelvar(TkTop *t, TkVar *v)
{
	v->ops->free(t, v);
	HOSTED_API(free)(v);
}

void
tkfreevar(TkTop *t, char *name)
{
	TkVar **l, *p;

	if(name == nil)
		return;
	l = &t->vars;
	for(p = *l; p != nil; p = p->link) {
		if(strcmp(p->name, name) == 0) {
			*l = p->link;
			tkdelvar(t, p);
			return;
		}
		l = &p->link;
	}
}

void
tkfreevars(TkTop *t)
{
	TkVar *v, *next;

	for(v = t->vars; v; v = next) {
		next = v->link;
		tkdelvar(t, v);
	}
	t->vars = nil;
}
