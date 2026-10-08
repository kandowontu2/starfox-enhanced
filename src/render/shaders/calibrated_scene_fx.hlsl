#include "calibrated_colour.hlsli"
#if defined(STARFOX_SCENE_FX_PROJECT)
RWStructuredBuffer<float4> projected : register(u0,space1);
cbuffer ProjectionSettings : register(b0,space2) {
    float4 viewRows[3]; // source delta to actual tracked eye, in metres.
    float4 projection; // P00, P11, P02, P12.
    uint width,height,scale,count;
    float4 originHigh,originLow,playerHigh,playerLow;
    uint hasPlayer;uint3 padding;
    float4 previousViewRows[3];
    float4 previousProjection,previousOriginHigh,previousOriginLow;
};
// SDL's Vulkan uniform descriptor range is 4 KiB even when the device can
// support larger buffers. Keep every binding below that portable range.
cbuffer PointsA : register(b1,space2) {float4 pointsA[192];};
cbuffer PointsB : register(b2,space2) {float4 pointsB[192];};
#if defined(STARFOX_SCENE_FX_PARTICLES)
cbuffer PreviousPointsA : register(b3,space2) {float4 previousPointsA[192];};
float4 previous_point(uint n) {
    float4 high=previousPointsA[n*2],low=previousPointsA[n*2+1];
    if(low.w==0) return 0;
    float3 delta=fmod(fmod(high.xyz-previousOriginHigh.xyz,65536.)+low.xyz-previousOriginLow.xyz,65536.);
    [unroll] for(uint axis=0;axis<3;++axis) {
        if(delta[axis]>32767.) delta[axis]-=65536.;
        else if(delta[axis]< -32768.) delta[axis]+=65536.;
    }
    float4 position=float4(delta,1);
    float3 p=float3(dot(previousViewRows[0],position),-dot(previousViewRows[1],position),-dot(previousViewRows[2],position))*256.;
    if(p.z<32 || p.z>10000) return 0;
    return float4(width*(1-previousProjection.z)*.5-.5+width*previousProjection.x*.5*p.x/p.z,
        height*(1+previousProjection.w)*.5-.5+height*previousProjection.y*.5*p.y/p.z,p.z,1);
}
#endif
float4 raw_point(uint index) {
    if(index<192) return pointsA[index];
    return pointsB[index-192];
}
float3 source_eye(float3 high,float3 low) {
    // Split conversion preserves sub-unit source coordinates even at large
    // wrapped origins without requiring double-precision GPU capabilities.
    float3 delta=fmod(fmod(high-originHigh.xyz,65536.)+low-originLow.xyz,65536.);
    [unroll] for(uint axis=0;axis<3;++axis) {
        if(delta[axis]>32767.) delta[axis]-=65536.;
        else if(delta[axis]< -32768.) delta[axis]+=65536.;
    }
    float4 p=float4(delta,1);
    return float3(dot(viewRows[0],p),-dot(viewRows[1],p),-dot(viewRows[2],p))*256.;
}
[numthreads(1,1,1)]
void scene_fx_project_main(uint3 thread:SV_DispatchThreadID) {
    if(any(thread!=0)) return;
    float focal=width*projection.x*.5;
    float cx=width*(1-projection.z)*.5-.5,cy=height*(1+projection.w)*.5-.5;
    float focalY=height*projection.y*.5;
    projected[0]=float4(cx,cy,focal,0);
    projected[1]=float4(focalY/focal,0,0,0);
    float focus=hasPlayer!=0?source_eye(playerHigh.xyz,playerLow.xyz).z:0;
    uint visible=0;
    [loop] for(uint n=0;n<count && visible<48;++n) {
        uint i=n*4;float4 low=raw_point(i+1),kind=raw_point(i+2);
        float3 p=source_eye(raw_point(i).xyz,low.xyz);
        float radius=low.w,strength=kind.x,type=kind.y,age=kind.z;
#if defined(STARFOX_SCENE_FX_PARTICLES)
        if(type<2 || type>7) continue;
#endif
        if(p.z<32 || p.z>10000 || (type==8 && p.z<max(32.,focus*.6))) continue;
        float size=clamp(focal*radius/p.z,.5*scale,(type==8?30.:180.)*scale);
        float4 extra=raw_point(i+3);
        if(type==1) extra=float4(p,radius);
        if(type==8) extra=float4(focus,max(64.,focus*.5),0,0);
        uint at=2+visible*3;
        // Shared effect equations use focal X. Y is expressed in an equivalent
        // focal-X coordinate system; the fragment adapter maps gathers back to
        // real pixels. No symmetric-FOV assumption or shared centre-eye image.
        projected[at]=float4(cx+focal*p.x/p.z,cy+focal*p.y/p.z,size,strength);
        projected[at+1]=float4(type,p.z,age,0);projected[at+2]=extra;
#if defined(STARFOX_SCENE_FX_PARTICLES)
        projected[146+visible]=previous_point(n);
#endif
        ++visible;
    }
    projected[0].w=float(visible);
}
#else
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> surfaces : register(t2,space2);
SamplerState surfaceSampler : register(s2,space2);
StructuredBuffer<float4> projected : register(t3,space2);
cbuffer Settings : register(b0,space3) {uint width,height,scale,flags;};
struct Fullscreen {float4 position:SV_Position;};
float3 authored_rgb(uint2 pixel) {
    float3 rgb=source.Load(int3(pixel,0)).rgb;
    return round(saturate((flags&1U)!=0?calibrated_encode_srgb(rgb):rgb)*255.)/255.;
}
float scene_sample(float x,float y,uint channel,uint2 origin,bool heat,float focus,float range,float plume) {
    y=projected[0].y+(y-projected[0].y)*projected[1].x;
    float2 at=clamp(float2(x,y),float2(0,0),float2(width-1,height-1));
    uint2 base=uint2(at);float2 fraction=at-base;float value=0;
    for(uint dy=0;dy<2;++dy) for(uint dx=0;dx<2;++dx) {
        uint2 pixel=min(base+uint2(dx,dy),uint2(width-1,height-1));
        float depth=surfaces.Load(int3(pixel,0)).w;
        if(ownership.Load(int3(pixel,0)).b==0 || (heat && depth>0 && (abs(depth-focus)<range || depth+8<plume))) pixel=origin;
        value+=authored_rgb(pixel)[channel]*(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);
    }
    return value;
}
float scene_channel(float x,float y,uint channel,float depth,float normal_x,float normal_y,float normal_z,uint2 origin) {
#define S_MIN min
#define S_MAX max
#define S_SAMPLE(a,b) scene_sample(a,b,channel,origin,false,0,0,0)
#define S_HEAT_SAMPLE(a,b,f,r,z) scene_sample(a,b,channel,origin,true,f,r,z)
#define S_DATA(i,j) projected[2+(i)][j]
#define S_CAMERA(i) projected[0][i]
#include "../../../include/starfox/render/scene_enhancements.inc"
#undef S_CAMERA
#undef S_DATA
#undef S_HEAT_SAMPLE
#undef S_SAMPLE
#undef S_MAX
#undef S_MIN
}
float4 scene_fx_fragment_main(Fullscreen input):SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    float4 result=source.Load(int3(pixel,0));
    if(projected[0].w==0 || ownership.Load(int3(pixel,0)).b==0) return result;
    float4 surface=surfaces.Load(int3(pixel,0));float3 rgb;
    float x=float(pixel.x),y=projected[0].y+(float(pixel.y)-projected[0].y)/projected[1].x;
    for(uint channel=0;channel<3;++channel)
        rgb[channel]=floor(saturate(scene_channel(x,y,channel,surface.w,surface.x,surface.y,surface.z,pixel))*255.+.5)/255.;
    result.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    return result; // Exact original alpha, no additive ownership bleed into UI.
}
#endif
