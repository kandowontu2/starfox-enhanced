// Source MSH_VIZIS: signed-word differences, wrapping signed 32-bit area,
// and odd-behind correction. Projected points are consumed on-device.
#include "source_visibility.hlsli"
[[vk::binding(0,0)]] StructuredBuffer<int4> projected : register(t0, space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> faces : register(t1, space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint> visible : register(u0, space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0, space2) { uint count; uint point_count; uint2 padding; };
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=count) return;
    if(faces[id.x].w==1U) {visible[id.x]=1;return;}
    // Destruction sprites test their original centre before fragment motion.
    if(faces[id.x].w==3U) {
        uint centre=faces[id.x].x;
        visible[id.x]=centre<point_count?uint(projected[centre].w>0):0;
        return;
    }
    uint3 indices=faces[id.x].xyz;
    if(any(indices>=point_count)) {visible[id.x]=0;return;}
    int4 a=projected[indices.x],b=projected[indices.y],c=projected[indices.z];
    visible[id.x]=sourceWordVisibility(a,b,c);
}
