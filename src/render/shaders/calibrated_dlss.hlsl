#include "calibrated_colour.hlsli"
#include "calibrated_ray_history.hlsli"
struct Fullscreen {float4 position:SV_Position;};
#if defined(STARFOX_DLSS_VERTEX)
Fullscreen dlss_vertex_main(uint vertex:SV_VertexID) {
    Fullscreen result;
    result.position=float4(vertex==1?3:-1,vertex==2?-3:1,0,1);return result;
}
#elif defined(STARFOX_DLSS_PACK) || defined(STARFOX_DLSS_PACK_MSAA)
Texture2D<float4> scene:register(t0,space2);SamplerState sceneSampler:register(s0,space2);
Texture2D<float4> ownership:register(t1,space2);SamplerState ownershipSampler:register(s1,space2);
Texture2D<float4> surface:register(t2,space2);SamplerState surfaceSampler:register(s2,space2);
Texture2D<float4> motion:register(t3,space2);SamplerState motionSampler:register(s3,space2);
#if defined(STARFOX_DLSS_PACK_MSAA)
Texture2DMS<float4> coverage:register(t4,space2);
#else
Texture2D<float4> coverage:register(t4,space2);
#endif
SamplerState coverageSampler:register(s4,space2);
#if defined(STARFOX_DLSS_PATTERN)
Texture2D<float> previousPhase:register(t5,space2);SamplerState previousPhaseSampler:register(s5,space2);
ByteAddressBuffer water[8]:register(t6,space2);
#else
ByteAddressBuffer water[8]:register(t5,space2);
#endif
cbuffer Settings:register(b0,space3) {
    uint width,height,srgb,rejectedLayers;float projectionZ,projectionW;float2 sampleDelta;
    uint waterWidth,waterHeight,waterCount,surfaceOffset;
    uint coverageFactor,coverageSamples,modelReflections,coveragePadding;
#if defined(STARFOX_DLSS_PATTERN)
    uint patternScale,patternHistory;uint2 patternPadding;uint4 patternPasses[3];
#endif
};
struct Packed {float4 color:SV_Target0;float depth:SV_Target1;float2 motion:SV_Target2;float bias:SV_Target3;
#if defined(STARFOX_DLSS_PATTERN)
    float phase:SV_Target4;
#endif
};
#if defined(STARFOX_DLSS_PATTERN)
uint pattern_phase(uint2 pixel,uint layer) {
    if(layer!=1 && layer!=2) return 0;
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
    return result?result+layer*65536:0;
}
#endif
float4 raster_owner(int2 at,uint sample) {
#if defined(STARFOX_DLSS_PACK_MSAA)
    return coverage.Load(at,sample);
#else
    return coverage.Load(int3(at,0));
#endif
}
Packed dlss_pack_main(Fullscreen input) {
    int3 pixel=int3(uint2(input.position.xy),0);Packed result;
    result.color=scene.Load(pixel);
    if(srgb!=0) result.color.rgb=calibrated_encode_srgb(result.color.rgb);
    float4 owner=ownership.Load(pixel);uint layer=uint(round(owner.b*255));
    float z=surface.Load(pixel).w;float4 prior=motion.Load(pixel);
    bool visible=(layer==1 || layer==2) && isfinite(z) && z>0;
    bool secondary=false;
    // Use EACH actual source receiver and ray sample, never the averaged or
    // centre label to certify a mixed MSAA/SSAA pixel. Invalid/missing liquid
    // normals cannot make its RGB borrow primary-model flow; only depth needs
    // that physical surface validation. Ordinary reflections need no aux plane.
    if(waterCount!=0 && (layer==1 || layer==2)) {
        uint factor=waterWidth/width;uint2 base=uint2(pixel.xy)*factor;
        for(uint sample=0;sample<waterCount;++sample)
            for(uint y=0;y<factor;++y) for(uint x=0;x<factor;++x) {
                uint index=(base.y+y)*waterWidth+base.x+x;
                uint word=water[sample].Load(index*4);
                float4 receiver=raster_owner(int2(base)+int2(x,y),sample);
                secondary=secondary || calibrated_secondary_radiance(word,receiver,modelReflections!=0);
                uint receiverLayer=uint(round(receiver.b*255.));
                if(surfaceOffset==0 || (word>>24)!=253 || receiver.r<=0
                    || (receiverLayer!=1 && receiverLayer!=2)) continue;
                float4 hit=asfloat(water[sample].Load4(surfaceOffset+index*16));
                float normalLength=dot(hit.xyz,hit.xyz);
                if(!all(isfinite(hit)) || hit.w<=0 || normalLength<.5 || normalLength>1.5) continue;
                z=visible?min(z,hit.w):hit.w;visible=true;
            }
    }
    // Convert source positive forward depth to the supplied calibrated camera's
    // [0,1] depth. A background/missing surface stays at the far plane.
    float ndc=visible?-projectionZ+projectionW/(z/256.):1;
    bool inFrustum=visible && isfinite(ndc) && ndc>=0 && ndc<1;
    result.depth=inFrustum?ndc:1;
    bool untracked=(layer==1 && (rejectedLayers&1U)!=0) || (layer==2 && (rejectedLayers&2U)!=0);
    // A clean centre label cannot certify a mixed SSAA/MSAA pixel. Reject if
    // ANY contributing raster sample belongs to an untracked post layer.
    // Read the existing resident ownership plane; no CPU projection or copy.
    if(rejectedLayers!=0 && (layer==1 || layer==2)) {
        for(uint y=0;y<coverageFactor;++y)for(uint x=0;x<coverageFactor;++x)
            for(uint sample=0;sample<coverageSamples;++sample) {
                int2 at=pixel.xy*int(coverageFactor)+int2(x,y);
#if defined(STARFOX_DLSS_PACK_MSAA)
                uint tag=uint(round(coverage.Load(at,sample).b*255));
#else
                uint tag=uint(round(coverage.Load(int3(at,0)).b*255));
#endif
                untracked=untracked || (tag==1 && (rejectedLayers&1U)!=0) || (tag==2 && (rejectedLayers&2U)!=0);
            }
    }
    bool valid=!secondary && !untracked && inFrustum && prior.w==1 && all(isfinite(prior.xyz)) && prior.z>0;
#if defined(STARFOX_DLSS_PATTERN)
    // The colour entering the SDK is already spatially resolved. Certify its
    // actual contributing sample footprint, not just the clean centre label.
    // SSAA omits class-zero protected strokes; MSAA averages every sample.
    // Unknown/mixed receivers and different ordered pattern phases must not
    // acquire a rigid-flow history witness by averaging ownership labels.
    uint phase=0;bool found=false,coherent=true,patterned=pattern_phase(uint2(pixel.xy)*coverageFactor,layer)!=0;
    for(uint y=0;y<coverageFactor;++y)for(uint x=0;x<coverageFactor;++x)
        for(uint sample=0;sample<coverageSamples;++sample) {
            int2 at=pixel.xy*int(coverageFactor)+int2(x,y);
#if defined(STARFOX_DLSS_PACK_MSAA)
            uint tag=uint(round(coverage.Load(at,sample).b*255));
#else
            uint tag=uint(round(coverage.Load(int3(at,0)).b*255));
            if(tag==0) continue;
#endif
            uint tapPhase=pattern_phase(uint2(at),tag);patterned=patterned || tapPhase!=0;
            coherent=coherent && tag==layer && (tag==1 || tag==2) && (!found || tapPhase==phase);
            if(!found) phase=tapPhase;found=true;
        }
    // Store eligible current phases even on a reset/no-previous-motion frame,
    // so the next accepted presentation can recover history. Wet, unknown,
    // missing-depth and untracked pixels can never become previous witnesses.
    // Three 5-bit phase slots plus receiver identity are exact in float32.
    // SDL's portable render-target table supports R32_FLOAT, not R32_UINT.
    result.phase=!secondary && !untracked && inFrustum && found && coherent?float(phase):-1;
    bool matches=patternHistory!=0 && result.phase>=0 && valid;
    if(matches) {
        // Geometric guides contain both raster jitters. Use them here BEFORE
        // removing jitter for the SDK ABI. Reject the whole positive footprint
        // on any mismatch; never renormalize a lone surviving phase.
        float2 previousPixel=float2(pixel.xy)+prior.xy;
        matches=all(previousPixel>=0) && all(previousPixel<=float2(width-1,height-1));
        if(matches) {
            int2 base=int2(floor(previousPixel));float2 fraction=previousPixel-float2(base);
            [unroll] for(uint dy=0;dy<2;++dy) [unroll] for(uint dx=0;dx<2;++dx) {
                float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);
                int2 tap=min(base+int2(dx,dy),int2(width-1,height-1));
                if(share>0 && previousPhase.Load(int3(tap,0))!=float(phase)) matches=false;
            }
        }
    }
    // Clean radiance must not inherit a previously patterned/invalid tap
    // either. On a reset the SDK already drops history, so unpatterned dry
    // geometry keeps its ordinary real guides without requiring old metadata.
    bool patternReject=(patterned || (patternHistory!=0 && inFrustum)) && !matches;
    valid=valid && !patternReject;
