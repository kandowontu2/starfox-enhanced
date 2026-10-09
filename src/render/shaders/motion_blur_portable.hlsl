// Centred-shutter forward reconstruction. Integer atomics avoid unsupported
// float atomics and order-dependent colour accumulation on mobile GPUs.
Texture2D<float4> sourceColour : register(t0,space0);
Texture2D<float4> underlayColour : register(t1,space0);
StructuredBuffer<uint> ownership : register(t2,space0);
StructuredBuffer<float> depth : register(t3,space0);
StructuredBuffer<float4> motion : register(t4,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> outputColour : register(u0,space1);
RWStructuredBuffer<uint> nearest : register(u1,space1);
RWStructuredBuffer<uint4> sums : register(u2,space1);
RWStructuredBuffer<float4> integral : register(u3,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,stage,sampleIndex;
    uint sampleCount,historyValid;float2 guideJitter;
    float shutter,radius,depthTolerance,time;
};
bool eligible(uint i) {
    uint tag=(ownership[i]>>8)&255;
    return tag<=5 && (tag!=1 || (ownership[i]&0x10000000)!=0);
}
#include "joint_particles.hlsli"
float depthAt(uint i) {
    if((historyValid&4)!=0) {
        uint packed=ownership[i];
        return (packed&0x01000000)!=0 && ((packed>>16)&255)==(packed&255)?depth[i*4+3]:0;
    }
    return depth[i];
}
uint guideAt(uint2 pixel) {
    if((historyValid&2)==0) return 0xffffffff;
    if(all(guideJitter==0)) return pixel.y*width+pixel.x;
    float2 at=clamp(float2(pixel)+guideJitter,float2(0,0),float2(width-1,height-1));
    uint2 base=uint2(at);float2 fraction=frac(at);
    uint chosen=0xffffffff;float closest=0,bestWeight=-1;
    for(uint y=0;y<2;++y) for(uint x=0;x<2;++x) {
        float weight=(x?fraction.x:1-fraction.x)*(y?fraction.y:1-fraction.y);
        if(weight<=0) continue;
        uint2 p=min(base+uint2(x,y),uint2(width-1,height-1));uint i=p.y*width+p.x;
        float z=depthAt(i);
        if(!eligible(i) || !isfinite(z) || z<=0) continue;
        if(chosen==0xffffffff || z<closest || (z==closest && weight>bestWeight)) {
            chosen=i;closest=z;bestWeight=weight;
        }
    }
    return chosen;
}
float3 decode(float3 c) {
    return float3(c.x<=.04045?c.x/12.92:pow((c.x+.055)/1.055,2.4),
        c.y<=.04045?c.y/12.92:pow((c.y+.055)/1.055,2.4),
        c.z<=.04045?c.z/12.92:pow((c.z+.055)/1.055,2.4));
}
float3 encode(float3 c) {
    c=saturate(c);
    return float3(c.x<=.0031308?c.x*12.92:1.055*pow(c.x,1/2.4)-.055,
        c.y<=.0031308?c.y*12.92:1.055*pow(c.y,1/2.4)-.055,
        c.z<=.0031308?c.z*12.92:1.055*pow(c.z,1/2.4)-.055);
}
float2 velocityAt(uint i,float z) {
    if((historyValid&1)==0 || shutter==0) return 0;
    float4 m=motion[i];float2 velocity=0;
    if((historyValid&1)!=0 && shutter>0 && m.w==1 && all(isfinite(m.xyz)) && abs(m.z-z)<=max(.001,abs(z)*.00001)) {
        float scale=max(abs(m.x),abs(m.y));
        if(scale>0) {
            // Avoid a subnormal reciprocal being flushed to zero when a
            // finite but extreme vector is normalized by a GPU driver.
            float2 bounded=scale>1e20?m.xy*1e-20:m.xy;
            float2 unit=bounded/max(abs(bounded.x),abs(bounded.y));float unitLength=length(unit);
            float travel=min(2*radius,scale*shutter*unitLength);
            if(travel>=.01) velocity=unit/unitLength*travel;
        }
    }
    return velocity;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint i=id.y*width+id.x,count=width*height;
    float4 original=sourceColour.Load(int3(id.xy,0));
    uint guide=guideAt(id.xy);
    float z=guide==0xffffffff?0:depthAt(guide);
    bool surface=eligible(i)&&original.a!=0&&isfinite(z)&&z>0;
    float2 velocity=surface?velocityAt(guide,z):float2(0,0);
    bool stationary=surface&&all(velocity==0);
    // Stationary samples always land on exactly their own pixel. Seed their
    // depth locally and add their colour at resolve instead of scattering
    // five atomics per stationary pixel on every shutter sample.
    uint initialDepth=stationary?asuint(z):0x7f800000;
    if(stage==0) {
        nearest[i]=initialDepth;sums[i]=0;
        integral[i]=0;nearest[count+i]=0;
        if(particleCount>0) {
            uint2 candidates=jointCandidates(float2(id.xy));
            nearest[count*2+i]=candidates.x;nearest[count*3+i]=candidates.y;
        }
        return;
    }
    if(stage==3) {
        uint2 candidates=particleCount>0?uint2(nearest[count*2+i],nearest[count*3+i]):uint2(0,0);
        if(any(candidates)) {
            uint4 total=sums[i];float front=asfloat(nearest[i]);
            if(stationary&&z-front<=max(.01,front*depthTolerance))
                total+=uint4(uint3(round(decode(original.rgb)*4096)),4096);
            float coverage=min(float(total.w)/4096,1);
            float3 foreground=total.w>0?float3(total.xyz)/float(total.w):0;
            float3 background=(!isfinite(z)||z<=0)?original.rgb:underlayColour.Load(int3(id.xy,0)).rgb;
            float3 covered=jointParticles(floor(encode(foreground)*255+.5)/255,float2(id.xy),isfinite(front)?front:0,candidates);
            float3 uncovered=jointParticles(background,float2(id.xy),0,candidates);
            integral[i]+=float4(decode(covered)*coverage+decode(uncovered)*(1-coverage),0);
            if(sampleIndex+1==sampleCount)
                outputColour[id.xy]=(!eligible(i)||original.a==0)?original:float4(encode(integral[i].rgb/sampleCount),original.a);
            nearest[i]=initialDepth;sums[i]=0;return;
        }
        if(nearest[count+i]==0) {
            // If a later sample reaches this pixel, initialize its preceding
            // exposure analytically then. Untouched scenery needs no colour
            // conversion, underlay fetch or accumulation at every sample.
            if(sampleIndex+1==sampleCount) outputColour[id.xy]=original;
            nearest[i]=initialDepth;sums[i]=0;return;
        }
        if(integral[i].w==0) {
            float3 prior=stationary?decode(original.rgb):decode(underlayColour.Load(int3(id.xy,0)).rgb);
            integral[i]=float4(prior*sampleIndex,1);
        }
        uint4 total=sums[i];float front=asfloat(nearest[i]);
        if(stationary&&z-front<=max(.01,front*depthTolerance))
            total+=uint4(uint3(round(decode(original.rgb)*4096)),4096);
        float coverage=float(total.w)/4096;
        float3 colour=coverage>0?float3(total.xyz)/float(total.w):0;
        float3 resolved=colour*min(coverage,1)+decode(underlayColour.Load(int3(id.xy,0)).rgb)*(1-min(coverage,1));
        integral[i]+=float4(resolved,0);
        if(sampleIndex+1==sampleCount)
            outputColour[id.xy]=(!eligible(i)||original.a==0||nearest[count+i]==0)?original:
                float4(encode(integral[i].rgb/sampleCount),original.a);
        nearest[i]=initialDepth;sums[i]=0;
        return;
    }
    if(!surface||stationary) return;
    // Even when every shutter tap leaves this pixel, reveal its underlay.
    if(stage==1 && any(velocity!=0)) InterlockedOr(nearest[count+i],1);
    float2 position=float2(id.xy)+velocity*time;
    int2 base=int2(floor(position));float2 f=position-base;
    float3 linearColour=decode(original.rgb);
    [unroll] for(int dy=0;dy<2;++dy) [unroll] for(int dx=0;dx<2;++dx) {
        int2 p=base+int2(dx,dy);
        if(any(p<0)||p.x>=int(width)||p.y>=int(height)) continue;
        uint n=p.y*width+p.x;
        if(!eligible(n)||sourceColour.Load(int3(p,0)).a==0) continue;
        float weight=(dx?f.x:1-f.x)*(dy?f.y:1-f.y);
        if(weight<=0) continue;
        if(stage==1) {
            InterlockedMin(nearest[n],asuint(z));
            if(any(velocity!=0)) InterlockedOr(nearest[count+n],1);
        } else {
            float front=asfloat(nearest[n]);
            if(z-front>max(.01,front*depthTolerance)) continue;
            uint w=uint(round(weight*4096));
            uint3 c=uint3(round(linearColour*w));
            InterlockedAdd(sums[n].x,c.x);InterlockedAdd(sums[n].y,c.y);
            InterlockedAdd(sums[n].z,c.z);InterlockedAdd(sums[n].w,w);
        }
    }
}
