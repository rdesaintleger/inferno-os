#include "lib9.h"
#include "mathi.h"
#include <inferno/fp.h>

/* gemm on float64_t (SoftFloat); the caller looks at the flags afterwards */
static const float64_t zero = { 0 };
static const float64_t one = { UINT64_C(0x3FF0000000000000) };

#define EQ(x, y)	f64_eq((x), (y))
#define ADD(x, y)	f64_add((x), (y))
#define MUL(x, y)	f64_mul((x), (y))

void
gemm(int transa, int transb, int m, int n, int k, float64_t alpha,
	float64_t *a, int lda,
	float64_t *b, int ldb, float64_t beta,
	float64_t *c, int ldc)
{
    int i1, i2, i3, nota, notb, i, j, jb, jc, l, la;
    float64_t temp;

    nota = transa=='N';
    notb = transb=='N';

    if(m == 0 || n == 0 || (EQ(alpha, zero) || k == 0) && EQ(beta, one)){
	return;
    }
    if(EQ(alpha, zero)){
	if(EQ(beta, zero)){
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    c[i + jc] = zero;
		}
	    }
	}else{
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    c[i + jc] = MUL(beta, c[i + jc]);
		}
	    }
	}
	return;
    }

    if(!a){
	if(notb){   /* C := alpha*B + beta*C. */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jb = j*ldb;
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    c[i + jc] = ADD(MUL(alpha, b[i+jb]), MUL(beta, c[i+jc]));
		}
	    }
	}else{   /* C := alpha*B' + beta*C. */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    c[i + jc] = ADD(MUL(alpha, b[j+i*ldb]), MUL(beta, c[i+jc]));
		}
	    }
	}
	return;
    }

    if(notb){
	if(nota){

/*          Form  C := alpha*A*B + beta*C. */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		if(EQ(beta, zero)){
		    i2 = m;
		    for(i = 0; i < i2; ++i){
			c[i + jc] = zero;
		    }
		}else if(!EQ(beta, one)){
		    i2 = m;
		    for(i = 0; i < i2; ++i){
			c[i + jc] = MUL(beta, c[i + jc]);
		    }
		}
		i2 = k;
		for(l = 0; l < i2; ++l){
		    la = l*lda;
		    if(!EQ(b[l + j*ldb], zero)){
			temp = MUL(alpha, b[l + j*ldb]);
			i3 = m;
			for(i = 0; i < i3; ++i){
			    c[i + jc] = ADD(c[i + jc], MUL(temp, a[i + la]));
			}
		    }
		}
	    }
	}else{

/*          Form  C := alpha*A'*B + beta*C */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    temp = zero;
		    i3 = k;
		    for(l = 0; l < i3; ++l){
			temp = ADD(temp, MUL(a[l + i*lda], b[l + j*ldb]));
		    }
		    if(EQ(beta, zero)){
			c[i + jc] = MUL(alpha, temp);
		    }else{
			c[i + jc] = ADD(MUL(alpha, temp), MUL(beta, c[i + jc]));
		    }
		}
	    }
	}
    }else{
	if(nota){

/*          Form  C := alpha*A*B' + beta*C */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		if(EQ(beta, zero)){
		    i2 = m;
		    for(i = 0; i < i2; ++i){
			c[i + jc] = zero;
		    }
		}else if(!EQ(beta, one)){
		    i2 = m;
		    for(i = 0; i < i2; ++i){
			c[i + jc] = MUL(beta, c[i + jc]);
		    }
		}
		i2 = k;
		for(l = 0; l < i2; ++l){
		    if(!EQ(b[j + l*ldb], zero)){
			temp = MUL(alpha, b[j + l*ldb]);
			i3 = m;
			for(i = 0; i < i3; ++i){
			    c[i + jc] = ADD(c[i + jc], MUL(temp, a[i + l*lda]));
			}
		    }
		}
	    }
	}else{

/*          Form  C := alpha*A'*B' + beta*C */
	    i1 = n;
	    for(j = 0; j < i1; ++j){
		jc = j*ldc;
		i2 = m;
		for(i = 0; i < i2; ++i){
		    temp = zero;
		    i3 = k;
		    for(l = 0; l < i3; ++l){
			temp = ADD(temp, MUL(a[l + i*lda], b[j + l*ldb]));
		    }
		    if(EQ(beta, zero)){
			c[i + jc] = MUL(alpha, temp);
		    }else{
			c[i + jc] = ADD(MUL(alpha, temp), MUL(beta, c[i + jc]));
		    }
		}
	    }
	}
    }
}
