#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> history : register(t2,space2);
SamplerState historySampler : register(s2,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,selection,flags;
    float decay,intensity;uint2 padding;
};
struct Fullscreen {float4 position : SV_Position;};
struct Persistent {float4 colour : SV_Target0;float4 memory : SV_Target1;};
Persistent persistence_fragment_main(Fullscreen input) {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    Persistent output;output.colour=source.Load(int3(pixel,0));output.memory=0;
    uint layer=uint(round(ownership.Load(int3(pixel,0)).b*255.));
    if(layer==0 || (layer!=1 && (selection&1U)==0)) return output;
    bool selected=layer==1?(selection&2U)!=0:(selection&1U)!=0;
    float3 rgb=round(saturate((flags&1U)!=0?calibrated_encode_srgb(output.colour.rgb):output.colour.rgb)*255.);
    // Match desktop phosphor: green persists longest, red/blue decay twice /
    // three times as quickly. Trails and long exposure use uniform RGB decay.
    float3 channelDecay=(flags&4U)!=0?float3(decay*decay,decay,decay*decay*decay):float3(decay,decay,decay);
    float3 remembered=(flags&2U)!=0?float3(0,0,0):history.Load(int3(pixel,0)).rgb*channelDecay;
    output.memory.rgb=max(selected?rgb:float3(0,0,0),remembered);
    float3 result=floor(clamp(rgb+(max(rgb,output.memory.rgb)-rgb)*intensity/100.,0.,255.)+.5)/255.;
    output.colour.rgb=(flags&1U)!=0?calibrated_decode_srgb(result):result;
    return output;
}
