// Vertex correspondence for temporal reconstruction. Inputs are paired
// ContinuousProjectedPoint buffers from the same topology and eye. Neither
// buffer may include temporal jitter. Output is previous-minus-current in
// render pixels, current linear camera depth, and an explicit validity flag.
// The optional depth path reconstructs the visible surface after clipping and
// applies rigid-object correspondence. The scene merge can evaluate the same
// helper with each foreground object's depth/transform instead of this pass.
#include "rigid_motion.hlsli"
struct Point { float4 camera; float4 screen; };
[[vk::binding(0,0)]] ByteAddressBuffer currentPoints : register(t0,space0);
[[vk::binding(1,0)]] ByteAddressBuffer previousPoints : register(t1,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<float4> motion : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count; uint resetHistory; float scaleX; float scaleY;
    uint width,height,surfaceReset,dispatchRowStride;
    float4 currentProjection,previousProjection;
    float4 previousRow0,previousRow1,previousRow2;
    float jitterX,jitterY,previousNear,padding;
};
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    id.x+=id.y*dispatchRowStride;
    if(id.x>=count) return;
    if(width!=0) {
        float z=asfloat(currentPoints.Load(id.x*4));
        motion[id.x]=rigidSurfaceMotion(id.x,width,resetHistory,z,currentProjection,previousProjection,
            previousRow0,previousRow1,previousRow2,float2(jitterX,jitterY),previousNear);
        return;
    }
    Point now,old;
    now.camera=asfloat(currentPoints.Load4(id.x*32));now.screen=asfloat(currentPoints.Load4(id.x*32+16));
    old.camera=asfloat(previousPoints.Load4(id.x*32));old.screen=asfloat(previousPoints.Load4(id.x*32+16));
    float4 result=0;
    if(resetHistory==0 && now.camera.w>0 && old.camera.w>0
        && now.camera.z>0 && old.camera.z>0 && now.screen.w>0 && old.screen.w>0
        && all(isfinite(now.screen)) && all(isfinite(old.screen))
        && all(isfinite(now.camera)) && all(isfinite(old.camera))
        && isfinite(scaleX) && isfinite(scaleY) && scaleX>0 && scaleY>0) {
        float2 delta=(old.screen.xy-now.screen.xy)*float2(scaleX,scaleY);
        if(all(isfinite(delta))) result=float4(delta,now.camera.z,1);
    }
    motion[id.x]=result;
}
