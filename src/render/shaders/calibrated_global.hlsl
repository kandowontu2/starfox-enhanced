#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,scale,flags;
    uint selections;float seconds;uint contrast,chromatic;
};
struct Fullscreen {float4 position : SV_Position;};
float3 authored_rgb(uint2 pixel) {
    float3 rgb=source.Load(int3(pixel,0)).rgb;
    return round(saturate((flags&1U)!=0?calibrated_encode_srgb(rgb):rgb)*255.)/255.;
}
float3 contrast_rgb(uint2 pixel) {
    float3 rgb=authored_rgb(pixel);
    if(contrast==0 || ownership.Load(int3(pixel,0)).b==0) return rgb;
    float exposure=contrast==1?1.15:contrast==2?1.35:1.65;
    float strength=contrast*.2;
    float3 lifted=rgb*exposure/(1+rgb*(exposure-1));
    float3 shaped=lifted*lifted*(3-2*lifted);
    // Preserve the byte rounding between the two desktop operations, without
    // an intermediate image or a second per-eye dispatch.
    return floor(saturate(lifted*(1-strength)+shaped*strength)*255.+.5)/255.;
}
float4 appearance_colour(uint2 pixel,float4 original) {
    uint layer=uint(round(ownership.Load(int3(pixel,0)).b*255.));
    if(layer==0 || (contrast==0 && chromatic==0)) return original;
    float3 rgb=contrast_rgb(pixel);
    if(layer==2 && chromatic!=0) {
        float amount=(chromatic==1?.6:chromatic==2?1.2:2.4)*scale;
        float2 delta=(2*(float2(pixel)+.5)/float2(width,height)-1)*amount;
        for(uint channel=0;channel<3;channel+=2) {
            float2 at=clamp(float2(pixel)+(channel==0?1:-1)*delta,float2(0,0),float2(width-1,height-1));
            uint2 lo=uint2(at),hi=min(lo+1,uint2(width-1,height-1));float2 f=at-lo;
            uint2 taps[4]={lo,uint2(hi.x,lo.y),uint2(lo.x,hi.y),hi};float v[4];
            for(uint tap=0;tap<4;++tap)
                v[tap]=uint(round(ownership.Load(int3(taps[tap],0)).b*255.))==2?contrast_rgb(taps[tap])[channel]:rgb[channel];
            rgb[channel]=floor(lerp(lerp(v[0],v[1],f.x),lerp(v[2],v[3],f.x),f.y)*255.+.5)/255.;
        }
    }
    original.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    return original;
}
float global_sample(float sx,float sy,uint channel,uint2 origin) {
    float2 at=clamp(float2(sx,sy),float2(0,0),float2(width-1,height-1));
    uint2 base=uint2(at);float2 fraction=at-base;float value=0;
    for(uint dy=0;dy<2;++dy) for(uint dx=0;dx<2;++dx) {
        uint2 pixel=min(base+uint2(dx,dy),uint2(width-1,height-1));
        if(ownership.Load(int3(pixel,0)).b==0) pixel=origin;
        value+=authored_rgb(pixel)[channel]*(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);
    }
    return value;
}
float global_channel(float x,float y,float w,float h,float step,float seconds,uint packed,uint channel,uint2 origin) {
#define G_MIN min
#define G_MAX max
#define G_SAMPLE(a,b) global_sample(a,b,channel,origin)
#include "../../../include/starfox/render/global_enhancements.inc"
#undef G_SAMPLE
#undef G_MIN
#undef G_MAX
}
float4 global_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    float4 output=source.Load(int3(pixel,0));
    if((flags&2U)!=0) return appearance_colour(pixel,output);
    if(selections==0 || ownership.Load(int3(pixel,0)).b==0) return output;
    float3 rgb;
    for(uint channel=0;channel<3;++channel) {
        float value=global_channel(float(pixel.x),float(pixel.y),float(width),float(height),float(scale),seconds,selections,channel,pixel);
        rgb[channel]=floor(clamp(value*255.+.5,0.,255.))/255.;
    }
    output.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    return output;
}
