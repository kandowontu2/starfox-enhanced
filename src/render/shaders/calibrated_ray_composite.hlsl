#include "calibrated_colour.hlsli"
#include "calibrated_materials.hlsli"
Texture2D<float4> colour : register(t0,space2);
SamplerState colourSampler : register(s0,space2);
Texture2D<float4> receiver : register(t1,space2);
SamplerState receiverSampler : register(s1,space2);
#if defined(STARFOX_CALIBRATED_EDGE_WITNESS)
Texture2D<float> edgeWitness : register(t2,space2);SamplerState edgeSampler : register(s2,space2);
ByteAddressBuffer shadows : register(t3,space2);
ByteAddressBuffer reflections : register(t4,space2);
#else
ByteAddressBuffer shadows : register(t2,space2);
ByteAddressBuffer reflections : register(t3,space2);
#endif
cbuffer Settings : register(b0,space3) {
    uint width,height,shadowRowBytes,flags;
    float shadowStrength,reflectionStrength;uint reflectionRowBytes,material;
    uint materialScale;float effectSeconds;uint2 padding;
    uint4 postEffects;
};
struct Fullscreen {float4 position : SV_Position;};
Fullscreen composite_vertex_main(uint vertex : SV_VertexID) {
    Fullscreen output;
    output.position=float4(vertex==1?3:-1,vertex==2?3:-1,0,1);
    return output;
}
float4 composed_colour(uint2 pixel) {
    float4 result=colour.Load(int3(pixel,0));
    float4 coverage=receiver.Load(int3(pixel,0));
    if(coverage.r==0) return result;
    if((flags&2U)!=0) {
        uint packed=reflections.Load(padding.x+pixel.y*reflectionRowBytes+pixel.x*4);
        if((packed>>24)==253U) {
            // A submerged opaque model is a transmitted receiver, not missing
            // geometry. The ray's nearer liquid replaces RGB only; protected
            // native artwork never enters this branch and alpha stays intact.
            float3 encoded=float3(packed&255U,(packed>>8)&255U,(packed>>16)&255U)/255.;
            result.rgb=(flags&4U)!=0?calibrated_decode_srgb(encoded):encoded;
            return result;
        }
    }
    // The optional hidden-world plane contains no primary model reflection.
    // A ray that misses water keeps the separately rasterized native world.
    if((flags&16U)!=0) return result;
    bool ground=coverage.r==1./255.;
    // Analytic ground words already contain Fresnel/material/quality shading.
    // Never blend them a second time or shade a foreground model with them.
    if(ground) {
        if((flags&2U)!=0) {
            uint packed=reflections.Load(pixel.y*reflectionRowBytes+pixel.x*4);
            if((packed>>24)==254U) {
                float3 encoded=float3(packed&255U,(packed>>8)&255U,(packed>>16)&255U)/255.;
                result.rgb=(flags&4U)!=0?calibrated_decode_srgb(encoded):encoded;
            }
        }
        return result;
    }
    if((flags&1U)!=0) {
        uint at=pixel.y*shadowRowBytes+pixel.x;
        uint value=(flags&8U)!=0 ? shadows.Load((pixel.y*width+pixel.x)*4)
            : (shadows.Load(at&~3U)>>((at&3U)*8))&255U;
        float shade=saturate(float(value)/255.);
        result.rgb*=1-shade*saturate(shadowStrength);
    }
    if((flags&2U)!=0) {
        uint packed=reflections.Load(pixel.y*reflectionRowBytes+pixel.x*4);
        if((packed>>24)==255U) {
            float3 reflected=float3(packed&255U,(packed>>8)&255U,(packed>>16)&255U)/255.;
            if((flags&4U)!=0) reflected=calibrated_decode_srgb(reflected);
            result.rgb=lerp(result.rgb,reflected,saturate(reflectionStrength)*coverage.g);
        }
    }
    // Never change alpha. Post-ray portraits, meters, text, menu and shutter
    // are drawn afterwards with the original depth and ordered native passes.
    return result;
}
int3 authored_rgb(float4 value) {
    return int3(round(saturate((flags&4U)!=0?calibrated_encode_srgb(value.rgb):value.rgb)*255.));
}
// The flat renderer applies the decorative finish before model/world styles.
// Neighbourhood styles must therefore sample finished neighbours, not apply a
// material over their already transformed output.
float4 finished_colour(uint2 pixel) {
    float4 result=composed_colour(pixel);
    bool water=(flags&2U)!=0 && (reflections.Load(padding.x+pixel.y*reflectionRowBytes+pixel.x*4)>>24)==253U;
    if(material!=0 && !water && receiver.Load(int3(pixel,0)).r>1./255.) {
        int3 rgb=authored_rgb(result);
        int light=dot(rgb,int3(77,150,29))/256;
        uint2 right=min(pixel+uint2(materialScale,0),uint2(width-1,height-1));
        uint2 below=min(pixel+uint2(0,materialScale),uint2(width-1,height-1));
        int rightLight=dot(authored_rgb(composed_colour(right)),int3(77,150,29))/256;
        int belowLight=dot(authored_rgb(composed_colour(below)),int3(77,150,29))/256;
        bool edge=abs(light-rightLight)>28 || abs(light-belowLight)>28;
        float3 encoded=float3(calibrated_material_colour(material,rgb,edge))/255.;
        result.rgb=(flags&4U)!=0?calibrated_decode_srgb(encoded):encoded;
    }
    return result;
}
#include "calibrated_post_effects.hlsli"
float4 composite_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);
    if(pixel.x>=width || pixel.y>=height) discard;
    return post_colour(pixel);
}
#if defined(STARFOX_CALIBRATED_EDGE_WITNESS)
struct WitnessOutput {float4 colour : SV_Target0;float witness : SV_Target1;};
WitnessOutput composite_witness_main(Fullscreen input) {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    uint branch;WitnessOutput result;result.colour=post_colour(pixel,branch);
    uint prior=(padding.y&4U)!=0?uint(edgeWitness.Load(int3(pixel,0))):0;
    uint shift=(padding.y&3U)*3;
    result.witness=float((prior&~(7U<<shift))|(branch<<shift));
    return result;
}
#endif
