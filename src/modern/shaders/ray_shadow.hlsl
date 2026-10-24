// MODERN ray-traced shadows, caster side (vr ray_expand.hlsl for SDL_GPU).
// One dispatch per compute-route world model: each lane turns one source
// triangle into frame eye space (native axes: +Y down, +Z forward, source
// units) and widens its object's bounds with atomics, so animated and
// exploding shapes need no CPU bounds. The scene fragment shader
// (scene_sdl.hlsl) traces sun rays against these objects and triangles.
// SDL_GPU compute bindings: read-only storage t#/space0, read-write storage
// u#/space1, uniforms b#/space2 (SPIR-V: set = space, binding = register).
#include "../../render/shaders/geometry_fp64.hlsli"
[[vk::binding(0,0)]] ByteAddressBuffer arena : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> topology : register(t1,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<float4> triangles : register(u0,space1);
// Per object: uint4 low (ordered x,y,z, first triangle), uint4 high
// (ordered x,y,z, triangle count; bit 31 = large scenery). The host seeds
// the bounds low=~0, high=0.
[[vk::binding(1,1)]] RWByteAddressBuffer objects : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count,pointCount,cornerCount,residualMode;
    // Arena word offsets of the points, residuals and corners regions.
    uint pointsWord,residualsWord,cornersWord,firstTriangle;
    uint objectIndex,settingsPad0,settingsPad1,settingsPad2;
    float4 row0,row1,row2;
    // Unit vector toward the sun (native eye space), for each triangle's
    // bounds across the sun's rays.
    float4 light;
};
// Float bits in an order unsigned comparison preserves.
uint ordered(float value) {
    uint bits=asuint(value);
    return (bits&0x80000000U)!=0?~bits:bits|0x80000000U;
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    uint3 indices=topology[id.x].xyz;
    float3 transformed[3];bool valid=true;
    for(uint c=0;c<3;++c) {
        transformed[c]=0;
        if(indices[c]>=cornerCount) {valid=false;continue;}
        uint index=arena.Load((cornersWord+indices[c]*4)*4);
        if(index>=pointCount) {valid=false;continue;}
        float4 p=asfloat(arena.Load4((pointsWord+index*8)*4));
        if(p.w==0) {valid=false;continue;}
        float3 value=p.xyz;
        if(residualMode!=0) {
            uint4 camera=arena.Load4((residualsWord+index*8)*4);
            uint4 screen=arena.Load4((residualsWord+index*8+4)*4);
            if(asfloat(camera.w)==3) {
                for(uint axis=0;axis<3;++axis)
                    value[axis]=asfloat(sf_to_float_bits(sf_make(camera[axis],screen[axis])));
            } else value+=asfloat(camera.xyz);
        }
        transformed[c]=float3(dot(row0,float4(value,1)),dot(row1,float4(value,1)),dot(row2,float4(value,1)));
        valid=valid && all(isfinite(transformed[c]));
    }
    // Four float4 per triangle: its corners, then its bounds on the plane
    // facing the sun (scene_sdl.hlsl shadow_basis): every shadow ray runs
    // nearly along the sun, so only points inside them can be blocked.
    uint at=(firstTriangle+id.x)*4;
    for(uint c=0;c<3;++c) triangles[at+c]=valid?float4(transformed[c],1):float4(0,0,0,0);
    float3 tangent=normalize(cross(light.xyz,abs(light.y)<.9?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(light.xyz,tangent);
    float2 across[3];
    for(uint c=0;c<3;++c) across[c]=float2(dot(transformed[c],tangent),dot(transformed[c],bitangent));
    triangles[at+3]=valid?float4(min(min(across[0],across[1]),across[2]),max(max(across[0],across[1]),across[2])):float4(1,1,0,0);
    if(!valid) return;
    float3 low=min(min(transformed[0],transformed[1]),transformed[2]);
    float3 high=max(max(transformed[0],transformed[1]),transformed[2]);
    uint base=objectIndex*32;
    uint previous;
    for(uint axis=0;axis<3;++axis) {
        objects.InterlockedMin(base+axis*4,ordered(low[axis]),previous);
        objects.InterlockedMax(base+16+axis*4,ordered(high[axis]),previous);
    }
}
