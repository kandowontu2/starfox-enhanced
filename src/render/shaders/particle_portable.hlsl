#include "geometry_fp64.hlsli"
// xyz are signed source words. current.w packs palette byte and trail bit 8.
struct Particle {int4 current,previous;};
struct Projected {int4 current,previous;};
[[vk::binding(0,0)]] StructuredBuffer<Particle> particles:register(t0,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Projected> projected:register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings:register(b0,space2) {
    uint2 ownerX,ownerY,ownerZ,alpha;
    uint width,height,count,padding;
    float eyeX,convergence;uint2 reserved;
};
Sf64 number(float x) {return sf_from_float_bits(asuint(x));}
Sf64 raw(uint2 x) {return sf_make(x.x,x.y);}
int truncSource(Sf64 x) {
    int exponent=int((x.hi>>20)&2047)-1023;
    if(exponent<0) return 0;
    if(exponent>30) return (x.hi>>31)!=0?-2147483647:2147483647;
    uint magnitude=sf_right(sf_make(x.lo,(x.hi&0xfffff)|0x100000),uint(52-exponent)).lo;
    return (x.hi>>31)!=0?-int(magnitude):int(magnitude);
}
Sf64 interpolate(int previous,int current) {
    int delta=current-previous;
    if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
    return sf_add(number(float(previous)),sf_mul(number(float(delta)),raw(alpha)));
}
int4 projectPoint(Sf64 x,Sf64 y,Sf64 z) {
    if(!sf_valid(x) || !sf_valid(y) || !sf_valid(z) || (z.hi>>31)!=0 || sf_less(z,number(256))) return 0;
    Sf64 px=sf_div(sf_mul(x,number(256)),z);
    Sf64 py=sf_div(sf_mul(y,number(256)),z);
    if(eyeX!=0 && convergence>0)
        px=sf_add(px,sf_mul(number(256*eyeX),sf_sub(sf_div(number(1),number(convergence)),sf_div(number(1),z))));
    int sx=int(width/2)+truncSource(px),sy=int(height/2)+truncSource(py);
    if(sx<0 || sy<0 || sx>=int(width)-1 || sy>=int(height)-1) return 0;
    return int4(sx,sy,0,1);
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    Particle p=particles[id.x];Projected result=(Projected)0;
    result.current=projectPoint(sf_add(raw(ownerX),interpolate(p.previous.x,p.current.x)),
        sf_add(raw(ownerY),interpolate(p.previous.y,p.current.y)),
        sf_add(raw(ownerZ),interpolate(p.previous.z,p.current.z)));
    if(result.current.w!=0) {
        result.current.z=p.current.w&255;
        if((p.current.w&256)!=0) {
            result.previous=projectPoint(sf_add(raw(ownerX),number(float(p.previous.x))),
                sf_add(raw(ownerY),number(float(p.previous.y))),sf_add(raw(ownerZ),number(float(p.previous.z))));
            if(result.previous.w==0) result.current=0;
            else result.current.w=3;
        }
    }
    projected[id.x]=result;
}
