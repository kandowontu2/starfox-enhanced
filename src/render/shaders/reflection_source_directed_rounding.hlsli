// Directed binary32 endpoints, without unconditional extra ULPs. This file is
// also compiled verbatim by the integer-dyadic host regression. No native FMA,
// binary64 shader arithmetic, driver rounding mode, epsilon or optical guard.
#ifndef STARFOX_REFLECTION_SOURCE_DIRECTED_ROUNDING
#define STARFOX_REFLECTION_SOURCE_DIRECTED_ROUNDING

OPTICAL_SCALAR_BOUNDARY float optical_add_bound(float a,float b,bool upper) {
    const uint aa=asuint(a)&0x7fffffffU,bb=asuint(b)&0x7fffffffU;
    if(aa<0x7f800000U && bb<0x7f800000U) {
        if(!aa)return b;
        if(!bb)return a;
        // DAZ can silently discard a nonzero input. Do not claim any arithmetic
        // enclosure in that case; exact zero identities above remain bitwise.
        if(aa<0x00800000U || bb<0x00800000U)intervalFailed=true;
        // |operands| in [2^-60,2^60]: every nonzero TwoSum residual/intermediate
        // is normal (at least 2^-83), even under FTZ. Cancellation may be exact
        // zero. Outside this proved range keep the original outward fallback.
        if(aa>=0x21800000U && aa<=0x5d800000U
            && bb>=0x21800000U && bb<=0x5d800000U) {
            precise float sum=a+b,v=sum-a;
            precise float error=(a-(sum-v))+(b-v);
            if(upper)return error>0?interval_up(sum):sum;
            return error<0?interval_down(sum):sum;
        }
    }
    precise float sum=a+b;
    return upper?interval_up(sum):interval_down(sum);
}

OPTICAL_SCALAR_BOUNDARY float optical_multiply_bound(float a,float b,bool upper) {
    const uint aa=asuint(a)&0x7fffffffU,bb=asuint(b)&0x7fffffffU;
    if(aa<0x7f800000U && bb<0x7f800000U) {
        if(!aa || !bb)return 0;
        if(asuint(a)==0x3f800000U)return b;
        if(asuint(b)==0x3f800000U)return a;
        if(asuint(a)==0xbf800000U)return asfloat(asuint(b)^0x80000000U);
        if(asuint(b)==0xbf800000U)return asfloat(asuint(a)^0x80000000U);
        if(aa<0x00800000U || bb<0x00800000U)intervalFailed=true;
        // Restricted Dekker product: all split products/error terms are normal
        // or exact zero (smallest possible nonzero product is 2^-106), and the
        // 4097 split cannot overflow. Do not trust a flushed residual elsewhere.
        if(aa>=0x30800000U && aa<=0x4e800000U
            && bb>=0x30800000U && bb<=0x4e800000U) {
            precise float product=a*b;
            precise float sa=a*4097,ah=sa-(sa-a),al=a-ah;
            precise float sb=b*4097,bh=sb-(sb-b),bl=b-bh;
            precise float error=((ah*bh-product)+ah*bl+al*bh)+al*bl;
            if(upper)return error>0?interval_up(product):product;
            return error<0?interval_down(product):product;
        }
    }
    precise float product=a*b;
    return upper?interval_up(product):interval_down(product);
}
#endif
