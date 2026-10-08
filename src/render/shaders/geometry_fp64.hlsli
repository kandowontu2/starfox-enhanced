// Portable binary64 geometry arithmetic using only 32-bit integer operations.
// Shared with host tests. Normal finite inputs and signed zero are supported;
// subnormal inputs/results, infinities and NaNs return a canonical NaN sentinel.
// Used by diagnostic GPU geometry. No shaderFloat64 or shaderInt64 requirement.
// Keep the heavy shader arithmetic shared: inlining at every geometry call
// site can exhaust compiler memory. Host tests retain ordinary C++ inlining.
#ifdef __cplusplus
#include <cstdint>
#define SF_UINT std::uint32_t
#define SF_LOOP
#define SF_NOINLINE
#else
#define SF_UINT uint
#define SF_LOOP [loop]
#ifndef SF_NOINLINE
#define SF_NOINLINE [noinline]
#endif
#endif
struct Sf64 { SF_UINT lo; SF_UINT hi; };
Sf64 sf_make(SF_UINT lo,SF_UINT hi) { Sf64 r;r.lo=lo;r.hi=hi;return r; }
bool sf_zero(Sf64 a) {return a.lo==0 && (a.hi&0x7fffffffU)==0;}
bool sf_valid(Sf64 a) {
    SF_UINT e=(a.hi>>20)&2047U;
    return e!=2047U && (e!=0 || sf_zero(a));
}
Sf64 sf_invalid() {return sf_make(0,0x7ff80000U);}
Sf64 sf_neg(Sf64 a) {return sf_make(a.lo,a.hi^0x80000000U);}
Sf64 sf_left(Sf64 a,SF_UINT n) {
    if(n==0)return a;
    if(n>=64)return sf_make(0,0);
    if(n>=32)return sf_make(0,a.lo<<(n-32));
    return sf_make(a.lo<<n,(a.hi<<n)|(a.lo>>(32-n)));
}
Sf64 sf_right(Sf64 a,SF_UINT n) {
    if(n==0)return a;
    if(n>=64)return sf_make(0,0);
    if(n>=32)return sf_make(a.hi>>(n-32),0);
    return sf_make((a.lo>>n)|(a.hi<<(32-n)),a.hi>>n);
}
Sf64 sf_jam(Sf64 a,SF_UINT n) {
    Sf64 r=sf_right(a,n);
    bool lost=false;
    if(n>=64)lost=(a.lo|a.hi)!=0;
    else if(n>32)lost=a.lo!=0 || (a.hi<<(64-n))!=0;
    else if(n==32)lost=a.lo!=0;
    else if(n!=0)lost=(a.lo<<(32-n))!=0;
    if(lost)r.lo|=1U;
    return r;
}
Sf64 sf_uadd(Sf64 a,Sf64 b) {
    SF_UINT lo=a.lo+b.lo;
    return sf_make(lo,a.hi+b.hi+SF_UINT(lo<a.lo));
}
Sf64 sf_usub(Sf64 a,Sf64 b) {
    return sf_make(a.lo-b.lo,a.hi-b.hi-SF_UINT(a.lo<b.lo));
}
bool sf_less(Sf64 a,Sf64 b) {return a.hi<b.hi || (a.hi==b.hi && a.lo<b.lo);}
SF_NOINLINE Sf64 sf_add(Sf64 a,Sf64 b) {
    if(!sf_valid(a) || !sf_valid(b))return sf_invalid();
    if(sf_zero(a) && sf_zero(b))return sf_make(0,a.hi&b.hi&0x80000000U);
    if(sf_zero(a))return b;
    if(sf_zero(b))return a;
    Sf64 abs_a=sf_make(a.lo,a.hi&0x7fffffffU),abs_b=sf_make(b.lo,b.hi&0x7fffffffU);
    if(sf_less(abs_a,abs_b)) {Sf64 swap=a;a=b;b=swap;}
    int exponent=int((a.hi>>20)&2047U);
    SF_UINT difference=SF_UINT(exponent)-((b.hi>>20)&2047U);
    Sf64 x=sf_left(sf_make(a.lo,(a.hi&0xfffffU)|0x100000U),3);
    Sf64 y=sf_jam(sf_left(sf_make(b.lo,(b.hi&0xfffffU)|0x100000U),3),difference);
    Sf64 value;
    if(((a.hi^b.hi)&0x80000000U)==0)value=sf_uadd(x,y);
    else value=sf_usub(x,y);
    if((value.lo|value.hi)==0)return sf_make(0,0);
    if((value.hi&0x1000000U)!=0) {value=sf_jam(value,1);++exponent;}
    else while((value.hi&0x800000U)==0) {value=sf_left(value,1);--exponent;}
    SF_UINT tail=value.lo&7U;
    Sf64 mantissa=sf_right(value,3);
    if(tail>4U || (tail==4U && (mantissa.lo&1U)!=0))mantissa=sf_uadd(mantissa,sf_make(1,0));
    if((mantissa.hi&0x200000U)!=0) {mantissa=sf_right(mantissa,1);++exponent;}
    if(exponent<=0 || exponent>=2047)return sf_invalid();
    return sf_make(mantissa.lo,(a.hi&0x80000000U)|(SF_UINT(exponent)<<20)|(mantissa.hi&0xfffffU));
}
Sf64 sf_sub(Sf64 a,Sf64 b) {return sf_add(a,sf_neg(b));}
Sf64 sf_from_float_bits(SF_UINT bits) {
    SF_UINT sign=bits&0x80000000U,exponent=(bits>>23)&255U,mantissa=bits&0x7fffffU;
    if(exponent==255U)return sf_invalid();
    if(exponent==0 && mantissa==0)return sf_make(0,sign);
    int bias=int(exponent)+896;
    if(exponent==0) {
        bias=897;
        while((mantissa&0x800000U)==0) {mantissa<<=1;--bias;}
        mantissa&=0x7fffffU;
    }
    return sf_make(mantissa<<29,sign|(SF_UINT(bias)<<20)|(mantissa>>3));
}
SF_NOINLINE Sf64 sf_div(Sf64 a,Sf64 b) {
    if(!sf_valid(a) || !sf_valid(b) || sf_zero(b))return sf_invalid();
    SF_UINT sign=(a.hi^b.hi)&0x80000000U;
    if(sf_zero(a))return sf_make(0,sign);
    int exponent=int((a.hi>>20)&2047U)-int((b.hi>>20)&2047U)+1023;
    Sf64 remainder=sf_make(a.lo,(a.hi&0xfffffU)|0x100000U);
    Sf64 denominator=sf_make(b.lo,(b.hi&0xfffffU)|0x100000U);
    if(sf_less(remainder,denominator)) {remainder=sf_left(remainder,1);--exponent;}
    // Generate the 53 significand bits and three rounding bits exactly.
    // Remainder stays below twice the 53-bit denominator throughout.
    Sf64 quotient=sf_make(0,0);
    SF_LOOP for(int bit=0;bit<56;++bit) {
        quotient=sf_left(quotient,1);
        if(!sf_less(remainder,denominator)) {
            remainder=sf_usub(remainder,denominator);quotient.lo|=1U;
        }
        remainder=sf_left(remainder,1);
    }
    if((remainder.lo|remainder.hi)!=0)quotient.lo|=1U;
    SF_UINT tail=quotient.lo&7U;
    Sf64 mantissa=sf_right(quotient,3);
    if(tail>4U || (tail==4U && (mantissa.lo&1U)!=0))mantissa=sf_uadd(mantissa,sf_make(1,0));
    if((mantissa.hi&0x200000U)!=0) {mantissa=sf_right(mantissa,1);++exponent;}
    if(exponent<=0 || exponent>=2047)return sf_invalid();
    return sf_make(mantissa.lo,sign|(SF_UINT(exponent)<<20)|(mantissa.hi&0xfffffU));
}
SF_UINT sf_to_float_bits(Sf64 a) {
    if(!sf_valid(a))return 0x7fc00000U;
    SF_UINT sign=a.hi&0x80000000U;
    if(sf_zero(a))return sign;
    int exponent=int((a.hi>>20)&2047U)-896;
    Sf64 mantissa=sf_make(a.lo,(a.hi&0xfffffU)|0x100000U);
    // Keep 24 significand bits plus guard/round/sticky, including gradual
    // binary32 underflow. Binary64 subnormal inputs remain unsupported.
    SF_UINT shift=26U+SF_UINT(exponent<=0 ? 1-exponent : 0);
    Sf64 rounded=sf_jam(mantissa,shift);
    SF_UINT value=rounded.lo>>3,tail=rounded.lo&7U;
    if(tail>4U || (tail==4U && (value&1U)!=0))++value;
    if(exponent<=0)exponent=0;
    if(value>=0x1000000U) {value>>=1;++exponent;}
    if(exponent==0 && value>=0x800000U)exponent=1;
    if(exponent>=255)return sign|0x7f800000U;
    return sign|(SF_UINT(exponent)<<23)|(value&0x7fffffU);
}
Sf64 sf_u32_product(SF_UINT a,SF_UINT b) {
    // Exact 32x32 -> 64 using only uint32. Neither cross-term addition can
    // overflow: (65535*65535)+65535 <= UINT32_MAX. This same code runs in
    // host tests, SPIR-V, DXIL and Metal, without a 64-bit shader capability.
    SF_UINT low=(a&65535U)*(b&65535U);
    SF_UINT cross=(a>>16)*(b&65535U)+(low>>16);
    SF_UINT carry=cross>>16;
    SF_UINT middle=(a&65535U)*(b>>16)+(cross&65535U);
    return sf_make((middle<<16)|(low&65535U),
        (a>>16)*(b>>16)+carry+(middle>>16));
}
SF_NOINLINE Sf64 sf_mul(Sf64 a,Sf64 b) {
    if(!sf_valid(a) || !sf_valid(b))return sf_invalid();
    SF_UINT sign=(a.hi^b.hi)&0x80000000U;
    if(sf_zero(a) || sf_zero(b))return sf_make(0,sign);
    // Multiply the two 53-bit significands into four 32-bit limbs. Cross
    // terms fit in 54 bits; the upper pair fits in 42. Explicit carries keep
    // every product bit, without indexed arrays or bit-extraction loops.
    SF_UINT x=(a.hi&0xfffffU)|0x100000U,y=(b.hi&0xfffffU)|0x100000U;
    Sf64 low=sf_u32_product(a.lo,b.lo);
    Sf64 middle=sf_uadd(sf_u32_product(a.lo,y),sf_u32_product(x,b.lo));
    middle=sf_uadd(middle,sf_make(low.hi,0));
    Sf64 upper=sf_uadd(sf_u32_product(x,y),sf_make(middle.hi,0));
    int top=(upper.hi&512U)!=0 ? 105 : 104;
    int exponent=int((a.hi>>20)&2047U)+int((b.hi>>20)&2047U)-1023+(top-104);
    // Keep the leading 56 bits, then jam ALL discarded bits into sticky.
    // top-55 is 49 or 50, so the shift within the second limb is 17 or 18.
    SF_UINT shift=SF_UINT(top-87);
    Sf64 rounded=sf_make((middle.lo>>shift)|(upper.lo<<(32U-shift)),
        (upper.lo>>shift)|(upper.hi<<(32U-shift)));
    if(low.lo!=0 || (middle.lo<<(32U-shift))!=0)rounded.lo|=1U;
    SF_UINT tail=rounded.lo&7U;
    Sf64 mantissa=sf_right(rounded,3);
    if(tail>4U || (tail==4U && (mantissa.lo&1U)!=0))mantissa=sf_uadd(mantissa,sf_make(1,0));
    if((mantissa.hi&0x200000U)!=0) {mantissa=sf_right(mantissa,1);++exponent;}
    if(exponent<=0 || exponent>=2047)return sf_invalid();
    return sf_make(mantissa.lo,sign|(SF_UINT(exponent)<<20)|(mantissa.hi&0xfffffU));
}
#undef SF_UINT
#undef SF_LOOP
