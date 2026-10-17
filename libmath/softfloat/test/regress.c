/*
 * Regression test of the SoftFloat copy used by Inferno.
 *
 * Runs deterministic pseudo-random vectors through every operation kept in
 * this copy (f64 and f32 arithmetic, comparisons and conversions, extF80),
 * with the four rounding modes, both tininess modes and the three extF80
 * rounding precisions, and prints one hash of all results and exception
 * flags.  The expected value is
 *
 *		e8f3b4a677c83197
 *
 * and it must be the same on every machine, with every compiler and every
 * optimisation level (it was checked on 32 and 64 bit builds at -O0, -O2,
 * -O3 and -Os, and strict C99).  A different value means a bug or a change of
 * behaviour.  To build and run it (hosted C, test only):
 *
 *	cc -I source/RISCV -I source/include -I ../../include/inferno \
 *		-o regress test/regress.c source/*.c source/RISCV/*.c && ./regress
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "softfloat.h"
static uint64_t s=88172645463325252ULL; static uint64_t rnd(void){s^=s<<13;s^=s>>7;s^=s<<17;return s;}
static uint64_t h=1469598103934665603ULL; static void mix(uint64_t v){h^=v;h*=1099511628211ULL;}
static uint64_t pick64(void){ uint64_t r=rnd(); switch(rnd()%7){
 case 0:return r; case 1:return r&0x800FFFFFFFFFFFFFULL;
 case 2:return (r&0x8000000000000000ULL)|0x7FF0000000000000ULL|(r&0xF);
 case 3:return (r&0x8000000000000000ULL)|((uint64_t)(1020+rnd()%8)<<52)|(r&0xFFFFFFFFFFFFFULL);
 case 4:return (r&0x8000000000000000ULL)|((uint64_t)(rnd()%3)<<52)|(r&0xFFFFFFFFFFFFFULL);
 default:return (r&0x8000000000000000ULL)|((uint64_t)(rnd()%2046+1)<<52)|(r&0xFFFFFFFFFFFFFULL);} }
static uint32_t pick32(void){ uint32_t r=(uint32_t)rnd(); switch(rnd()%4){case 0:return r;case 1:return (r&0x80000000)|0x7F800000|(r&7);default:return (r&0x80000000)|((uint32_t)(rnd()%254+1)<<23)|(r&0x7FFFFF);} }
static extFloat80_t pick80(void){ extFloat80_t x; uint16_t se=(uint16_t)(rnd()%0x7FFF)|(rnd()&1?0x8000:0); if(rnd()%8==0) se|=0x7FFF; if(rnd()%11==0) se&=0x8000; x.signExp=se; x.signif=rnd()|(rnd()%8?0x8000000000000000ULL:0); return x;}
static void fl(void){ mix(softfloat_exceptionFlags); softfloat_exceptionFlags=0; }
static void m80(extFloat80_t r){ mix(r.signif); mix(r.signExp); fl(); }
int main(void){
  static const uint_fast8_t rm[4]={softfloat_round_near_even,softfloat_round_minMag,softfloat_round_min,softfloat_round_max};
  static const uint_fast8_t rp[3]={80,64,32};
  for(long i=0;i<300000;i++){
    softfloat_roundingMode=rm[i&3];
    softfloat_detectTininess=(i&8)?softfloat_tininess_beforeRounding:softfloat_tininess_afterRounding;
    float64_t a,b,z; a.v=pick64(); b.v=pick64();
    z=f64_add(a,b);mix(z.v);fl(); z=f64_sub(a,b);mix(z.v);fl(); z=f64_mul(a,b);mix(z.v);fl(); z=f64_div(a,b);mix(z.v);fl();
    z=f64_sqrt(a);mix(z.v);fl(); z=f64_rem(a,b);mix(z.v);fl(); z=f64_roundToInt(a,rm[(i>>2)&3],true);mix(z.v);fl(); z=f64_roundToInt(a,rm[(i>>4)&3],false);mix(z.v);fl();
    mix(f64_eq(a,b));fl(); mix(f64_lt(a,b));fl(); mix(f64_le(a,b));fl(); mix(f64_eq_signaling(a,b));fl(); mix(f64_lt_quiet(a,b));fl(); mix(f64_le_quiet(a,b));fl(); mix(f64_isSignalingNaN(a));
    mix((uint64_t)f64_to_i32(a,rm[i&3],true));fl(); mix((uint64_t)f64_to_i64(a,rm[i&3],true));fl(); mix((uint64_t)f64_to_ui32(a,rm[i&3],true));fl(); mix((uint64_t)f64_to_ui64(a,rm[i&3],true));fl();
    mix((uint64_t)f64_to_i32_r_minMag(a,true));fl(); mix((uint64_t)f64_to_i64_r_minMag(a,false));fl(); mix((uint64_t)f64_to_ui32_r_minMag(a,true));fl(); mix((uint64_t)f64_to_ui64_r_minMag(a,true));fl();
    mix(i32_to_f64((int32_t)rnd()).v);fl(); mix(i64_to_f64((int64_t)rnd()).v);fl(); mix(ui32_to_f64((uint32_t)rnd()).v);fl(); mix(ui64_to_f64(rnd()).v);fl();
    float32_t x; x.v=pick32(); mix(f32_to_f64(x).v);fl(); mix(f64_to_f32(a).v);fl();
    {   float32_t u,v,w; u.v=pick32(); v.v=pick32();
        w=f32_add(u,v);mix(w.v);fl(); w=f32_sub(u,v);mix(w.v);fl(); w=f32_mul(u,v);mix(w.v);fl(); w=f32_div(u,v);mix(w.v);fl();
        w=f32_sqrt(u);mix(w.v);fl(); w=f32_rem(u,v);mix(w.v);fl(); w=f32_roundToInt(u,rm[(i>>2)&3],true);mix(w.v);fl(); w=f32_roundToInt(u,rm[(i>>4)&3],false);mix(w.v);fl();
        mix(f32_eq(u,v));fl(); mix(f32_lt(u,v));fl(); mix(f32_le(u,v));fl(); mix(f32_eq_signaling(u,v));fl(); mix(f32_lt_quiet(u,v));fl(); mix(f32_le_quiet(u,v));fl(); mix(f32_isSignalingNaN(u));
        mix((uint64_t)f32_to_i32(u,rm[i&3],true));fl(); mix((uint64_t)f32_to_i64(u,rm[i&3],true));fl(); mix((uint64_t)f32_to_ui32(u,rm[i&3],true));fl(); mix((uint64_t)f32_to_ui64(u,rm[i&3],true));fl();
        mix((uint64_t)f32_to_i32_r_minMag(u,true));fl(); mix((uint64_t)f32_to_i64_r_minMag(u,false));fl(); mix((uint64_t)f32_to_ui32_r_minMag(u,true));fl(); mix((uint64_t)f32_to_ui64_r_minMag(u,true));fl();
        mix(i32_to_f32((int32_t)rnd()).v);fl(); mix(i64_to_f32((int64_t)rnd()).v);fl(); mix(ui32_to_f32((uint32_t)rnd()).v);fl(); mix(ui64_to_f32(rnd()).v);fl();
    }
    extFloat80_t p=pick80(),q=pick80(); softfloat_roundingMode=rm[i&3]; extF80_roundingPrecision=rp[(i>>1)%3];
    m80(extF80_add(p,q)); m80(extF80_sub(p,q)); m80(extF80_mul(p,q)); m80(extF80_div(p,q)); m80(extF80_sqrt(p)); m80(extF80_rem(p,q)); m80(extF80_roundToInt(p,rm[(i>>2)&3],true));
    mix(extF80_eq(p,q));fl(); mix(extF80_lt(p,q));fl(); mix(extF80_le(p,q));fl(); mix(extF80_eq_signaling(p,q));fl(); mix(extF80_lt_quiet(p,q));fl(); mix(extF80_le_quiet(p,q));fl(); mix(extF80_isSignalingNaN(p));
    mix(f64_to_extF80(a).signif);fl(); mix(f64_to_extF80(a).signExp); mix(extF80_to_f64(p).v);fl(); mix(extF80_to_f32(p).v);fl(); m80(f32_to_extF80(x));
    mix((uint64_t)extF80_to_i32(p,rm[i&3],true));fl(); mix((uint64_t)extF80_to_i64(p,rm[i&3],true));fl(); mix((uint64_t)extF80_to_i32_r_minMag(p,true));fl(); mix((uint64_t)extF80_to_i64_r_minMag(p,true));fl();
    m80(i32_to_extF80((int32_t)rnd())); m80(i64_to_extF80((int64_t)rnd()));
    extF80_roundingPrecision=80;
  }
  printf("%016llx\n",(unsigned long long)h); return 0; }
