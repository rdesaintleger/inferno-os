#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
/*
 * dtoatest - fd_strtod and fd_dtoa (libmath/dtoa.c) against the C library of
 * the host: strtod, and printf with %e and %f, which round correctly.
 * fd_dtoa in mode 0 must give the shortest string that reads back as the same
 * number, and the closest one among those; in modes 2 and 3, the digits of %e
 * and %f.  fd_strtod must give the same number as strtod, and stop at the same
 * place, except on the extensions of the host (hexadecimal numbers, "inf").
 *
 * usage: dtoatest
 * Pseudo-random values (the sequence is always the same).
 */
#include <inferno/fp.h>
extern float64_t fd_strtod(const char*, char**);
extern int fd_dtoa(float64_t, int, int, int*, int*, char*, int);
static uint64_t rng=88172645463325252ULL;
static uint64_t rnd(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return rng; }
static double bits2d(uint64_t u){ double d; memcpy(&d,&u,8); return d; }
static uint64_t d2bits(double d){ uint64_t u; memcpy(&u,&d,8); return u; }
static int fails;
#define FAIL(...) do{ if(fails++<30){printf(__VA_ARGS__);printf("\n");} }while(0)
static void check(double d){
	char buf[400], ref[64], dig[400]; int dp,sg,n,i;
	float64_t f; f.v=d2bits(d);
	if(isnan(d)||isinf(d)) return;
	/* shortest */
	n=fd_dtoa(f,0,0,&dp,&sg,buf,sizeof buf);
	/* build string d1d2..e(dp-1) */
	snprintf(ref,sizeof ref,"%s0.%se%d", sg?"-":"", buf, dp);
	if(d!=0){
		double r=strtod(ref,0);
		if(d2bits(r)!=d2bits(d)) FAIL("shortest does not round trip %a -> %s",d,ref);
		if(n>1){
			char e[64]; snprintf(e,sizeof e,"%.*e",n-2,fabs(d));
			if(strtod(e,0)==fabs(d)) FAIL("not shortest %a: %s (%s)",d,ref,e);
		}
		{ char e[64]; snprintf(e,sizeof e,"%.*e",n-1,fabs(d));
		  char *p=e; int j=0; char dd[64];
		  for(;*p&&*p!='e';p++) if(*p!='.') dd[j++]=*p; dd[j]=0;
		  while(j>1&&dd[j-1]=='0') dd[--j]=0;
		  if(strcmp(dd,buf)) { double a=strtod(e,0); if(a==fabs(d)) FAIL("not closest %a: ours %s glibc %s",d,buf,dd);} }
	}
	/* fixed modes */
	for(i=1;i<=20;i+=3){
		char e[64]; n=fd_dtoa(f,2,i,&dp,&sg,buf,sizeof buf);
		snprintf(e,sizeof e,"%.*e",i-1,fabs(d));
		/* compare digits and exponent */
		char dd[64]; int j=0; char *p=e; int ex=0;
		for(;*p&&*p!='e';p++) if(*p!='.') dd[j++]=*p; dd[j]=0;
		if(*p=='e') ex=atoi(p+1);
		while(j>1&&dd[j-1]=='0') dd[--j]=0;
		if(d==0){ continue; }
		if(strcmp(dd,buf)||dp!=ex+1) FAIL("mode2 %d %a: ours %s e%d glibc %s e%d",i,d,buf,dp,dd,ex+1);
	}
	for(i=0;i<=20;i+=2){
		char e[512]; n=fd_dtoa(f,3,i,&dp,&sg,buf,sizeof buf);
		snprintf(e,sizeof e,"%.*f",i,fabs(d));
		/* digits string from %f without point, strip leading zeros / trailing zeros */
		char dd[512]; int j=0; char *p=e; int ip=0, seenpt=0, lead=1, zeros_after=0;
		/* integer digit count */
		char *dot=strchr(e,'.'); ip = dot? (int)(dot-e): (int)strlen(e);
		for(;*p;p++){ if(*p=='.') continue; dd[j++]=*p; } dd[j]=0;
		/* value = 0.dd * 10^ip */
		int s0=0; while(dd[s0]=='0' && s0<j-0) s0++;
		if(s0==j){ /* zero result */ if(n!=0 && !(n==1&&buf[0]=='0')) FAIL("mode3 %d %a zero: ours %s",i,d,buf); continue; }
		char *q=dd+s0; int dpr=ip-s0; int l=strlen(q); while(l>0&&q[l-1]=='0') q[--l]=0;
		if(strcmp(q,buf)||dpr!=dp) FAIL("mode3 %d %a: ours %s e%d glibc %s e%d (%s)",i,d,buf,dp,q,dpr,e);
	}
}
static void checkstr(const char *s){
	char *e1,*e2; double a=strtod(s,&e1); float64_t b=fd_strtod(s,&e2);
	if(d2bits(a)!=b.v) FAIL("strtod %s: glibc %a ours %a",s,a,bits2d(b.v));
	if(e1!=e2) FAIL("strtod end %s: %ld vs %ld",s,(long)(e1-s),(long)(e2-s));
}
int main(void){
	int i; char s[1200];
	double sp[]={0,1,0.1,0.5,1.5,2.5,1e23,5e-324,2.2250738585072014e-308,2.225073858507201e-308,1.7976931348623157e308,9007199254740993.0,123456789012345678.0,1e22,1e21,0.3,100,999999.5,0.05,9.5,99.5,1e-5,4.35,0.125,0.375};
	for(i=0;i<(int)(sizeof sp/sizeof *sp);i++){ check(sp[i]); check(-sp[i]); }
	for(i=0;i<300000;i++){
		uint64_t u=rnd();
		if(i%3==0) u=(u&0x800FFFFFFFFFFFFFULL)|((rnd()%2047)<<52);   /* any exponent */
		else if(i%3==1){ u=(u&0x800FFFFFFFFFFFFFULL)|((uint64_t)(1023-30+rnd()%60)<<52); }
		else { double dd=(double)(int64_t)(rnd()%2000000)/ (double)(1+rnd()%1000); u=d2bits(dd); }
		double d=bits2d(u); if(isnan(d)||isinf(d)) continue;
		check(d);
		if(i%4==0){ snprintf(s,sizeof s,"%.17g",d); checkstr(s); snprintf(s,sizeof s,"%.*e",(int)(rnd()%25),d); checkstr(s); snprintf(s,sizeof s,"%.15g",d); checkstr(s);}
	}
	/* halfway cases and long inputs */
	checkstr("9007199254740993"); checkstr("9007199254740992.5"); checkstr("9007199254740993.0000000000000000000000000000001");
	checkstr("2.4703282292062327e-324"); checkstr("2.4703282292062328e-324"); checkstr("4.9406564584124654e-324");
	checkstr("1.7976931348623158e308"); checkstr("1.7976931348623159e308"); checkstr("1e309"); checkstr("1e-400"); checkstr("0e5"); checkstr("-0"); checkstr(".5"); checkstr("5."); checkstr("1e"); checkstr("1e+"); checkstr("  12abc"); checkstr("."); checkstr("-"); checkstr("1.5e3x");
	checkstr("infinity"); checkstr("-Infinity"); checkstr("nan"); checkstr("NaN"); checkstr("1e99999999999"); checkstr("1e-99999999999");
	{ /* 800+ digits */ int k; for(k=0;k<1000;k++) s[k]='1'; strcpy(s+1000,"e-1000"); checkstr(s); for(k=0;k<1100;k++) s[k]='0'; s[0]='0'; s[1]='.'; s[1099]='5'; s[1100]=0; checkstr(s); }
	printf("fails=%d\n",fails);
	return fails!=0;
}
