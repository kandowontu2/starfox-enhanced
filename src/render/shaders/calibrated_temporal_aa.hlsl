#include "calibrated_colour.hlsli"
#include "calibrated_ray_history.hlsli"
Texture2D<float4> source : register(t0,space2);SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> surface : register(t2,space2);SamplerState surfaceSampler : register(s2,space2);
Texture2D<float4> motion : register(t3,space2);SamplerState motionSampler : register(s3,space2);
Texture2D<float4> history : register(t4,space2);SamplerState historySampler : register(s4,space2);
Texture2D<float4> metadata : register(t5,space2);SamplerState metadataSampler : register(s5,space2);
Texture2D<float> edgeWitness : register(t6,space2);SamplerState edgeSampler : register(s6,space2);
Texture2D<float> centreEdgeWitness : register(t7,space2);SamplerState centreEdgeSampler : register(s7,space2);
ByteAddressBuffer liquid : register(t8,space2);
cbuffer Settings : register(b0,space3) {
    uint width,height,flags,rejectedLayers;float weight;uint patternScale;uint2 edgeMasks;
    float2 jitter;uint2 padding3;uint4 patternPasses[3];
};
struct Fullscreen {float4 position : SV_Position;};
struct Temporal {float4 colour : SV_Target0;float4 memory : SV_Target1;float4 metadata : SV_Target2;};
uint temporal_pattern_phase(uint2 pixel,uint layer) {
    if((flags&8U)==0 || (layer!=1 && layer!=2)) return 0;
    uint2 p=pixel/patternScale;uint result=0;
    [unroll] for(uint i=0;i<3;++i) {
        uint effect=layer==1?patternPasses[i].x:patternPasses[i].y,phase=0;
        if(effect==5) {
            const uint bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
            phase=1+bayer[(p.y%4)*4+p.x%4];
        } else if(effect==10 || effect==25) phase=17+p.y%2;
        else if(effect==70) phase=19+p.x%3+(p.y%3==2?3:0);
        result|=phase<<(i*5);
    }
    // 3 five-bit phases plus receiver identity fit exactly in float metadata.
    return result?result+layer*65536:0;
}
uint temporal_pattern_phase_at(uint2 pixel) {
    if((flags&8U)==0) return 0;
    return temporal_pattern_phase(pixel,uint(round(ownership.Load(int3(pixel,0)).b*255.)));
}
float3 temporal_bytes(float3 rgb) {
    return round(saturate((flags&1U)!=0?calibrated_encode_srgb(rgb):rgb)*255.);
}
uint temporal_edge_key(uint2 pixel,uint layer,bool centre) {
    if((flags&16U)==0 || (layer!=1 && layer!=2)) return 0;
    uint expected=layer==1?edgeMasks.x:edgeMasks.y;if(expected==0) return 0;
    float value=centre?centreEdgeWitness.Load(int3(pixel,0)):edgeWitness.Load(int3(pixel,0));
    if(!isfinite(value) || value<0 || value>511 || value!=round(value)) return 0xffffffffU;
    uint bits=uint(value);
    if((bits&expected)!=expected || (bits&~(expected*7U))!=0) return 0xffffffffU;
    return bits+layer*1024;
}
uint temporal_edge_key_at(uint2 pixel) {
    return temporal_edge_key(pixel,uint(round(ownership.Load(int3(pixel,0)).b*255.)),false);
}
float4 temporal_present_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    // These two bindings are centre-raster ink and AA ownership in this entry.
    float4 original=motion.Load(int3(pixel,0));
    if(history.Load(int3(pixel,0)).a<=.5) return original;
    uint phase=temporal_pattern_phase(pixel,uint(round(history.Load(int3(pixel,0)).b*255.)));
    uint edge=temporal_edge_key(pixel,uint(round(history.Load(int3(pixel,0)).b*255.)),true);
    if(edge==0xffffffffU) return original;
    float2 at=clamp(float2(pixel)+jitter,float2(0,0),float2(width-1,height-1));
    int2 base=int2(floor(at));float2 fraction=at-float2(base);
    float3 result=0;bool geometry=false;
    [unroll] for(uint dy=0;dy<2;++dy) [unroll] for(uint dx=0;dx<2;++dx) {
        int2 tap=min(base+int2(dx,dy),int2(width-1,height-1));
        float depth=surface.Load(int3(tap,0)).a;
        bool owns=ownership.Load(int3(tap,0)).a>.5 && depth>0 && isfinite(depth);
        float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);
        // De-jittering must not blend a screen-locked stripe into another
        // phase. Keep the independently styled centre raster in that case.
        if(owns && share>0 && temporal_pattern_phase_at(uint2(tap))!=phase) return original;
        if(owns && share>0 && temporal_edge_key_at(uint2(tap))!=edge) return original;
        result+=(owns?source.Load(int3(tap,0)).rgb:original.rgb)*share;
        geometry=geometry || (owns && share>0);
    }
    return geometry?float4(result,original.a):original;
}
bool temporal_history_eligible(uint2 pixel) {
    float4 owner=ownership.Load(int3(pixel,0));
    uint layer=uint(round(owner.b*255.));
    // Exclude the actual composed liquid/ground/model reflection, including
    // stored eligibility. A later dry pixel at the same Z must not inherit it.
    bool secondary=false;
    if((flags&4U)!=0) secondary=calibrated_secondary_radiance(
        liquid.Load((pixel.y*width+pixel.x)*4),owner,(flags&32U)!=0);
    // Store no distorted radiance as eligible history. A later clean actor at
    // the same depth must not inherit a previously warped/world-styled pixel.
    bool untracked=(layer==1 && (rejectedLayers&1U)!=0) || (layer==2 && (rejectedLayers&2U)!=0);
    return owner.a>.5 && !secondary && !untracked && temporal_edge_key_at(pixel)!=0xffffffffU;
}
Temporal temporal_aa_fragment_main(Fullscreen input) {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width || pixel.y>=height) discard;
    Temporal output;output.colour=source.Load(int3(pixel,0));output.memory=output.colour;
    bool eligible=temporal_history_eligible(pixel);
    uint phase=temporal_pattern_phase_at(pixel);
    uint edge=temporal_edge_key_at(pixel);
    float depth=surface.Load(int3(pixel,0)).a;output.metadata=float4(depth,eligible?1:0,phase,edge);
    float4 guide=motion.Load(int3(pixel,0));
    if((flags&2U)==0 || !eligible || guide.w<=0 || depth<=0 || guide.z<=0
        || !isfinite(depth) || !all(isfinite(guide))) return output;
    // Guides already include CURRENT and PREVIOUS sample jitter. Do not add
    // pose-only offsets or jitter again. Integer pixel indices match CPU TAA.
    float2 priorPixel=float2(pixel)+guide.xy;
    if(any(priorPixel<0) || any(priorPixel>float2(width-1,height-1))) return output;
    int2 base=int2(floor(priorPixel));float2 fraction=priorPixel-float2(base);
    float3 prior=0;float sum=0;
    [unroll] for(uint dy=0;dy<2;++dy) [unroll] for(uint dx=0;dx<2;++dx) {
        int2 tap=min(base+int2(dx,dy),int2(width-1,height-1));
        float4 meta=metadata.Load(int3(tap,0));
        float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);
        if(meta.y<=.5 || !isfinite(meta.x) || abs(meta.x-guide.z)>max(.01,guide.z*.01)) continue;
        // Reject the complete footprint, never renormalize surviving phases.
        if(share>0 && meta.z!=float(phase)) return output;
        if(share>0 && meta.w!=float(edge)) return output;
        prior+=temporal_bytes(history.Load(int3(tap,0)).rgb)*share;sum+=share;
    }
    // Never stretch a single surviving history tap across an occlusion edge.
    if(sum<.75) return output;
    float3 low=255,high=0;
    [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
        int2 tap=clamp(int2(pixel)+int2(dx,dy),int2(0,0),int2(width-1,height-1));
        if(!temporal_history_eligible(uint2(tap)) || temporal_pattern_phase_at(uint2(tap))!=phase
            || temporal_edge_key_at(uint2(tap))!=edge) continue;
        float3 rgb=temporal_bytes(source.Load(int3(tap,0)).rgb);low=min(low,rgb);high=max(high,rgb);
    }
    float3 current=temporal_bytes(output.colour.rgb),clipped=clamp(prior/sum,low,high);
    float3 rgb=floor(clamp(current*(1-weight)+clipped*weight,0,255)+.5)/255.;
    output.colour.rgb=(flags&1U)!=0?calibrated_decode_srgb(rgb):rgb;
    output.memory=output.colour;return output; // Alpha and protected ink stay exact.
}
