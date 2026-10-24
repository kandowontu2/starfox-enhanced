// MODERN: the desktop's 2D filters for cartridge-authored art. A layer drawn
// at source resolution (one texel per SNES pixel, transparent where empty)
// is reconstructed at the output scale with the same EDGE / xBRZ / sharp
// bilinear / CRT / ScaleFX rules as effects_compute.hlsl's filterSample,
// then composited over the frame.
// Compute (STARFOX_LAYER_FILTER_COMPUTE): read-only storage texture and
// buffer t#/space0, read-write storage texture u#/space1, uniforms b#/space2.
// Composite: fragment sampled texture and sampler t#/s#/space2.
#if defined(STARFOX_LAYER_FILTER_COMPUTE)
Texture2D<float4> filterSource : register(t0,space0);
// GpuScaleFx output: 3x the source extent, normalized RGBA (filter 5 only).
StructuredBuffer<float4> scalefxPixels : register(t1,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> filtered : register(u0,space1);
// width/height: output; scale: output pixels per source pixel (filter
// factor); filter: render::TwoDFilter; sourceWidth/sourceHeight: the layer.
cbuffer Settings : register(b0,space2) {uint width,height,scale,filter,sourceWidth,sourceHeight,pad0,pad1;};
uint4 sourceCell(int2 p) {
    p=clamp(p,int2(0,0),int2(sourceWidth-1,sourceHeight-1));
    return uint4(filterSource.Load(int3(p,0))*255+.5);
}
#include "../../render/shaders/xbrz_compute.hlsli"
#include "../../render/shaders/edge_corners.hlsli"
uint4 filterSample(uint2 p,uint factor) {
    if(filter==5) {
        uint2 extent=uint2(sourceWidth,sourceHeight)*3;
        uint2 q=min(p*3/factor,extent-1);
        return uint4(scalefxPixels[q.y*extent.x+q.x]*255+.5);
    }
    if(filter==2) return xbrzSample(p,factor);
    if(filter==1) {
        int2 cell=int2(p/factor);uint2 local=p%factor;
        uint corner=edgeCorners[factor*(factor-1)*(2*factor-1)/6-1+local.y*factor+local.x];
        uint4 c=sourceCell(cell),u=sourceCell(cell+int2(0,-1)),r=sourceCell(cell+int2(1,0));
        uint4 d=sourceCell(cell+int2(0,1)),l=sourceCell(cell+int2(-1,0)),v=c;
        if(corner==1 && all(l==u) && any(l!=d) && any(u!=r)) v=u;
        else if(corner==2 && all(u==r) && any(u!=l) && any(r!=d)) v=r;
        else if(corner==3 && all(d==l) && any(d!=r) && any(l!=u)) v=l;
        else if(corner==4 && all(r==d) && any(r!=l) && any(d!=u)) v=d;
        return c.a>0 && v.a==0?c:v;
    }
    float2 at=(float2(p)+.5)/factor-.5;
    int2 lo=int2(floor(at));float2 f=saturate((at-lo-.5)*1.5+.5);
    uint4 samples[4]={sourceCell(lo),sourceCell(lo+int2(1,0)),sourceCell(lo+int2(0,1)),sourceCell(lo+1)};
    float weights[4]={(1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y};
    float alpha=0;float3 sum=0;
    for(uint i=0;i<4;++i) {float w=weights[i]*samples[i].a;alpha+=w;sum+=w*samples[i].rgb;}
    if(alpha<=.5) return 0;
    float3 value=sum/alpha;
    if(filter==4) {value+=max(0,value-128)*.08;value*=p.y%factor<(factor+1)/2?1:.76;}
    return uint4(uint3(clamp(value+.5,0,255)),uint(alpha+.5));
}
[numthreads(8,8,1)]
void filter_main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    filtered[id.xy]=float4(filterSample(id.xy,scale))/255.;
}
#else
Texture2D<float4> layer : register(t0,space2);
SamplerState layer_sampler : register(s0,space2);
// xy: the filled part of the layer texture (a capped filter factor).
cbuffer CompositeSettings : register(b0,space3) {float4 extent;};
struct Composite {float4 position : SV_Position;float2 uv : TEXCOORD0;};
// One triangle covering the target; uv 0..1 over the visible part.
Composite composite_vertex(uint id : SV_VertexID) {
    Composite output;
    output.uv=float2((id<<1)&2,id&2);
    output.position=float4(output.uv*float2(2,-2)+float2(-1,1),0,1);
    return output;
}
float4 composite_fragment(Composite input) : SV_Target {
    return layer.Sample(layer_sampler,input.uv*extent.xy);
}
#endif
