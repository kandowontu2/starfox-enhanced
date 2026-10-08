#include "calibrated_colour.hlsli"
#include "calibrated_ray_history.hlsli"
Texture2D<float4> ownership : register(t0,space0);SamplerState ownerSampler : register(s0,space0);
ByteAddressBuffer current : register(t1,space0);
ByteAddressBuffer history : register(t2,space0);
RWByteAddressBuffer resolved : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,previousWidth,previousHeight,flags;float weight;uint prefixStride,padding;
};
float3 reflection_history_colour(uint word) {
    float3 rgb=float3(word&255U,(word>>8)&255U,(word>>16)&255U)/255.;
    return (flags&2U)!=0?calibrated_decode_srgb(rgb):rgb;
}
uint reflection_history_pack(float3 rgb,uint alpha) {
    rgb=(flags&2U)!=0?calibrated_encode_srgb(saturate(rgb)):saturate(rgb);
    uint3 bytes=uint3(floor(rgb*255.+.5));return bytes.x|(bytes.y<<8)|(bytes.z<<16)|alpha;
}
bool reflection_history_eligible(uint2 pixel,uint word,float4 witness) {
    float4 owner=ownership.Load(int3(pixel,0));
    bool separated=(flags&4U)!=0;
    if(separated) {
        uint count=width*height,index=pixel.y*width+pixel.x;
        uint incoming=current.Load(count*(prefixStride+48)+index*4);
        float4 base=asfloat(current.Load4(count*(prefixStride+52)+index*16));
        float4 response=asfloat(current.Load4(count*(prefixStride+68)+index*16));
        if((incoming>>24)!=255U || base.w!=1 || response.w!=0 || !all(isfinite(base))
            || !all(isfinite(response)) || any(base.rgb<0) || any(base.rgb>65504)
            || any(response.rgb<0) || any(response.rgb>1) || !any(response.rgb>0)) return false;
    }
    return owner.a>.5 && calibrated_secondary_radiance(word,owner,true)
        && (separated || (word>>24)==255U) && witness.w==1 && all(isfinite(witness)) && witness.z>0;
}
[numthreads(8,8,1)]
void reflection_history_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint count=width*height,index=id.y*width+id.x;
    uint word=current.Load(index*4);
    float4 guide=asfloat(current.Load4(count*prefixStride+index*16));
    uint4 identity=current.Load4(count*(prefixStride+16)+index*16);
    float4 witness=asfloat(current.Load4(count*(prefixStride+32)+index*16));
    bool eligible=reflection_history_eligible(id.xy,word,witness)
        && all(identity.xy!=0xffffffffU) && all(witness.xy>=0) && witness.x+witness.y<=1.00001;
    // Preserve every current guide verbatim; only history eligibility is local.
    // A covered/protected pixel can never seed a later unprotected reflection.
    resolved.Store4(count*prefixStride+index*16,asuint(guide));resolved.Store4(count*(prefixStride+16)+index*16,identity);
    resolved.Store4(count*(prefixStride+32)+index*16,asuint(float4(witness.xyz,eligible?1:0)));
    resolved.Store(index*4,word);
    // A single buffer/lifetime retains the liquid layers. Copy raw words so
    // even protected/invalid surface payloads remain byte-exact; neither the
    // world underlay nor this frame's surface follows the reflected feature.
    if(prefixStride==24) resolved.Store(count*4+index*4,current.Load(count*4+index*4));
    if(prefixStride==20 || prefixStride==24) {
        uint surfaceOffset=count*(prefixStride-16)+index*16;
        resolved.Store4(surfaceOffset,current.Load4(surfaceOffset));
    }
    bool separated=(flags&4U)!=0;
    uint incoming=separated?current.Load(count*(prefixStride+48)+index*4):word;
    float4 baseLight=0,response=0;
    if(separated) {
        baseLight=asfloat(current.Load4(count*(prefixStride+52)+index*16));
        response=asfloat(current.Load4(count*(prefixStride+68)+index*16));
        resolved.Store(count*(prefixStride+48)+index*4,incoming);
        resolved.Store4(count*(prefixStride+52)+index*16,asuint(baseLight));
        resolved.Store4(count*(prefixStride+68)+index*16,asuint(response));
    }
    if(!eligible || (flags&1U)==0 || weight==0 || guide.w!=1 || !all(isfinite(guide))
        || guide.z<=0 || any(identity.zw==0xffffffffU)) return;
    // Native guides are pixel CENTRES; convert once to integer-index space.
    float2 at=guide.xy-.5;
    if(any(at<0) || any(at>float2(previousWidth-1,previousHeight-1))) return;
    int2 base=int2(floor(at));float2 fraction=at-float2(base);
    uint oldCount=previousWidth*previousHeight;float3 prior=0;
    // A receding analytic plane has a different Z at each pixel centre.
    // Its reciprocal depth is affine in screen coordinates: compare the
    // reconstructed depth at the old optical feature, not each neighbour's
    // depth against that one point. Even a tiny nonzero bilinear share must
    // retain its identity/feature checks; never discard it to hide rejection.
    bool analyticGround=separated && ((word>>24)==254U || (word>>24)==253U)
        && identity.x==0xfffffffeU && identity.z==0xfffffffeU;
    float inverseDepth=0;
    [unroll] for(uint dy=0;dy<2;++dy) [unroll] for(uint dx=0;dx<2;++dx) {
        float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);if(share<=0) continue;
        int2 tap=min(base+int2(dx,dy),int2(previousWidth-1,previousHeight-1));uint old=tap.y*previousWidth+tap.x;
        uint4 oldIdentity=history.Load4(oldCount*(prefixStride+16)+old*16);
        float4 oldWitness=asfloat(history.Load4(oldCount*(prefixStride+32)+old*16));
        if(any(oldIdentity.xy!=identity.zw) || oldWitness.w!=1 || !all(isfinite(oldWitness)) || oldWitness.z<=0
            || (!analyticGround && abs(oldWitness.z-guide.z)>max(.01,guide.z*.005))) return;
        if(analyticGround) inverseDepth+=share/oldWitness.z;
        // Check the secondary feature, not just primary receiver depth. Bound
        // interpolation by that SAME old hit's local barycentric footprint.
        float2 footprint=.002;
        [unroll] for(int axis=0;axis<2;++axis) {
            float2 gradient=0;
            [unroll] for(int side=-1;side<=1;side+=2) {
                int2 next=tap;next[axis]+=side;
                if(any(next<0) || any(next>=int2(previousWidth,previousHeight))) continue;
                uint n=next.y*previousWidth+next.x;
                uint4 labels=history.Load4(oldCount*(prefixStride+16)+n*16);
                float4 feature=asfloat(history.Load4(oldCount*(prefixStride+32)+n*16));
                if(all(labels.xy==identity.zw) && feature.w==1 && all(isfinite(feature)))
                    gradient=max(gradient,abs(feature.xy-oldWitness.xy)*1.5);
            }
            // Both axes contribute to a diagonal bilinear footprint. A max
            // over them rejects valid secondary features even on a plane.
            // Scale each axis by this tap's ACTUAL distance to the optical
            // feature; retain the absolute .05 anti-disocclusion ceiling.
            footprint+=gradient*abs(at[axis]-float(tap[axis]));
        }
        if(any(abs(oldWitness.xy-witness.xy)>min(footprint,.05))) return;
        uint oldWord=history.Load((separated?oldCount*(prefixStride+48):0)+old*4);if((oldWord>>24)!=255U) return;
        prior+=reflection_history_colour(oldWord)*share;
    }
    if(analyticGround && (!isfinite(inverseDepth) || inverseDepth<=0
        || abs(1/inverseDepth-guide.z)>max(.01,guide.z*.005))) return;
    float3 low=reflection_history_colour(incoming),high=low;
    [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
        int2 tap=clamp(int2(id.xy)+int2(dx,dy),int2(0,0),int2(width-1,height-1));uint n=tap.y*width+tap.x;
        uint4 labels=current.Load4(count*(prefixStride+16)+n*16);float4 feature=asfloat(current.Load4(count*(prefixStride+32)+n*16));
        uint candidate=current.Load(n*4);
        if(any(labels.xy!=identity.xy) || !reflection_history_eligible(uint2(tap),candidate,feature)) continue;
        uint incident=separated?current.Load(count*(prefixStride+48)+n*4):candidate;
        float3 colour=reflection_history_colour(incident);low=min(low,colour);high=max(high,colour);
    }
    float3 radiance=lerp(reflection_history_colour(incoming),clamp(prior,low,high),weight);
    if(separated) {
        // Only incoming light follows secondary motion. Preserve this frame's
        // transmission, direct light and material response byte-for-byte.
        resolved.Store(count*(prefixStride+48)+index*4,reflection_history_pack(radiance,0xff000000U));
        radiance=baseLight.rgb+radiance*response.rgb;
    }
    resolved.Store(index*4,reflection_history_pack(radiance,word&0xff000000U));
}
