// Isolated arithmetic candidate. Not installed into the frozen game source.
// TwoSum/TwoProduct have exact residuals in this deliberately bounded normal
// range. Products of the least significant operand bits stay above 2^-126;
// splitting by 4097 stays far below overflow. Unsafe ranges keep the existing
// one-ULP outward fallback, including subnormal/FTZ-sensitive operands.
bool endpoint_safe(float a) {
    return isfinite(a) && (a==0 || (abs(a)>=3.637978807091713e-12F
        && abs(a)<=274877906944.F)); // [2^-38, 2^38]
}
float endpoint_sum_bound(float a,float b,bool upper) {
    precise float s=a+b;
    if(!endpoint_safe(a) || !endpoint_safe(b) || !isfinite(s))
        return upper?interval_up(s):interval_down(s);
    precise float v=s-a,e=(a-(s-v))+(b-v);
    // The residual decides the exact rounding direction; a zero residual is
    // exact, not an estimated epsilon or an enlarged acceptance tolerance.
    if(upper)return e>0?interval_up(s):s;
    return e<0?interval_down(s):s;
}
float endpoint_product_bound(float a,float b,bool upper) {
    precise float p=a*b;
    if(!endpoint_safe(a) || !endpoint_safe(b) || !isfinite(p))
        return upper?interval_up(p):interval_down(p);
    precise float sa=a*4097.F,ah=sa-(sa-a),al=a-ah;
    precise float sb=b*4097.F,bh=sb-(sb-b),bl=b-bh;
    precise float e=((ah*bh-p)+ah*bl+al*bh)+al*bl;
    if(upper)return e>0?interval_up(p):p;
    return e<0?interval_down(p):p;
}

