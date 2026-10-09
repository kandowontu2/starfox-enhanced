// Bounded model-only per-quadrature incident history. Geometry is borrowed
// from the submitted ray stream; no CPU pixels/vertices, extra ray traversal,
// eight motion images or transport of the current conductor response.
#include "calibrated_colour.hlsli"
#include "calibrated_ray_history.hlsli"
#include "reflection_rough_hit_motion.hlsli"
#include "reflection_lobe_record.hlsli"
Texture2D<float4> ownership : register(t0,space0);SamplerState ownerSampler : register(s0,space0);
ByteAddressBuffer current : register(t1,space0);
ByteAddressBuffer history : register(t2,space0);
ByteAddressBuffer geometry : register(t3,space0);
RWByteAddressBuffer resolved : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,previousWidth,previousHeight;
    uint lobes,flags,previousVertices,previousMapping;
    uint triangles,geometryBytes;float weight,roughness;
    float4 projection,clip;
};
float3 incident_colour(uint word) {
    float3 rgb=float3(word&255U,(word>>8)&255U,(word>>16)&255U)/255.;
    return (flags&2U)!=0?calibrated_decode_srgb(rgb):rgb;
}
uint incident_pack(float3 rgb,uint alpha) {
    rgb=(flags&2U)!=0?calibrated_encode_srgb(saturate(rgb)):saturate(rgb);
    uint3 bytes=uint3(floor(rgb*255.+.5));return bytes.x|(bytes.y<<8)|(bytes.z<<16)|alpha;
}
uint3 lobe_record(ByteAddressBuffer image,uint count,uint index,uint lobe) {
    return image.Load3(count*28+(index*lobes+lobe)*12);
}
bool finite_record(uint3 record) {
    return record.x!=0xffffffffU && (record.z>>24)==255U
        && uint(record.y&65535U)+(record.y>>16)<=65535U;
}
bool receiver_valid(uint2 pixel,uint word,uint primary,float depth,float4 response) {
    float4 owner=ownership.Load(int3(pixel,0));
    return owner.a>.5 && calibrated_secondary_radiance(word,owner,true) && (word>>24)==255U
        && primary<triangles && isfinite(depth) && depth>0 && response.w==1
        && all(isfinite(response)) && all(response.rgb>=0) && all(response.rgb<=1) && any(response.rgb>0);
}
bool previous_sample(uint2 pixel,uint primary,uint3 record,uint lobe,float4 guide,out float3 prior) {
    prior=0;
    if((flags&1U)==0 || weight==0 || guide.w!=1 || !all(isfinite(guide)) || guide.z<=0) return false;
    uint matchedPrimary=geometry.Load(previousMapping+primary*4);
    uint matchedSecondary=geometry.Load(previousMapping+record.x*4);
    if(matchedPrimary==0xffffffffU || matchedSecondary==0xffffffffU) return false;
    float2 at=guide.xy-.5;
    if(any(at<0) || any(at>float2(previousWidth-1,previousHeight-1))) return false;
    int2 base=int2(floor(at));float2 fraction=at-float2(base),feature=reflection_lobe_bary(record.y);
    uint oldCount=previousWidth*previousHeight;
    [unroll] for(uint dy=0;dy<2;++dy) [unroll] for(uint dx=0;dx<2;++dx) {
        float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);if(share<=0) continue;
        int2 tap=min(base+int2(dx,dy),int2(previousWidth-1,previousHeight-1));uint old=tap.y*previousWidth+tap.x;
        uint oldPrimary=history.Load(oldCount*4+old*4);
        float depth=asfloat(history.Load(oldCount*8+old*4));uint3 witness=lobe_record(history,oldCount,old,lobe);
        if(oldPrimary!=matchedPrimary || witness.x!=matchedSecondary || !finite_record(witness)
            || !isfinite(depth) || depth<=0 || abs(depth-guide.z)>max(.01,guide.z*.005)) return false;
        float2 oldFeature=reflection_lobe_bary(witness.y),footprint=2./65535.+.002;
        [unroll] for(int axis=0;axis<2;++axis) {
            float2 gradient=0;
            [unroll] for(int side=-1;side<=1;side+=2) {
                int2 next=tap;next[axis]+=side;
                if(any(next<0) || any(next>=int2(previousWidth,previousHeight))) continue;
                uint n=next.y*previousWidth+next.x;uint3 adjacent=lobe_record(history,oldCount,n,lobe);
                if(history.Load(oldCount*4+n*4)==matchedPrimary && adjacent.x==matchedSecondary && finite_record(adjacent))
                    gradient=max(gradient,abs(reflection_lobe_bary(adjacent.y)-oldFeature)*1.5);
            }
            footprint+=gradient*abs(at[axis]-float(tap[axis]));
        }
        if(any(abs(oldFeature-feature)>min(footprint,.05))) return false;
        prior+=incident_colour(witness.z)*share;
    }
    return true;
}
[numthreads(8,8,1)]
void reflection_lobes_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint count=width*height,index=id.y*width+id.x,word=current.Load(index*4);
    uint primary=current.Load(count*4+index*4);float depth=asfloat(current.Load(count*8+index*4));
    float4 response=asfloat(current.Load4(count*12+index*16));
    bool eligible=receiver_valid(id.xy,word,primary,depth,response);
    resolved.Store(index*4,word);resolved.Store(count*4+index*4,eligible?primary:0xffffffffU);
    resolved.Store(count*8+index*4,asuint(depth));resolved.Store4(count*12+index*16,asuint(response));
    float3 sum=0;bool changed=false;
    [loop] for(uint lobe=0;lobe<lobes;++lobe) {
        uint3 record=lobe_record(current,count,index,lobe);float3 incoming=incident_colour(record.z);
        bool finite=eligible && record.x<triangles && finite_record(record);
        uint3 saved=record;saved.x=finite?record.x:0xffffffffU;
        if(finite && (flags&1U)!=0 && weight>0) {
            uint p=previousVertices+primary*48,q=previousVertices+record.x*48;
            ReflectionRoughFrame frame;
            frame.a=asfloat(geometry.Load4(p));frame.b=asfloat(geometry.Load4(p+16));frame.c=asfloat(geometry.Load4(p+32));
            frame.projection=projection;frame.extentClip=float4(previousWidth,previousHeight,clip.xy);
            frame.settings=float4(roughness,lobe,0,0);
            float4 guide=0;
            if(frame.a.w==1 && frame.b.w==1 && frame.c.w==1)
                guide=reflection_rough_hit_motion(frame,asfloat(geometry.Load4(q)),asfloat(geometry.Load4(q+16)),
                    asfloat(geometry.Load4(q+32)),reflection_lobe_bary(record.y));
            float3 prior;
            if(previous_sample(id.xy,primary,record,lobe,guide,prior)) {
                float3 low=incoming,high=incoming;
                [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
                    int2 tap=clamp(int2(id.xy)+int2(dx,dy),int2(0,0),int2(width-1,height-1));uint n=tap.y*width+tap.x;
                    uint3 adjacent=lobe_record(current,count,n,lobe);
                    uint adjacentPrimary=current.Load(count*4+n*4);
                    if(adjacentPrimary!=primary || adjacent.x!=record.x || !finite_record(adjacent)) continue;
                    float adjacentDepth=asfloat(current.Load(count*8+n*4));float4 adjacentResponse=asfloat(current.Load4(count*12+n*16));
                    if(!receiver_valid(uint2(tap),current.Load(n*4),adjacentPrimary,adjacentDepth,adjacentResponse)) continue;
                    float3 rgb=incident_colour(adjacent.z);low=min(low,rgb);high=max(high,rgb);
                }
                incoming=lerp(incoming,clamp(prior,low,high),weight);saved.z=incident_pack(incoming,record.z&0xff000000U);changed=true;
            }
        }
        resolved.Store3(count*28+(index*lobes+lobe)*12,saved);sum+=incoming;
    }
    if(changed) resolved.Store(index*4,incident_pack(sum/lobes*response.rgb,word&0xff000000U));
}
