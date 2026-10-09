#include "source_visibility.hlsli"
struct Point { float4 camera; float4 screen; };
[[vk::binding(0,0)]] StructuredBuffer<Point> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> faces : register(t1,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint> visible : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) { uint count; uint pointCount; uint2 padding; };
// Compensated FP32 arithmetic keeps cancellation around edge-on faces from
// replacing the CPU's 1e-12 tangent tolerance with ordinary float noise.
// No shaderFloat64 requirement: the same operations compile for Metal.
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    if(faces[id.x].w==1U) {visible[id.x]=1;return;}
    if(faces[id.x].w==3U) {
        uint centre=faces[id.x].x;
        visible[id.x]=centre<pointCount?uint(points[centre].screen.w>0):0;
        return;
    }
    uint3 index=faces[id.x].xyz;
    if(any(index>=pointCount)) {visible[id.x]=0;return;}
    float4 av=points[index.x].camera,bv=points[index.y].camera,cv=points[index.z].camera;
    visible[id.x]=sourceContinuousVisibility(av,bv,cv,faces[id.x].w);
}
