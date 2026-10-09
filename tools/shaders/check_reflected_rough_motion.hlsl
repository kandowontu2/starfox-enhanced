// Diagnostic only: execute the actual candidate optical helper, with no ray
// query or downloaded oracle in the implementation under test.
#include "../../src/render/shaders/reflection_rough_hit_motion.hlsli"
ByteAddressBuffer cases : register(t0,space0);
RWByteAddressBuffer results : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint count;uint3 reserved;};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    uint at=id.x*176;
    ReflectionRoughFrame old;
    old.a=asfloat(cases.Load4(at));old.b=asfloat(cases.Load4(at+16));old.c=asfloat(cases.Load4(at+32));
    old.projection=asfloat(cases.Load4(at+112));old.extentClip=asfloat(cases.Load4(at+128));old.settings=asfloat(cases.Load4(at+144));
    float4 qa=asfloat(cases.Load4(at+48)),qb=asfloat(cases.Load4(at+64)),qc=asfloat(cases.Load4(at+80));
    results.Store4(id.x*48,asuint(reflection_rough_hit_motion(old,qa,qb,qc,asfloat(cases.Load2(at+96)))));
    float3 hit,outgoing;float depth,bias;
    bool valid=reflection_rough_frame_valid(old) && reflection_rough_ray(asfloat(cases.Load2(at+160)),old,hit,outgoing,depth,bias);
    results.Store4(id.x*48+16,asuint(valid?float4(hit,bias):float4(0,0,0,0)));
    results.Store4(id.x*48+32,asuint(valid?float4(outgoing,depth):float4(0,0,0,0)));
}
