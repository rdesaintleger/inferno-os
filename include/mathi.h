/*
 * floating point control and status register masks
 */
enum
{
	INVAL		= 0x0001,
	ZDIV		= 0x0002,
	OVFL		= 0x0004,
	UNFL		= 0x0008,
	INEX		= 0x0010,
	RND_NR		= 0x0000,
	RND_NINF	= 0x0100,
	RND_PINF	= 0x0200,
	RND_Z		= 0x0300,
	RND_MASK	= 0x0300
};

extern	double	ipow10(int);
extern	void	FPinit(void);
extern	double	dot(int, double*, double*);
extern	ulong	FPcontrol(ulong, ulong);
extern	ulong	FPstatus(ulong, ulong);
extern	void	gemm(int, int, int, int, int, double,
			double*, int, double*, int, double, double*, int);
extern	ulong	getFPstatus(void);
extern	ulong	getFPcontrol(void);
extern	char*	g_fmt(char *, double, int);
extern	int	iamax(int, double*);
extern	double	fdim(double, double);
extern	double	fmax(double, double);
extern	double	fmin(double, double);
extern	double	norm2(int, double*);
extern	double	norm1(int, double*);
extern	double	strtod(const char *, char **);
