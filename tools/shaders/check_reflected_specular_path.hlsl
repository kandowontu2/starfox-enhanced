// Execute shared optical code. No ray query or CPU inverse oracle supplies
// the expected pixel; the check's binary64 forward trace generates the input.
#include "../../src/render/shaders/reflection_specular_path_motion.hlsli"
ByteAddressBuffer cases : register(t0,space0);
RWByteAddressBuffer results : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint count;uint3 reserved;};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    uint at=id.x*336;
    ReflectionRoughFrame old;
    old.a=asfloat(cases.Load4(at));old.b=asfloat(cases.Load4(at+16));old.c=asfloat(cases.Load4(at+32));
    old.projection=asfloat(cases.Load4(at+48));old.extentClip=asfloat(cases.Load4(at+64));
    old.settings=asfloat(cases.Load4(at+80));
    ReflectionSpecularPlane planes[4];
    [unroll] for(uint hop=0;hop<4;++hop) {
        planes[hop].a=asfloat(cases.Load4(at+96+hop*48));
        planes[hop].b=asfloat(cases.Load4(at+112+hop*48));
        planes[hop].c=asfloat(cases.Load4(at+128+hop*48));
    }
    uint hops=cases.Load(at+320);float4 terminal=asfloat(cases.Load4(at+288));
    results.Store4(id.x*48,asuint(reflection_specular_path_motion(old,planes,hops,terminal)));
    float3 origin,outgoing;float depth,bias;
    bool valid=reflection_specular_forward(asfloat(cases.Load2(at+304)),old,planes,hops,true,origin,outgoing,depth,bias);
    results.Store4(id.x*48+16,asuint(valid?float4(origin,bias):float4(0,0,0,0)));
    results.Store4(id.x*48+32,asuint(valid?float4(outgoing,depth):float4(0,0,0,0)));
}
