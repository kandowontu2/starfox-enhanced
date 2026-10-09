cbuffer ParticleSettings : register(b1,space2) {
    uint uniformParticleCount;float particleScale,particleExposure,particlePad;
    float4 particleData[144];
    float4 particlePrevious[48];
};
#if defined(STARFOX_JOINT_RESIDENT_PARTICLES)
StructuredBuffer<float4> residentParticles : register(t5,space0);
#define particleCount uint(residentParticles[0].w)
float4 particlePoint(uint n) {
    float4 p=residentParticles[2+n*3];
    p.y=residentParticles[0].y+(p.y-residentParticles[0].y)*residentParticles[1].x;
    return p;
}
float4 particleKind(uint n) {return residentParticles[3+n*3];}
float4 particleColour(uint n) {return residentParticles[4+n*3];}
float4 particlePrior(uint n) {return residentParticles[146+n];}
float particleAspect() {return residentParticles[1].x;}
#else
#define particleCount uniformParticleCount
float4 particlePoint(uint n) {return particleData[n*3];}
float4 particleKind(uint n) {return particleData[n*3+1];}
float4 particleColour(uint n) {return particleData[n*3+2];}
float4 particlePrior(uint n) {return particlePrevious[n];}
float particleAspect() {return 1;}
#endif
// Input/output are display-referred, matching the ordinary particle renderer.
// Foreground and uncovered scenery are evaluated separately before coverage
// blending; never apply opaque debris to an already averaged foreground.
uint2 jointCandidates(float2 pixel) {
    uint2 mask=0;
    for(uint n=0;n<particleCount;++n) {
        float4 p=particlePoint(n),kind=particleKind(n);
        bool moving=sampleCount>1&&(historyValid&1)!=0&&shutter>0&&particlePrior(n).w>0;
        float r=moving?max(p.z,p.z*(kind.y/32)):p.z;
        float2 extent=float2(r+4*particleScale,(r+4*particleScale)*(kind.x==4?5:1)*particleAspect())+(moving?radius:0)+2;
        if(all(abs(pixel-p.xy)<=extent)) mask[n/32]|=1u<<(n%32);
    }
    return mask;
}
float3 jointParticles(float3 colour,float2 pixel,float foregroundDepth,uint2 candidates) {
    while(any(candidates)) {
        uint word=candidates.x?0:1;
        uint n=word*32+firstbitlow(candidates[word]);
        candidates[word]&=candidates[word]-1;
        float4 p=particlePoint(n),kind=particleKind(n);
        float type=kind.x,z=kind.y,age=kind.z;
        float4 prior=particlePrior(n);
        if(prior.w>0 && (historyValid&1)!=0 && shutter>0) {
            float3 delta=(prior.xyz-float3(p.xy,z))*shutter;
            if(all(isfinite(delta))) {
                float magnitude=max(abs(delta.x),abs(delta.y));
                float bound=magnitude>0?min(1,(2*radius/magnitude)/length(delta.xy/magnitude)):1;
                float nextZ=z+delta.z*time*bound;
                if(nextZ<32||nextZ>10000) continue;
                p.xy+=delta.xy*time*bound;p.z*=z/nextZ;z=nextZ;
                if(type==7) age-=time*particleExposure*bound;
            }
        }
        if(foregroundDepth>0&&foregroundDepth+8<z) continue;
        float2 d=pixel-p.xy;d.y/=particleAspect();float stretch=type==4?5:1;
        if(abs(d.x)>p.z+4*particleScale||abs(d.y)>(p.z+4*particleScale)*stretch) continue;
        if(type==7) {
            float a=age*11,u=abs(d.x*cos(a)-d.y*sin(a)),v=abs(d.x*sin(a)+d.y*cos(a));
            float coverage=saturate((p.z-max(u,v))/max(.5,particleScale*.5))*p.w;
            colour=colour*(1-coverage)+particleColour(n).rgb*coverage;
        } else {
            float alpha=max(0,1-length(float2(d.x,d.y/stretch))/max(p.z,.5));
            colour+=alpha*alpha*p.w*particleColour(n).rgb;
        }
    }
    return floor(saturate(colour)*255+.5)/255;
}
