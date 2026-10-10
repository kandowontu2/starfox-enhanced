// Directed binary32 endpoints, without unconditional extra ULPs. This file is
// also compiled verbatim by the integer-dyadic host regression. No native FMA,
// binary64 shader arithmetic, driver rounding mode, epsilon or optical guard.
#ifndef STARFOX_REFLECTION_SOURCE_DIRECTED_ROUNDING
#define STARFOX_REFLECTION_SOURCE_DIRECTED_ROUNDING

// Preserve the explicit scalar boundaries and every original input/fallback
// guard. The product's exact residual sign is calculated once for both directed
// endpoints; each caller consumes the invocation-private upper bound immediately.
#ifndef OPTICAL_DIRECTED_BOUNDARY
#define OPTICAL_DIRECTED_BOUNDARY [noinline]
#endif
OPTICAL_DIRECTED_BOUNDARY float optical_add_bound(float a,float b,bool upper) {
    const uint aa=asuint(a)&0x7fffffffU,bb=asuint(b)&0x7fffffffU;
    if(aa<0x7f800000U && bb<0x7f800000U) {
        if(!aa)return b;
        if(!bb)return a;
        // DAZ can silently discard a nonzero input. Do not claim any arithmetic
        // enclosure in that case; exact zero identities above remain bitwise.
        if(aa<0x00800000U || bb<0x00800000U)intervalFailed=true;
        // Same [2^-60,2^60] range. Magnitude ordering establishes FastTwoSum's
        // |large| >= |small| premise. Every nonzero residual/intermediate is
        // normal (at least 2^-83); cancellation may be exact zero under FTZ.
        if(aa>=0x21800000U && aa<=0x5d800000U
            && bb>=0x21800000U && bb<=0x5d800000U) {
            precise float large=aa>=bb?a:b,small=aa>=bb?b:a;
            precise float sum=large+small,recovered=sum-large;
            precise float error=small-recovered;
            if(upper)return error>0?interval_up(sum):sum;
            return error<0?interval_down(sum):sum;
        }
    }
    precise float sum=a+b;
    return upper?interval_up(sum):interval_down(sum);
}

// One scalar-only boundary for both endpoints. This is invocation-private,
// like intervalFailed and the existing interval_*_upper values; no recursion
// or asynchronous access. Every path publishes its upper endpoint.
static float optical_product_upper=0;
OPTICAL_DIRECTED_BOUNDARY float optical_product_bounds(float a,float b) {
    const uint aa=asuint(a)&0x7fffffffU,bb=asuint(b)&0x7fffffffU;
    if(aa<0x7f800000U && bb<0x7f800000U) {
        if(!aa || !bb){optical_product_upper=0;return 0;}
        if(asuint(a)==0x3f800000U){optical_product_upper=b;return b;}
        if(asuint(b)==0x3f800000U){optical_product_upper=a;return a;}
        if(asuint(a)==0xbf800000U){
            const float result=asfloat(asuint(b)^0x80000000U);
            optical_product_upper=result;return result;
        }
        if(asuint(b)==0xbf800000U){
            const float result=asfloat(asuint(a)^0x80000000U);
            optical_product_upper=result;return result;
        }
        if(aa<0x00800000U || bb<0x00800000U)intervalFailed=true;
        // Same [2^-30,2^30] range: the rounded product is always normal.
        // Each normalized significand has exactly24 bits. Split16+8 yields
        // its exact48-bit product in two uint32 limbs using four products:
        // low16*low16 <= 0xfffe0001; cross < 2^25; high8*high8 <= 65025.
        // uint32 wrap plus the explicit carry is intentional, not float error.
        if(aa>=0x30800000U && aa<=0x4e800000U
            && bb>=0x30800000U && bb<=0x4e800000U) {
            precise float product=a*b;
            const uint am=(aa&0x7fffffU)|0x800000U,bm=(bb&0x7fffffU)|0x800000U;
            const uint al=am&0xffffU,ah=am>>16,bl=bm&0xffffU,bh=bm>>16;
            const uint base=al*bl,cross=al*bh+ah*bl;
            const uint exactLow=base+(cross<<16);
            const uint exactHigh=ah*bh+(cross>>16)+(exactLow<base?1U:0U);
            const uint rounded=asuint(product)&0x7fffffffU;
            const uint shift=(rounded>>23)-(aa>>23)-(bb>>23)+150U;
            // Normal24-bit significands multiply into [2^46,2^48).
            // RN binary32 therefore requires exactly23 or24 discarded bits.
            // Keep unexpected representations fail-closed before any shift.
            if(shift<23U || shift>24U) {
                intervalFailed=true;
                optical_product_upper=interval_up(product);
                return interval_down(product);
            }
            const uint rm=(rounded&0x7fffffU)|0x800000U;
            const uint roundedLow=rm<<shift,roundedHigh=rm>>(32U-shift);
            const bool greater=exactHigh>roundedHigh
                || (exactHigh==roundedHigh && exactLow>roundedLow);
            const bool less=exactHigh<roundedHigh
                || (exactHigh==roundedHigh && exactLow<roundedLow);
            const bool negative=((asuint(a)^asuint(b))&0x80000000U)!=0;
            optical_product_upper=(negative?less:greater)?interval_up(product):product;
            return (negative?greater:less)?interval_down(product):product;
        }
    }
    precise float product=a*b;
    optical_product_upper=interval_up(product);
    return interval_down(product);
}
// Retain the scalar API used by the unchanged independent dyadic regression.
// Native interval callers use the pair directly, not two duplicate products.
float optical_multiply_bound(float a,float b,bool upper) {
    const float lower=optical_product_bounds(a,b);
    return upper?optical_product_upper:lower;
}
#endif
