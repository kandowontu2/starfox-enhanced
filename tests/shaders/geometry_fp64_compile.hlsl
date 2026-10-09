// Compile-only portability probe; this is not a production rendering shader.
#include "../../src/render/shaders/geometry_fp64.hlsli"
[[vk::binding(0,0)]] StructuredBuffer<uint4> inputs : register(t0,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint4> outputs : register(u0,space1);
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint4 pair=inputs[id.x];
    Sf64 a=sf_make(pair.x,pair.y),b=sf_make(pair.z,pair.w);
    Sf64 sum=sf_add(a,b),difference=sf_sub(a,b),product=sf_mul(a,b),quotient=sf_div(a,b);
    outputs[id.x*2]=uint4(sum.lo,sum.hi,difference.lo,difference.hi);
    outputs[id.x*2+1]=uint4(product.lo,product.hi,quotient.lo,quotient.hi);
}
