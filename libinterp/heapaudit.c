#include "lib9.h"
#include "interp.h"
#include "pool.h"


typedef struct Audit Audit;
struct Audit
{
	Type*	t;
	ulong	n;
	ulong	size;
	Audit*	hash;
};
Audit*	ahash[128];
extern	Pool*	heapmem;
extern void conslog(char*, ...);
#define	conslog	print

typedef struct Typed	Typed;
typedef struct Ptyped	Ptyped;

extern Type Trdchan;
extern Type Twrchan;

struct Typed
{
	char*	name;
	Type*	ptr;
} types[] =
{
	{"array",	&Tarray},
	{"byte",	&Tbyte},
	{"channel",	&Tchannel},
	{"list",	&Tlist},
	{"modlink",	&Tmodlink},
	{"ptr",		&Tptr},
	{"string",	&Tstring},

	{"rdchan",	&Trdchan},
	{"wrchan",	&Twrchan},
	{"unspec",	nil},

	0
};

extern Type* TDisplay;
extern Type* TFont;
extern Type* TImage;
extern Type* TScreen;
extern Type* TFD;
extern Type* TFileIO;
extern Type* Tread;
extern Type* Twrite;
extern Type* fakeTkTop;

extern Type* TSigAlg;
extern Type* TCertificate;
extern Type* TSK;
extern Type* TPK;
extern Type* TDigestState;
extern Type* TAuthinfo;
extern Type* TDESstate;
extern Type* TIPint;

struct Ptyped
{
	char*	name;
	Type**	ptr;
} ptypes[] =
{
	{"Display",	&TDisplay},
	{"Font",	&TFont},
	{"Image",	&TImage},
	{"Screen",	&TScreen},

	{"SigAlg",	&TSigAlg},
	{"Certificate",	&TCertificate},
	{"SK",		&TSK},
	{"PK",		&TPK},
	{"DigestState",	&TDigestState},
	{"Authinfo",	&TAuthinfo},
	{"DESstate",	&TDESstate},
	{"IPint",	&TIPint},

	{"FD",		&TFD},
	{"FileIO",	&TFileIO},

/*	{"Fioread",	&Tread},	*/
/*	{"Fiowrite",	&Twrite},	*/

	{"TkTop",	&fakeTkTop},

	0
};
