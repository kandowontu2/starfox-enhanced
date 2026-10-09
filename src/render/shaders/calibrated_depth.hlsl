#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> surfaces : register(t2,space2);
SamplerState surfaceSampler : register(s2,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,scale,flags;
    uint modes;float depthFocalY;uint2 padding;
    float4 depthCamera;
};
struct Fullscreen {float4 position : SV_Position;};
float3 authored_rgb(uint2 pixel) {
    float3 rgb=source.Load(int3(pixel,0)).rgb;
    return round(saturate((flags&1U)!=0?calibrated_encode_srgb(rgb):rgb)*255.)/255.;
}
float depth_surface(float x,float y,uint field) {
    // Match the desktop truncation rule, including its partial border taps.
    int2 p=int2(x,y);
    if(any(p<0) || p.x>=int(width) || p.y>=int(height) || ownership.Load(int3(p,0)).b==0) return 0;
    return surfaces.Load(int3(p,0))[field];
}
float depth_sample(float x,float y,uint channel,uint2 origin) {
    float2 at=clamp(float2(x,y),float2(0,0),float2(width-1,height-1));
    uint2 base=uint2(at);float2 f=at-base;float value=0;
    for(uint dy=0;dy<2;++dy) for(uint dx=0;dx<2;++dx) {
        uint2 p=min(base+uint2(dx,dy),uint2(width-1,height-1));
        if(ownership.Load(int3(p,0)).b==0) p=origin;
        value+=authored_rgb(p)[channel]*(dx?f.x:1-f.x)*(dy?f.y:1-f.y);
    }
    return value;
}
float ambient(float x,float y) {
#define D_MIN min
#define D_MAX max
#define D_SURFACE(a,b,c) depth_surface(a,b,c)
#define D_CAMERA(i) depthCamera[i]
#define D_FOCAL_Y depthFocalY
#define D_LOOP [loop]
#include "../../../include/starfox/render/ambient_occlusion.inc"
#undef D_LOOP
#undef D_FOCAL_Y
#undef D_CAMERA
#undef D_SURFACE
#undef D_MIN
#undef D_MAX
}
float depth_channel(float x,float y,uint channel,float ambient) {
#define D_MIN min
#define D_MAX max
#define D_COLOUR(a,b) depth_sample(a,b,channel,uint2(x,y))
#define D_SURFACE(a,b,c) depth_surface(a,b,c)
#define D_CAMERA(i) depthCamera[i]
#include "../../../include/starfox/render/depth_enhancements.inc"
#undef D_CAMERA
#undef D_SURFACE
#undef D_COLOUR
#undef D_MIN
#undef D_MAX
}
float4 depth_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    float4 output=source.Load(int3(pixel,0));
    if(modes==0 || ownership.Load(int3(pixel,0)).b==0 || surfaces.Load(int3(pixel,0)).w<=0) return output;
    float occlusion=ambient(float(pixel.x),float(pixel.y));float3 rgb;
    // AO is shared across RGB. In AO-only mode there is no neighbourhood colour
    // gather at all; the DOF path's three channels are explicitly exposed to
    // common-load elimination by the offline compiler.
    [branch] if((modes&12U)==0) rgb=authored_rgb(pixel)*occlusion;
    else [unroll] for(uint channel=0;channel<3;++channel)
        rgb[channel]=depth_channel(float(pixel.x),float(pixel.y),channel,occlusion);
    rgb=floor(saturate(rgb)*255.+.5)/255.;
    output.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    return output;
}
