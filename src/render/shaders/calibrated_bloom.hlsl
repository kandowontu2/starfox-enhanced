#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> core : register(t2,space2);
SamplerState coreSampler : register(s2,space2);
Texture2D<float4> halo : register(t3,space2);
SamplerState haloSampler : register(s3,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,step,flags;
    uint reducedWidth,reducedHeight,model,world;
    uint stage;uint3 padding;
};
struct Fullscreen {float4 position : SV_Position;};
float3 authored_rgb(uint2 pixel) {
    float3 rgb=source.Load(int3(pixel,0)).rgb;
    return round(saturate((flags&1U)!=0?calibrated_encode_srgb(rgb):rgb)*255.)/255.;
}
float4 bloom_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);
    if(stage==0) {
        float3 bright=0;float strengths[4]={0,.4,.85,1.5};uint level=max(model,world);
        // Identical y-then-x bright extraction, soft knee and per-source
        // strengths as flat bloom. Partial edge cells retain the full divisor.
        for(uint y=pixel.y*step;y<min((pixel.y+1)*step,height);++y)
            for(uint x=pixel.x*step;x<min((pixel.x+1)*step,width);++x) {
                uint2 at=uint2(x,y);uint layer=uint(round(ownership.Load(int3(at,0)).b*255.));
                uint selected=layer==1?world:layer==2?model:0;
                if(selected==0) continue;
                float3 rgb=authored_rgb(at);float peak=max(rgb.r,max(rgb.g,rgb.b));peak*=peak;
                float knee=clamp(peak-.25,0.,.3);
                float contribution=max(peak-.4,knee*knee/.6)/max(peak,.001);
                contribution*=strengths[selected]/strengths[level];
                bright+=rgb*rgb*contribution/(float(step)*float(step));
            }
        return float4(bright,0);
    }
    if(stage<5) {
        float3 sum=0;int radius=stage<=2?1:4;bool horizontal=stage==1 || stage==3;
        for(int n=-radius;n<=radius;++n) {
            int2 at=clamp(int2(pixel)+(horizontal?int2(n,0):int2(0,n)),int2(0,0),int2(reducedWidth-1,reducedHeight-1));
            sum+=source.Load(int3(at,0)).rgb;
        }
        return float4(sum/float(radius*2+1),0);
    }
    float4 output=source.Load(int3(pixel,0));
    // Protected native UI/emissive/empty ownership never emits or receives
    // glow. Preserve its exact colour and every source pixel's alpha.
    if(max(model,world)==0 || ownership.Load(int3(pixel,0)).b==0) return output;
    float2 at=max(0.,(float2(pixel)+.5)/float(step)-.5);
    uint2 lo=min(uint2(at),uint2(reducedWidth-1,reducedHeight-1));
    uint2 hi=min(uint2(at)+1,uint2(reducedWidth-1,reducedHeight-1));
    float2 f=frac(at);uint2 taps[4]={lo,uint2(hi.x,lo.y),uint2(lo.x,hi.y),hi};float3 values[4];
    for(uint tap=0;tap<4;++tap)
        values[tap]=core.Load(int3(taps[tap],0)).rgb*.6+halo.Load(int3(taps[tap],0)).rgb*.8;
    float3 glow=lerp(lerp(values[0],values[1],f.x),lerp(values[2],values[3],f.x),f.y);
    float strengths[4]={0,.4,.85,1.5};float3 rgb=authored_rgb(pixel);
    rgb=floor(sqrt(saturate(rgb*rgb+glow*strengths[max(model,world)]))*255.+.5)/255.;
    output.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    return output;
}
