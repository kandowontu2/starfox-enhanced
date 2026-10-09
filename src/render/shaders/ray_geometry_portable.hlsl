#include "geometry_fp64.hlsli"
struct Point {uint4 camera;uint4 screen;};
[[vk::binding(0,0)]] StructuredBuffer<Point> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<Point> residuals : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint4> triangles : register(t2,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<float4> positions : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint triangleCount,pointCount,mode,firstVertex;
    float4 row0,row1,row2;
};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=triangleCount) return;
    bool valid=mode<=2;
    uint3 indices=0;if(valid) indices=triangles[id.x].xyz;
    float3 result[3];
    for(uint c=0;c<3;++c) {
        result[c]=0;uint index=indices[c];
        if(index>=pointCount) {valid=false;continue;}
        Point p=points[index];
        float3 value=mode==0?float3(asint(p.camera.xyz)):asfloat(p.camera.xyz);
        valid=valid && (mode==0?p.camera.w==0:asfloat(p.camera.w)>0);
        if(mode==2) {
            Point tail=residuals[index];
            if(asfloat(tail.camera.w)==3) {
                for(uint axis=0;axis<3;++axis)
                    value[axis]=asfloat(sf_to_float_bits(sf_make(tail.camera[axis],tail.screen[axis])));
            } else value+=asfloat(tail.camera.xyz);
        }
        result[c]=float3(dot(row0,float4(value,1)),dot(row1,float4(value,1)),dot(row2,float4(value,1)));
        valid=valid && all(isfinite(result[c]));
    }
    for(uint c=0;c<3;++c) positions[firstVertex+id.x*3+c]=valid?float4(result[c],1):float4(0,0,0,0);
}
