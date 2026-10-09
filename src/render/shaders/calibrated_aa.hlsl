// Native calibrated-eye adapters. Spatial equations match effects_compute;
// SMAA 1x edge/search/area/corner processing uses the upstream SMAA library.
#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
Texture2D<float4> edgesTex : register(t2,space2);
SamplerState edgesSampler : register(s2,space2);
Texture2D<float4> blendTex : register(t3,space2);
SamplerState blendSampler : register(s3,space2);
Texture2D<float4> areaTex : register(t4,space2);
SamplerState areaSampler : register(s4,space2);
Texture2D<float4> searchTex : register(t5,space2);
SamplerState searchSampler : register(s5,space2);
cbuffer Settings : register(b0,space3) {uint width,height,type,quality;uint srgb,stage;uint2 padding;};
struct Fullscreen {float4 position : SV_Position;};
// FSR's full-resolution world result and original native ownership/ink.
// Keep protected artwork and source opacity exact in the negotiated format.
float4 aa_upscale_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 p=uint2(input.position.xy);if(p.x>=width || p.y>=height) discard;
    float4 original=edgesTex.Load(int3(p,0));
    uint layer=uint(round(ownership.Load(int3(p,0)).b*255.));
    return layer==1 || layer==2?float4(source.Load(int3(p,0)).rgb,original.a):original;
}
bool eligible(uint2 p) {return ownership.Load(int3(p,0)).a==1;}
uint2 bounded(int2 p) {return uint2(clamp(p,int2(0,0),int2(width-1,height-1)));}
uint3 authored(uint2 p) {
    float3 rgb=source.Load(int3(p,0)).rgb;
    return uint3(round(saturate(srgb!=0?calibrated_encode_srgb(rgb):rgb)*255.));
}
float4 colour_result(float3 rgb,float4 original) {
    original.rgb=srgb!=0?calibrated_decode_srgb(rgb):rgb;return original;
}
uint light(uint3 c) {return (c.r*77+c.g*150+c.b*29)/256;}
float4 aa_spatial_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 p=uint2(input.position.xy);float4 original=source.Load(int3(p,0));
    if(quality==0 || !eligible(p) || p.x==0 || p.y==0 || p.x+1==width || p.y+1==height) return original;
    uint3 c=authored(p),l=eligible(p-uint2(1,0))?authored(p-uint2(1,0)):c;
    uint3 r=eligible(p+uint2(1,0))?authored(p+uint2(1,0)):c;
    uint3 u=eligible(p-uint2(0,1))?authored(p-uint2(0,1)):c;
    uint3 d=eligible(p+uint2(0,1))?authored(p+uint2(0,1)):c;
    int lv=light(l),rv=light(r),uv=light(u),dv=light(d),v=light(c);
    uint low=min(v,min(min(lv,rv),min(uv,dv))),high=max(v,max(max(lv,rv),max(uv,dv)));
    uint floorValue=quality==1?20:quality==3?6:12,divisor=quality==1?6:quality==3?12:8,weight=quality==1?6:quality==3?1:2;
    if(high-low<max(floorValue,high/divisor)) return original;
    bool vertical=type==1?abs(uv+dv-2*v)<abs(lv+rv-2*v):abs(lv-rv)>=abs(uv-dv);
    uint3 result=type==2?(c*weight+l+r+u+d)/(weight+4):(c*weight+(vertical?u+d:l+r))/(weight+2);
    return colour_result(float3(result)/255.,original);
}
float4 smaa_colour(float4 value) {
    // Edge detection uses authored bytes, as in the regular renderer. Lookup
    // textures/weights are linear; never apply colour gamma to them.
    if(stage==0) value.rgb=round(saturate(srgb!=0?calibrated_encode_srgb(value.rgb):value.rgb)*255.)/255.;
    return value;
}
#define SMAA_RT_METRICS float4(1.0/width,1.0/height,width,height)
#define SMAA_CUSTOM_SL 1
#define SMAATexture2D(tex) Texture2D<float4> tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex,coord) smaa_colour(tex.SampleLevel(sourceSampler,coord,0))
#define SMAASampleLevelZeroPoint(tex,coord) smaa_colour(tex.Load(int3(clamp(int2((coord)*float2(width,height)),int2(0,0),int2(width-1,height-1)),0)))
#define SMAASampleLevelZeroOffset(tex,coord,offset) smaa_colour(tex.SampleLevel(sourceSampler,coord,0,offset))
#define SMAASample(tex,coord) SMAASampleLevelZero(tex,coord)
#define SMAASamplePoint(tex,coord) SMAASampleLevelZeroPoint(tex,coord)
#define SMAASampleOffset(tex,coord,offset) SMAASampleLevelZeroOffset(tex,coord,offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#define SMAA_THRESHOLD (quality==1?.15:.1)
#define SMAA_MAX_SEARCH_STEPS (quality==1?4:quality==2?8:16)
#define SMAA_MAX_SEARCH_STEPS_DIAG (quality<3?0:8)
#define SMAA_CORNER_ROUNDING (quality<3?100:25)
// Write zero even on no-edge pixels: reused intermediate targets must never
// retain weights/edges from a prior eye, frame, cancelled submission or resize.
#define discard return float2(0,0)
#include "../../../third_party/smaa/SMAA.hlsl"
#undef discard
float4 aa_edges_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 p=uint2(input.position.xy);if(!eligible(p)) return 0;
    float2 uv=(float2(p)+.5)/float2(width,height);float4 offsets[3];SMAAEdgeDetectionVS(uv,offsets);
    float2 edges=SMAAColorEdgeDetectionPS(uv,offsets,source);
    if(p.x==0 || !eligible(p-uint2(1,0))) edges.x=0;
    if(p.y==0 || !eligible(p-uint2(0,1))) edges.y=0;
    return float4(edges,0,0);
}
float4 aa_weights_fragment_main(Fullscreen input) : SV_Target0 {
    float2 uv=input.position.xy/float2(width,height),pixel;float4 offsets[3];
    SMAABlendingWeightCalculationVS(uv,pixel,offsets);
    return SMAABlendingWeightCalculationPS(uv,pixel,offsets,edgesTex,areaTex,searchTex,0);
}
float3 guarded_colour(float2 uv,uint2 origin) {
    float2 at=clamp(uv*float2(width,height)-.5,0.,float2(width-1,height-1));
    uint2 lo=uint2(at);float2 f=at-lo;float3 rgb=0;
    for(uint y=0;y<2;++y) for(uint x=0;x<2;++x) {
        uint2 p=min(lo+uint2(x,y),uint2(width-1,height-1));if(!eligible(p)) p=origin;
        rgb+=float3(authored(p))/255.*(x?f.x:1-f.x)*(y?f.y:1-f.y);
    }
    return rgb;
}
float4 aa_blend_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 p=uint2(input.position.xy);float4 original=source.Load(int3(p,0));if(!eligible(p)) return original;
    float2 uv=input.position.xy/float2(width,height);float4 offset;SMAANeighborhoodBlendingVS(uv,offset);
    // Upstream SMAANeighborhoodBlendingPS, with only its colour sampling
    // adapted to guarded authored-byte taps. Weight selection is unchanged.
    float4 a;a.x=blendTex.SampleLevel(sourceSampler,offset.xy,0).a;
    a.y=blendTex.SampleLevel(sourceSampler,offset.zw,0).g;a.wz=blendTex.Load(int3(p,0)).xz;
    if(dot(a,float4(1,1,1,1))<1.e-5) return original;
    bool horizontal=max(a.x,a.z)>max(a.y,a.w);
    float4 delta=horizontal?float4(a.x,0,a.z,0):float4(0,a.y,0,a.w);
    float2 weights=horizontal?a.xz:a.yw;weights/=weights.x+weights.y;
    float4 coordinates=uv.xyxy+delta*float4(1./width,1./height,-1./width,-1./height);
    float3 rgb=weights.x*guarded_colour(coordinates.xy,p)+weights.y*guarded_colour(coordinates.zw,p);
    return colour_result(floor(saturate(rgb)*255.+.5)/255.,original);
}
float4 aa_supersample_fragment_main(Fullscreen input) : SV_Target0 {
    uint2 p=uint2(input.position.xy);
    // This entry binds native ink as edgesTex and high-resolution ownership
    // as blendTex. Ownership B is the raster class, not source colour/alpha.
    float4 native=edgesTex.Load(int3(p,0));
    if(round(ownership.Load(int3(p,0)).b*255.)==0) return native;
    float3 sum=0;uint count=0;
    for(uint y=0;y<type;++y) for(uint x=0;x<type;++x) {
        uint2 q=p*type+uint2(x,y);
        // Do not smear tiny HUD/emissive strokes into adjacent world pixels.
        // Keep native coverage for those strokes rather than box-filtering it.
        if(round(blendTex.Load(int3(q,0)).b*255.)==0) continue;
        sum+=source.Load(int3(q,0)).rgb;++count;
    }
    // Texture loads and the sRGB render target do the linear-light conversion.
    // Opacity retains the original native raster's exact value.
    return count?float4(sum/count,native.a):native;
}
