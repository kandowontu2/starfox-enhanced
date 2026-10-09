#include "../../render/shaders/geometry_fp64.hlsli"
struct Point {float4 camera;float4 screen;};
[[vk::binding(0,0)]] ByteAddressBuffer points;
[[vk::binding(1,0)]] StructuredBuffer<Point> residuals;
[[vk::binding(2,0)]] StructuredBuffer<uint4> corners;
[[vk::binding(3,0)]] StructuredBuffer<uint4> triangles;
[[vk::binding(0,1)]] RWStructuredBuffer<float4> positions;
[[vk::binding(0,2)]] cbuffer Settings {
    uint count;uint pointCount;uint cornerCount;uint residualMode;
    float4 row0;float4 row1;float4 row2;
};
// One triangle per lane, independent of view visibility/BSP traversal.
// Output is three float4 vertices (16-byte DXR vertex stride). Invalid input
// produces a degenerate triangle, never stale positions or out-of-range reads.
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    float3 transformed[3];bool valid=true;
    uint3 indices=triangles[id.x].xyz;
    for(uint c=0;c<3;++c) {
        transformed[c]=0;
        if(indices[c]>=cornerCount) {valid=false;continue;}
        uint index=corners[indices[c]].x;
        if(index>=pointCount) {valid=false;continue;}
        // High bit selects an ordinary interleaved vertex buffer: low bits
        // carry byte stride, position float3 is first. Procedural vertices
        // must be resolved by their own producer before selecting this mode.
        bool legacy=(residualMode&0x80000000U)!=0;
        uint stride=legacy?(residualMode&0x7fffffffU):32U;
        uint needed=legacy?(stride>=160?160U:12U):16U;
        uint bytes;points.GetDimensions(bytes);
        if(stride<needed || (stride&3U)!=0 || bytes<needed || index>(bytes-needed)/stride) {valid=false;continue;}
        float4 p=legacy?float4(asfloat(points.Load3(index*stride)),1):asfloat(points.Load4(index*stride));
        float3 value=p.xyz;
        if(p.w==0) {valid=false;continue;}
        if(!legacy && residualMode!=0) {
            Point tail=residuals[index];
            if(tail.camera.w==3) {
                for(uint axis=0;axis<3;++axis)
                    value[axis]=asfloat(sf_to_float_bits(sf_make(asuint(tail.camera[axis]),asuint(tail.screen[axis]))));
            } else value+=tail.camera.xyz;
        }
        transformed[c]=float3(dot(row0,float4(value,1)),dot(row1,float4(value,1)),dot(row2,float4(value,1)));
        // SceneVertex ABI: texture flags at 148, billboard offset at 152.
        // Same eye-facing placement as scene.hlsl, converted to +Y-down rays.
        if(legacy && stride>=160 && (points.Load(index*stride+148)&4U)!=0) {
            float2 offset=asfloat(points.Load2(index*stride+152));
            if((points.Load(index*stride+148)&134217728U)!=0) {
                float2 size_depth=asfloat(points.Load2(index*stride+88));
                bool sprite_valid=all(isfinite(size_depth)) && size_depth.x>0 && size_depth.y>=128;
                float dimension=sprite_valid?clamp(trunc(size_depth.x*256./size_depth.y),0.,240.):0;
                valid=valid && sprite_valid && dimension>0;
                offset*=dimension*size_depth.y/512.;
            }
            float2 scale=float2(length(float3(row0.x,row1.x,row2.x)),length(float3(row0.y,row1.y,row2.y)));
            transformed[c].xy+=offset*scale*float2(1,-1);
        }
        valid=valid && all(isfinite(transformed[c]));
    }
    for(uint c=0;c<3;++c) positions[id.x*3+c]=valid?float4(transformed[c],1):float4(0,0,0,0);
}
