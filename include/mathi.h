#include <inferno/fp.h>

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

extern	void	FPinit(void);
extern	void	FPsave(void*);
extern	void	FPrestore(void*);
extern	ulong	FPflags(void);
extern	float64_t	dot(int, float64_t*, float64_t*);
extern	ulong	FPcontrol(ulong, ulong);
extern	ulong	FPstatus(ulong, ulong);
extern	void	gemm(int, int, int, int, int, float64_t,
			float64_t*, int, float64_t*, int, float64_t, float64_t*, int);
extern	ulong	getFPstatus(void);
extern	ulong	getFPcontrol(void);
extern	char*	fd_g_fmt(char *, float64_t, int);
extern	int	fd_dtoa(float64_t, int, int, int*, int*, char*, int);
extern	int	iamax(int, float64_t*);
extern	float64_t	fd_fdim(float64_t, float64_t);
extern	float64_t	fd_fmax(float64_t, float64_t);
extern	float64_t	fd_fmin(float64_t, float64_t);
extern	float64_t	norm2(int, float64_t*);
extern	float64_t	norm1(int, float64_t*);
extern	float64_t	fd_strtod(const char *, char **);
