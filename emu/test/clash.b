implement Clash;
include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
	Display: import draw;
include "tk.m";
	tk: Tk;

# Regression test for a bug that dates from the 2006 import (present at 36616fe, d3495cc).
# newbutton() (libtk/buton.c) links the widget into t->root with tkaddchild() and only
# then checks -variable; on the TkNotvt error it called tkfreeobj(tk), which frees the
# widget without unlinking it.  The next walk of t->root (tksetvar, from creating a
# checkbutton) read freed memory: here it looped forever in tkvarchanged.
# Fixed by using tkdestroy() in the error path.
# usage: clash   expected: "1 .l", "2 ", "3 !variable is wrong type", "4 .k", "5 done"
# (the freed block is only reused when the channel still holds a message, hence the send)

Clash: module { init: fn(ctxt: ref Draw->Context, argv: list of string); };

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	d := Display.allocate(nil);
	top := tk->toplevel(d, "");
	c := chan[4] of string;
	sys->print("1 %s\n", tk->cmd(top, "radiobutton .l -text hi -variable x -value a"));
	sys->print("2 %s\n", tk->namechan(top, c, "x"));
	tk->cmd(top, "send x hello");			# left in the buffer
	sys->print("3 %s\n", tk->cmd(top, "radiobutton .m -text hi -variable x -value b"));	# !variable is wrong type
	sys->print("4 %s\n", tk->cmd(top, "checkbutton .k -text k -variable y"));
	sys->print("5 done\n");
}
