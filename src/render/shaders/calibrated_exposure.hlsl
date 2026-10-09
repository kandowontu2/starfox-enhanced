#include "calibrated_colour.hlsli"
#if defined(STARFOX_EXPOSURE_APPLY)
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float> exposure : register(t2,space2);
SamplerState exposureSampler : register(s2,space2);
cbuffer Settings : register(b0,space3) {uint width,height,quality,flags;float delta;uint3 padding;};
struct Fullscreen {float4 position : SV_Position;};
float exposure_linear(float v) {return v<=.04045?v/12.92:pow((v+.055)/1.055,2.4);}
float exposure_encoded(float v) {return v<=.0031308?v*12.92:1.055*pow(v,1./2.4)-.055;}
float4 exposure_apply_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    float4 output=source.Load(int3(pixel,0));float stops=exposure.Load(int3(0,0,0));
    if(output.a==0 || ownership.Load(int3(pixel,0)).b==0 || stops==0) return output;
    float gain=exp2(stops);
    float3 authored=round(saturate((flags&1U)!=0?calibrated_encode_srgb(output.rgb):output.rgb)*255.)/255.;
    float3 result;
    for(uint c=0;c<3;++c) result[c]=floor(saturate(exposure_encoded(min(1,exposure_linear(authored[c])*gain)))*255.+.5)/255.;
    output.rgb=(flags&1U)!=0?calibrated_decode_srgb(result):result;return output;
}
#else
cbuffer Settings : register(b0,space2) {uint width,height,quality,flags;float delta;uint3 padding;};
groupshared uint histogram[64];
#if defined(STARFOX_EXPOSURE_METER)
Texture2D<float4> source : register(t0,space0);
SamplerState sourceSampler : register(s0,space0);
Texture2D<float4> ownership : register(t1,space0);
SamplerState ownershipSampler : register(s1,space0);
RWTexture2D<uint> tiles : register(u0,space1);
[numthreads(8,8,1)]
void exposure_meter_main(uint3 group : SV_GroupID,uint lane : SV_GroupIndex) {
    histogram[lane]=0;GroupMemoryBarrierWithGroupSync();
    for(uint y=0;y<4;++y) for(uint x=0;x<4;++x) {
        uint2 at=group.xy*32+uint2(lane%8,lane/8)+uint2(x,y)*8;
        if(at.x>=width || at.y>=height || ownership.Load(int3(at,0)).b==0) continue;
        float4 color=source.Load(int3(at,0));if(color.a==0) continue;
        float3 authored=round(saturate((flags&1U)!=0?calibrated_encode_srgb(color.rgb):color.rgb)*255.)/255.;
        float luminance=dot(calibrated_decode_srgb(authored),float3(.2126,.7152,.0722));
        if(luminance<1./1024) continue;
        uint bin=uint(clamp(int((log2(luminance)+10)*6.4),0,63));InterlockedAdd(histogram[bin],1);
    }
    GroupMemoryBarrierWithGroupSync();uint tile=group.y*((width+31)/32)+group.x;
    tiles[uint2(lane+(tile/4096)*64,tile%4096)]=histogram[lane];
}
#else
Texture2D<float> accepted : register(t0,space0);
SamplerState acceptedSampler : register(s0,space0);
// Integer histograms are storage-load resources, not sampled textures. DX12
// supports R32_UINT loads/stores but does not advertise filtered sampling.
Texture2D<uint> tiles : register(t1,space0);
RWTexture2D<float> pending : register(u0,space1);
[numthreads(8,8,1)]
void exposure_reduce_main(uint lane : SV_GroupIndex) {
    uint total=0,count=((width+31)/32)*((height+31)/32);
    for(uint tile=0;tile<count;++tile) total+=tiles.Load(int3(lane+(tile/4096)*64,tile%4096,0));
    histogram[lane]=total;GroupMemoryBarrierWithGroupSync();
    if(lane!=0) return;
    uint samples=0;for(uint b=0;b<64;++b) samples+=histogram[b];
    uint trim=samples/20,cumulative=0,inliers=0;float sum=0;
    for(uint b=0;b<64;++b) {
        uint end=cumulative+histogram[b],first=max(cumulative,trim),last=min(end,samples-trim);
        if(last>first) {inliers+=last-first;sum+=float(last-first)*(-10+(float(b)+.5)/6.4);}cumulative=end;
    }
    float limit=.5*quality,target=inliers!=0?clamp(log2(.18)-sum/inliers,-limit,limit):0;
    float stops=0;
    if((flags&2U)==0) {
        stops=accepted.Load(int3(0,0,0));float tau=target<stops?.25:.8;
        stops+=(target-stops)*(1-exp(-delta/tau));
    }
    pending[uint2(0,0)]=stops;
}
#endif
#endif
