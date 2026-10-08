// Include after geometry_fp64.hlsli. Exact base-16 long division using the
// same uint32 limbs, normal-input domain and round-to-nearest-even contract.
// The reference emits one quotient bit in each of 56 loop iterations. Here
// four bits are selected against precomputed denominator multiples, so no
// native float64 capability, approximate reciprocal or precision loss is used.
#ifdef __cplusplus
#define SF_RADIX_UINT std::uint32_t
#define SF_RADIX_LOOP
#else
#define SF_RADIX_UINT uint
#define SF_RADIX_LOOP [loop]
#endif
SF_NOINLINE Sf64 sf_div_radix16(Sf64 a,Sf64 b) {
    if(!sf_valid(a) || !sf_valid(b) || sf_zero(b))return sf_invalid();
    SF_RADIX_UINT sign=(a.hi^b.hi)&0x80000000U;
    if(sf_zero(a))return sf_make(0,sign);
    int exponent=int((a.hi>>20)&2047U)-int((b.hi>>20)&2047U)+1023;
    Sf64 remainder=sf_make(a.lo,(a.hi&0xfffffU)|0x100000U);
    Sf64 denominator=sf_make(b.lo,(b.hi&0xfffffU)|0x100000U);
    if(sf_less(remainder,denominator)) {remainder=sf_left(remainder,1);--exponent;}
    const Sf64 d2=sf_left(denominator,1),d4=sf_left(denominator,2),d8=sf_left(denominator,3);
    Sf64 quotient=sf_make(0,0);
    // Entry remainder is below 2D. 8R is below 16D (<2^57), and subtracting
    // 8D,4D,2D,D selects floor(8R/D) exactly. The final left shift restores
    // the reference's next remainder, including the last sticky-bit state.
    SF_RADIX_LOOP for(int digit=0;digit<14;++digit) {
        remainder=sf_left(remainder,3);
        SF_RADIX_UINT value=0;
        if(!sf_less(remainder,d8)) {remainder=sf_usub(remainder,d8);value|=8U;}
        if(!sf_less(remainder,d4)) {remainder=sf_usub(remainder,d4);value|=4U;}
        if(!sf_less(remainder,d2)) {remainder=sf_usub(remainder,d2);value|=2U;}
        if(!sf_less(remainder,denominator)) {remainder=sf_usub(remainder,denominator);value|=1U;}
        quotient=sf_left(quotient,4);quotient.lo|=value;
        remainder=sf_left(remainder,1);
    }
    if((remainder.lo|remainder.hi)!=0)quotient.lo|=1U;
    SF_RADIX_UINT tail=quotient.lo&7U;
    Sf64 mantissa=sf_right(quotient,3);
    if(tail>4U || (tail==4U && (mantissa.lo&1U)!=0))mantissa=sf_uadd(mantissa,sf_make(1,0));
    if((mantissa.hi&0x200000U)!=0) {mantissa=sf_right(mantissa,1);++exponent;}
    if(exponent<=0 || exponent>=2047)return sf_invalid();
    return sf_make(mantissa.lo,sign|(SF_RADIX_UINT(exponent)<<20)|(mantissa.hi&0xfffffU));
}
#undef SF_RADIX_UINT
#undef SF_RADIX_LOOP
