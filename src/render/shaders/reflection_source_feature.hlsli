#ifndef STARFOX_REFLECTION_SOURCE_FEATURE
#define STARFOX_REFLECTION_SOURCE_FEATURE
bool control_valid(uint control) {
    const uint hops=control&7U,mask=(control>>4)&15U,kind=control>>8;
    return hops<=4 && (mask>>hops)==0 && kind>=1 && kind<=3;
}
bool feature_valid(float3 f,uint kind) {
    if(!all(isfinite(f)))return false;
    if(kind==1)return f.z==0 && all(f.xy>=0) && f.x+f.y<=1;
    // Candidate enclosure only: rounded dot products must not delete a tap.
    // The real source guard retains its .0001 unit-vector bound.
    return (kind==2 || kind==3) && abs(dot(f,f)-1)<.000101;
}
#endif
