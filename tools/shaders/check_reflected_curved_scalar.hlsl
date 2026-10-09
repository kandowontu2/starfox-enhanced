// Isolate the EXACT production binary32 high/low arithmetic on real drivers.
// This is scalar diagnosis, not an inverse guide or native radiance acceptance.
#define STARFOX_CURVED_SCALAR_ONLY 1
#include "../../src/render/shaders/reflection_curved_precision.hlsli"
ByteAddressBuffer cases:register(t0,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer Settings:register(b0,space2) {uint count,unused0,unused1,unused2;};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count)return;
    float4 input=asfloat(cases.Load4(id.x*16));float2 a=input.xy,b=input.zw;
    uint at=id.x*48;
#if defined(STARFOX_CURVED_DIVISION_TRACE)
    // Observable intermediate values isolate a driver transformation. This
    // diagnostic variant cannot stand in for the ordinary scalar gate.
    CurvedScalar q=curved_scalar(a.x/b.x,0),p=curved_dd_mul_core(q.high,q.low,b.x,b.y);
    CurvedScalar r=curved_dd_add_core(a.x,a.y,-p.high,-p.low);
    results.Store2(at,asuint(float2(q.high,q.low)));
    results.Store2(at+8,asuint(float2(p.high,p.low)));
    results.Store2(at+16,asuint(float2(r.high,r.low)));
    q=curved_dd_add_core(q.high,q.low,r.high/b.x,0);
    p=curved_dd_mul_core(q.high,q.low,b.x,b.y);r=curved_dd_add_core(a.x,a.y,-p.high,-p.low);
    results.Store2(at+24,asuint(float2(q.high,q.low)));
    results.Store2(at+32,asuint(float2(p.high,p.low)));
    results.Store2(at+40,asuint(float2(r.high,r.low)));
#else
    results.Store2(at,asuint(curved_dd_add(a,b)));
    results.Store2(at+8,asuint(curved_dd_mul(a,b)));
    results.Store2(at+16,asuint(curved_dd_div(a,b)));
    results.Store2(at+24,asuint(curved_dd_sqrt(curved_dd_add(curved_dd_abs(a),float2(1,0)))));
    results.Store2(at+32,asuint(curved_dd_sin(a)));
    results.Store2(at+40,asuint(curved_dd_cos(a)));
#endif
}