#endif
    // Geometric native motion includes sample displacement. SDK motion must
    // exclude it, but retain real camera/object displacement in render pixels.
    result.motion=valid?prior.xy+sampleDelta:float2(-3.402823466e38,-3.402823466e38);
    // Unknown foreground correspondence must not fall back to rigid camera
    // history. Empty far sky is distinct from an untracked foreground model.
    result.bias=secondary || untracked || (!valid && (visible || layer==2))?1:0;
#if defined(STARFOX_DLSS_PATTERN)
    if(patternReject) result.bias=1;
#endif
    return result;
}
#else
Texture2D<float4> reconstructed:register(t0,space2);SamplerState reconstructedSampler:register(s0,space2);
Texture2D<float4> ownership:register(t1,space2);SamplerState ownershipSampler:register(s1,space2);
Texture2D<float4> ink:register(t2,space2);SamplerState inkSampler:register(s2,space2);
cbuffer Settings:register(b0,space3) {
    uint width,height,srgb,padding;float projectionZ,projectionW;float2 sampleDelta;
};
float4 dlss_present_main(Fullscreen input):SV_Target0 {
    int3 pixel=int3(uint2(input.position.xy),0);
    float4 original=ink.Load(pixel);uint layer=uint(round(ownership.Load(pixel).b*255));
    if((layer!=1 && layer!=2) || original.a==0) return original;
    float3 result=reconstructed.Load(pixel).rgb;
    if(srgb!=0) result=calibrated_decode_srgb(result);
    return float4(result,original.a);
}
#endif
