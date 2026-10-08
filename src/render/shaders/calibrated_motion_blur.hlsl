#include "calibrated_colour.hlsli"
#if defined(STARFOX_CALIBRATED_MOTION_PACK)
Texture2D<float4> source : register(t0,space0);SamplerState sourceSampler : register(s0,space0);
Texture2D<float4> underlay : register(t1,space0);SamplerState underlaySampler : register(s1,space0);
Texture2D<float4> ownership : register(t2,space0);SamplerState ownershipSampler : register(s2,space0);
Texture2D<float4> surfaces : register(t3,space0);SamplerState surfacesSampler : register(s3,space0);
Texture2D<float4> motion : register(t4,space0);SamplerState motionSampler : register(s4,space0);
ByteAddressBuffer waterRays : register(t5,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> packedSource : register(u0,space1);
[[vk::image_format("rgba8")]] RWTexture2D<float4> packedUnderlay : register(u1,space1);
RWStructuredBuffer<uint> packedOwnership : register(u2,space1);
RWStructuredBuffer<float> packedDepth : register(u3,space1);
RWStructuredBuffer<float4> packedMotion : register(u4,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,srgb,waterSurfaceOffset;float2 sampleDelta;float2 guideJitter;
};
[numthreads(8,8,1)]
void motion_pack_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    int3 pixel=int3(id.xy,0);uint i=id.y*width+id.x;
    float4 foreground=source.Load(pixel),background=underlay.Load(pixel);
    // The shared portable reconstruction consumes authored sRGB bytes and
    // integrates in linear light. Native sRGB targets already decode on Load.
    if(srgb!=0) {
        foreground.rgb=calibrated_encode_srgb(foreground.rgb);
        background.rgb=calibrated_encode_srgb(background.rgb);
    }
    packedSource[id.xy]=foreground;packedUnderlay[id.xy]=background;
    uint layer=uint(round(ownership.Load(pixel).b*255));
    float z=surfaces.Load(pixel).w;float4 prior=motion.Load(pixel);
    if(waterSurfaceOffset!=0 && (layer==1 || layer==2) && ownership.Load(pixel).r>0
        && (waterRays.Load(i*4)>>24)==253U) {
        float4 liquid=asfloat(waterRays.Load4(waterSurfaceOffset+i*16));
        if(all(isfinite(liquid)) && liquid.w>0) {
            layer=1;z=liquid.w;
            // A refracted surface has no rigid submerged-model velocity.
            // Keep it current-only until a liquid correspondence is supplied;
            // moving dry models retain their original physical shutter motion.
            prior=0;
        }
    }
    // Native classes 1/2 are world/model; 0 and every unknown label are
    // protected. Do NOT use the AA alpha bit as world/model ownership.
    bool eligible=layer==1 || layer==2;
    packedOwnership[i]=(eligible?(layer==2?2U:3U):1U)<<8;
    bool surface=eligible && isfinite(z) && z>0;
    bool valid=surface && prior.w==1 && all(isfinite(prior.xyz)) && prior.z>0;
    packedDepth[i]=surface?z:0;
    // TAA needs previous-depth metadata; blur needs CURRENT visible depth.
    // They cannot share the raw float4 convention. Remove the difference of
    // raster sample offsets without erasing physical camera/object motion.
    packedMotion[i]=float4(valid?prior.xy+sampleDelta:float2(0,0),surface?z:0,valid?1:0);
}
#else
Texture2D<float4> source : register(t0,space2);SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> blurred : register(t1,space2);SamplerState blurredSampler : register(s1,space2);
Texture2D<float4> ownership : register(t2,space2);SamplerState ownershipSampler : register(s2,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,srgb,padding;float2 sampleDelta;float2 guideJitter;
};
struct Fullscreen {float4 position : SV_Position;};
float4 motion_present_main(Fullscreen input):SV_Target0 {
    int3 pixel=int3(uint2(input.position.xy),0);
    float4 original=source.Load(pixel);uint layer=uint(round(ownership.Load(pixel).b*255));
    // Restore from native input, not a quantized staging copy. Protected ink
    // and alpha stay exact in all four colour formats.
    if((layer!=1 && layer!=2) || original.a==0) return original;
    float3 result=blurred.Load(pixel).rgb;
    float3 originalBytes=round(saturate(srgb!=0?calibrated_encode_srgb(original.rgb):original.rgb)*255);
    if(all(round(result*255)==originalBytes)) return original;
    if(srgb!=0) result=calibrated_decode_srgb(result);
    return float4(result,original.a);
}
#endif
