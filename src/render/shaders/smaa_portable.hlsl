// SMAA 1x compute adapter. Algorithm and lookup tables are upstream SMAA.
Texture2D<float4> colorTex : register(t0,space0);
Texture2D<float4> edgesTex : register(t1,space0);
Texture2D<float4> blendTex : register(t2,space0);
Texture2D<float4> areaTex : register(t3,space0);
Texture2D<float4> searchTex : register(t4,space0);
SamplerState linearClamp : register(s0,space0);
StructuredBuffer<uint> packedPixels : register(t5,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> target : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint width,height,stage,quality,packedTags;uint3 pad;};
#define SMAA_RT_METRICS float4(1.0/width,1.0/height,width,height)
#define SMAA_CUSTOM_SL 1
#define SMAATexture2D(tex) Texture2D<float4> tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex,coord) tex.SampleLevel(linearClamp,coord,0)
#define SMAASampleLevelZeroPoint(tex,coord) tex.Load(int3(clamp(int2((coord)*float2(width,height)),int2(0,0),int2(width-1,height-1)),0))
#define SMAASampleLevelZeroOffset(tex,coord,offset) tex.SampleLevel(linearClamp,coord,0,offset)
#define SMAASample(tex,coord) SMAASampleLevelZero(tex,coord)
#define SMAASamplePoint(tex,coord) SMAASampleLevelZeroPoint(tex,coord)
#define SMAASampleOffset(tex,coord,offset) SMAASampleLevelZeroOffset(tex,coord,offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#define SMAA_THRESHOLD (quality==1?.15:.1)
#define SMAA_MAX_SEARCH_STEPS (quality==1?4:(quality==2?8:16))
#define SMAA_MAX_SEARCH_STEPS_DIAG (quality<3?0:8)
#define SMAA_CORNER_ROUNDING (quality<3?100:25)
// Pixel-shader discard becomes a zero-edge result in compute. Every output
// pixel is written each pass, so no stale weights survive a disappearing edge.
#define discard return float2(0,0)
#include "../../../third_party/smaa/SMAA.hlsl"
#undef discard
bool protectedPixel(uint2 p) {
    p=min(p,uint2(width-1,height-1));uint i=p.y*width+p.x;
    uint tag=packedTags!=0?(packedPixels[i]>>8)&255:(packedPixels[i/4]>>((i&3)*8))&255;
    return tag==1 || tag==2 || tag==4;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint2 p=id.xy;if(p.x>=width || p.y>=height) return;
    float2 uv=(float2(p)+.5)/float2(width,height);
    if(stage==0) {
        float4 offsets[3];SMAAEdgeDetectionVS(uv,offsets);
        float2 edges=SMAAColorEdgeDetectionPS(uv,offsets,colorTex);
        if(protectedPixel(p)) edges=0;
        else {
            if(p.x==0 || protectedPixel(p-uint2(1,0))) edges.x=0;
            if(p.y==0 || protectedPixel(p-uint2(0,1))) edges.y=0;
        }
        target[p]=float4(edges,0,0);
    } else if(stage==1) {
        float2 pixel;float4 offsets[3];SMAABlendingWeightCalculationVS(uv,pixel,offsets);
        target[p]=SMAABlendingWeightCalculationPS(uv,pixel,offsets,edgesTex,areaTex,searchTex,0);
    } else {
        float4 current=colorTex.Load(int3(p,0)),offset;
        SMAANeighborhoodBlendingVS(uv,offset);
        float4 blended=SMAANeighborhoodBlendingPS(uv,offset,colorTex,blendTex);
        target[p]=protectedPixel(p)?current:float4(blended.rgb,current.a);
    }
}
