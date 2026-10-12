// Isolated, exact-recipe native arithmetic control; not a colour/root owner.
#include "../../src/render/shaders/reflection_source_optical_interval.hlsli"
#include "../ci/checked_endpoints.hlsli"
ByteAddressBuffer input:register(t0,space0);
RWByteAddressBuffer output:register(u0,space1);
cbuffer Settings:register(b0,space2) {uint count,reserved0,reserved1,reserved2;};
[numthreads(64,1,1)]
void checked_endpoints_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count || reserved0 || reserved1 || reserved2)return;
    const uint2 raw=input.Load2(id.x*8);const float a=asfloat(raw.x),b=asfloat(raw.y);
    uint4 bounds=0;uint flags=0;
    [unroll] for(uint n=0;n<4;++n) {
        intervalFailed=false;
        const float bound=n>=2?endpoint_product_bound(a,b,(n&1)!=0):endpoint_sum_bound(a,b,(n&1)!=0);
        bounds[n]=asuint(bound);flags|=uint(intervalFailed)<<n;
    }
    output.Store4(id.x*32,uint4(raw,flags,0));output.Store4(id.x*32+16,bounds);
}

