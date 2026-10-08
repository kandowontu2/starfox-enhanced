// Particle-only exposure pass. Surface lighting and optical distortions are
// separate stages; the host must reject those types rather than drop them.
Texture2D<float4> sourceColour : register(t0,space0);
StructuredBuffer<uint> ownership : register(t1,space0);
StructuredBuffer<float4> surfaces : register(t2,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> outputColour : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,samples,count;
    float4 shutter; // exposure / presentation interval, radius cap, exposure seconds, draw scale
    float4 data[144];
    float4 previous[48]; // projected XY, camera depth, correspondence valid
};
float3 decode(float3 c) {
    return float3(c.x<=.04045?c.x/12.92:pow((c.x+.055)/1.055,2.4),
        c.y<=.04045?c.y/12.92:pow((c.y+.055)/1.055,2.4),
        c.z<=.04045?c.z/12.92:pow((c.z+.055)/1.055,2.4));
}
float3 encode(float3 c) {
    return float3(c.x<=.0031308?c.x*12.92:1.055*pow(c.x,1/2.4)-.055,
        c.y<=.0031308?c.y*12.92:1.055*pow(c.y,1/2.4)-.055,
        c.z<=.0031308?c.z*12.92:1.055*pow(c.z,1/2.4)-.055);
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint i=id.y*width+id.x,packed=ownership[i];
    float4 original=sourceColour.Load(int3(id.xy,0));
    if(((packed>>8)&255)==1) {outputColour[id.xy]=original;return;}
    // One conservative coverage test per particle/pixel, outside the exposure
    // loop. Travel is capped in XY and visible samples never cross depth 32.
    // A two-word mask keeps every candidate's original compositing order.
    uint2 candidates=0;
    for(uint n=0;n<count;++n) {
        float4 particle=data[n*3],kind=data[n*3+1],prior=previous[n];
        bool moving=samples>1 && shutter.x>0 && prior.w>0 && all(isfinite(prior.xyz)) && prior.z>0;
        float radius=moving?max(particle.z,particle.z*(kind.y/32)):particle.z;
        float travel=moving?shutter.y:0;
        float2 extent=float2(radius+4*shutter.w,(radius+4*shutter.w)*(kind.x==4?5:1))+travel+2;
        if(all(abs(float2(id.xy)-particle.xy)<=extent)) candidates[n/32]|=1u<<(n%32);
    }
    if(!any(candidates)) {outputColour[id.xy]=original;return;}
    float depth=(packed&0x01000000) && ((packed>>16)&255)==(packed&255)?surfaces[i].w:0;
    float3 integral=0;
    for(uint tap=0;tap<samples;++tap) {
        float time=samples==1?0:float(tap)/float(samples-1)-.5;
        float3 colour=original.rgb;
        uint2 remaining=candidates;
        while(any(remaining)) {
            uint word=remaining.x?0:1;
            uint n=word*32+firstbitlow(remaining[word]);
            remaining[word]&=remaining[word]-1;
            float4 particle=data[n*3],kind=data[n*3+1];
            float type=kind.x,z=kind.y,age=kind.z;
            if(type<2 || type>7) continue;
            float4 prior=previous[n];
            if(prior.w>0 && all(isfinite(prior.xyz)) && prior.z>0 && z>0) {
                float3 delta=(prior.xyz-float3(particle.xy,z))*shutter.x;
                if(all(isfinite(delta))) {
                    // Normalize before measuring so finite large travel does not
                    // overflow the squared length and collapse the trail to zero.
                    float magnitude=max(abs(delta.x),abs(delta.y));
                    float bound=magnitude>0?min(1,(2*shutter.y/magnitude)/length(delta.xy/magnitude)):1;
                    float nextZ=z+delta.z*time*bound;
                    if(nextZ<32 || nextZ>10000) continue;
                    particle.xy+=delta.xy*time*bound;particle.z*=z/nextZ;z=nextZ;
                    if(type==7) age-=time*shutter.z*bound;
                }
            }
            if(depth>0 && depth+8<z) continue;
            float2 delta=float2(id.xy)-particle.xy;
            float stretch=type==4?5:1;
            if(abs(delta.x)>particle.z+4*shutter.w || abs(delta.y)>(particle.z+4*shutter.w)*stretch) continue;
            if(type==7) {
                float angle=age*11;
                float u=abs(delta.x*cos(angle)-delta.y*sin(angle));
                float v=abs(delta.x*sin(angle)+delta.y*cos(angle));
                float coverage=saturate((particle.z-max(u,v))/max(.5,shutter.w*.5))*particle.w;
                colour=colour*(1-coverage)+data[n*3+2].rgb*coverage;
            } else {
                float distance=length(float2(delta.x,delta.y/stretch));
                float alpha=max(0,1-distance/max(particle.z,.5));alpha*=alpha*particle.w;
                colour+=alpha*data[n*3+2].rgb;
            }
        }
        // The CPU scene renderer produces 8-bit samples before integration.
        integral+=decode(floor(saturate(colour)*255+.5)/255);
    }
    outputColour[id.xy]=float4(encode(saturate(integral/samples)),original.a);
}
